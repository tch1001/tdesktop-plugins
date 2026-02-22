/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugin_window.h"

#include "plugins/plugin_manager.h"
#include "core/file_utilities.h"
#include "core/application.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "base/debug_log.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include "base/flat_map.h"

namespace {

base::unique_qptr<PluginsWindow> g_pluginsWindow;
base::flat_map<Plugins::PluginState*, base::unique_qptr<PluginLogsWindow>> g_logsWindows;

} // namespace

// ============================================================
// PluginsWindow
// ============================================================

PluginsWindow::PluginsWindow(QWidget *parent)
: QWidget(parent) {
	setWindowTitle("Lua Plugins");
	setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
	resize(600, 500);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(8, 8, 8, 8);
	mainLayout->setSpacing(4);

	// Title
	auto *titleLabel = new QLabel("Lua Plugins", this);
	titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; padding: 4px;");
	mainLayout->addWidget(titleLabel);

	// Scroll area for plugin list — Ui::VerticalLayout sits inside a scroll area
	_scrollArea = new QScrollArea(this);
	_scrollArea->setWidgetResizable(true);
	_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *scrollContent = new QWidget();
	auto *scrollLayout = new QVBoxLayout(scrollContent);
	scrollLayout->setContentsMargins(4, 4, 4, 4);
	scrollLayout->setSpacing(0);

	_pluginList = new Ui::VerticalLayout(scrollContent);
	scrollLayout->addWidget(_pluginList);
	scrollLayout->addStretch();

	_scrollArea->setWidget(scrollContent);
	mainLayout->addWidget(_scrollArea, 1);

	// Bottom buttons
	auto *btnRow = new QHBoxLayout();
	btnRow->setSpacing(8);

	auto *loadBtn = new QPushButton("Load Script...", this);
	connect(loadBtn, &QPushButton::clicked, this, &PluginsWindow::loadNewPlugin);
	btnRow->addWidget(loadBtn);
	btnRow->addStretch();

	auto *closeBtn = new QPushButton("Close", this);
	connect(closeBtn, &QPushButton::clicked, this, &PluginsWindow::close);
	btnRow->addWidget(closeBtn);

	mainLayout->addLayout(btnRow);

	// Initial build
	rebuildPluginList();

	// React to plugin list changes (always on main thread)
	Plugins::Manager::instance().pluginsChanged() | rpl::start_with_next([=] {
		rebuildPluginList();
	}, _lifetime);
}

PluginsWindow::~PluginsWindow() {
	if (g_pluginsWindow.get() == this) {
		g_pluginsWindow = nullptr;
	}
}

void PluginsWindow::rebuildPluginList() {
	if (!_pluginList) return;

	_pluginList->clear();

	auto &manager = Plugins::Manager::instance();
	const auto &plugins = manager.plugins();

	if (plugins.empty()) {
		_pluginList->add(
			object_ptr<Ui::FlatLabel>(
				_pluginList,
				u"No plugins loaded. Click \"Load Script...\" to add one."_q,
				st::boxLabel),
			st::boxRowPadding);
		return;
	}

	for (const auto &plugin : plugins) {
		auto *raw = plugin.get();

		// Toggle (enable/disable)
		const auto row = _pluginList->add(
			object_ptr<Ui::SettingsButton>(
				_pluginList,
				rpl::single(raw->name()),
				st::defaultSettingsButton));
		row->toggleOn(rpl::single(raw->enabled()));
		row->toggledChanges() | rpl::start_with_next([raw](bool enabled) {
			raw->setEnabled(enabled);
		}, row->lifetime());

		// Reload
		const auto reloadBtn = _pluginList->add(
			object_ptr<Ui::LinkButton>(
				_pluginList,
				u"Reload"_q),
			QMargins(st::boxRowPadding.left() + 20, 0, st::boxRowPadding.right(), 4));
		reloadBtn->setClickedCallback([raw] { raw->reload(); });

		// View Logs
		const auto logsBtn = _pluginList->add(
			object_ptr<Ui::LinkButton>(
				_pluginList,
				u"View Logs"_q),
			QMargins(st::boxRowPadding.left() + 20, 0, st::boxRowPadding.right(), 4));
		logsBtn->setClickedCallback([=] { showLogsWindow(raw); });

		// Remove
		const auto removeBtn = _pluginList->add(
			object_ptr<Ui::LinkButton>(
				_pluginList,
				u"Remove"_q),
			QMargins(st::boxRowPadding.left() + 20, 0, st::boxRowPadding.right(), 8));
		removeBtn->setClickedCallback([raw] {
			Plugins::Manager::instance().removePlugin(raw);
		});

		Ui::AddDivider(_pluginList);
	}
}

