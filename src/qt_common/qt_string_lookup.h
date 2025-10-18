// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include "frozen/map.h"
#include "frozen/string.h"
#include <QObject>

namespace QtCommon::StringLookup {

Q_NAMESPACE

// fake tr to make linguist happy
constexpr const frozen::string tr(const char *data)
{
    return frozen::string{data};
}

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
    FwInstallNoOp,
    FwInstallNoNCAs,
    FwInstallFailedDelete,
    FwInstallFailedCopy,
    FwInstallFailedCorrupted,
};

static const frozen::map<StringKey, frozen::string, 20> strings = {
    {SavesTooltip, tr("Contains game save data. DO NOT REMOVE UNLESS YOU KNOW WHAT YOU'RE DOING!")},
    {ShadersTooltip, tr("Contains Vulkan and OpenGL pipeline caches. Generally safe to remove.")},
    {UserNandTooltip, tr("Contains updates and DLC for games.")},
    {SysNandTooltip, tr("Contains firmware and applet data.")},
    {ModsTooltip, tr("Contains game mods, patches, and cheats.")},

    // Key install results
    {KeyInstallSuccess, tr("Decryption Keys were successfully installed")},
    {KeyInstallInvalidDir, tr("Unable to read key directory, aborting")},
    {KeyInstallErrorFailedCopy, tr("One or more keys failed to copy.")},
    {KeyInstallErrorWrongFilename, tr("Verify your keys file has a .keys extension and try again.")},
    {KeyInstallErrorFailedInit,
     tr("Decryption Keys failed to initialize. Check that your dumping tools are up to date and "
        "re-dump keys.")},

    // fw install
    {FwInstallSuccess, tr("Successfully installed firmware version %1")},
    {FwInstallNoOp, tr("")},
    {FwInstallNoNCAs, tr("Unable to locate potential firmware NCA files")},
    {FwInstallFailedDelete, tr("Failed to delete one or more firmware files.")},
    {FwInstallFailedCopy, tr("One or more firmware files failed to copy into NAND.")},
    {FwInstallFailedCorrupted,
     tr("Firmware installation cancelled, firmware may be in a bad state or corrupted. Restart "
        "Eden or re-install firmware.")},
};

static inline const QString Lookup(StringKey key)
{
    return QObject::tr(strings.at(key).data());
}

} // namespace QtCommon::StringLookup
