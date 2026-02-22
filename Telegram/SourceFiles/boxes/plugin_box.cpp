/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/plugin_box.h"

#include "plugins/plugin_manager.h"
#include "core/file_utilities.h"
#include "core/application.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/vertical_list.h"
#include "ui/painter.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"

#include <QtCore/QDateTime>
#include <QtWidgets/QScrollBar>

namespace {

void ShowLogsBox(
	not_null<Ui::GenericBox*> parentBox,
	not_null<Plugins::PluginState*> plugin)
{
	parentBox->getDelegate()->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Logs: %1"_q.arg(plugin->name())));
		box->setWidth(st::boxWideWidth);
		box->setMaxHeight(500);

		const auto layout = box->verticalLayout();

		const auto rebuildLogs = [=] {
			while (layout->count() > 0) {
				delete layout->widgetAt(0);
			}
                    const auto &logs = plugin->logs();
                    if (logs.empty()) {
				layout->add(
					object_ptr<Ui::FlatLabel>(
						layout,
						u"No logs yet."_q,
						st::boxLabel),
					st::boxRowPadding);
			} else {
				for (const auto &entry : logs) {
					const auto line = u"[%1] %2"_q
						.arg(entry.timestamp.toString(u"hh:mm:ss"_q))
						.arg(entry.message);
					layout->add(
						object_ptr<Ui::FlatLabel>(
							layout,
							line,
							st::boxLabel),
						st::boxRowPadding);
				}
			}
			layout->resizeToWidth(layout->width());
		};

		rebuildLogs();

		plugin->logsUpdated() | rpl::start_with_next([=] {
			rebuildLogs();
		}, box->lifetime());

		box->addButton(rpl::single(u"Clear"_q), [=] {
			plugin->clearLogs();
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void RebuildPluginList(
	not_null<Ui::VerticalLayout*> container,
	not_null<Ui::GenericBox*> box)
{
	// Clear existing children
	while (container->count() > 0) {
		delete container->widgetAt(0);
	}

	auto &manager = Plugins::Manager::instance();
	const auto &plugins = manager.plugins();

	if (plugins.empty()) {
		container->add(
			object_ptr<Ui::FlatLabel>(
				container,
				u"No plugins loaded. Click \"Load Script...\" to add one."_q,
				st::boxLabel),
			st::boxRowPadding);
		container->resizeToWidth(container->width());
		return;
	}

	for (const auto &plugin : plugins) {
		auto *raw = plugin.get();

		const auto row = container->add(
			object_ptr<Ui::SettingsButton>(
				container,
				rpl::single(raw->name()),
				st::defaultSettingsButton));

		row->toggleOn(rpl::single(raw->enabled()));

		row->toggledChanges() | rpl::start_with_next([raw](bool enabled) {
			raw->setEnabled(enabled);
		}, row->lifetime());

		// View logs on right click / context menu would be nice, but for
		// simplicity we add a second label-button below the toggle row.
		const auto logsBtn = container->add(
			object_ptr<Ui::LinkButton>(
				container,
				u"View Logs"_q),
			QMargins(
				st::boxRowPadding.left() + 20,
				0,
				st::boxRowPadding.right(),
				4));

		logsBtn->setClickedCallback([raw, box] {
			ShowLogsBox(box, raw);
		});

		const auto removeBtn = container->add(
			object_ptr<Ui::LinkButton>(
				container,
				u"Remove"_q),
			QMargins(
				st::boxRowPadding.left() + 20,
				0,
				st::boxRowPadding.right(),
				8));

		removeBtn->setClickedCallback([raw, container, box] {
			Plugins::Manager::instance().removePlugin(raw);
			// The pluginsChanged signal will trigger a rebuild
		});

		Ui::AddDivider(container);
	}

	container->resizeToWidth(container->width());
}

} // namespace

void PluginsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller)
{
	box->setTitle(rpl::single(u"Lua Plugins"_q));
	box->setWidth(st::boxWideWidth);
	box->setMaxHeight(500);

	// Plugin list area
	const auto content = box->verticalLayout();

	const auto pluginList = content->add(
		object_ptr<Ui::VerticalLayout>(content));

	const auto rebuild = [=] {
		RebuildPluginList(pluginList, box);
	};

	rebuild();

	// Rebuild on list changes
	Plugins::Manager::instance().pluginsChanged() | rpl::start_with_next([=] {
		rebuild();
	}, box->lifetime());

	box->addButton(rpl::single(u"Load Script..."_q), [=] {
		FileDialog::GetOpenPath(
			Core::App().getFileDialogParent(),
			u"Select Lua Script"_q,
			u"Lua scripts (*.lua)"_q,
			[=](FileDialog::OpenResult &&result) {
				if (result.paths.isEmpty()) return;
				const auto path = result.paths.front();
				auto &mgr = Plugins::Manager::instance();
				auto *plugin = mgr.loadPlugin(path);
				plugin->setEnabled(true);
			});
	});

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}
