// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_CONTENT_UTIL_H
#define QT_CONTENT_UTIL_H

#include <QObject>
#include "common/common_types.h"
#include "core/core.h"
#include "qt_common/qt_common.h"

namespace QtCommon::Content {

//
bool CheckGameFirmware(u64 program_id, Core::System &system, QObject *parent);

static constexpr std::array<const char *, 6> FIRMWARE_RESULTS
    = {"Successfully installed firmware version %1",
       "",
       "Unable to locate potential firmware NCA files",
       "Failed to delete one or more firmware files.",
       "One or more firmware files failed to copy into NAND.",
       "Firmware installation cancelled, firmware may be in a bad state or corrupted."
       "Restart Eden or re-install firmware."};

enum class FirmwareInstallResult {
    Success,
    NoOp,
    NoNCAs,
    FailedDelete,
    FailedCopy,
    FailedCorrupted,
};

FirmwareInstallResult InstallFirmware(
    const QString &location,
    bool recursive,
    QtProgressCallback callback,
    FileSys::VfsFilesystem *vfs);

QString UnzipFirmwareToTmp(const QString &location);

inline constexpr const char *GetFirmwareInstallResultString(FirmwareInstallResult result)
{
    return FIRMWARE_RESULTS.at(static_cast<std::size_t>(result));
}

// Content //
void VerifyGameContents(const std::string &game_path, QtProgressCallback callback);
}
#endif // QT_CONTENT_UTIL_H
