/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"

class QWidget;
class QScrollArea;
class QPlainTextEdit;

namespace Plugins {
class PluginState;
} // namespace Plugins

namespace Ui {
class VerticalLayout;
} // namespace Ui

// ---- Plugin list window ----
class PluginsWindow : public QWidget {
public:
	explicit PluginsWindow(QWidget *parent = nullptr);
	~PluginsWindow();

private:
	void rebuildPluginList();
	void showLogsWindow(Plugins::PluginState *plugin);
	void loadNewPlugin();

	QScrollArea *_scrollArea = nullptr;
	Ui::VerticalLayout *_pluginList = nullptr;
	rpl::lifetime _lifetime;
};

// ---- Per-plugin log viewer ----
class PluginLogsWindow : public QWidget {
public:
	explicit PluginLogsWindow(Plugins::PluginState *plugin, QWidget *parent = nullptr);
	~PluginLogsWindow();

	// Call when re-showing a previously hidden window to catch up on missed logs
	void refreshLogs();

private:
	void appendNewLogs();
	bool isScrolledToBottom() const;
	void scrollToBottom();

	Plugins::PluginState *_plugin = nullptr;
	QPlainTextEdit *_logView = nullptr; // simple text editor — no layout headaches
	int _shownLogCount = 0;            // how many log entries are currently displayed
	rpl::lifetime _lifetime;
};

// Opens (or raises) the global plugins management window
void ShowPluginsWindow();
