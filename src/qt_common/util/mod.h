// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include "common/common_types.h"
#include "frontend_common/mod_manager.h"

namespace QtCommon::Mod {

QString GetModFolder(const QString &root, const QString &fallbackName);

FrontendCommon::ModInstallResult InstallMod(const QString& path, const QString& fallbackName,
                                            const u64 program_id, const bool copy = true);

FrontendCommon::ModInstallResult InstallModFromZip(const QString &path, const u64 program_id);

}
