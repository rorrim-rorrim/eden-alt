# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# TODO(crueter): Create a common CPMUtil.cmake that does this for me
include(CPM)
set(CPM_USE_LOCAL_PACKAGES ON)
CPMAddPackage(
    NAME cubeb
    URL "https://github.com/mozilla/cubeb/archive/fa02160712.zip"
    FIND_PACKAGE_ARGUMENTS "CONFIG" # not sure this works outside of gentoo
    OPTIONS
        "USE_SANITIZERS OFF"
)

if (cubeb_ADDED)
    if (NOT MSVC)
        if (TARGET speex)
            target_compile_options(speex PRIVATE -Wno-sign-compare)
        endif()

        set_target_properties(cubeb PROPERTIES COMPILE_OPTIONS "")
        target_compile_options(cubeb INTERFACE
            -Wno-implicit-const-int-float-conversion
            -Wno-shadow
            -Wno-missing-declarations
            -Wno-return-type
            -Wno-maybe-uninitialized
        )
    else()
        target_compile_options(cubeb PRIVATE
            /wd4456
            /wd4458
        )
    endif()
endif()
