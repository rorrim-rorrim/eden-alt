// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include "frozen/map.h"
#include "frozen/string.h"
#include <QObject>

namespace QtCommon::StringLookup {

Q_NAMESPACE

// TODO(crueter): QML interface
enum StringKey {
    SavesTooltip,
    ShadersTooltip,
    UserNandTooltip,
    SysNandTooltip,
    ModsTooltip,

    // Key install results
    KeyInstallSuccess,
    KeyInstallInvalidDir,
    KeyInstallErrorFailedCopy,
    KeyInstallErrorWrongFilename,
    KeyInstallErrorFailedInit,

    // Firmware install results
    FwInstallSuccess,
    FwInstallNoNCAs,
    FwInstallFailedDelete,
    FwInstallFailedCopy,
    FwInstallFailedCorrupted,
};

static const frozen::map<StringKey, frozen::string, 15> strings = {
    {SavesTooltip, QT_TR_NOOP("Contains game save data. DO NOT REMOVE UNLESS YOU KNOW WHAT YOU'RE DOING!")},
    {ShadersTooltip, QT_TR_NOOP("Contains Vulkan and OpenGL pipeline caches. Generally safe to remove.")},
    {UserNandTooltip, QT_TR_NOOP("Contains updates and DLC for games.")},
    {SysNandTooltip, QT_TR_NOOP("Contains firmware and applet data.")},
    {ModsTooltip, QT_TR_NOOP("Contains game mods, patches, and cheats.")},

    // Key install results
    {KeyInstallSuccess, QT_TR_NOOP("Decryption Keys were successfully installed")},
    {KeyInstallInvalidDir, QT_TR_NOOP("Unable to read key directory, aborting")},
    {KeyInstallErrorFailedCopy, QT_TR_NOOP("One or more keys failed to copy.")},
    {KeyInstallErrorWrongFilename, QT_TR_NOOP("Verify your keys file has a .keys extension and try again.")},
    {KeyInstallErrorFailedInit,
     QT_TR_NOOP("Decryption Keys failed to initialize. Check that your dumping tools are up to date and "
        "re-dump keys.")},

    // fw install
    {FwInstallSuccess, QT_TR_NOOP("Successfully installed firmware version %1")},
    {FwInstallNoNCAs, QT_TR_NOOP("Unable to locate potential firmware NCA files")},
    {FwInstallFailedDelete, QT_TR_NOOP("Failed to delete one or more firmware files.")},
    {FwInstallFailedCopy, QT_TR_NOOP("One or more firmware files failed to copy into NAND.")},
    {FwInstallFailedCorrupted,
     QT_TR_NOOP("Firmware installation cancelled, firmware may be in a bad state or corrupted. Restart "
        "Eden or re-install firmware.")},
};

static inline const QString Lookup(StringKey key)
{
    return QObject::tr(strings.at(key).data());
}

} // namespace QtCommon::StringLookup