void PluginsWindow::showLogsWindow(Plugins::PluginState *plugin) {
	auto it = g_logsWindows.find(plugin);
	if (it != g_logsWindows.end()) {
		if (auto *w = it->second.get()) {
			w->refreshLogs(); // sync any logs added while hidden
			w->show();
			w->raise();
			w->activateWindow();
			return;
		}
	}
	auto win = base::make_unique_q<PluginLogsWindow>(plugin);
	auto *raw = win.get();
	g_logsWindows[plugin] = std::move(win);
	raw->show();
	raw->raise();
	raw->activateWindow();
}

void PluginsWindow::loadNewPlugin() {
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		u"Select Lua Script"_q,
		u"Lua scripts (*.lua)"_q,
		[=](FileDialog::OpenResult &&result) {
			if (result.paths.isEmpty()) return;
			auto &mgr = Plugins::Manager::instance();
			auto *plugin = mgr.loadPlugin(result.paths.front());
			if (plugin) {
				plugin->setEnabled(true);
			}
		});
}

// ============================================================
// PluginLogsWindow
// — Uses a plain QPlainTextEdit so there are zero layout sizing issues.
// ============================================================

PluginLogsWindow::PluginLogsWindow(Plugins::PluginState *plugin, QWidget *parent)
: QWidget(parent)
, _plugin(plugin)
, _shownLogCount(0) {
	setWindowTitle("Logs: " + plugin->name());
	setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
	resize(750, 520);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(8, 8, 8, 8);
	mainLayout->setSpacing(4);

	// Title row
	auto *titleLabel = new QLabel("Plugin Logs: " + plugin->name(), this);
	titleLabel->setStyleSheet("font-size: 13px; font-weight: bold;");
	mainLayout->addWidget(titleLabel);

	// Log view — plain text, read-only, monospaced, scrollable
	_logView = new QPlainTextEdit(this);
	_logView->setReadOnly(true);
	_logView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
	_logView->setMaximumBlockCount(0); // unlimited
	_logView->setFont(QFont("Courier New", 9));
	_logView->setStyleSheet(
		"QPlainTextEdit {"
		"  background: #1e1e1e;"
		"  color: #d4d4d4;"
		"  border: 1px solid #444;"
		"  padding: 4px;"
		"}");
	mainLayout->addWidget(_logView, 1);

	// Button row
	auto *btnRow = new QHBoxLayout();
	btnRow->setSpacing(8);

	auto *clearBtn = new QPushButton("Clear", this);
	connect(clearBtn, &QPushButton::clicked, [=] {
		_plugin->clearLogs();
		_logView->clear();
		_shownLogCount = 0;
		LOG(("PluginWindow: logs cleared for '%1'").arg(_plugin->name()));
	});
	btnRow->addWidget(clearBtn);

	auto *copyBtn = new QPushButton("Copy All", this);
	connect(copyBtn, &QPushButton::clicked, [=] {
		_logView->selectAll();
		_logView->copy();
		_logView->moveCursor(QTextCursor::End);
	});
	btnRow->addWidget(copyBtn);

	btnRow->addStretch();

	auto *closeBtn = new QPushButton("Close", this);
	connect(closeBtn, &QPushButton::clicked, this, &PluginLogsWindow::close);
	btnRow->addWidget(closeBtn);

	mainLayout->addLayout(btnRow);

	// Show all existing logs immediately
	refreshLogs();

	// Subscribe: logsUpdated fires on main thread → append directly
	plugin->logsUpdated() | rpl::start_with_next([=] {
		LOG(("PluginWindow: logsUpdated received for '%1', shownCount=%2, totalCount=%3")
			.arg(_plugin ? _plugin->name() : "null")
			.arg(_shownLogCount)
			.arg(_plugin ? (int)_plugin->logs().size() : -1));
		appendNewLogs();
	}, _lifetime);

	LOG(("PluginWindow: PluginLogsWindow created for '%1'").arg(plugin->name()));
}

