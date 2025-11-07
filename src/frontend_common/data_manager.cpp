// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_manager.h"
#include "common/assert.h"
#include "common/fs/path_util.h"
#include <fmt/format.h>

namespace FrontendCommon::DataManager {

namespace fs = std::filesystem;

const fs::path GetDataDir(DataDir dir, const std::string &user_id)
{
    const fs::path nand_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir);

    switch (dir) {
    case DataDir::Saves:
        return (nand_dir / "user" / "save" / "0000000000000000" / user_id).string();
    case DataDir::UserNand:
        return (nand_dir / "user" / "Contents" / "registered").string();
    case DataDir::SysNand:
        // NB: do NOT delete save
        // that contains profile data and other stuff
        return (nand_dir / "system" / "Contents" / "registered").string();
    case DataDir::Mods:
        return Common::FS::GetEdenPathString(Common::FS::EdenPath::LoadDir);
    case DataDir::Shaders:
        return Common::FS::GetEdenPathString(Common::FS::EdenPath::ShaderDir);
    default:
        UNIMPLEMENTED();
    }

    return "";
}

const std::string GetDataDirString(DataDir dir, const std::string &user_id)
{
    return GetDataDir(dir, user_id).string();
}

u64 ClearDir(DataDir dir, const std::string &user_id)
{
    fs::path data_dir = GetDataDir(dir, user_id);
    u64 result = fs::remove_all(data_dir);

    // mkpath at the end just so it actually exists
    fs::create_directories(data_dir);
    return result;
}

const std::string ReadableBytesSize(u64 size)
{
    static constexpr u64 base = 1000;
    static constexpr std::array<std::string, 6> units{"B", "KB", "MB", "GB", "TB", "PB"};

    if (size == 0) {
        return "0 B";
    }

    auto const digit_groups = std::min<u64>(static_cast<u64>(std::log10(size) / std::log10(base)),
                                         static_cast<u64>(units.size()));
    return fmt::format("{:.1f} {}", size / std::pow(base, digit_groups), units[digit_groups]);
}

u64 DataDirSize(DataDir dir)
{
    fs::path data_dir = GetDataDir(dir);
    u64 size = 0;

    if (!fs::exists(data_dir))
        return 0;

    for (const auto &entry : fs::recursive_directory_iterator(data_dir)) {
        if (!entry.is_directory()) {
            size += entry.file_size();
        }
    }

    return size;
}

} // namespace FrontendCommon::DataManager
