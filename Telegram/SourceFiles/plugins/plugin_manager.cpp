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
#include "api/api_sending.h"
#include "api/api_editing.h"
#include "calls/calls_instance.h"
#include "calls/calls_call.h"
#include "storage/file_upload.h"
#include "window/window_session_controller.h"
#include "core/application.h"
#include "base/unixtime.h"
#include "webrtc/webrtc_video_track.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <crl/crl_on_main.h>
#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>
#include <atomic>

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
	LOG(("PluginManager: setEnabled(%1) for plugin '%2'").arg(enabled).arg(_name));
	if (_enabled) {
		if (!load()) {
			_enabled = false;
			LOG(("PluginManager: load() failed for plugin '%1'").arg(_name));
		}
	} else {
		unload();
	}
}

void PluginState::clearLogs() {
	_logs.clear();
	_logsUpdated.fire({});
}

void PluginState::addLogError(const QString &msg) {
	addLog(msg);
}

bool PluginState::reload() {
	if (!_enabled) {
		return false; // Can't reload if not enabled
	}
	const bool wasEnabled = _enabled;
	unload();
	_enabled = wasEnabled; // Restore enabled state
	return load();
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
	LOG(("PluginManager: Lua script loaded successfully for plugin '%1'").arg(_name));

	// Call onLoad() if defined - wrap in try-catch to prevent crashes
	try {
		callHook("onLoad");
	} catch (...) {
		addLog("[ERROR] Exception in onLoad hook");
		// Don't fail loading if onLoad crashes, just log it
	}
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
	LOG(("PluginManager[%1]: %2").arg(_name, msg));
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

// Helper to push QString to Lua stack
static void pushQString(lua_State *L, const QString &str) {
	const auto utf = str.toUtf8();
	lua_pushlstring(L, utf.constData(), utf.size());
}

// Helper to call a Lua hook with string arguments
// Returns true if hook was called successfully, false if hook doesn't exist or error occurred
bool callLuaHook(
	lua_State *L,
	PluginState *self,
	const char *hookName,
	const std::vector<QString> &stringArgs,
	const std::vector<bool> &boolArgs = {},
	const std::vector<int64> &intArgs = {}) {
	if (!L) return false;
	lua_getglobal(L, hookName);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return false; // Hook not defined — normal, not an error
	}

	LOG(("PluginManager: calling Lua hook '%1'").arg(hookName));
	int argCount = 0;
	for (const auto &str : stringArgs) {
		pushQString(L, str);
		argCount++;
	}
	for (const bool b : boolArgs) {
		lua_pushboolean(L, b ? 1 : 0);
		argCount++;
	}
	for (const int64 i : intArgs) {
		lua_pushinteger(L, i);
		argCount++;
	}
	
	if (lua_pcall(L, argCount, 0, 0) != LUA_OK) {
		const QString err = QString::fromUtf8(lua_tostring(L, -1));
		if (self) {
			self->addLogError("[ERROR] Hook " + QString(hookName) + " failed: " + err);
		}
		lua_pop(L, 1);
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
	callLuaHook(_L, this, "onNewMessage", {chatName, senderName, text}, {isOutgoing});
}

void PluginState::fireMessageEdited(
	const QString &chatName,
	const QString &senderName,
	const QString &oldText,
	const QString &newText,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageEdited", {chatName, senderName, oldText, newText}, {isOutgoing});
}

void PluginState::fireMessageDeleted(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageDeleted", {chatName, senderName, text}, {isOutgoing});
}

void PluginState::fireMessageSent(
	const QString &chatName,
	const QString &text,
	bool isScheduled)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageSent", {chatName, text}, {isScheduled});
}

void PluginState::fireMessageReaction(
	const QString &chatName,
	const QString &senderName,
	const QString &text,
	const QString &reaction,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onMessageReaction", {chatName, senderName, text, reaction}, {isOutgoing});
}

void PluginState::firePeerUpdated(
	const QString &peerName,
	const QString &updateType)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPeerUpdated", {peerName, updateType});
}

