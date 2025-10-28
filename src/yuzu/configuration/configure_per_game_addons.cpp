// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
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
#include "qt_common/config/uisettings.h"
#include "ui_configure_per_game_addons.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_per_game_addons.h"

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

    connect(item_model, &QStandardItemModel::itemChanged, this,
            &ConfigurePerGameAddons::OnItemChanged);
    connect(item_model, &QStandardItemModel::itemChanged,
            [] { UISettings::values.is_game_list_reload_pending.exchange(true); });
}

ConfigurePerGameAddons::~ConfigurePerGameAddons() = default;

void ConfigurePerGameAddons::ApplyConfiguration() {
    bool any_variant_checked = false;
    for (auto* v : update_variant_items) {
        if (v && v->checkState() == Qt::Checked) {
            any_variant_checked = true;
            break;
        }
    }
    if (any_variant_checked && default_update_item &&
        default_update_item->checkState() == Qt::Unchecked) {
        default_update_item->setCheckState(Qt::Checked);
    }

    std::vector<std::string> disabled_addons;

    for (const auto& item : list_items) {
        const auto disabled = item.front()->checkState() == Qt::Unchecked;
        if (disabled) {
            const auto key = item.front()->data(Qt::UserRole).toString();
            disabled_addons.push_back(key.toStdString());
        }
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

    // Reset model and caches to avoid duplicates
    item_model->removeRows(0, item_model->rowCount());
    list_items.clear();
    default_update_item = nullptr;
    update_variant_items.clear();

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, file);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    for (const auto& patch : pm.GetPatches(update_raw)) {
        const auto display_name = QString::fromStdString(patch.name);
        const auto version_q = QString::fromStdString(patch.version);

        QString toggle_key = display_name;
        const bool is_update = (patch.type == FileSys::PatchType::Update);
        const bool is_default_update_row = is_update && patch.version.empty();
        if (is_update) {
            if (is_default_update_row) {
                toggle_key = QStringLiteral("Update");
            } else if (!patch.version.empty() && patch.version != "PACKED") {
                toggle_key = QStringLiteral("Update v%1").arg(version_q);
            } else {
                toggle_key = QStringLiteral("Update");
            }
        }

        auto* const first_item = new QStandardItem;
        first_item->setText(display_name);
        first_item->setCheckable(true);
        first_item->setData(toggle_key, Qt::UserRole);

        const bool disabled_match_key =
            std::find(disabled.begin(), disabled.end(), toggle_key.toStdString()) != disabled.end();
        const bool disabled_all_updates =
            is_update &&
            std::find(disabled.begin(), disabled.end(), std::string("Update")) != disabled.end();

        const bool patch_disabled =
            disabled_match_key || (is_update && !is_default_update_row && disabled_all_updates);
        first_item->setCheckState(patch_disabled ? Qt::Unchecked : Qt::Checked);

        auto* const second_item = new QStandardItem{version_q};

        if (is_default_update_row) {
            QList<QStandardItem*> row{first_item, second_item};
            item_model->appendRow(row);
            list_items.push_back(row);
            default_update_item = first_item;
            tree_view->expand(first_item->index());
        } else if (is_update && default_update_item != nullptr) {
            QList<QStandardItem*> row{first_item, second_item};
            default_update_item->appendRow(row);
            list_items.push_back(row);
            update_variant_items.push_back(first_item);
        } else {
            QList<QStandardItem*> row{first_item, second_item};
            item_model->appendRow(row);
            list_items.push_back(row);
        }
    }

    tree_view->expandAll();
    tree_view->resizeColumnToContents(1);
}

void ConfigurePerGameAddons::OnItemChanged(QStandardItem* item) {
    if (!item)
        return;
    const auto key = item->data(Qt::UserRole).toString();
    const bool is_update_row = key.startsWith(QStringLiteral("Update"));
    if (!is_update_row)
        return;

    if (item == default_update_item) {
        if (default_update_item->checkState() == Qt::Unchecked) {
            for (auto* v : update_variant_items) {
                if (v && v->checkState() != Qt::Unchecked)
                    v->setCheckState(Qt::Unchecked);
            }
        }
        return;
    }

    if (item->checkState() == Qt::Checked) {
        for (auto* v : update_variant_items) {
            if (v && v != item && v->checkState() != Qt::Unchecked)
                v->setCheckState(Qt::Unchecked);
        }
        if (default_update_item && default_update_item->checkState() == Qt::Unchecked) {
            default_update_item->setCheckState(Qt::Checked);
        }
    }
}
