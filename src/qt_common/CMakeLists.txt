# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(qt_common STATIC
    qt_common.h
    qt_common.cpp

    uisettings.cpp
    uisettings.h

    qt_config.cpp
    qt_config.h

    shared_translation.cpp
    shared_translation.h
    qt_path_util.h qt_path_util.cpp
    qt_game_util.h qt_game_util.cpp
)

create_target_directory_groups(qt_common)
target_link_libraries(qt_common PUBLIC core Qt6::Core Qt6::Gui SimpleIni::SimpleIni QuaZip::QuaZip)

if (NOT WIN32)
    target_include_directories(qt_common PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
endif()
