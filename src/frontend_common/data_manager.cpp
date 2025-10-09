// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_manager.h"
#include "common/assert.h"
#include "common/fs/path_util.h"
#include <filesystem>
#include <fmt/format.h>

namespace FrontendCommon::DataManager {

namespace fs = std::filesystem;

const std::string GetDataDir(DataDir dir)
{
    const fs::path nand_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir);

    switch (dir) {
    case DataDir::Saves:
        return (nand_dir / "user" / "save" / "0000000000000000").string();
    case DataDir::UserNand:
        return (nand_dir / "user" / "Contents" / "registered").string();
    case DataDir::SysNand:
        return (nand_dir / "system").string();
    case DataDir::Mods:
        return Common::FS::GetEdenPathString(Common::FS::EdenPath::LoadDir);
    case DataDir::Shaders:
        return Common::FS::GetEdenPathString(Common::FS::EdenPath::ShaderDir);
    default:
        UNIMPLEMENTED();
    }

    return "";
}

u64 ClearDir(DataDir dir)
{
    fs::path data_dir = GetDataDir(dir);
    u64 result = fs::remove_all(data_dir);

    // mkpath at the end just so it actually exists
    fs::create_directories(data_dir);
    return result;
}

const std::string ReadableBytesSize(u64 size)
{
    static constexpr std::array units{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    if (size == 0) {
        return "0 B";
    }

    const int digit_groups = (std::min) (static_cast<int>(std::log10(size) / std::log10(1024)),
                                         static_cast<int>(units.size()));
    return fmt::format("{:.1f} {}", size / std::pow(1024, digit_groups), units[digit_groups]);
}

u64 DataDirSize(DataDir dir)
{
    fs::path data_dir = GetDataDir(dir);
    u64 size = 0;

    if (!fs::exists(data_dir))
        return 0;

    for (const auto& entry : fs::recursive_directory_iterator(data_dir)) {
        if (!entry.is_directory()) {
            size += entry.file_size();
        }
    }

    return size;
}

}
