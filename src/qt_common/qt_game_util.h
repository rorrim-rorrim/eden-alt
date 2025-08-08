// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_GAME_UTIL_H
#define QT_GAME_UTIL_H

#include "common/fs/path_util.h"
#include "frontend_common/content_manager.h"
#include <array>

namespace QtCommon {

static constexpr std::array<const char *, 3> GAME_VERIFICATION_RESULTS = {
    "The operation completed successfully.",
    "File contents may be corrupt or missing..",
    "Firmware installation cancelled, firmware may be in a bad state or corrupted. "
    "File contents could not be checked for validity."
};

inline constexpr const char *GetGameVerificationResultString(ContentManager::GameVerificationResult result)
{
    return GAME_VERIFICATION_RESULTS.at(static_cast<std::size_t>(result));
}

bool CreateShortcutLink(const std::filesystem::path& shortcut_path,
                        const std::string& comment,
                        const std::filesystem::path& icon_path,
                        const std::filesystem::path& command,
                        const std::string& arguments,
                        const std::string& categories,
                        const std::string& keywords,
                        const std::string& name);

bool MakeShortcutIcoPath(const u64 program_id,
                         const std::string_view game_file_name,
                         std::filesystem::path& out_icon_path);

void OpenEdenFolder(const Common::FS::EdenPath &path);
void OpenRootDataFolder();
void OpenNANDFolder();
void OpenSDMCFolder();
void OpenModFolder();
void OpenLogFolder();
}

#endif // QT_GAME_UTIL_H