void PluginState::fireUserOnlineStatusChanged(
	const QString &userName,
	bool isOnline)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onUserOnlineStatusChanged", {userName}, {isOnline});
}

void PluginState::fireChatOpened(const QString &chatName)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onChatOpened", {chatName});
}

void PluginState::fireChatClosed(const QString &chatName)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onChatClosed", {chatName});
}

void PluginState::fireCallStarted(
	const QString &userName,
	bool isVideo,
	bool isOutgoing)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onCallStarted", {userName}, {isVideo, isOutgoing});
}

void PluginState::fireCallEnded(
	const QString &userName,
	const QString &reason)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onCallEnded", {userName, reason});
}

void PluginState::fireFileUploadStarted(
	const QString &fileName,
	int64 fileSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadStarted", {fileName}, {}, {fileSize});
}

void PluginState::fireFileUploadProgress(
	const QString &fileName,
	int64 uploaded,
	int64 total)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadProgress", {fileName}, {}, {uploaded, total});
}

void PluginState::fireFileUploadCompleted(
	const QString &fileName,
	bool success)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileUploadCompleted", {fileName}, {success});
}

void PluginState::fireFileDownloadStarted(
	const QString &fileName,
	int64 fileSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadStarted", {fileName}, {}, {fileSize});
}

void PluginState::fireFileDownloadProgress(
	const QString &fileName,
	int64 downloaded,
	int64 total)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadProgress", {fileName}, {}, {downloaded, total});
}

void PluginState::fireFileDownloadCompleted(
	const QString &fileName,
	bool success)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onFileDownloadCompleted", {fileName}, {success});
}

void PluginState::firePacketReceived(
	const QString &packetType,
	int packetSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPacketReceived", {packetType}, {}, {packetSize});
}

void PluginState::firePacketSent(
	const QString &packetType,
	int packetSize)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onPacketSent", {packetType}, {}, {packetSize});
}

void PluginState::fireConnectionStateChanged(
	const QString &state)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onConnectionStateChanged", {state});
}

void PluginState::fireHistoryUpdated(
	const QString &chatName,
	const QString &updateType)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onHistoryUpdated", {chatName, updateType});
}

