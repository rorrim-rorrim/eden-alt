// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ryujinx_dialog.h"
#include "qt_common/util/fs.h"
#include "ui_ryujinx_dialog.h"
#include <filesystem>
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
    QtCommon::FS::LinkRyujinx(from, to);
}
