// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>

#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>
#include <QString>
#include <QTimer>
#include <QTreeView>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/xts_archive.h"
#include "core/loader/loader.h"
#include "ui_configure_per_game_addons.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_per_game_addons.h"
#include "qt_common/config/uisettings.h"

ConfigurePerGameAddons::ConfigurePerGameAddons(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui{std::make_unique<Ui::ConfigurePerGameAddons>()}, system{system_} {
    ui->setupUi(this);

    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setContextMenuPolicy(Qt::NoContextMenu);

    item_model->insertColumns(0, 2);
    item_model->setHeaderData(0, Qt::Horizontal, tr("Patch Name"));
    item_model->setHeaderData(1, Qt::Horizontal, tr("Version"));

    tree_view->header()->setStretchLastSection(false);
    tree_view->header()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    tree_view->header()->setMinimumSectionSize(150);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    ui->scrollArea->setEnabled(!system.IsPoweredOn());

    connect(item_model, &QStandardItemModel::itemChanged,
            [] { UISettings::values.is_game_list_reload_pending.exchange(true); });
}

ConfigurePerGameAddons::~ConfigurePerGameAddons() = default;

void ConfigurePerGameAddons::ApplyConfiguration() {
    std::vector<std::string> disabled_addons;

    // Helper function to recursively collect disabled items
    std::function<void(QStandardItem*)> collect_disabled = [&](QStandardItem* item) {
        if (item == nullptr) {
            return;
        }

        // Check if this item is disabled
        if (item->isCheckable() && item->checkState() == Qt::Unchecked) {
            // Use the stored key from UserRole, falling back to text
            const auto key = item->data(Qt::UserRole).toString();
            if (!key.isEmpty()) {
                disabled_addons.push_back(key.toStdString());
            } else {
                disabled_addons.push_back(item->text().toStdString());
            }
        }

        // Process children (for cheats under mods)
        for (int row = 0; row < item->rowCount(); ++row) {
            collect_disabled(item->child(row, 0));
        }
    };

    // Process all root items
    for (int row = 0; row < item_model->rowCount(); ++row) {
        collect_disabled(item_model->item(row, 0));
    }

    auto current = Settings::values.disabled_addons[title_id];
    std::sort(disabled_addons.begin(), disabled_addons.end());
    std::sort(current.begin(), current.end());
    if (disabled_addons != current) {
        Common::FS::RemoveFile(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                               "game_list" / fmt::format("{:016X}.pv.txt", title_id));
    }

    Settings::values.disabled_addons[title_id] = disabled_addons;
}

void ConfigurePerGameAddons::LoadFromFile(FileSys::VirtualFile file_) {
    file = std::move(file_);
    LoadConfiguration();
}

void ConfigurePerGameAddons::SetTitleId(u64 id) {
    this->title_id = id;
}

void ConfigurePerGameAddons::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigurePerGameAddons::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGameAddons::LoadConfiguration() {
    if (file == nullptr) {
        return;
    }

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, file);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    // Get the build ID from the main executable for cheat enumeration
    const auto build_id = pm.GetBuildID(update_raw);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    // Map to store parent items for mods (for adding cheat children)
    std::map<std::string, QStandardItem*> mod_items;

    for (const auto& patch : pm.GetPatches(update_raw, build_id)) {
        const auto name = QString::fromStdString(patch.name);

        // For cheats, we need to use the full key (parent::name) for storage
        std::string storage_key;
        if (patch.type == FileSys::PatchType::Cheat && !patch.parent_name.empty()) {
            storage_key = patch.parent_name + "::" + patch.name;
        } else {
            storage_key = patch.name;
        }

        auto* const first_item = new QStandardItem;
        first_item->setText(name);
        first_item->setCheckable(true);

        // Store the storage key as user data for later retrieval
        first_item->setData(QString::fromStdString(storage_key), Qt::UserRole);

        const auto patch_disabled =
            std::find(disabled.begin(), disabled.end(), storage_key) != disabled.end();

        first_item->setCheckState(patch_disabled ? Qt::Unchecked : Qt::Checked);

        auto* const version_item = new QStandardItem{QString::fromStdString(patch.version)};

        if (patch.type == FileSys::PatchType::Cheat && !patch.parent_name.empty()) {
            // This is a cheat - add as child of its parent mod
            auto parent_it = mod_items.find(patch.parent_name);
            if (parent_it != mod_items.end()) {
                parent_it->second->appendRow(QList<QStandardItem*>{first_item, version_item});
            } else {
                // Parent not found (shouldn't happen), add to root
                list_items.push_back(QList<QStandardItem*>{first_item, version_item});
                item_model->appendRow(list_items.back());
            }
        } else {
            // This is a top-level item (Update, Mod, DLC)
            list_items.push_back(QList<QStandardItem*>{first_item, version_item});
            item_model->appendRow(list_items.back());

            // Store mod items for later cheat attachment
            if (patch.type == FileSys::PatchType::Mod) {
                mod_items[patch.name] = first_item;
            }
        }
    }

    tree_view->expandAll();
    tree_view->resizeColumnToContents(1);
}
