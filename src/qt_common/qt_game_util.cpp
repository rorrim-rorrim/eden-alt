// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_game_util.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/file_sys/savedata_factory.h"
#include "frontend_common/content_manager.h"
#include "qt_common.h"
#include "qt_common/uisettings.h"
#include "qt_frontend_util.h"

#include <QDesktopServices>
#include <QUrl>

#ifdef _WIN32
#include "common/scope_exit.h"
#include "common/string_util.h"
#include <shlobj.h>
#include <windows.h>
#else
#include "fmt/ostream.h"
#include <fstream>
#endif

namespace QtCommon::Game {

bool CreateShortcutLink(const std::filesystem::path& shortcut_path,
                        const std::string& comment,
                        const std::filesystem::path& icon_path,
                        const std::filesystem::path& command,
                        const std::string& arguments,
                        const std::string& categories,
                        const std::string& keywords,
                        const std::string& name)
try {
#ifdef _WIN32 // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT
    {
        CoUninitialize();
    };
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT
    {
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    };
    HRESULT hres = CoCreateInstance(CLSID_ShellLink,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(command.c_str());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(std::filesystem::path{shortcut_path / (name + ".lnk")}.c_str(), TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__) // Any desktop NIX
    std::filesystem::path shortcut_path_full = shortcut_path / (name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", icon_path.string());
    }
    fmt::print(shortcut_stream, "TryExec={}\n", command.string());
    fmt::print(shortcut_stream, "Exec={} {}\n", command.string(), arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#else                                                                   // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

bool MakeShortcutIcoPath(const u64 program_id,
                         const std::string_view game_file_name,
                         std::filesystem::path& out_icon_path)
{
    // Get path to Yuzu icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = Common::FS::GetEdenPath(Common::FS::EdenPath::IconsDir);
    ico_extension = "ico";
#elif defined(__linux__) || defined(__FreeBSD__)
    out_icon_path = Common::FS::GetDataDirectory("XDG_DATA_HOME") / "icons/hicolor/256x256";
#endif
    // Create icons directory if it doesn't exist
    if (!Common::FS::CreateDirs(out_icon_path)) {
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /= (program_id == 0 ? fmt::format("eden-{}.{}", game_file_name, ico_extension)
                                      : fmt::format("eden-{:016X}.{}", program_id, ico_extension));
    return true;
}

void OpenEdenFolder(const Common::FS::EdenPath& path)
{
    QDesktopServices::openUrl(QUrl(QString::fromStdString(Common::FS::GetEdenPathString(path))));
}

void OpenRootDataFolder()
{
    OpenEdenFolder(Common::FS::EdenPath::EdenDir);
}

void OpenNANDFolder()
{
    OpenEdenFolder(Common::FS::EdenPath::NANDDir);
}

void OpenSDMCFolder()
{
    OpenEdenFolder(Common::FS::EdenPath::SDMCDir);
}

void OpenModFolder()
{
    OpenEdenFolder(Common::FS::EdenPath::LoadDir);
}

void OpenLogFolder()
{
    OpenEdenFolder(Common::FS::EdenPath::LogDir);
}

static QString GetGameListErrorRemoving(QtCommon::Game::InstalledEntryType type)
{
    switch (type) {
    case QtCommon::Game::InstalledEntryType::Game:
        return tr("Error Removing Contents");
    case QtCommon::Game::InstalledEntryType::Update:
        return tr("Error Removing Update");
    case QtCommon::Game::InstalledEntryType::AddOnContent:
        return tr("Error Removing DLC");
    default:
        return QStringLiteral("Error Removing <Invalid Type>");
    }
}

// Game Content //
void RemoveBaseContent(u64 program_id, InstalledEntryType type)
{
    const auto res = ContentManager::RemoveBaseContent(system->GetFileSystemController(),
                                                       program_id);
    if (res) {
        QtCommon::Frontend::Information(rootObject,
                                        "Successfully Removed",
                                        "Successfully removed the installed base game.");
    } else {
        QtCommon::Frontend::Warning(
            rootObject,
            GetGameListErrorRemoving(type),
            tr("The base game is not installed in the NAND and cannot be removed."));
    }
}

void RemoveUpdateContent(u64 program_id, InstalledEntryType type)
{
    const auto res = ContentManager::RemoveUpdate(system->GetFileSystemController(), program_id);
    if (res) {
        QtCommon::Frontend::Information(rootObject,
                                        "Successfully Removed",
                                        "Successfully removed the installed update.");
    } else {
        QtCommon::Frontend::Warning(rootObject,
                                    GetGameListErrorRemoving(type),
                                    tr("There is no update installed for this title."));
    }
}

void RemoveAddOnContent(u64 program_id, InstalledEntryType type)
{
    const size_t count = ContentManager::RemoveAllDLC(*system, program_id);
    if (count == 0) {
        QtCommon::Frontend::Warning(rootObject,
                                    GetGameListErrorRemoving(type),
                                    tr("There are no DLCs installed for this title."));
        return;
    }

    QtCommon::Frontend::Information(rootObject,
                                    tr("Successfully Removed"),
                                    tr("Successfully removed %1 installed DLC.").arg(count));
}

// Global Content //

void RemoveTransferableShaderCache(u64 program_id, GameListRemoveTarget target)
{
    const auto target_file_name = [target] {
        switch (target) {
        case GameListRemoveTarget::GlShaderCache:
            return "opengl.bin";
        case GameListRemoveTarget::VkShaderCache:
            return "vulkan.bin";
        default:
            return "";
        }
    }();
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Transferable Shader Cache"),
                                    tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveFile(target_file)) {
        QtCommon::Frontend::Information(rootObject,
                                        tr("Successfully Removed"),
                                        tr("Successfully removed the transferable shader cache."));
    } else {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Transferable Shader Cache"),
                                    tr("Failed to remove the transferable shader cache."));
    }
}

void RemoveVulkanDriverPipelineCache(u64 program_id)
{
    static constexpr std::string_view target_file_name = "vulkan_pipelines.bin";

    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        return;
    }
    if (!Common::FS::RemoveFile(target_file)) {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Vulkan Driver Pipeline Cache"),
                                    tr("Failed to remove the driver pipeline cache."));
    }
}

void RemoveAllTransferableShaderCaches(u64 program_id)
{
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto program_shader_cache_dir = shader_cache_dir / fmt::format("{:016x}", program_id);

    if (!Common::FS::Exists(program_shader_cache_dir)) {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Transferable Shader Caches"),
                                    tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveDirRecursively(program_shader_cache_dir)) {
        QtCommon::Frontend::Information(rootObject,
                                        tr("Successfully Removed"),
                                        tr("Successfully removed the transferable shader caches."));
    } else {
        QtCommon::Frontend::Warning(
            rootObject,
            tr("Error Removing Transferable Shader Caches"),
            tr("Failed to remove the transferable shader cache directory."));
    }
}

void RemoveCustomConfiguration(u64 program_id, const std::string& game_path)
{
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(game_path));
    const auto config_file_name = program_id == 0
                                      ? Common::FS::PathToUTF8String(file_path.filename())
                                            .append(".ini")
                                      : fmt::format("{:016X}.ini", program_id);
    const auto custom_config_file_path = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir)
                                         / "custom" / config_file_name;

    if (!Common::FS::Exists(custom_config_file_path)) {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Custom Configuration"),
                                    tr("A custom configuration for this title does not exist."));
        return;
    }

    if (Common::FS::RemoveFile(custom_config_file_path)) {
        QtCommon::Frontend::Information(rootObject,
                                        tr("Successfully Removed"),
                                        tr("Successfully removed the custom game configuration."));
    } else {
        QtCommon::Frontend::Warning(rootObject,
                                    tr("Error Removing Custom Configuration"),
                                    tr("Failed to remove the custom game configuration."));
    }
}

