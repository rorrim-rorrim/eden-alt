// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_dialog.h"
#include "frontend_common/data_manager.h"
#include "qt_common/qt_content_util.h"
#include "qt_common/qt_string_lookup.h"
#include "ui_data_dialog.h"

#include <QDesktopServices>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

DataDialog::DataDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::DataDialog>())
{
    ui->setupUi(this);

    // TODO: Should we make this a single widget that pulls data from a model?
#define WIDGET(name) \
    ui->page->addWidget(new DataWidget(FrontendCommon::DataManager::DataDir::name, \
                                       QtCommon::StringLookup::name##Tooltip, \
                                       this));

    WIDGET(Saves)
    WIDGET(Shaders)
    WIDGET(UserNand)
    WIDGET(SysNand)
    WIDGET(Mods)

#undef WIDGET

    connect(ui->labels, &QListWidget::itemSelectionChanged, this, [this]() {
        ui->page->setCurrentIndex(ui->labels->currentRow());
    });
}

DataDialog::~DataDialog() = default;

DataWidget::DataWidget(FrontendCommon::DataManager::DataDir data_dir,
                       QtCommon::StringLookup::StringKey tooltip,
                       QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::DataWidget>())
    , m_dir(data_dir)
{
    ui->setupUi(this);

    ui->tooltip->setText(QtCommon::StringLookup::Lookup(tooltip));

    ui->clear->setIcon(QIcon::fromTheme(QStringLiteral("trash")));
    ui->open->setIcon(QIcon::fromTheme(QStringLiteral("folder")));

    connect(ui->clear, &QPushButton::clicked, this, &DataWidget::clear);
    connect(ui->open, &QPushButton::clicked, this, &DataWidget::open);

    scan();
}

void DataWidget::clear() {
    QtCommon::Content::ClearDataDir(m_dir);
    scan();
}

void DataWidget::open() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FrontendCommon::DataManager::GetDataDir(m_dir))));
}

void DataWidget::scan() {
    ui->size->setText(tr("Calculating..."));

    QFutureWatcher<u64> *watcher = new QFutureWatcher<u64>(this);

    connect(watcher, &QFutureWatcher<u64>::finished, this, [=, this]() {
        u64 size = watcher->result();
        ui->size->setText(
            QString::fromStdString(FrontendCommon::DataManager::ReadableBytesSize(size)));
        watcher->deleteLater();
    });

    watcher->setFuture(
        QtConcurrent::run([this]() { return FrontendCommon::DataManager::DataDirSize(m_dir); }));
}
