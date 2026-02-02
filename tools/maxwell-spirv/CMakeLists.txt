# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(maxwell-spirv
    main.cpp
    recompiler_impl.cpp
    reference_impl.cpp
)
target_link_libraries(maxwell-spirv PRIVATE common shader_recompiler Threads::Threads)
target_include_directories(maxwell-spirv PRIVATE ${CMAKE_SOURCE_DIR}/src)
if(UNIX AND NOT APPLE)
    install(TARGETS maxwell-spirv)
endif()
create_target_directory_groups(maxwell-spirv)
