#include <filesystem>
#include <JlCompress.h>
#include "frontend_common/mod_manager.h"
#include "mod.h"
#include "qt_common/abstract/frontend.h"

namespace QtCommon::Mod {
QString GetModFolder(const QString& root, const QString& fallbackName) {
    namespace fs = std::filesystem;

    const auto std_root = root.toStdString();
    auto std_path = FrontendCommon::GetModFolder(std_root);

    QString default_name;
    if (!fallbackName.isEmpty())
        default_name = fallbackName;
    else if (std_path)
        default_name = QString::fromStdString(std_path->filename());
    else
        default_name = root.split(QLatin1Char('/')).last();

    QString name = QtCommon::Frontend::GetTextInput(
        tr("Mod Name"), tr("What should this mod be called?"), default_name);

    qDebug() << "Naming mod:" << name;

    // if std_path is empty, frontend_common could not determine mod type and/or name.
    // so we have to prompt the user and set up the structure ourselves
    if (!std_path) {
        // TODO: Carboxyl impl.
        const QStringList choices = {
            tr("RomFS"),
            tr("ExeFS/Patch"),
            tr("Cheat"),
        };

        int choice = QtCommon::Frontend::Choice(
            tr("Mod Type"),
            tr("Could not detect mod type automatically. Please manually "
               "specify the type of mod you downloaded.\n\nMost mods are RomFS mods, but patches "
               "(.pchtxt) are typically ExeFS mods."),
            choices);

        std::string to_make;

        switch (choice) {
        case 0:
            to_make = "romfs";
            break;
        case 1:
            to_make = "exefs";
            break;
        case 2:
            to_make = "cheats";
            break;
        default:
            return QString();
        }

        // now make a temp directory...
        const auto mod_dir = fs::temp_directory_path() / "eden" / "mod" / name.toStdString();
        const auto tmp = mod_dir / to_make;
        fs::remove_all(mod_dir);
        if (!fs::create_directories(tmp))
            return QString();

        std_path = mod_dir;


        // ... and copy everything from the root to the temp dir
        for (const auto& entry : fs::directory_iterator(root.toStdString())) {
            const auto target = tmp / entry.path().filename();

            fs::copy(entry.path(), target,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }
    } else {
        // Rename the existing mod folder.
        const auto new_path = std_path->parent_path() / name.toStdString();
        fs::rename(std_path.value(), new_path);
        std_path = new_path;
    }

    qDebug() << "Mod path" << std_path->string();

    return QString::fromStdString(std_path->string());
}

bool InstallMod(const QString& path, const QString& fallbackName, const u64 program_id,
                const bool copy) {
    const auto target = GetModFolder(path, fallbackName);
    return FrontendCommon::InstallMod(target.toStdString(), program_id, copy);
}

bool InstallModFromZip(const QString& path, const u64 program_id) {
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path() / "eden" / "unzip_mod"};

    fs::remove_all(tmp);
    if (!fs::create_directories(tmp))
        return false;

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip{path};

    // TODO(crueter): use QtCompress
    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty())
        return false;

    const auto fallback = fs::path{path.toStdString()}.stem();

    return InstallMod(qCacheDir, QString::fromStdString(fallback.string()), program_id, false);
}

} // namespace QtCommon::Mod
