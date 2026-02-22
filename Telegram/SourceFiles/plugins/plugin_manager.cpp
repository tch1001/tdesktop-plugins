/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugin_manager.h"

#include "data/data_changes.h"
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>

namespace Plugins {

//
// PluginState
//

PluginState::PluginState(const QString &path)
: _name(QFileInfo(path).baseName())
, _path(path) {
}

PluginState::~PluginState() {
	unload();
}

rpl::producer<> PluginState::logsUpdated() const {
	return _logsUpdated.events();
}

void PluginState::setEnabled(bool enabled) {
	if (_enabled == enabled) return;
	_enabled = enabled;
	if (_enabled) {
		if (!load()) {
			_enabled = false;
		}
	} else {
		unload();
	}
}

void PluginState::clearLogs() {
	_logs.clear();
	_logsUpdated.fire({});
}

bool PluginState::load() {
	if (_L) {
		unload();
	}
	_L = luaL_newstate();
	if (!_L) {
		_lastError = "Failed to create Lua state";
		addLog("[ERROR] " + _lastError);
		return false;
	}

	luaL_openlibs(_L);

	// Register custom print/log functions that write to our log buffer
	lua_pushlightuserdata(_L, this);
	lua_pushcclosure(_L, &PluginState::luaLog, 1);
	lua_setglobal(_L, "log");

	// Override print() to also write to our buffer
	lua_pushlightuserdata(_L, this);
	lua_pushcclosure(_L, &PluginState::luaPrint, 1);
	lua_setglobal(_L, "print");

	const auto pathUtf = _path.toUtf8();
	const int result = luaL_loadfile(_L, pathUtf.constData());
	if (result != LUA_OK) {
		_lastError = QString::fromUtf8(lua_tostring(_L, -1));
		addLog("[ERROR] Load failed: " + _lastError);
		lua_close(_L);
		_L = nullptr;
		return false;
	}

	// Execute the script body (define functions)
	if (lua_pcall(_L, 0, 0, 0) != LUA_OK) {
		_lastError = QString::fromUtf8(lua_tostring(_L, -1));
		addLog("[ERROR] Run failed: " + _lastError);
		lua_pop(_L, 1);
		lua_close(_L);
		_L = nullptr;
		return false;
	}

	addLog("[INFO] Plugin loaded: " + _name);

	// Call onLoad() if defined
	callHook("onLoad");
	return true;
}

void PluginState::unload() {
	if (!_L) return;
	callHook("onUnload");
	lua_close(_L);
	_L = nullptr;
	addLog("[INFO] Plugin unloaded: " + _name);
}

void PluginState::addLog(const QString &msg) {
	_logs.push_back({ msg, QDateTime::currentDateTime() });
	_logsUpdated.fire({});
}

bool PluginState::callHook(const char *hookName, int nargs, int nret) {
	if (!_L) return false;
	lua_getglobal(_L, hookName);
	if (!lua_isfunction(_L, -1)) {
		lua_pop(_L, 1);
		// Remove the extra args if pushed
		if (nargs > 0) lua_pop(_L, nargs);
		return false;
	}
	// Move function before args (args were pushed before this call in some variants)
	// For simplicity, we use a convention: args are pushed AFTER calling this,
	// so here we just call with no args from the stack except what was passed.
	if (lua_pcall(_L, nargs, nret, 0) != LUA_OK) {
		const QString err = QString::fromUtf8(lua_tostring(_L, -1));
		addLog("[ERROR] Hook " + QString(hookName) + " failed: " + err);
		lua_pop(_L, 1);
		return false;
	}
	return true;
}

void PluginState::fireNewMessage(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;

	lua_getglobal(_L, "onNewMessage");
	if (!lua_isfunction(_L, -1)) {
		lua_pop(_L, 1);
		return;
	}

	const auto chatUtf = chatName.toUtf8();
	const auto senderUtf = senderName.toUtf8();
	const auto textUtf = text.toUtf8();

	lua_pushlstring(_L, chatUtf.constData(), chatUtf.size());
	lua_pushlstring(_L, senderUtf.constData(), senderUtf.size());
	lua_pushlstring(_L, textUtf.constData(), textUtf.size());
	lua_pushboolean(_L, isOutgoing ? 1 : 0);

	if (lua_pcall(_L, 4, 0, 0) != LUA_OK) {
		const QString err = QString::fromUtf8(lua_tostring(_L, -1));
		addLog("[ERROR] onNewMessage failed: " + err);
		lua_pop(_L, 1);
	}
}

int PluginState::luaPrint(lua_State *L) {
	auto *self = static_cast<PluginState*>(lua_touserdata(L, lua_upvalueindex(1)));
	const int n = lua_gettop(L);
	QString msg;
	for (int i = 1; i <= n; i++) {
		if (i > 1) msg += '\t';
		if (lua_isstring(L, i)) {
			msg += QString::fromUtf8(lua_tostring(L, i));
		} else {
			lua_getglobal(L, "tostring");
			lua_pushvalue(L, i);
			lua_call(L, 1, 1);
			msg += QString::fromUtf8(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	self->addLog(msg);
	return 0;
}

int PluginState::luaLog(lua_State *L) {
	return luaPrint(L);
}

//
// Manager
//

Manager &Manager::instance() {
	static Manager inst;
	return inst;
}

void Manager::setSession(not_null<Main::Session*> session) {
	_sessionLifetime.destroy();

	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::NewAdded
	) | rpl::start_with_next([this](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();

		for (auto &plugin : _plugins) {
			if (plugin->enabled() && plugin->isLoaded()) {
				plugin->fireNewMessage(chatName, senderName, text, isOutgoing);
			}
		}
	}, _sessionLifetime);
}

void Manager::clearSession() {
	_sessionLifetime.destroy();
}

PluginState *Manager::loadPlugin(const QString &path) {
	// Don't load the same path twice
	for (const auto &p : _plugins) {
		if (p->path() == path) {
			return p.get();
		}
	}
	auto plugin = std::make_unique<PluginState>(path);
	auto *raw = plugin.get();
	_plugins.push_back(std::move(plugin));
	_pluginsChanged.fire({});
	return raw;
}

void Manager::removePlugin(PluginState *plugin) {
	_plugins.erase(
		std::remove_if(
			_plugins.begin(),
			_plugins.end(),
			[plugin](const std::unique_ptr<PluginState> &p) {
				return p.get() == plugin;
			}),
		_plugins.end());
	_pluginsChanged.fire({});
}

const std::vector<std::unique_ptr<PluginState>> &Manager::plugins() const {
	return _plugins;
}

rpl::producer<> Manager::pluginsChanged() const {
	return _pluginsChanged.events();
}

} // namespace Plugins
