# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2020 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# TODO(crueter): Remove this entirely; notably, cubeb and ffmpeg conflict due to some weird ksuser stuff
function(copy_yuzu_FFmpeg_deps target_dir)
    include(WindowsCopyFiles)
    set(DLL_DEST "$<TARGET_FILE_DIR:${target_dir}>/")
    file(READ "${FFmpeg_PATH}/requirements.txt" FFmpeg_REQUIRED_DLLS)
    string(STRIP "${FFmpeg_REQUIRED_DLLS}" FFmpeg_REQUIRED_DLLS)

    message(STATUS "[CopyYuzuFFmpegDeps] Copying FFmpeg deps from ${FFmpeg_LIBRARY_DIR} to ${DLL_DEST}")
    message(STATUS "[CopyYuzuFFmpegDeps] FFmpeg DLLs: ${FFmpeg_REQUIRED_DLLS}")
    windows_copy_files(${target_dir} ${FFmpeg_LIBRARY_DIR} ${DLL_DEST} ${FFmpeg_REQUIRED_DLLS})
endfunction(copy_yuzu_FFmpeg_deps)