void PluginState::fireUnreadCountChanged(
	const QString &chatName,
	int unreadCount)
{
	if (!_L || !_enabled) return;
	callLuaHook(_L, this, "onUnreadCountChanged", {chatName}, {}, {unreadCount});
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

	LOG(("PluginManager: setSession called, %1 plugins loaded").arg(_plugins.size()));

	// Helper lambda to fire events to all enabled plugins
	const auto fireToAll = [this](auto &&func) {
		for (auto &plugin : _plugins) {
			if (plugin->enabled() && plugin->isLoaded()) {
				func(plugin.get());
			}
		}
	};

	// Message Updates: New messages — use realtime to fire immediately, not batched
	session->changes().realtimeMessageUpdates(
		Data::MessageUpdate::Flag::NewAdded
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		if (!item) return;
		const auto history = item->history();
		if (!history) return;
		const auto peer = history->peer;
		const QString chatName = peer ? peer->name() : u"unknown"_q;
		const QString senderName = (item->from() && item->from() != peer)
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();

		LOG(("PluginManager: onNewMessage fired, chat=%1, sender=%2").arg(chatName, senderName));
		fireToAll([=](PluginState *plugin) {
			plugin->fireNewMessage(chatName, senderName, text, isOutgoing);
		});
	}, _sessionLifetime);

	// Message Updates: Edited messages
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::Edited
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString newText = item->originalText().text;
		// For edited messages, we don't have the old text easily accessible
		// So we'll pass the new text as both old and new for now
		const QString oldText = newText; // TODO: Store previous text if needed
		const bool isOutgoing = item->out();

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageEdited(chatName, senderName, oldText, newText, isOutgoing);
		});
	}, _sessionLifetime);

	// Message Updates: Deleted messages
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::Destroyed
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageDeleted(chatName, senderName, text, isOutgoing);
		});
	}, _sessionLifetime);

	// Message Updates: Reactions
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::NewUnreadReaction
	) | rpl::start_with_next([=](const Data::MessageUpdate &update) {
		const auto item = update.item;
		const auto history = item->history();
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString senderName = item->from()
			? item->from()->name()
			: chatName;
		const QString text = item->originalText().text;
		const bool isOutgoing = item->out();
		// Extract reaction info if available
		const QString reaction = "unknown"; // TODO: Extract actual reaction

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageReaction(chatName, senderName, text, reaction, isOutgoing);
		});
	}, _sessionLifetime);

	// History Updates: Message sent
	session->changes().historyUpdates(
		Data::HistoryUpdate::Flag::MessageSent
	) | rpl::start_with_next([=](const Data::HistoryUpdate &update) {
		const auto history = update.history;
		const auto peer = history->peer;
		const QString chatName = peer->name();
		// Get the last sent message
		const QString text = "Message sent"; // TODO: Get actual message text
		const bool isScheduled = false;

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageSent(chatName, text, isScheduled);
		});
	}, _sessionLifetime);

	// History Updates: Scheduled message sent
	session->changes().historyUpdates(
		Data::HistoryUpdate::Flag::ScheduledSent
	) | rpl::start_with_next([=](const Data::HistoryUpdate &update) {
		const auto history = update.history;
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const QString text = "Scheduled message sent";
		const bool isScheduled = true;

		fireToAll([=](PluginState *plugin) {
			plugin->fireMessageSent(chatName, text, isScheduled);
		});
	}, _sessionLifetime);

	// History Updates: Unread count changed
	session->changes().historyUpdates(
		Data::HistoryUpdate::Flag::UnreadView
	) | rpl::start_with_next([=](const Data::HistoryUpdate &update) {
		const auto history = update.history;
		const auto peer = history->peer;
		const QString chatName = peer->name();
		const int unreadCount = history->unreadCount();

		fireToAll([=](PluginState *plugin) {
			plugin->fireUnreadCountChanged(chatName, unreadCount);
		});
	}, _sessionLifetime);

	// Peer Updates: Various peer changes
	session->changes().peerUpdates(
		Data::PeerUpdate::Flag::Name
		| Data::PeerUpdate::Flag::Photo
		| Data::PeerUpdate::Flag::OnlineStatus
		| Data::PeerUpdate::Flag::About
		| Data::PeerUpdate::Flag::Username
	) | rpl::start_with_next([=](const Data::PeerUpdate &update) {
		const auto peer = update.peer;
		const QString peerName = peer->name();
		QString updateType = "unknown";
		
		if (update.flags & Data::PeerUpdate::Flag::Name) {
			updateType = "name";
		} else if (update.flags & Data::PeerUpdate::Flag::Photo) {
			updateType = "photo";
		} else if (update.flags & Data::PeerUpdate::Flag::OnlineStatus) {
			updateType = "online_status";
			// Also fire online status change for users
			if (const auto user = peer->asUser()) {
				const auto lastseen = user->lastseen();
				const bool isOnline = lastseen.isOnline(::base::unixtime::now());
				fireToAll([=](PluginState *plugin) {
					plugin->fireUserOnlineStatusChanged(peerName, isOnline);
				});
			}
		} else if (update.flags & Data::PeerUpdate::Flag::About) {
			updateType = "about";
		} else if (update.flags & Data::PeerUpdate::Flag::Username) {
			updateType = "username";
		}

		fireToAll([=](PluginState *plugin) {
			plugin->firePeerUpdated(peerName, updateType);
		});
	}, _sessionLifetime);

	// Call events
	Core::App().calls().currentCallValue(
	) | rpl::start_with_next([=](Calls::Call *call) {
		if (!call) return;
		const auto user = call->user();
		const QString userName = user->name();
		// Check if video call by checking remote video state (simplified)
		const bool isVideo = (call->remoteVideoState() != Webrtc::VideoState::Inactive);
		const bool isOutgoing = call->type() == Calls::Call::Type::Outgoing;
		
		fireToAll([=](PluginState *plugin) {
			plugin->fireCallStarted(userName, isVideo, isOutgoing);
		});
	}, _sessionLifetime);

	// Hook into call state changes - we'll monitor when calls end
	// Note: This is a simplified approach; full implementation would track call lifecycle
	Core::App().calls().currentCallValue(
	) | rpl::filter([=](Calls::Call *call) {
		return call == nullptr; // Call ended
	}) | rpl::start_with_next([=] {
		// When call becomes null, it means call ended
		// We'll use a simple approach here
		fireToAll([=](PluginState *plugin) {
			plugin->fireCallEnded("Unknown", "ended");
		});
	}, _sessionLifetime);

	// File upload events - Photo uploads
	session->uploader().photoProgress(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadProgress("photo", 0, 0);
		});
	}, _sessionLifetime);

	session->uploader().photoReady(
	) | rpl::start_with_next([=](const Storage::UploadedMedia &media) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("photo", true);
		});
	}, _sessionLifetime);

	session->uploader().photoFailed(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("photo", false);
		});
	}, _sessionLifetime);

	// Document uploads
	session->uploader().documentProgress(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadProgress("document", 0, 0);
		});
	}, _sessionLifetime);

	session->uploader().documentReady(
	) | rpl::start_with_next([=](const Storage::UploadedMedia &media) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("document", true);
		});
	}, _sessionLifetime);

	session->uploader().documentFailed(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("document", false);
		});
	}, _sessionLifetime);

	// Secure uploads (passport files)
	session->uploader().secureProgress(
	) | rpl::start_with_next([=](const Storage::UploadSecureProgress &progress) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadProgress("secure_file", progress.offset, progress.size);
		});
	}, _sessionLifetime);

	session->uploader().secureReady(
	) | rpl::start_with_next([=](const Storage::UploadSecureDone &done) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("secure_file", true);
		});
	}, _sessionLifetime);

	session->uploader().secureFailed(
	) | rpl::start_with_next([=](const FullMsgId &fullId) {
		fireToAll([=](PluginState *plugin) {
			plugin->fireFileUploadCompleted("secure_file", false);
		});
	}, _sessionLifetime);

	// File download events - Note: Download manager API is more complex
	// We'll add basic hooks that can be extended later

	// Note: Download progress and start events would need additional hooks
	// in the download manager. For now, we'll add basic completion hooks.

	// Network packet events will be added by modifying connection code
	// See connection_tcp.cpp modifications below
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

