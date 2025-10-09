// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "frontend_common/data_manager.h"
#include "qt_content_util.h"
#include "common/fs/fs.h"
#include "core/hle/service/acc/profile_manager.h"
#include "frontend_common/content_manager.h"
#include "frontend_common/firmware_manager.h"
#include "qt_common/qt_common.h"
#include "qt_common/qt_game_util.h"
#include "qt_common/qt_progress_dialog.h"
#include "qt_frontend_util.h"

#include <QFuture>
#include <QtConcurrentRun>
#include <JlCompress.h>
#include <qfuturewatcher.h>

namespace QtCommon::Content {

bool CheckGameFirmware(u64 program_id, QObject* parent)
{
    if (FirmwareManager::GameRequiresFirmware(program_id)
        && !FirmwareManager::CheckFirmwarePresence(*system)) {
        auto result = QtCommon::Frontend::ShowMessage(
            QMessageBox::Warning,
            tr("Game Requires Firmware"),
            tr("The game you are trying to launch requires firmware to boot or to get past the "
               "opening menu. Please <a href='https://yuzu-mirror.github.io/help/quickstart'>"
               "dump and install firmware</a>, or press \"OK\" to launch anyways."),
            QMessageBox::Ok | QMessageBox::Cancel,
            parent);

        return result == QMessageBox::Ok;
    }

    return true;
}

void InstallFirmware(const QString& location, bool recursive)
{
    QtCommon::Frontend::QtProgressDialog progress(tr("Installing Firmware..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();

    // Declare progress callback.
    auto callback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    static constexpr const char* failedTitle = "Firmware Install Failed";
    static constexpr const char* successTitle = "Firmware Install Succeeded";
    QMessageBox::Icon icon;
    FirmwareInstallResult result;

    const auto ShowMessage = [&]() {
        QtCommon::Frontend::ShowMessage(icon,
                                        tr(failedTitle),
                                        tr(GetFirmwareInstallResultString(result)));
    };

    LOG_INFO(Frontend, "Installing firmware from {}", location.toStdString());

    // Check for a reasonable number of .nca files (don't hardcode them, just see if there's some in
    // there.)
    std::filesystem::path firmware_source_path = location.toStdString();
    if (!Common::FS::IsDir(firmware_source_path)) {
        return;
    }

    std::vector<std::filesystem::path> out;
    const Common::FS::DirEntryCallable dir_callback =
        [&out](const std::filesystem::directory_entry& entry) {
            if (entry.path().has_extension() && entry.path().extension() == ".nca") {
                out.emplace_back(entry.path());
            }

            return true;
        };

    callback(100, 10);

    if (recursive) {
        Common::FS::IterateDirEntriesRecursively(firmware_source_path,
                                                 dir_callback,
                                                 Common::FS::DirEntryFilter::File);
    } else {
        Common::FS::IterateDirEntries(firmware_source_path,
                                      dir_callback,
                                      Common::FS::DirEntryFilter::File);
    }

    if (out.size() <= 0) {
        result = FirmwareInstallResult::NoNCAs;
        icon = QMessageBox::Warning;
        ShowMessage();
        return;
    }

    // Locate and erase the content of nand/system/Content/registered/*.nca, if any.
    auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    if (sysnand_content_vdir->IsWritable()
        && !sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
        result = FirmwareInstallResult::FailedDelete;
        icon = QMessageBox::Critical;
        ShowMessage();
        return;
    }

    LOG_INFO(Frontend,
             "Cleaned nand/system/Content/registered folder in preparation for new firmware.");

    callback(100, 20);

    auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");

    bool success = true;
    int i = 0;
    for (const auto& firmware_src_path : out) {
        i++;
        auto firmware_src_vfile = vfs->OpenFile(firmware_src_path.generic_string(),
                                                FileSys::OpenMode::Read);
        auto firmware_dst_vfile = firmware_vdir->CreateFileRelative(
            firmware_src_path.filename().string());

        if (!VfsRawCopy(firmware_src_vfile, firmware_dst_vfile)) {
            LOG_ERROR(Frontend,
                      "Failed to copy firmware file {} to {} in registered folder!",
                      firmware_src_path.generic_string(),
                      firmware_src_path.filename().string());
            success = false;
        }

        if (callback(100, 20 + static_cast<int>(((i) / static_cast<float>(out.size())) * 70.0))) {
            result = FirmwareInstallResult::FailedCorrupted;
            icon = QMessageBox::Warning;
            ShowMessage();
            return;
        }
    }

    if (!success) {
        result = FirmwareInstallResult::FailedCopy;
        icon = QMessageBox::Critical;
        ShowMessage();
        return;
    }

    // Re-scan VFS for the newly placed firmware files.
    system->GetFileSystemController().CreateFactories(*vfs);

    auto VerifyFirmwareCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(90 + static_cast<int>((processed_size * 10) / total_size));
        return progress.wasCanceled();
    };

    auto results = ContentManager::VerifyInstalledContents(*QtCommon::system,
                                                           *QtCommon::provider,
                                                           VerifyFirmwareCallback,
                                                           true);

    if (results.size() > 0) {
        const auto failed_names = QString::fromStdString(
            fmt::format("{}", fmt::join(results, "\n")));
        progress.close();
        QtCommon::Frontend::Critical(
            tr("Firmware integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
        return;
    }

    progress.close();

    const auto pair = FirmwareManager::GetFirmwareVersion(*system);
    const auto firmware_data = pair.first;
    const std::string display_version(firmware_data.display_version.data());

    result = FirmwareInstallResult::Success;
    QtCommon::Frontend::Information(
        rootObject,
        tr(successTitle),
        tr(GetFirmwareInstallResultString(result)).arg(QString::fromStdString(display_version)));
}

QString UnzipFirmwareToTmp(const QString& location)
{
    namespace fs = std::filesystem;
    fs::path tmp{fs::temp_directory_path()};

    if (!fs::create_directories(tmp / "eden" / "firmware")) {
        return QString();
    }

    tmp /= "eden";
    tmp /= "firmware";

    QString qCacheDir = QString::fromStdString(tmp.string());

    QFile zip(location);

    QStringList result = JlCompress::extractDir(&zip, qCacheDir);
    if (result.isEmpty()) {
        return QString();
    }

    return qCacheDir;
}

// Content //
void VerifyGameContents(const std::string& game_path)
{
    QtCommon::Frontend::QtProgressDialog progress(tr("Verifying integrity..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    const auto callback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const auto result = ContentManager::VerifyGameContents(*system, game_path, callback);

    switch (result) {
    case ContentManager::GameVerificationResult::Success:
        QtCommon::Frontend::Information(rootObject,
                                        tr("Integrity verification succeeded!"),
                                        tr("The operation completed successfully."));
        break;
    case ContentManager::GameVerificationResult::Failed:
        QtCommon::Frontend::Critical(rootObject,
                                     tr("Integrity verification failed!"),
                                     tr("File contents may be corrupt or missing."));
        break;
    case ContentManager::GameVerificationResult::NotImplemented:
        QtCommon::Frontend::Warning(
            rootObject,
            tr("Integrity verification couldn't be performed"),
            tr("Firmware installation cancelled, firmware may be in a bad state or corrupted. "
               "File contents could not be checked for validity."));
    }
}

void InstallKeys()
{
    const QString key_source_location
        = QtCommon::Frontend::GetOpenFileName(tr("Select Dumped Keys Location"),
                                              {},
                                              QStringLiteral("Decryption Keys (*.keys)"),
                                              {},
                                              QtCommon::Frontend::Option::ReadOnly);

    if (key_source_location.isEmpty()) {
        return;
    }

    FirmwareManager::KeyInstallResult result
        = FirmwareManager::InstallKeys(key_source_location.toStdString(), "keys");

    system->GetFileSystemController().CreateFactories(*QtCommon::vfs);

    switch (result) {
    case FirmwareManager::KeyInstallResult::Success:
        QtCommon::Frontend::Information(tr("Decryption Keys install succeeded"),
                                        tr("Decryption Keys were successfully installed"));
        break;
    default:
        QtCommon::Frontend::Critical(tr("Decryption Keys install failed"),
                                     tr(FirmwareManager::GetKeyInstallResultString(result)));
        break;
    }
}

void VerifyInstalledContents()
{
    // Initialize a progress dialog.
    QtCommon::Frontend::QtProgressDialog progress(tr("Verifying integrity..."),
                                                  tr("Cancel"),
                                                  0,
                                                  100,
                                                  QtCommon::rootObject);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    // Declare progress callback.
    auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const std::vector<std::string> result
        = ContentManager::VerifyInstalledContents(*QtCommon::system,
                                                  *QtCommon::provider,
                                                  QtProgressCallback);
    progress.close();

    if (result.empty()) {
        QtCommon::Frontend::Information(tr("Integrity verification succeeded!"),
                                        tr("The operation completed successfully."));
    } else {
        const auto failed_names = QString::fromStdString(fmt::format("{}", fmt::join(result, "\n")));
        QtCommon::Frontend::Critical(
            tr("Integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
    }
}

void FixProfiles()
{
    // Reset user save files after config is initialized and migration is done.
    // Doing it at init time causes profiles to read from the wrong place entirely if NAND dir is not default
    // TODO: better solution
    system->GetProfileManager().ResetUserSaveFile();
    std::vector<std::string> orphaned = system->GetProfileManager().FindOrphanedProfiles();

    // no orphaned dirs--all good :)
    if (orphaned.empty())
        return;

    // otherwise, let the user know
    QString qorphaned;

    // max. of 8 orphaned profiles is fair, I think
    // 33 = 32 (UUID) + 1 (\n)
    qorphaned.reserve(8 * 33);

    for (const std::string& s : orphaned) {
        qorphaned = qorphaned % QStringLiteral("\n") % QString::fromStdString(s);
    }

    QtCommon::Frontend::Critical(
        tr("Orphaned Profiles Detected!"),
        tr("UNEXPECTED BAD THINGS MAY HAPPEN IF YOU DON'T READ THIS!\n"
           "Eden has detected the following save directories with no attached profile:\n"
           "%1\n\n"
           "Click \"OK\" to open your save folder and fix up your profiles.\n"
           "Hint: copy the contents of the largest or last-modified folder  elsewhere, "
           "delete all orphaned profiles, and move your copied contents to the good profile.")
            .arg(qorphaned));

    QtCommon::Game::OpenSaveFolder();
}

void ClearDataDir(FrontendCommon::DataManager::DataDir dir)
{
    auto result = QtCommon::Frontend::Warning(tr("Really clear data?"),
                                              tr("Important data may be lost!"),
                                              QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    result = QtCommon::Frontend::Warning(
        tr("Are you REALLY sure?"),
        tr("Once deleted, your data will NOT come back!\n"
           "Only do this if you're 100% sure you want to delete this data."),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    QtCommon::Frontend::QtProgressDialog dialog(tr("Clearing..."), QString(), 0, 0);
    dialog.show();

    FrontendCommon::DataManager::ClearDir(dir);

    dialog.close();
}

void ExportDataDir(FrontendCommon::DataManager::DataDir data_dir, std::function<void()> callback)
{
    const std::string dir = FrontendCommon::DataManager::GetDataDir(data_dir);

    const QString zip_dump_location
        = QtCommon::Frontend::GetSaveFileName(tr("Select Export Location"),
                                              QStringLiteral("export.zip"),
                                              tr("Zipped Archives (*.zip)"));

    if (zip_dump_location.isEmpty())
        return;

    QMetaObject::Connection* connection = new QMetaObject::Connection;
    *connection = QObject::connect(qApp, &QGuiApplication::aboutToQuit, rootObject, [=]() mutable {
        QtCommon::Frontend::Warning(tr("Still Exporting"),
                                    tr("Eden is still exporting some data, and will continue "
                                       "running in the background until it's done."));
    });

    QtCommon::Frontend::QtProgressDialog* progress = new QtCommon::Frontend::QtProgressDialog(
        tr("Compressing, this may take a while..."), tr("Background"), 0, 0, rootObject);

    progress->setWindowModality(Qt::WindowModal);
    progress->show();
    QGuiApplication::processEvents();

    QFuture<bool> future = QtConcurrent::run([&]() {
        return JlCompress::compressDir(zip_dump_location,
                                       QString::fromStdString(dir),
                                       true,
                                       QDir::Hidden | QDir::Files | QDir::Dirs);
    });

    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(rootObject);

    QObject::connect(watcher, &QFutureWatcher<bool>::finished, rootObject, [=]() {
        progress->close();
        progress->deleteLater();
        QObject::disconnect(*connection);
        delete connection;

        if (watcher->result()) {
            QtCommon::Frontend::Information(tr("Exported Successfully"),
                                            tr("Data was exported successfully."));
        } else {
            QtCommon::Frontend::Critical(
                tr("Export Failed"),
                tr("Ensure you have write permissions on the targeted directory and try again."));
        }

        watcher->deleteLater();
        callback();
    });

    watcher->setFuture(future);
}

void ImportDataDir(FrontendCommon::DataManager::DataDir data_dir, std::function<void()> callback)
{
    const std::string dir = FrontendCommon::DataManager::GetDataDir(data_dir);

    using namespace QtCommon::Frontend;

    const QString zip_dump_location = GetOpenFileName(tr("Select Import Location"),
                                                      {},
                                                      tr("Zipped Archives (*.zip)"));

    if (zip_dump_location.isEmpty())
        return;

    StandardButton button = Warning(
        tr("Import Warning"),
        tr("All previous data in this directory will be deleted. Are you sure you wish to "
           "proceed?"),
        StandardButton::Yes | StandardButton::No);

    if (button != QMessageBox::Yes)
        return;

    FrontendCommon::DataManager::ClearDir(data_dir);

    QMetaObject::Connection* connection = new QMetaObject::Connection;
    *connection = QObject::connect(qApp, &QGuiApplication::aboutToQuit, rootObject, [=]() mutable {
        Warning(tr("Still Importing"),
                tr("Eden is still importing some data, and will continue "
                   "running in the background until it's done."));
    });

    QtProgressDialog* progress = new QtProgressDialog(tr("Decompressing, this may take a while..."),
                                                      tr("Background"),
                                                      0,
                                                      0,
                                                      rootObject);

    progress->setWindowModality(Qt::WindowModal);
    progress->show();
    QGuiApplication::processEvents();

    QFuture<bool> future = QtConcurrent::run([=]() {
        return !JlCompress::extractDir(zip_dump_location,
                                       QString::fromStdString(dir)).empty();
    });

    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(rootObject);

    QObject::connect(watcher, &QFutureWatcher<bool>::finished, rootObject, [=]() {
        progress->close();
        progress->deleteLater();
        QObject::disconnect(*connection);
        delete connection;

        if (watcher->result()) {
            Information(tr("Imported Successfully"), tr("Data was imported successfully."));
        } else {
            Critical(
                tr("Import Failed"),
                tr("Ensure you have read permissions on the targeted directory and try again."));
        }

        watcher->deleteLater();
        callback();
    });

    watcher->setFuture(future);
}

} // namespace QtCommon::Content
