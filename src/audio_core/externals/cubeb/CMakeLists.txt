# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

include(CPM)
set(CPM_USE_LOCAL_PACKAGES ON)
CPMAddPackage(
    URI "gh:mozilla/cubeb#fa02160712"
    FIND_PACKAGE_ARGUMENTS "CONFIG" # not sure this works outside of gentoo
)

if (cubeb_ADDED)
    set(BUILD_TESTS OFF)
    set(BUILD_TOOLS OFF)

    if (NOT MSVC)
        if (TARGET speex)
            target_compile_options(speex PRIVATE -Wno-sign-compare)
        endif()
        target_compile_options(cubeb PRIVATE
            -Wno-implicit-const-int-float-conversion
            -Wno-shadow
            -Wno-missing-declarations -Wno-return-type
        )
    else()
        target_compile_options(cubeb PRIVATE
            /wd4456
            /wd4458
        )
    endif()
endif()