void RemoveCacheStorage(u64 program_id, FileSys::VfsFilesystem* vfs)
{
    const auto nand_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir);
    auto vfs_nand_dir = vfs->OpenDirectory(Common::FS::PathToUTF8String(nand_dir),
                                           FileSys::OpenMode::Read);

    const auto cache_storage_path
        = FileSys::SaveDataFactory::GetFullPath({},
                                                vfs_nand_dir,
                                                FileSys::SaveDataSpaceId::User,
                                                FileSys::SaveDataType::Cache,
                                                0 /* program_id */,
                                                {},
                                                0);

    const auto path = Common::FS::ConcatPathSafe(nand_dir, cache_storage_path);

    // Not an error if it wasn't cleared.
    Common::FS::RemoveDirRecursively(path);
}

// Metadata //
void ResetMetadata()
{
    const QString title = tr("Reset Metadata Cache");

    if (!Common::FS::Exists(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir)
                            / "game_list/")) {
        QtCommon::Frontend::Warning(rootObject, title, tr("The metadata cache is already empty."));
    } else if (Common::FS::RemoveDirRecursively(
                   Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list")) {
        QtCommon::Frontend::Information(rootObject,
                                        title,
                                        tr("The operation completed successfully."));
        UISettings::values.is_game_list_reload_pending.exchange(true);
    } else {
        QtCommon::Frontend::Warning(
            rootObject,
            title,
            tr("The metadata cache couldn't be deleted. It might be in use or non-existent."));
    }
}

} // namespace QtCommon::Game
