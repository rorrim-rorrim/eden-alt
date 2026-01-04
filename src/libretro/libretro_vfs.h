// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <filesystem>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "eden_libretro/libretro.h"

namespace LibretroVFS {

// VFS file handle wrapper
struct VFSFileHandle {
    FILE* fp = nullptr;
    std::string path;
    unsigned mode = 0;

    VFSFileHandle() = default;
    ~VFSFileHandle() {
        if (fp) {
            fclose(fp);
            fp = nullptr;
        }
    }
};

// VFS directory handle wrapper
struct VFSDirHandle {
    std::filesystem::directory_iterator iter;
    std::filesystem::directory_iterator end;
    std::string current_name;
    bool is_current_dir = false;
    bool has_entry = false;

    VFSDirHandle() = default;
};

// VFS Implementation Functions
inline const char* vfs_get_path(struct retro_vfs_file_handle* stream) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle) return nullptr;
    return handle->path.c_str();
}

inline struct retro_vfs_file_handle* vfs_open(const char* path, unsigned mode, unsigned hints) {
    (void)hints;

    if (!path) return nullptr;

    std::string mode_str;
    if (mode & RETRO_VFS_FILE_ACCESS_READ) {
        if (mode & RETRO_VFS_FILE_ACCESS_WRITE) {
            if (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) {
                mode_str = "r+b";
            } else {
                mode_str = "w+b";
            }
        } else {
            mode_str = "rb";
        }
    } else if (mode & RETRO_VFS_FILE_ACCESS_WRITE) {
        if (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) {
            mode_str = "r+b";
        } else {
            mode_str = "wb";
        }
    } else {
        return nullptr;
    }

    FILE* fp = fopen(path, mode_str.c_str());
    if (!fp) return nullptr;

    auto* handle = new VFSFileHandle();
    handle->fp = fp;
    handle->path = path;
    handle->mode = mode;

    return reinterpret_cast<struct retro_vfs_file_handle*>(handle);
}

inline int vfs_close(struct retro_vfs_file_handle* stream) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle) return -1;

    delete handle;
    return 0;
}

inline int64_t vfs_size(struct retro_vfs_file_handle* stream) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp) return -1;

    long current = ftell(handle->fp);
    if (current < 0) return -1;

    if (fseek(handle->fp, 0, SEEK_END) != 0) return -1;
    long size = ftell(handle->fp);

    if (fseek(handle->fp, current, SEEK_SET) != 0) return -1;

    return static_cast<int64_t>(size);
}

inline int64_t vfs_tell(struct retro_vfs_file_handle* stream) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp) return -1;

    return static_cast<int64_t>(ftell(handle->fp));
}

inline int64_t vfs_seek(struct retro_vfs_file_handle* stream, int64_t offset, int seek_position) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp) return -1;

    int whence;
    switch (seek_position) {
        case RETRO_VFS_SEEK_POSITION_START:   whence = SEEK_SET; break;
        case RETRO_VFS_SEEK_POSITION_CURRENT: whence = SEEK_CUR; break;
        case RETRO_VFS_SEEK_POSITION_END:     whence = SEEK_END; break;
        default: return -1;
    }

    if (fseek(handle->fp, static_cast<long>(offset), whence) != 0) return -1;

    return static_cast<int64_t>(ftell(handle->fp));
}

inline int64_t vfs_read(struct retro_vfs_file_handle* stream, void* s, uint64_t len) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp || !s) return -1;

    return static_cast<int64_t>(fread(s, 1, static_cast<size_t>(len), handle->fp));
}

inline int64_t vfs_write(struct retro_vfs_file_handle* stream, const void* s, uint64_t len) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp || !s) return -1;

    return static_cast<int64_t>(fwrite(s, 1, static_cast<size_t>(len), handle->fp));
}

inline int vfs_flush(struct retro_vfs_file_handle* stream) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp) return -1;

    return fflush(handle->fp);
}

inline int vfs_remove(const char* path) {
    if (!path) return -1;
    return std::remove(path);
}

