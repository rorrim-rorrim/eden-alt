#include "qt_game_util.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"

#include <QDesktopServices>
#include <QUrl>
#include "fmt/ostream.h"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace QtCommon {

bool CreateShortcutLink(const std::filesystem::path& shortcut_path,
                                     const std::string& comment,
                                     const std::filesystem::path& icon_path,
                                     const std::filesystem::path& command,
                                     const std::string& arguments, const std::string& categories,
                                     const std::string& keywords, const std::string& name) try {
#ifdef _WIN32 // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT {
        CoUninitialize();
    };
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT {
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    };
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
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
#else // Unsupported platform
    return false;
#endif
}
 catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

bool MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                                      std::filesystem::path& out_icon_path) {
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

void OpenRootDataFolder() {
    QDesktopServices::openUrl(QUrl(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::EdenDir))));
}

void OpenNANDFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::NANDDir))));
}

void OpenSDMCFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::SDMCDir))));
}

void OpenModFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::LoadDir))));
}

void OpenLogFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::LogDir))));
}

}
