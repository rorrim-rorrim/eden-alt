# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(yuzu_room_standalone
    yuzu_room_standalone.cpp
)

set_target_properties(yuzu_room_standalone PROPERTIES OUTPUT_NAME "eden-room")

target_link_libraries(yuzu_room_standalone PRIVATE yuzu-room)

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu_room_standalone RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (YUZU_STATIC_BUILD)
    # yuzu_room_standalone requires us to explicitly link libpthread, libgcc, and libstdc++ as static
    # At a guess, it's probably because Qt handles the Qt executable for us, whereas this does not
    target_link_options(yuzu_room_standalone PRIVATE -static -lpthread)
    target_link_options(yuzu_room_standalone PRIVATE -static-libgcc -static-libstdc++)
endif()

create_target_directory_groups(yuzu_room_standalone)
