// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_dialog.h"
#include "frontend_common/data_manager.h"
#include "qt_common/qt_content_util.h"
#include "qt_common/qt_frontend_util.h"
#include "qt_common/qt_progress_dialog.h"
#include "qt_common/qt_string_lookup.h"
#include "ui_data_dialog.h"

#include <QDesktopServices>
#include <QFutureWatcher>
#include <QMenu>
#include <QProgressDialog>
#include <QtConcurrent/qtconcurrentrun.h>
#include <qnamespace.h>

DataDialog::DataDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::DataDialog>())
{
    ui->setupUi(this);

    std::size_t row = 0;
#define TABLE_ITEM(label, name, data_dir) \
    QTableWidgetItem *name##Label = new QTableWidgetItem(tr(label)); \
    name##Label->setToolTip( \
        QtCommon::StringLookup::Lookup(QtCommon::StringLookup::data_dir##Tooltip)); \
    ui->sizes->setItem(row, 0, name##Label); \
    DataItem *name##Item = new DataItem(FrontendCommon::DataManager::DataDir::data_dir, this); \
    ui->sizes->setItem(row, 1, name##Item); \
    ++row;

    TABLE_ITEM("Saves", save, Saves)
    TABLE_ITEM("Shaders", shaders, Shaders)
    TABLE_ITEM("UserNAND", user, UserNand)
    TABLE_ITEM("SysNAND", sys, SysNand)
    TABLE_ITEM("Mods", mods, Mods)

#undef TABLE_ITEM

    QObject::connect(ui->sizes, &QTableWidget::customContextMenuRequested, this, [this]() {
        auto items = ui->sizes->selectedItems();
        if (items.empty())
            return;

        QTableWidgetItem *selected = items.at(0);
        DataItem *item = (DataItem *) ui->sizes->item(selected->row(), 1);

        QMenu *menu = new QMenu(this);
        QAction *open = menu->addAction(tr("Open"));
        QObject::connect(open, &QAction::triggered, this, [item]() {
            auto data_dir
                = item->data(DataItem::DATA_DIR).value<FrontendCommon::DataManager::DataDir>();

            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QString::fromStdString(FrontendCommon::DataManager::GetDataDir(data_dir))));
        });

        QAction *clear = menu->addAction(tr("Clear"));
        QObject::connect(clear, &QAction::triggered, this, [item]() {
            auto data_dir
                = item->data(DataItem::DATA_DIR).value<FrontendCommon::DataManager::DataDir>();

            QtCommon::Content::ClearDataDir(data_dir);

            item->scan();
        });

        menu->exec(QCursor::pos());
    });
}

DataDialog::~DataDialog() = default;

DataItem::DataItem(FrontendCommon::DataManager::DataDir data_dir, QWidget *parent)
    : QTableWidgetItem(QObject::tr("Calculating"))
    , m_parent(parent)
    , m_dir(data_dir)
{
    setData(DataItem::DATA_DIR, QVariant::fromValue(m_dir));
    scan();
}

bool DataItem::operator<(const QTableWidgetItem &other) const
{
    return this->data(DataRole::SIZE).toULongLong() < other.data(DataRole::SIZE).toULongLong();
}

void DataItem::reset() {
    setText(QStringLiteral("0 B"));
    setData(DataItem::SIZE, QVariant::fromValue(0ULL));
}

void DataItem::scan() {
    m_watcher = new QFutureWatcher<u64>(m_parent);

    m_parent->connect(m_watcher, &QFutureWatcher<u64>::finished, m_parent, [=, this]() {
        u64 size = m_watcher->result();
        setText(QString::fromStdString(FrontendCommon::DataManager::ReadableBytesSize(size)));
        setData(DataItem::SIZE, QVariant::fromValue(size));
    });

    m_watcher->setFuture(
        QtConcurrent::run([this]() { return FrontendCommon::DataManager::DataDirSize(m_dir); }));
}
