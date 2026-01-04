# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Eden Libretro Core

add_library(eden_libretro SHARED
    libretro_core.cpp
    emu_window_libretro.cpp
    emu_window_libretro.h
    libretro.h
    libretro_vfs.h
)

# Create eden_libretro alias target
add_library(Eden::LibretroCore ALIAS eden_libretro)

# Set output name based on platform
if(WIN32)
    set_target_properties(eden_libretro PROPERTIES
        OUTPUT_NAME "eden_libretro"
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(eden_libretro PROPERTIES
        OUTPUT_NAME "eden_libretro"
        PREFIX ""
        SUFFIX ".dylib"
    )
else()
    set_target_properties(eden_libretro PROPERTIES
        OUTPUT_NAME "eden_libretro"
        PREFIX ""
        SUFFIX ".so"
    )
endif()

# Include directories
target_include_directories(eden_libretro PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against core libraries
target_link_libraries(eden_libretro PRIVATE
    common
    core
    video_core
    audio_core
    hid_core
    input_common
    frontend_common
    network
    Boost::headers
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(eden_libretro PRIVATE
        ws2_32
        winmm
        iphlpapi
    )
    
    # Disable console window on Windows
    if(MSVC)
        set_target_properties(eden_libretro PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(eden_libretro PRIVATE
        pthread
        dl
    )
endif()

# OpenGL support
find_package(OpenGL REQUIRED)
target_link_libraries(eden_libretro PRIVATE OpenGL::GL)

# GLAD for OpenGL loading
if(TARGET glad)
    target_link_libraries(eden_libretro PRIVATE glad)
endif()

# Enable position-independent code for shared library
set_target_properties(eden_libretro PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Export symbols for libretro API
# Note: RETRO_CALLCONV and RETRO_API are defined in libretro.h based on platform
# We don't need to override them here as the header handles it correctly

# Compiler warnings
if(MSVC)
    target_compile_options(eden_libretro PRIVATE
        /W4
        /wd4100  # unreferenced formal parameter
        /wd4244  # conversion warnings
    )
else()
    target_compile_options(eden_libretro PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# Install rules
install(TARGETS eden_libretro
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Copy info file for RetroArch
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/eden_libretro.info.in
    ${CMAKE_BINARY_DIR}/eden_libretro.info
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/eden_libretro.info
    DESTINATION share/libretro/info
)
