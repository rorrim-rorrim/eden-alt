#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fmt/format.h>
#include "common/fs/fs.h"
#include "common/fs/fs_types.h"
#include "common/logging/backend.h"
#include "frontend_common/data_manager.h"
#include "mod_manager.h"

namespace FrontendCommon {

// TODO: Handle cases where the folder appears to contain multiple mods.
std::optional<std::filesystem::path> GetModFolder(const std::string& root) {
    std::filesystem::path path;
    bool found;

    auto callback = [&path, &found](const std::filesystem::directory_entry& entry) -> bool {
        const auto name = entry.path().filename().string();
        static constexpr const std::array<std::string, 5> valid_names = {"exefs",
                                                                         "romfs"
                                                                         "romfs_ext",
                                                                         "cheats", "romfslite"};

        if (std::ranges::find(valid_names, name) != valid_names.end()) {
            path = entry.path().parent_path();
            found = true;
        }

        return true;
    };

    Common::FS::IterateDirEntriesRecursively(root, callback, Common::FS::DirEntryFilter::Directory);

    if (found)
        return path;

    return std::nullopt;
}

bool InstallMod(const std::filesystem::path& path, const u64 program_id, const bool copy) {
    const auto program_id_string = fmt::format("{:016X}", program_id);
    const auto mod_name = path.filename();
    const auto mod_dir =
        DataManager::GetDataDir(DataManager::DataDir::Mods) / program_id_string / mod_name;

    // pre-emptively remove any existing mod here
    std::filesystem::remove_all(mod_dir);

    // now copy
    try {
        if (copy)
            std::filesystem::copy(path, mod_dir, std::filesystem::copy_options::recursive);
        else
            std::filesystem::rename(path, mod_dir);
    } catch (std::exception& e) {
        LOG_ERROR(Frontend, "Mod install failed with message {}", e.what());
        return false;
    }

    LOG_INFO(Frontend, "Copied mod from {} to {}", path.string(), mod_dir.string());

    return true;
}

} // namespace FrontendCommon
