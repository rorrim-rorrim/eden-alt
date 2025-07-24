// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_COMMON_H
#define QT_COMMON_H

#include <array>

#include <QWindow>
#include "core/core.h"
#include <core/frontend/emu_window.h>

#include <core/file_sys/vfs/vfs_real.h>

namespace QtCommon {
static constexpr std::array<const char *, 3> METADATA_RESULTS = {
    "The operation completed successfully.",
    "The metadata cache couldn't be deleted. It might be in use or non-existent.",
    "The metadata cache is already empty.",
};

enum class MetadataResult {
    Success,
    Failure,
    Empty,
};
/**
 * @brief ResetMetadata Reset game list metadata.
 * @return A result code.
 */
MetadataResult ResetMetadata();

/**
 * \brief Get a string representation of a result from ResetMetadata.
 * \param result The result code.
 * \return A string representation of the passed result code.
 */
inline constexpr const char *GetResetMetadataResultString(MetadataResult result)
{
    return METADATA_RESULTS.at(static_cast<std::size_t>(result));
}

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
    std::function<bool(std::size_t, std::size_t)> QtProgressCallback,
    Core::System *system,
    FileSys::VfsFilesystem *vfs,
    QWidget *parent = nullptr);

QString UnzipFirmwareToTmp(const QString &location);

inline constexpr const char *GetFirmwareInstallResultString(FirmwareInstallResult result)
{
    return FIRMWARE_RESULTS.at(static_cast<std::size_t>(result));
}

Core::Frontend::WindowSystemType GetWindowSystemType();

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow *window);
} // namespace QtCommon
#endif
