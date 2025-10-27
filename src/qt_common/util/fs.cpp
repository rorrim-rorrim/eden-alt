// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fs.h"
#include "common/fs/ryujinx_compat.h"
#include "common/fs/symlink.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/qt_string_lookup.h"

namespace fs = std::filesystem;

namespace QtCommon::FS {

void LinkRyujinx(std::filesystem::path &from, std::filesystem::path &to) {
    std::error_code ec;

    // "ignore" errors--if the dir fails to be deleted, error handling later will handle it
    fs::remove_all(to, ec);

    if (Common::FS::CreateSymlink(from, to)) {
        QtCommon::Frontend::Information(tr("Linked Save Data"), tr("Save data has been linked."));
    } else {
        QtCommon::Frontend::Critical(tr("Failed to link save data"),
                                     tr("Could not link directory:\n\t%1\nTo:\n\t%2")
                                         .arg(QString::fromStdString(from.string()),
                                              QString::fromStdString(to.string())));
    }
}

u64 GetRyujinxSaveID(const u64 &program_id)
{
    auto path = Common::FS::GetKvdbPath();
    std::vector<Common::FS::IMEN> imens;
    Common::FS::IMENReadResult res = Common::FS::ReadKvdb(path, imens);

    if (res == Common::FS::IMENReadResult::Success) {
        // TODO: this can probably be done with std::find_if but I'm lazy
        for (const Common::FS::IMEN &imen : imens) {
            if (imen.title_id == program_id)
                return imen.save_id;
        }

        QtCommon::Frontend::Critical(
            tr("Could not find Ryujinx save data"),
            StringLookup::Lookup(StringLookup::RyujinxNoSaveId).arg(program_id, 0, 16));
    } else {
        // TODO: make this long thing a function or something
        QString caption = StringLookup::Lookup(
            static_cast<StringLookup::StringKey>((int) res + (int) StringLookup::KvdbNonexistent));
        QtCommon::Frontend::Critical(tr("Could not find Ryujinx save data"), caption);
    }

    return -1;
}

} // namespace QtCommon::FS
