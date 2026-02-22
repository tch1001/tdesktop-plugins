/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <rpl/event_stream.h>
#include <rpl/lifetime.h>

struct lua_State;

namespace Main {
class Session;
} // namespace Main

namespace Plugins {

struct LogEntry {
	QString message;
	QDateTime timestamp;
};

class PluginState {
public:
	explicit PluginState(const QString &path);
	~PluginState();

	[[nodiscard]] const QString &name() const { return _name; }
	[[nodiscard]] const QString &path() const { return _path; }
	[[nodiscard]] bool enabled() const { return _enabled; }
	[[nodiscard]] bool isLoaded() const { return _L != nullptr; }
	[[nodiscard]] const QVector<LogEntry> &logs() const { return _logs; }
	[[nodiscard]] rpl::producer<> logsUpdated() const;
	[[nodiscard]] QString lastError() const { return _lastError; }

	void setEnabled(bool enabled);
	void clearLogs();

	// Called by Manager to dispatch events
	void fireNewMessage(
		const QString &chatName,
		const QString &senderName,
		const QString &text,
		bool isOutgoing);

private:
	bool load();
	void unload();
	void addLog(const QString &msg);
	bool callHook(const char *hookName, int nargs = 0, int nret = 0);
	static int luaPrint(lua_State *L);
	static int luaLog(lua_State *L);

	QString _name;
	QString _path;
	bool _enabled = false;
	lua_State *_L = nullptr;
	QVector<LogEntry> _logs;
	QString _lastError;
	rpl::event_stream<> _logsUpdated;
};

class Manager {
public:
	[[nodiscard]] static Manager &instance();

	void setSession(not_null<Main::Session*> session);
	void clearSession();

	[[nodiscard]] PluginState *loadPlugin(const QString &path);
	void removePlugin(PluginState *plugin);
	[[nodiscard]] const std::vector<std::unique_ptr<PluginState>> &plugins() const;
	[[nodiscard]] rpl::producer<> pluginsChanged() const;

private:
	Manager() = default;

	std::vector<std::unique_ptr<PluginState>> _plugins;
	rpl::lifetime _sessionLifetime;
	rpl::event_stream<> _pluginsChanged;
};

} // namespace Plugins
