#include "qt_common.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "uisettings.h"

#include <QGuiApplication>
#include <QStringLiteral>
#include <QWindow>
#include "common/logging/log.h"
#include "core/frontend/emu_window.h"

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
        return Empty;
    } else if (Common::FS::RemoveDirRecursively(
                   Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list")) {
        return Success;
        UISettings::values.is_game_list_reload_pending.exchange(true);
    } else {
        return Failure;
    }
}

Core::Frontend::WindowSystemType GetWindowSystemType() {
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

Core::Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow* window) {
    Core::Frontend::EmuWindow::WindowSystemInfo wsi;
    wsi.type = GetWindowSystemType();

#if defined(WIN32)
    // Our Win32 Qt external doesn't have the private API.
    wsi.render_surface = reinterpret_cast<void*>(window->winId());
#elif defined(__APPLE__)
    wsi.render_surface = reinterpret_cast<void* (*)(id, SEL)>(objc_msgSend)(
        reinterpret_cast<id>(window->winId()), sel_registerName("layer"));
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

}
