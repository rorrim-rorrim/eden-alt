// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "frontend_common/firmware_manager.h"
#include "uisettings.h"

#include <QGuiApplication>
#include <QStringLiteral>
#include "common/logging/log.h"
#include "core/frontend/emu_window.h"

#include <QFile>

#include <QMessageBox>
#include "qt_frontend_util.h"

#include <JlCompress.h>

#if !defined(WIN32) && !defined(__APPLE__)
#include <qpa/qplatformnativeinterface.h>
#elif defined(__APPLE__)
#include <objc/message.h>
#endif

namespace QtCommon {
MetadataResult ResetMetadata()
{
    if (!Common::FS::Exists(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir)
                            / "game_list/")) {
        return MetadataResult::Empty;
    } else if (Common::FS::RemoveDirRecursively(
                   Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list")) {
        return MetadataResult::Success;
        UISettings::values.is_game_list_reload_pending.exchange(true);
    } else {
        return MetadataResult::Failure;
    }
}

Core::Frontend::WindowSystemType GetWindowSystemType()
{
    // Determine WSI type based on Qt platform.
    QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("windows"))
        return Core::Frontend::WindowSystemType::Windows;
    else if (platform_name == QStringLiteral("xcb"))
        return Core::Frontend::WindowSystemType::X11;
    else if (platform_name == QStringLiteral("wayland"))
        return Core::Frontend::WindowSystemType::Wayland;
    else if (platform_name == QStringLiteral("wayland-egl"))
        return Core::Frontend::WindowSystemType::Wayland;
    else if (platform_name == QStringLiteral("cocoa"))
        return Core::Frontend::WindowSystemType::Cocoa;
    else if (platform_name == QStringLiteral("android"))
        return Core::Frontend::WindowSystemType::Android;

    LOG_CRITICAL(Frontend, "Unknown Qt platform {}!", platform_name.toStdString());
    return Core::Frontend::WindowSystemType::Windows;
} // namespace Core::Frontend::WindowSystemType

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow* window)
{
    Core::Frontend::EmuWindow::WindowSystemInfo wsi;
    wsi.type = GetWindowSystemType();

#if defined(WIN32)
    // Our Win32 Qt external doesn't have the private API.
    wsi.render_surface = reinterpret_cast<void*>(window->winId());
#elif defined(__APPLE__)
    wsi.render_surface = reinterpret_cast<void* (*) (id, SEL)>(
        objc_msgSend)(reinterpret_cast<id>(window->winId()), sel_registerName("layer"));
#else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    wsi.display_connection = pni->nativeResourceForWindow("display", window);
    if (wsi.type == Core::Frontend::WindowSystemType::Wayland)
        wsi.render_surface = window ? pni->nativeResourceForWindow("surface", window) : nullptr;
    else
        wsi.render_surface = window ? reinterpret_cast<void*>(window->winId()) : nullptr;
#endif
    wsi.render_surface_scale = window ? static_cast<float>(window->devicePixelRatio()) : 1.0f;

    return wsi;
}

FirmwareInstallResult InstallFirmware(
    const QString& location,
    bool recursive,
    std::function<bool(std::size_t, std::size_t)> QtProgressCallback,
    Core::System* system,
    FileSys::VfsFilesystem* vfs,
    QWidget* parent)
{
    static constexpr const char* failedTitle = "Firmware Install Failed";
    static constexpr const char* successTitle = "Firmware Install Failed";
    static constexpr QMessageBox::StandardButtons buttons = QMessageBox::Ok;

    QMessageBox::Icon icon;
    FirmwareInstallResult result;

    const auto ShowMessage = [&]() {
        QtCommon::Frontend::ShowMessage(icon,
                                        failedTitle,
                                        GetFirmwareInstallResultString(result),
                                        buttons,
                                        parent);
    };

    LOG_INFO(Frontend, "Installing firmware from {}", location.toStdString());

    // Check for a reasonable number of .nca files (don't hardcode them, just see if there's some in
    // there.)
    std::filesystem::path firmware_source_path = location.toStdString();
    if (!Common::FS::IsDir(firmware_source_path)) {
        return FirmwareInstallResult::NoOp;
    }

    std::vector<std::filesystem::path> out;
    const Common::FS::DirEntryCallable callback =
        [&out](const std::filesystem::directory_entry& entry) {
            if (entry.path().has_extension() && entry.path().extension() == ".nca") {
                out.emplace_back(entry.path());
            }

            return true;
        };

    QtProgressCallback(100, 10);

    if (recursive) {
        Common::FS::IterateDirEntriesRecursively(firmware_source_path,
                                                 callback,
                                                 Common::FS::DirEntryFilter::File);
    } else {
        Common::FS::IterateDirEntries(firmware_source_path,
                                      callback,
                                      Common::FS::DirEntryFilter::File);
    }

    if (out.size() <= 0) {
        result = FirmwareInstallResult::NoNCAs;
        icon = QMessageBox::Warning;
        ShowMessage();
        return result;
    }

    // Locate and erase the content of nand/system/Content/registered/*.nca, if any.
    auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    if (sysnand_content_vdir->IsWritable()
        && !sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
        result = FirmwareInstallResult::FailedDelete;
        icon = QMessageBox::Critical;
        ShowMessage();
        return result;
    }

    LOG_INFO(Frontend,
             "Cleaned nand/system/Content/registered folder in preparation for new firmware.");

    QtProgressCallback(100, 20);

    auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");

    bool success = true;
    int i = 0;
    for (const auto& firmware_src_path : out) {
        i++;
        auto firmware_src_vfile = vfs->OpenFile(firmware_src_path.generic_string(),
                                                FileSys::OpenMode::Read);
        auto firmware_dst_vfile = firmware_vdir
                                      ->CreateFileRelative(firmware_src_path.filename().string());

        if (!VfsRawCopy(firmware_src_vfile, firmware_dst_vfile)) {
            LOG_ERROR(Frontend,
                      "Failed to copy firmware file {} to {} in registered folder!",
                      firmware_src_path.generic_string(),
                      firmware_src_path.filename().string());
            success = false;
        }

        if (QtProgressCallback(100,
                               20
                                   + static_cast<int>(((i) / static_cast<float>(out.size()))
                                                      * 70.0))) {
            result = FirmwareInstallResult::FailedCorrupted;
            icon = QMessageBox::Warning;
            ShowMessage();
            return result;
        }
    }

    if (!success) {
        result = FirmwareInstallResult::FailedCopy;
        icon = QMessageBox::Critical;
        ShowMessage();
        return result;
    }

    // Re-scan VFS for the newly placed firmware files.
    system->GetFileSystemController().CreateFactories(*vfs);

    const auto pair = FirmwareManager::GetFirmwareVersion(*system);
    const auto firmware_data = pair.first;
    const std::string display_version(firmware_data.display_version.data());

    result = FirmwareInstallResult::Success;
    QtCommon::Frontend::ShowMessage(QMessageBox::Information,
                                    qApp->tr(successTitle),
                                    qApp->tr(GetFirmwareInstallResultString(result))
                                        .arg(QString::fromStdString(display_version)),
                                    buttons,
                                    parent);
    return result;
}

QString UnzipFirmwareToTmp(const QString& location)
{
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path()};

    if (!fs::create_directories(tmp / "eden" / "firmware")) {
        return "";
    }

    tmp /= "eden";
    tmp /= "firmware";

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip(location);

    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty()) {
        return "";
    }

    return qCacheDir;
}
} // namespace QtCommon
