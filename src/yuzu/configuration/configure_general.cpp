// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <utility>
#include <vector>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "ui_configure_general.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/configuration/shared_widget.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/util/game.h"

ConfigureGeneral::ConfigureGeneral(const Core::System& system_,
                                   std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
                                   const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureGeneral>()}, system{system_} {
    ui->setupUi(this);

    apply_funcs.push_back([this](bool) {
        Settings::values.external_dirs.clear();
        for (int i = 0; i < ui->external_dirs_list->count(); ++i) {
            QListWidgetItem* item = ui->external_dirs_list->item(i);
            if (item) {
                Settings::values.external_dirs.push_back(item->text().toStdString());
            }
        }
        auto& fs_controller = const_cast<Core::System&>(system).GetFileSystemController();
        fs_controller.RebuildExternalContentIndex();
        QtCommon::Game::ResetMetadata(false);
        UISettings::values.is_game_list_reload_pending.exchange(true);
    });

    Setup(builder);

    SetConfiguration();

    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);
    connect(ui->add_dir_button, &QPushButton::clicked, this, &ConfigureGeneral::OnAddDirClicked);
    connect(ui->remove_dir_button, &QPushButton::clicked, this,
            &ConfigureGeneral::OnRemoveDirClicked);
    connect(ui->external_dirs_list, &QListWidget::itemSelectionChanged, this,
            &ConfigureGeneral::OnDirSelectionChanged);

    ui->remove_dir_button->setEnabled(false);

    if (!Settings::IsConfiguringGlobal()) {
        ui->button_reset_defaults->setVisible(false);
        ui->DataDirsGroupBox->setVisible(false);
    }
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    LoadExternalDirs();
}

void ConfigureGeneral::Setup(const ConfigurationShared::Builder& builder) {
    QLayout& general_layout = *ui->general_widget->layout();
    QLayout& linux_layout = *ui->linux_widget->layout();

    std::map<u32, QWidget*> general_hold{};
    std::map<u32, QWidget*> linux_hold{};

    std::vector<Settings::BasicSetting*> settings;

    auto push = [&settings](auto& list) {
        for (auto setting : list) {
            settings.push_back(setting);
        }
    };

    push(UISettings::values.linkage.by_category[Settings::Category::UiGeneral]);
    push(Settings::values.linkage.by_category[Settings::Category::Linux]);

    // Only show Linux group on Unix
#ifndef __unix__
    ui->LinuxGroupBox->setVisible(false);
#endif

    for (const auto setting : settings) {
        auto* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        switch (setting->GetCategory()) {
        case Settings::Category::UiGeneral:
            general_hold.emplace(setting->Id(), widget);
            break;
        case Settings::Category::Linux:
            linux_hold.emplace(setting->Id(), widget);
            break;
        default:
            widget->deleteLater();
        }
    }

    for (const auto& [id, widget] : general_hold) {
        general_layout.addWidget(widget);
    }
    for (const auto& [id, widget] : linux_hold) {
        linux_layout.addWidget(widget);
    }
}

// Called to set the callback when resetting settings to defaults
void ConfigureGeneral::SetResetCallback(std::function<void()> callback) {
    reset_callback = std::move(callback);
}

void ConfigureGeneral::ResetDefaults() {
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Eden"),
        tr("This reset all settings and remove all per-game configurations. This will not delete "
           "game directories, profiles, or input profiles. Proceed?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::No) {
        return;
    }
    UISettings::values.reset_to_defaults = true;
    UISettings::values.is_game_list_reload_pending.exchange(true);
    reset_callback();
    SetConfiguration();
}

void ConfigureGeneral::ApplyConfiguration() {
    bool powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(powered_on);
    }
}

void ConfigureGeneral::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGeneral::LoadExternalDirs() {
    ui->external_dirs_list->clear();
    for (const auto& dir : Settings::values.external_dirs) {
        ui->external_dirs_list->addItem(QString::fromStdString(dir));
    }
}

void ConfigureGeneral::OnAddDirClicked() {
    QString default_path = QDir::homePath();
    if (ui->external_dirs_list->count() > 0) {
        default_path = ui->external_dirs_list->item(ui->external_dirs_list->count() - 1)->text();
    }

    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"), default_path);
    if (!dir.isEmpty()) {
        if (ui->external_dirs_list->findItems(dir, Qt::MatchExactly).isEmpty()) {
            ui->external_dirs_list->addItem(dir);
        } else {
            QMessageBox::warning(this, tr("Directory already added"),
                                 tr("The directory \"%1\" is already in the list.").arg(dir));
        }
    }
}

void ConfigureGeneral::OnRemoveDirClicked() {
    for (auto* item : ui->external_dirs_list->selectedItems()) {
        delete ui->external_dirs_list->takeItem(ui->external_dirs_list->row(item));
    }
}

void ConfigureGeneral::OnDirSelectionChanged() {
    ui->remove_dir_button->setEnabled(!ui->external_dirs_list->selectedItems().isEmpty());
}
