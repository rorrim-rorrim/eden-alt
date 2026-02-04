// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <utility>

#include <fmt/format.h>

#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>
#include <QString>
#include <QTimer>
#include <QTreeView>

#include "common/common_types.h"
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

    connect(item_model, &QStandardItemModel::itemChanged, this,
            &ConfigurePerGameAddons::OnItemChanged);
    connect(item_model, &QStandardItemModel::itemChanged,
            [] { UISettings::values.is_game_list_reload_pending.exchange(true); });
}

ConfigurePerGameAddons::~ConfigurePerGameAddons() = default;

void ConfigurePerGameAddons::OnItemChanged(QStandardItem* item) {
    if (update_items.size() > 1 && item->checkState() == Qt::Checked) {
        auto it = std::find(update_items.begin(), update_items.end(), item);
        if (it != update_items.end()) {
            for (auto* update_item : update_items) {
                if (update_item != item && update_item->checkState() == Qt::Checked) {
                    disconnect(item_model, &QStandardItemModel::itemChanged, this,
                              &ConfigurePerGameAddons::OnItemChanged);
                    update_item->setCheckState(Qt::Unchecked);
                    connect(item_model, &QStandardItemModel::itemChanged, this,
                           &ConfigurePerGameAddons::OnItemChanged);
                }
            }
        }
    }
}

void ConfigurePerGameAddons::ApplyConfiguration() {
    std::vector<std::string> disabled_addons;

    for (const auto& item : list_items) {
        const auto disabled = item.front()->checkState() == Qt::Unchecked;
        if (disabled) {
            QVariant userData = item.front()->data(Qt::UserRole);
            if (userData.isValid() && userData.canConvert<quint32>() && item.front()->text() == QStringLiteral("Update")) {
                quint32 numeric_version = userData.toUInt();
                disabled_addons.push_back(fmt::format("Update@{}", numeric_version));
            } else {
                disabled_addons.push_back(item.front()->text().toStdString());
            }
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

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, file);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    update_items.clear();
    list_items.clear();
    item_model->removeRows(0, item_model->rowCount());

    std::vector<FileSys::Patch> patches = pm.GetPatches(update_raw);

    size_t multi_version_update_count = 0;
    for (const auto& patch : patches) {
        if (patch.type == FileSys::PatchType::Update && patch.numeric_version != 0) {
            multi_version_update_count++;
        }
    }

    bool has_saved_multi_version_settings = false;
    if (multi_version_update_count > 1) {
        for (const auto& patch : patches) {
            if (patch.type == FileSys::PatchType::Update && patch.numeric_version != 0) {
                std::string disabled_key = fmt::format("Update@{}", patch.numeric_version);
                if (std::find(disabled.begin(), disabled.end(), disabled_key) != disabled.end()) {
                    has_saved_multi_version_settings = true;
                    break;
                }
            }
        }
    }

    bool has_enabled_update = false;
    bool is_first_multi_version_update = true;

    for (const auto& patch : patches) {
        const auto name = QString::fromStdString(patch.name);

        auto* const first_item = new QStandardItem;
        first_item->setText(name);
        first_item->setCheckable(true);

        if (patch.type == FileSys::PatchType::Update && patch.numeric_version != 0) {
            first_item->setData(static_cast<quint32>(patch.numeric_version), Qt::UserRole);
        }

        bool patch_disabled = false;
        if (patch.type == FileSys::PatchType::Update && patch.numeric_version != 0 && multi_version_update_count > 1) {
            if (has_saved_multi_version_settings) {
                std::string disabled_key = fmt::format("Update@{}", patch.numeric_version);
                patch_disabled = std::find(disabled.begin(), disabled.end(), disabled_key) != disabled.end();
            } else {
                patch_disabled = !is_first_multi_version_update;
            }
            is_first_multi_version_update = false;
        } else {
            patch_disabled = std::find(disabled.begin(), disabled.end(), name.toStdString()) != disabled.end();
        }

        bool should_enable = !patch_disabled;

        if (patch.type == FileSys::PatchType::Update) {
            if (should_enable) {
                if (has_enabled_update) {
                    should_enable = false;
                } else {
                    has_enabled_update = true;
                }
            }
            update_items.push_back(first_item);
        }

        first_item->setCheckState(should_enable ? Qt::Checked : Qt::Unchecked);

        list_items.push_back(QList<QStandardItem*>{
            first_item, new QStandardItem{QString::fromStdString(patch.version)}});
        item_model->appendRow(list_items.back());
    }

    tree_view->resizeColumnToContents(1);
}
