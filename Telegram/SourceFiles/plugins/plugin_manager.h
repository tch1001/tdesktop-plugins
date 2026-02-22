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
	[[nodiscard]] const std::vector<LogEntry> &logs() const { return _logs; }
	[[nodiscard]] rpl::producer<> logsUpdated() const;
	[[nodiscard]] QString lastError() const { return _lastError; }

	void setEnabled(bool enabled);
	void clearLogs();
	bool reload(); // Reload the plugin from disk

	// Called by Manager to dispatch events
	void fireNewMessage(
		const QString &chatName,
		const QString &senderName,
		const QString &text,
		bool isOutgoing);
	void fireMessageEdited(
		const QString &chatName,
		const QString &senderName,
		const QString &oldText,
		const QString &newText,
		bool isOutgoing);
	void fireMessageDeleted(
		const QString &chatName,
		const QString &senderName,
		const QString &text,
		bool isOutgoing);
	void fireMessageSent(
		const QString &chatName,
		const QString &text,
		bool isScheduled);
	void fireMessageReaction(
		const QString &chatName,
		const QString &senderName,
		const QString &text,
		const QString &reaction,
		bool isOutgoing);
	void firePeerUpdated(
		const QString &peerName,
		const QString &updateType);
	void fireUserOnlineStatusChanged(
		const QString &userName,
		bool isOnline);
	void fireChatOpened(const QString &chatName);
	void fireChatClosed(const QString &chatName);
	void fireCallStarted(
		const QString &userName,
		bool isVideo,
		bool isOutgoing);
	void fireCallEnded(
		const QString &userName,
		const QString &reason);
	void fireFileUploadStarted(
		const QString &fileName,
		int64 fileSize);
	void fireFileUploadProgress(
		const QString &fileName,
		int64 uploaded,
		int64 total);
	void fireFileUploadCompleted(
		const QString &fileName,
		bool success);
	void fireFileDownloadStarted(
		const QString &fileName,
		int64 fileSize);
	void fireFileDownloadProgress(
		const QString &fileName,
		int64 downloaded,
		int64 total);
	void fireFileDownloadCompleted(
		const QString &fileName,
		bool success);
	void firePacketReceived(
		const QString &packetType,
		int packetSize);
	void firePacketSent(
		const QString &packetType,
		int packetSize);
	void fireConnectionStateChanged(
		const QString &state);
	void fireHistoryUpdated(
		const QString &chatName,
		const QString &updateType);
	void fireUnreadCountChanged(
		const QString &chatName,
		int unreadCount);

	// Public method for error logging from hooks
	void addLogError(const QString &msg);

private:
	void addLog(const QString &msg);
	bool load();
	void unload();
	bool callHook(const char *hookName, int nargs = 0, int nret = 0);
	static int luaPrint(lua_State *L);
	static int luaLog(lua_State *L);

	QString _name;
	QString _path;
	bool _enabled = false;
	lua_State *_L = nullptr;
	std::vector<LogEntry> _logs;
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

	// Called from connection layer to fire network events
	void firePacketReceived(const QString &packetType, int packetSize);
	void firePacketSent(const QString &packetType, int packetSize);
	void fireConnectionStateChanged(const QString &state);

private:
	Manager() = default;

	std::vector<std::unique_ptr<PluginState>> _plugins;
	rpl::lifetime _sessionLifetime;
	rpl::event_stream<> _pluginsChanged;
};

} // namespace Plugins
