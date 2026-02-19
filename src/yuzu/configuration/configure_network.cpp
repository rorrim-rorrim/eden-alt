// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QtConcurrent/QtConcurrent>
#include "common/settings.h"
#include "core/core.h"
#include "core/internal_network/network_interface.h"
#include "ui_configure_network.h"
#include "yuzu/configuration/configure_network.h"
#include "yuzu/util/limitable_input_dialog.h"

ConfigureNetwork::ConfigureNetwork(const Core::System& system_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureNetwork>()), system{system_} {
    ui->setupUi(this);

    ui->network_interface->addItem(tr("None"));
    for (const auto& iface : Network::GetAvailableNetworkInterfaces()) {
        ui->network_interface->addItem(QString::fromStdString(iface.name));
    }

    ui->blocked_domains_list->clear();
    for (const auto& s : Settings::values.blocked_domains) {
        ui->blocked_domains_list->addItem(QString::fromStdString(s));
    }

    connect(ui->add_blocked_domains_button, &QPushButton::pressed, this, [this, parent] {
        if (const QString s = LimitableInputDialog::GetText(parent, tr("Add blocked domain"), tr("Input DNS match rule"), 0, 64); !s.isEmpty()) {
            ui->blocked_domains_list->addItem(s);
        }
    });
    connect(ui->remove_blocked_domains_button, &QPushButton::pressed, this, [this] {
        auto selected = ui->blocked_domains_list->selectedItems();
        if (!selected.isEmpty()) {
            qDeleteAll(ui->blocked_domains_list->selectedItems());
        }
    });
    connect(ui->blocked_domains_list, &QListWidget::itemSelectionChanged, this, [this] {
        ui->remove_blocked_domains_button->setEnabled(!ui->blocked_domains_list->selectedItems().isEmpty());
    });

    this->SetConfiguration();
}

ConfigureNetwork::~ConfigureNetwork() = default;

void ConfigureNetwork::ApplyConfiguration() {
    Settings::values.network_interface = ui->network_interface->currentText().toStdString();
    Settings::values.airplane_mode = ui->airplane_mode->isChecked();

    std::vector<std::string> new_dirs;
    new_dirs.reserve(ui->blocked_domains_list->count());
    for (int i = 0; i < ui->blocked_domains_list->count(); ++i) {
        new_dirs.push_back(ui->blocked_domains_list->item(i)->text().toStdString());
    }
    if (new_dirs != Settings::values.blocked_domains) {
        Settings::values.blocked_domains = std::move(new_dirs);
    }
}

// void ConfigureGeneral::UpdateExternalContentList() {
//     ui->blocked_domains_list->clear();
//     for (const auto& dir : Settings::values.blocked_domains) {
//         ui->blocked_domains_list->addItem(QString::fromStdString(dir));
//     }
// }

void ConfigureNetwork::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureNetwork::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureNetwork::SetConfiguration() {
    const bool runtime_lock = !system.IsPoweredOn();

    const std::string& network_interface = Settings::values.network_interface.GetValue();
    const bool& airplane_mode = Settings::values.airplane_mode.GetValue();

    ui->network_interface->setCurrentText(QString::fromStdString(network_interface));
    ui->network_interface->setEnabled(runtime_lock);

    ui->airplane_mode->setChecked(airplane_mode);
    ui->network_interface->setEnabled(true);
}