PluginLogsWindow::~PluginLogsWindow() {
	auto it = g_logsWindows.find(_plugin);
	if (it != g_logsWindows.end() && it->second.get() == this) {
		g_logsWindows.erase(it);
	}
}

void PluginLogsWindow::refreshLogs() {
	if (!_plugin || !_logView) return;

	const auto &logs = _plugin->logs();
	const int total = static_cast<int>(logs.size());

	LOG(("PluginWindow: refreshLogs for '%1': shownCount=%2, totalCount=%3")
		.arg(_plugin->name())
		.arg(_shownLogCount)
		.arg(total));

	if (total == 0) {
		if (_shownLogCount > 0) {
			_logView->clear();
			_shownLogCount = 0;
		}
		if (_logView->toPlainText().isEmpty()) {
			_logView->setPlainText("(no logs yet)");
		}
		return;
	}

	// Full rebuild if count decreased (clear happened) or first time
	if (total < _shownLogCount) {
		_logView->clear();
		_shownLogCount = 0;
	}

	// Remove the "(no logs yet)" placeholder if it's there
	if (_shownLogCount == 0 && !_logView->toPlainText().isEmpty()) {
		_logView->clear();
	}

	// Append only the new entries
	const bool wasAtBottom = isScrolledToBottom();
	for (int i = _shownLogCount; i < total; ++i) {
		const auto &entry = logs[i];
		const QString line = u"[%1] %2"_q
			.arg(entry.timestamp.toString(u"hh:mm:ss"_q))
			.arg(entry.message);
		_logView->appendPlainText(line);
	}
	_shownLogCount = total;

	if (wasAtBottom) {
		scrollToBottom();
	}
}

void PluginLogsWindow::appendNewLogs() {
	if (!_plugin || !_logView) return;

	const auto &logs = _plugin->logs();
	const int total = static_cast<int>(logs.size());

	// Count went backwards → full refresh (e.g. clear was called)
	if (total < _shownLogCount) {
		_logView->clear();
		_shownLogCount = 0;
	}

	if (total == 0) {
		return;
	}

	// Remove placeholder text
	if (_shownLogCount == 0 && _logView->toPlainText() == "(no logs yet)") {
		_logView->clear();
	}

	const bool wasAtBottom = isScrolledToBottom();
	for (int i = _shownLogCount; i < total; ++i) {
		const auto &entry = logs[i];
		const QString line = u"[%1] %2"_q
			.arg(entry.timestamp.toString(u"hh:mm:ss"_q))
			.arg(entry.message);
		_logView->appendPlainText(line);
	}
	_shownLogCount = total;

	if (wasAtBottom) {
		scrollToBottom();
	}
}

bool PluginLogsWindow::isScrolledToBottom() const {
	if (!_logView) return true;
	auto *sb = _logView->verticalScrollBar();
	return sb->value() >= sb->maximum() - 4;
}

void PluginLogsWindow::scrollToBottom() {
	if (!_logView) return;
	_logView->verticalScrollBar()->setValue(
		_logView->verticalScrollBar()->maximum());
}

// ============================================================
// Global entry point
// ============================================================

void ShowPluginsWindow() {
	if (!g_pluginsWindow) {
		g_pluginsWindow = base::make_unique_q<PluginsWindow>(nullptr);
	}
	g_pluginsWindow->show();
	g_pluginsWindow->raise();
	g_pluginsWindow->activateWindow();
}
