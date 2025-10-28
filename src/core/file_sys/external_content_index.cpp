// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/file_sys/external_content_index.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include "common/fs/fs_util.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/loader/loader.h"

namespace fs = std::filesystem;

namespace FileSys {

ExternalContentIndexer::ExternalContentIndexer(VirtualFilesystem vfs,
                                               ManualContentProvider& provider,
                                               ExternalContentPaths paths)
    : m_vfs(std::move(vfs)), m_provider(provider), m_paths(std::move(paths)) {}

void ExternalContentIndexer::Rebuild() {
    m_provider.ClearAllEntries();
    m_updates_by_title.clear();
    m_all_dlc.clear();

    for (const auto& dir : m_paths.update_dirs) {
        IndexUpdatesDir(dir);
    }
    for (const auto& dir : m_paths.dlc_dirs) {
        IndexDlcDir(dir);
    }

    Commit();
}

static std::string ToLowerCopy(const std::string& s) {
    std::string out;
    out.resize(s.size());
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

void ExternalContentIndexer::IndexUpdatesDir(const std::string& dir) {
    try {
        const fs::path p = Common::FS::ToU8String(dir);
        std::error_code ec;
        if (!fs::exists(p, ec) || ec)
            return;

        if (fs::is_directory(p, ec) && !ec) {
            for (const auto& entry : fs::recursive_directory_iterator(
                     p, fs::directory_options::skip_permission_denied, ec)) {
                if (entry.is_directory(ec))
                    continue;
                TryIndexFileAsContainer(Common::FS::ToUTF8String(entry.path().u8string()), true);
            }
            TryIndexLooseDir(Common::FS::ToUTF8String(p.u8string()), true);
        } else {
            TryIndexFileAsContainer(Common::FS::ToUTF8String(p.u8string()), true);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(Loader, "Error accessing update directory '{}': {}", dir, e.what());
    }
}

void ExternalContentIndexer::IndexDlcDir(const std::string& dir) {
    try {
        const fs::path p = Common::FS::ToU8String(dir);
        std::error_code ec;
        if (!fs::exists(p, ec) || ec)
            return;

        if (fs::is_directory(p, ec) && !ec) {
            for (const auto& entry : fs::recursive_directory_iterator(
                     p, fs::directory_options::skip_permission_denied, ec)) {
                if (entry.is_directory(ec))
                    continue;
                TryIndexFileAsContainer(Common::FS::ToUTF8String(entry.path().u8string()), false);
            }
            TryIndexLooseDir(Common::FS::ToUTF8String(p.u8string()), false);
        } else {
            TryIndexFileAsContainer(Common::FS::ToUTF8String(p.u8string()), false);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(Loader, "Error accessing DLC directory '{}': {}", dir, e.what());
    }
}

void ExternalContentIndexer::TryIndexFileAsContainer(const std::string& path, bool is_update) {
    const auto lower = ToLowerCopy(path);
    if (lower.size() >= 4 && lower.rfind(".nsp") == lower.size() - 4) {
        if (auto vf = m_vfs->OpenFile(path, OpenMode::Read)) {
            ParseContainerNSP(vf, is_update);
        }
    }
}

void ExternalContentIndexer::TryIndexLooseDir(const std::string& dir, bool is_update) {
    fs::path p = Common::FS::ToU8String(dir);
    std::error_code ec;
    if (!fs::is_directory(p, ec) || ec)
        return;

    for (const auto& entry :
         fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file(ec))
            continue;
        const auto path = Common::FS::ToUTF8String(entry.path().u8string());
        const auto lower = ToLowerCopy(path);
        if (lower.size() >= 9 && lower.rfind(".cnmt.nca") == lower.size() - 9) {
            if (auto vf = m_vfs->OpenFile(path, OpenMode::Read)) {
                ParseLooseCnmtNca(
                    vf, Common::FS::ToUTF8String(entry.path().parent_path().u8string()), is_update);
            }
        }
    }
}

void ExternalContentIndexer::ParseContainerNSP(VirtualFile file, bool is_update) {
    if (file == nullptr)
        return;
    NSP nsp(file);
    if (nsp.GetStatus() != Loader::ResultStatus::Success) {
        LOG_WARNING(Loader, "ExternalContent: NSP parse failed");
        return;
    }

    const auto title_map = nsp.GetNCAs();
    if (title_map.empty())
        return;

    for (const auto& [title_id, nca_map] : title_map) {
        std::shared_ptr<NCA> meta_nca;
        for (const auto& [key, nca_ptr] : nca_map) {
            if (nca_ptr && nca_ptr->GetType() == NCAContentType::Meta) {
                meta_nca = nca_ptr;
                break;
            }
        }
        if (!meta_nca)
            continue;

        auto cnmt_opt = ExtractCnmtFromMetaNca(*meta_nca);
        if (!cnmt_opt)
            continue;
        const auto& cnmt = *cnmt_opt;

        const auto base_id = BaseTitleId(title_id);

        if (is_update && cnmt.GetType() == TitleType::Update) {
            ParsedUpdate candidate{};
            // Register updates under their Update TID so PatchManager can find/apply them
            candidate.title_id = FileSys::GetUpdateTitleID(base_id);
            candidate.version = cnmt.GetTitleVersion();
            for (const auto& rec : cnmt.GetContentRecords()) {
                const auto it = nca_map.find({cnmt.GetType(), rec.type});
                if (it != nca_map.end() && it->second) {
                    candidate.ncas[rec.type] = it->second->GetBaseFile();
                }
            }
            auto& vec = m_updates_by_title[base_id];
            vec.emplace_back(std::move(candidate));
        } else if (cnmt.GetType() == TitleType::AOC) {
            const auto dlc_title_id = cnmt.GetTitleID();
            for (const auto& rec : cnmt.GetContentRecords()) {
                const auto it = nca_map.find({cnmt.GetType(), rec.type});
                if (it != nca_map.end() && it->second) {
                    m_all_dlc.push_back(
                        ParsedDlcRecord{dlc_title_id, {}, it->second->GetBaseFile()});
                }
            }
        }
    }
}

void ExternalContentIndexer::ParseLooseCnmtNca(VirtualFile meta_nca_file, const std::string& folder,
                                               bool is_update) {
    if (meta_nca_file == nullptr)
        return;

    NCA meta(meta_nca_file);

    if (!IsMeta(meta))
        return;

    auto cnmt_opt = ExtractCnmtFromMetaNca(meta);
    if (!cnmt_opt)
        return;
    const auto& cnmt = *cnmt_opt;

    const auto base_id = BaseTitleId(cnmt.GetTitleID());

    if (is_update && cnmt.GetType() == TitleType::Update) {
        ParsedUpdate candidate{};
        // Register updates under their Update TID so PatchManager can find/apply them
        candidate.title_id = FileSys::GetUpdateTitleID(base_id);
        candidate.version = cnmt.GetTitleVersion();

        for (const auto& rec : cnmt.GetContentRecords()) {
            const auto file_name = Common::HexToString(rec.nca_id) + ".nca";
            const auto full = Common::FS::ToUTF8String(
                (fs::path(Common::FS::ToU8String(folder)) / fs::path(file_name)).u8string());
            if (auto vf = m_vfs->OpenFile(full, OpenMode::Read)) {
                candidate.ncas[rec.type] = vf;
            }
        }

        auto& vec = m_updates_by_title[base_id];
        vec.emplace_back(std::move(candidate));
    } else if (cnmt.GetType() == TitleType::AOC) {
        const auto dlc_title_id = cnmt.GetTitleID();
        for (const auto& rec : cnmt.GetContentRecords()) {
            const auto file_name = Common::HexToString(rec.nca_id) + ".nca";
            const auto full = Common::FS::ToUTF8String(
                (fs::path(Common::FS::ToU8String(folder)) / fs::path(file_name)).u8string());
            if (auto vf = m_vfs->OpenFile(full, OpenMode::Read)) {
                ParsedDlcRecord dl{dlc_title_id, {}, vf};
                m_all_dlc.push_back(std::move(dl));
            }
        }
    }
}

std::optional<CNMT> ExternalContentIndexer::ExtractCnmtFromMetaNca(const NCA& meta_nca) {
    if (meta_nca.GetStatus() != Loader::ResultStatus::Success)
        return std::nullopt;

    const auto subs = meta_nca.GetSubdirectories();
    if (subs.empty() || !subs[0])
        return std::nullopt;

    const auto files = subs[0]->GetFiles();
    if (files.empty() || !files[0])
        return std::nullopt;

    CNMT cnmt(files[0]);
    return cnmt;
}

ExternalContentIndexer::TitleID ExternalContentIndexer::BaseTitleId(TitleID id) {
    return FileSys::GetBaseTitleID(id);
}

bool ExternalContentIndexer::IsMeta(const NCA& nca) {
    return nca.GetType() == NCAContentType::Meta;
}

void ExternalContentIndexer::Commit() {
    // Updates: register all discovered versions per base title under unique variant TIDs,
    // and additionally register the highest version under the canonical update TID for default
    // usage.
    size_t update_variants_count = 0;
    for (auto& [base_title, vec] : m_updates_by_title) {
        if (vec.empty())
            continue;
        // sort ascending by version, dedupe identical versions (for NAND overlap, for example)
        std::stable_sort(vec.begin(), vec.end(), [](const ParsedUpdate& a, const ParsedUpdate& b) {
            return a.version < b.version;
        });
        vec.erase(std::unique(vec.begin(), vec.end(),
                              [](const ParsedUpdate& a, const ParsedUpdate& b) {
                                  return a.version == b.version;
                              }),
                  vec.end());

        // highest version for canonical TID
        const auto& latest = vec.back();
        for (const auto& [rtype, file] : latest.ncas) {
            if (!file)
                continue;
            const auto canonical_tid = FileSys::GetUpdateTitleID(base_title);
            m_provider.AddEntry(TitleType::Update, rtype, canonical_tid, file);
        }

        // variants under update_tid + i (i starts at1 to avoid colliding with canonical)
        for (size_t i = 0; i < vec.size(); ++i) {
            const auto& upd = vec[i];
            const u64 variant_tid = FileSys::GetUpdateTitleID(base_title) + static_cast<u64>(i + 1);
            for (const auto& [rtype, file] : upd.ncas) {
                if (!file)
                    continue;
                m_provider.AddEntry(TitleType::Update, rtype, variant_tid, file);
            }
        }
        update_variants_count += vec.size();
    }

    // DLC: additive
    for (const auto& dlc : m_all_dlc) {
        if (!dlc.file)
            continue;
        m_provider.AddEntry(TitleType::AOC, ContentRecordType::Data, dlc.title_id, dlc.file);
    }

    LOG_INFO(Loader,
             "ExternalContent: registered updates for {} titles ({} variants), {} DLC records",
             m_updates_by_title.size(), update_variants_count, m_all_dlc.size());
}

} // namespace FileSys