void Manager::firePacketReceived(const QString &packetType, int packetSize) {
	// Rate limit: at most 1 dispatch per 100ms to avoid flooding the main thread
	static std::atomic<qint64> sLastRxFire{ 0 };
	const auto now = QDateTime::currentMSecsSinceEpoch();
	const auto prev = sLastRxFire.load(std::memory_order_relaxed);
	if (now - prev < 100) return;
	sLastRxFire.store(now, std::memory_order_relaxed);

	// Marshal to main thread — TCP hooks are called from background threads
	crl::on_main([this, packetType, packetSize] {
		for (auto &plugin : _plugins) {
			if (plugin->enabled() && plugin->isLoaded()) {
				plugin->firePacketReceived(packetType, packetSize);
			}
		}
	});
}

void Manager::firePacketSent(const QString &packetType, int packetSize) {
	// Rate limit: at most 1 dispatch per 100ms
	static std::atomic<qint64> sLastTxFire{ 0 };
	const auto now = QDateTime::currentMSecsSinceEpoch();
	const auto prev = sLastTxFire.load(std::memory_order_relaxed);
	if (now - prev < 100) return;
	sLastTxFire.store(now, std::memory_order_relaxed);

	// Marshal to main thread
	crl::on_main([this, packetType, packetSize] {
		for (auto &plugin : _plugins) {
			if (plugin->enabled() && plugin->isLoaded()) {
				plugin->firePacketSent(packetType, packetSize);
			}
		}
	});
}

void Manager::fireConnectionStateChanged(const QString &state) {
	// Marshal to main thread — may be called from background threads
	crl::on_main([this, state] {
		for (auto &plugin : _plugins) {
			if (plugin->enabled() && plugin->isLoaded()) {
				plugin->fireConnectionStateChanged(state);
			}
		}
	});
}

} // namespace Plugins
