// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlink.h"

#ifdef _WIN32
#include <winbase.h>
#endif

namespace fs = std::filesystem;

// The sole purpose of this file is to treat symlinks like symlinks on POSIX,
// or treat them as directory junctions on Windows.
// This is because, for some inexplicable reason, Microsoft has locked symbolic
// links behind a "security policy", whereas directory junctions--functionally identical
// for directories, by the way--are not. Why? I don't know.

namespace Common::FS {

bool CreateSymlink(const fs::path &from, const fs::path &to)
{
    // TODO: test this, + does it need symlink perms?
#ifdef _WIN32
    return CreateSymbolicLinkW(to.wstring().c_str(),
                               from.wstring().c_str(),
                               SYMBOLIC_LINK_FLAG_DIRECTORY);
#else
    std::error_code ec;
    fs::create_directory_symlink(from, to, ec);
    return !ec;
#endif
}

bool IsSymlink(const fs::path &path)
{
#ifdef _WIN32
    return fs::status(path).type() == fs::file_type::junction;
#else
    return fs::status(path).type() == fs::file_type::symlink;
#endif
}

} // namespace Common::FS
