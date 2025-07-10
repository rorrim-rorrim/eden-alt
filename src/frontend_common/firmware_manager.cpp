#include "firmware_manager.h"
#include <filesystem>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"

#include "common/logging/backend.h"

#include "core/crypto/key_manager.h"
#include "frontend_common/content_manager.h"

FirmwareManager::KeyInstallResult FirmwareManager::InstallDecryptionKeys(std::string location)
{
    LOG_INFO(Frontend, "Installing key files from {}", location);

    const std::filesystem::path prod_key_path = location;
    const std::filesystem::path key_source_path = prod_key_path.parent_path();
    if (!Common::FS::IsDir(key_source_path)) {
        return InvalidDir;
    }

    bool prod_keys_found = false;
    std::vector<std::filesystem::path> source_key_files;

    if (Common::FS::Exists(prod_key_path)) {
        prod_keys_found = true;
        source_key_files.emplace_back(prod_key_path);
    }

    if (Common::FS::Exists(key_source_path / "title.keys")) {
        source_key_files.emplace_back(key_source_path / "title.keys");
    }

    if (Common::FS::Exists(key_source_path / "key_retail.bin")) {
        source_key_files.emplace_back(key_source_path / "key_retail.bin");
    }

    if (source_key_files.empty() || !prod_keys_found) {
        return ErrorWrongFilename;
    }

    const auto yuzu_keys_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::KeysDir);
    for (const auto &key_file : source_key_files) {
        std::filesystem::path destination_key_file = yuzu_keys_dir / key_file.filename();
        if (!std::filesystem::copy_file(key_file,
                                        destination_key_file,
                                        std::filesystem::copy_options::overwrite_existing)) {
            LOG_ERROR(Frontend,
                      "Failed to copy file {} to {}",
                      key_file.string(),
                      destination_key_file.string());
            return ErrorFailedCopy;
        }
    }

    // Reinitialize the key manager

    Core::Crypto::KeyManager::Instance().ReloadKeys();

    if (ContentManager::AreKeysPresent()) {
        return Success;
    }

    // let the frontend handle checking everything else
    return ErrorFailedInit;
}

FirmwareManager::FirmwareCheckResult FirmwareManager::VerifyFirmware(Core::System &system)
{
    if (!CheckFirmwarePresence(system)) {
        return ErrorFirmwareMissing;
    } else {
        const auto pair = GetFirmwareVersion(system);
        const auto firmware_data = pair.first;
        const auto result = pair.second;

        if (result.IsError()) {
            LOG_INFO(Frontend, "Unable to read firmware");
            return ErrorFirmwareCorrupted;
        }

        // TODO: update this whenever newer firmware is properly supported
        if (firmware_data.major > 19) {
            return ErrorFirmwareTooNew;
        }
    }

    return FirmwareGood;
}
