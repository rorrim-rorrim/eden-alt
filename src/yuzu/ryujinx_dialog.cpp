// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/abstract/qt_frontend_util.h"
#include "ryujinx_dialog.h"
#include "ui_ryujinx_dialog.h"
#include <filesystem>
#include <system_error>
#include <fmt/format.h>

namespace fs = std::filesystem;

RyujinxDialog::RyujinxDialog(std::filesystem::path eden_path,
                             std::filesystem::path ryu_path,
                             QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RyujinxDialog)
    , m_eden(eden_path.make_preferred())
    , m_ryu(ryu_path.make_preferred())
{
    ui->setupUi(this);

    connect(ui->eden, &QPushButton::clicked, this, &RyujinxDialog::fromEden);
    connect(ui->ryujinx, &QPushButton::clicked, this, &RyujinxDialog::fromRyujinx);
}

RyujinxDialog::~RyujinxDialog()
{
    delete ui;
}

void RyujinxDialog::fromEden()
{
    accept();
    link(m_eden, m_ryu);
}

void RyujinxDialog::fromRyujinx()
{
    accept();
    link(m_ryu, m_eden);
}

void RyujinxDialog::link(std::filesystem::path &from, std::filesystem::path &to)
{
    std::error_code ec;

    // "ignore" errors--if the dir fails to be deleted, error handling later will handle it
    fs::remove_all(to, ec);

#ifdef _WIN32
    const std::string command = fmt::format("mklink /J {} {}", to.string(), from.string());
    system(command.c_str());
#else
    try {
        fs::create_directory_symlink(from, to);
    } catch (std::exception &e) {
        QtCommon::Frontend::Critical(tr("Failed to link save data"),
                                     tr("Could not link directory:\n\t%1\nTo:\n\t%2\n\nError: %3")
                                         .arg(QString::fromStdString(from.string()),
                                              QString::fromStdString(to.string()),
                                              QString::fromStdString(e.what())));
        return;
    }
#endif

    QtCommon::Frontend::Information(tr("Linked Save Data"), tr("Save data has been linked."));
}