inline int vfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return -1;
    return std::rename(old_path, new_path);
}

inline int64_t vfs_truncate(struct retro_vfs_file_handle* stream, int64_t length) {
    auto* handle = reinterpret_cast<VFSFileHandle*>(stream);
    if (!handle || !handle->fp) return -1;

#ifdef _WIN32
    if (_chsize(_fileno(handle->fp), static_cast<long>(length)) != 0) return -1;
#else
    if (ftruncate(fileno(handle->fp), static_cast<off_t>(length)) != 0) return -1;
#endif

    return 0;
}

inline int vfs_stat(const char* path, int32_t* size) {
    if (!path) return 0;

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) return 0;

    int flags = 0;
    if (std::filesystem::exists(status)) {
        flags |= RETRO_VFS_STAT_IS_VALID;

        if (std::filesystem::is_directory(status)) {
            flags |= RETRO_VFS_STAT_IS_DIRECTORY;
        }

        if (std::filesystem::is_character_file(status)) {
            flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
        }

        if (size && std::filesystem::is_regular_file(status)) {
            auto file_size = std::filesystem::file_size(path, ec);
            if (!ec) {
                *size = static_cast<int32_t>(file_size);
            }
        }
    }

    return flags;
}

inline int vfs_mkdir(const char* dir) {
    if (!dir) return -1;

    std::error_code ec;
    if (std::filesystem::create_directories(dir, ec)) {
        return 0;
    }
    return ec ? -1 : 0;
}

inline struct retro_vfs_dir_handle* vfs_opendir(const char* dir, bool include_hidden) {
    (void)include_hidden;

    if (!dir) return nullptr;

    std::error_code ec;
    auto iter = std::filesystem::directory_iterator(dir, ec);
    if (ec) return nullptr;

    auto* handle = new VFSDirHandle();
    handle->iter = std::move(iter);
    handle->end = std::filesystem::directory_iterator();

    return reinterpret_cast<struct retro_vfs_dir_handle*>(handle);
}

inline bool vfs_readdir(struct retro_vfs_dir_handle* dirstream) {
    auto* handle = reinterpret_cast<VFSDirHandle*>(dirstream);
    if (!handle) return false;

    if (handle->iter == handle->end) {
        handle->has_entry = false;
        return false;
    }

    handle->current_name = handle->iter->path().filename().string();
    handle->is_current_dir = handle->iter->is_directory();
    handle->has_entry = true;

    std::error_code ec;
    handle->iter.increment(ec);

    return true;
}

inline const char* vfs_dirent_get_name(struct retro_vfs_dir_handle* dirstream) {
    auto* handle = reinterpret_cast<VFSDirHandle*>(dirstream);
    if (!handle || !handle->has_entry) return nullptr;

    return handle->current_name.c_str();
}

inline bool vfs_dirent_is_dir(struct retro_vfs_dir_handle* dirstream) {
    auto* handle = reinterpret_cast<VFSDirHandle*>(dirstream);
    if (!handle || !handle->has_entry) return false;

    return handle->is_current_dir;
}

inline int vfs_closedir(struct retro_vfs_dir_handle* dirstream) {
    auto* handle = reinterpret_cast<VFSDirHandle*>(dirstream);
    if (!handle) return -1;

    delete handle;
    return 0;
}

// Get the VFS interface
inline struct retro_vfs_interface* GetVFSInterface() {
    static struct retro_vfs_interface vfs_interface = {
        vfs_get_path,
        vfs_open,
        vfs_close,
        vfs_size,
        vfs_tell,
        vfs_seek,
        vfs_read,
        vfs_write,
        vfs_flush,
        vfs_remove,
        vfs_rename,
        vfs_truncate,
        vfs_stat,
        vfs_mkdir,
        vfs_opendir,
        vfs_readdir,
        vfs_dirent_get_name,
        vfs_dirent_is_dir,
        vfs_closedir
    };
    return &vfs_interface;
}

} // namespace LibretroVFS
