// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "common/common_types.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/nca_metadata.h"

namespace FileSys {

class ManualContentProvider;
class NCA;

struct ExternalContentPaths {
    std::vector<std::string> update_dirs;
    std::vector<std::string> dlc_dirs;
};

class ExternalContentIndexer {
public:
    ExternalContentIndexer(VirtualFilesystem vfs,
                           ManualContentProvider& provider,
                           ExternalContentPaths paths);

    void Rebuild();

private:
    using TitleID = u64;

    struct ParsedUpdate {
        TitleID title_id{};
        u32 version{};
        std::unordered_map<ContentRecordType, VirtualFile> ncas;
    };

    struct ParsedDlcRecord {
        TitleID title_id{};
        NcaID nca_id{};
        VirtualFile file{};
    };

    void IndexUpdatesDir(const std::string& dir);
    void IndexDlcDir(const std::string& dir);

    void TryIndexFileAsContainer(const std::string& path, bool is_update);
    void TryIndexLooseDir(const std::string& dir, bool is_update);

    void ParseContainerNSP(VirtualFile file, bool is_update);
    void ParseLooseCnmtNca(VirtualFile meta_nca_file, const std::string& folder, bool is_update);

    static std::optional<CNMT> ExtractCnmtFromMetaNca(const NCA& meta_nca);
    static TitleID BaseTitleId(TitleID id);
    static bool IsMeta(const NCA& nca);

    void Commit();

private:
    VirtualFilesystem m_vfs;
    ManualContentProvider& m_provider;
    ExternalContentPaths m_paths;

    std::unordered_map<TitleID, std::vector<ParsedUpdate>> m_updates_by_title;

    std::vector<ParsedDlcRecord> m_all_dlc;
};

} // namespace FileSys