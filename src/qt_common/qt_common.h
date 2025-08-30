// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_COMMON_H
#define QT_COMMON_H

#include <QWindow>
#include "core/core.h"
#include <core/frontend/emu_window.h>

#include <core/file_sys/vfs/vfs_real.h>

namespace QtCommon {

extern QObject *rootObject;
extern Core::System *system;

typedef std::function<bool(std::size_t, std::size_t)> QtProgressCallback;

Core::Frontend::WindowSystemType GetWindowSystemType();

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow *window);

static inline void SetRootObject(QObject *parent)
{
    rootObject = parent;
}

static inline void SetSystem(Core::System *newSystem)
{
    system = newSystem;
}

const QString tr(const char *str);
const QString tr(const std::string &str);

} // namespace QtCommon
#endif
