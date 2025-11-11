# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

function(FixMsysPath target)
    get_target_property(include_dir ${target} INTERFACE_INCLUDE_DIRECTORIES)

    if (include_dir MATCHES "^C:")
        return()
    endif()

    set(include_dir "C:/msys64${include_dir}")
    set_target_properties(${target} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${include_dir})
endfunction()
