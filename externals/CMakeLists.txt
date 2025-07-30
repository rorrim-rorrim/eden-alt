# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2016 Citra Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# cpm
include(CPM)

# Dynarmic has cmake_minimum_required(3.12) and we may want to override
# some of its variables, which is only possible in 3.13+
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Disable tests/tools in all externals supporting the standard option name
set(BUILD_TESTING OFF)

# Build only static externals
set(BUILD_SHARED_LIBS OFF)

# Skip install rules for all externals
set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

# Xbyak (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    set(CPM_USE_LOCAL_PACKAGES OFF)

    CPMAddPackage(
        NAME xbyak
        URL "https://github.com/Lizzie841/xbyak/archive/4e44f4614d.zip"
        URL_HASH SHA512=5824e92159e07fa36a774aedd3b3ef3541d0241371d522cffa4ab3e1f215fa5097b1b77865b47b2481376c704fa079875557ea463ca63d0a7fd6a8a20a589e70
    )
endif()

# Oaknut (also used by Dynarmic, so needs to be added first)
if (ARCHITECTURE_arm64 AND NOT TARGET merry::oaknut)
    add_subdirectory(oaknut)
endif()

# Dynarmic
if ((ARCHITECTURE_x86_64 OR ARCHITECTURE_arm64) AND NOT TARGET dynarmic::dynarmic)
    set(DYNARMIC_IGNORE_ASSERTS ON)
    add_subdirectory(dynarmic)
    add_library(dynarmic::dynarmic ALIAS dynarmic)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# mbedtls
# TODO(crueter): test local mbedtls
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME mbedtls
    URL "https://github.com/Mbed-TLS/mbedtls/archive/8c88150ca1.zip"
    URL_HASH SHA512=769ad1e94c570671071e1f2a5c0f1027e0bf6bcdd1a80ea8ac970f2c86bc45ce4e31aa88d6d8110fc1bed1de81c48bc624df1b38a26f8b340a44e109d784a966
    PATCHES
        ${CMAKE_SOURCE_DIR}/.patch/mbedtls/0001-cmake-version.patch
)

if (mbedtls_ADDED)
    target_include_directories(mbedtls PUBLIC ${mbedtls_SOURCE_DIR}/include)

    if (NOT MSVC)
        target_compile_options(mbedcrypto PRIVATE
            -Wno-unused-but-set-variable
            -Wno-string-concatenation)
    endif()
endif()

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)
if (ENABLE_MICROPROFILE)
    target_compile_definitions(microprofile INTERFACE MICROPROFILE_ENABLED=1)
else()
    target_compile_definitions(microprofile INTERFACE MICROPROFILE_ENABLED=0)
endif()

# GCC bugs
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND MINGW)
    target_compile_options(microprofile INTERFACE "-Wno-array-bounds")
endif()

# libusb
if (ENABLE_LIBUSB AND NOT TARGET libusb::usb)
    add_subdirectory(libusb)
endif()

# SDL2
if (YUZU_USE_EXTERNAL_SDL2)
    if (NOT WIN32)
        # Yuzu itself needs: Atomic Audio Events Joystick Haptic Sensor Threads Timers
        # Since 2.0.18 Atomic+Threads required for HIDAPI/libusb (see https://github.com/libsdl-org/SDL/issues/5095)
        # Yuzu-cmd also needs: Video (depends on Loadso/Dlopen)
        # CPUinfo also required for SDL Audio, at least until 2.28.0 (see https://github.com/libsdl-org/SDL/issues/7809)
        set(SDL_UNUSED_SUBSYSTEMS
            File Filesystem
            Locale Power Render)
        foreach(_SUB ${SDL_UNUSED_SUBSYSTEMS})
          string(TOUPPER ${_SUB} _OPT)
          set(SDL_${_OPT} OFF)
        endforeach()

        set(HIDAPI ON)
    endif()
    if (APPLE)
        set(SDL_FILE ON)
    endif()

    set(CPM_USE_LOCAL_PACKAGES OFF)

    if ("${YUZU_SYSTEM_PROFILE}" STREQUAL "steamdeck")
        set(SDL_HASH cc016b0046)
        set(SDL_PIPEWIRE OFF) # build errors out with this on
        set(SDL_SHA512SUM 34d5ef58da6a4f9efa6689c82f67badcbd741f5a4f562a9c2c30828fa839830fb07681c5dc6a7851520e261c8405a416ac0a2c2513b51984fb3b4fa4dcb3e20b)
    else()
        set(SDL_HASH 2e4c12cd2c)
        set(SDL_SHA512SUM d95af47f469a312876f8ab361074a1e7b8083db19935a102d9c6e5887ace6008e64475a8c54b00164b40cad86492bb1b2366084efdd0b2555e5fea6d9c5da80e)
    endif()

    CPMAddPackage(
        NAME SDL2
        URL "https://github.com/libsdl-org/SDL/archive/${SDL_HASH}.zip"
        URL_HASH SHA512=${SDL_SHA512SUM}
    )
endif()

# ENet
if (NOT TARGET enet::enet)
    add_subdirectory(enet)
    target_include_directories(enet INTERFACE ./enet/include)
    add_library(enet::enet ALIAS enet)
endif()

# TODO(crueter): Create a common CPMUtil.cmake that does this for me
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME cubeb
    URL "https://github.com/mozilla/cubeb/archive/fa02160712.zip"
    FIND_PACKAGE_ARGUMENTS "CONFIG" # not sure this works outside of gentoo
    OPTIONS
        "USE_SANITIZERS OFF"
        "BUILD_TESTS OFF"
        "BUILD_TOOLS OFF"
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

# DiscordRPC
if (USE_DISCORD_PRESENCE AND NOT TARGET DiscordRPC::discord-rpc)
    set(BUILD_EXAMPLES OFF)
    add_subdirectory(discord-rpc)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
    add_library(DiscordRPC::discord-rpc ALIAS discord-rpc)
endif()

# Sirit
# TODO(crueter): spirv-tools doesn't work w/ system
set(CPM_USE_LOCAL_PACKAGES OFF)

CPMAddPackage(
    NAME SPIRV-Headers
    URL "https://github.com/KhronosGroup/SPIRV-Headers/archive/4e209d3d7e.zip"
    URL_HASH SHA512=f48bbe18341ed55ea0fe280dbbbc0a44bf222278de6e716e143ca1e95ca320b06d4d23d6583fbf8d03e1428f3dac8fa00e5b82ddcd6b425e6236d85af09550a4
)

set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage(
    NAME sirit
    URL "https://github.com/raphaelthegreat/sirit/archive/51fcf9720f.zip"
    URL_HASH SHA512=a8f98ea0c51763b89924d836ad482ebdfe9130251cf4e14733ccaacc885ae8cc4c8b03d1dc43e8861609e5f7929c16f935879c1f6bf61866fd75077954774394
    OPTIONS
        "SIRIT_USE_SYSTEM_SPIRV_HEADERS ON"
)

# httplib
if ((ENABLE_WEB_SERVICE OR ENABLE_QT_UPDATE_CHECKER))
    set(CPM_USE_LOCAL_PACKAGES ON)

    # TODO(crueter): fix local package (gentoo?)
    CPMAddPackage(
        NAME httplib
        VERSION 0.12
        URL "https://github.com/yhirose/cpp-httplib/archive/a609330e4c.zip"
        URL_HASH SHA512=dd3fd0572f8367d8549e1319fd98368b3e75801a293b0c3ac9b4adb806473a4506a484b3d389dc5bee5acc460cb90af7a20e5df705a1696b56496b30b9ce7ed2
        FIND_PACKAGE_ARGUMENTS "MODULE"
        OPTIONS
            "HTTPLIB_REQUIRE_OPENSSL ON"
    )
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE)
    set(CPM_USE_LOCAL_PACKAGES ON)

    CPMAddPackage(
        NAME cpp-jwt
        VERSION 1.4
        URL "https://github.com/arun11299/cpp-jwt/archive/10ef5735d8.zip"
        URL_HASH SHA512=ebba3d26b33a3b0aa909f475e099594560edbce10ecd03e76d7fea68549a28713ea606d363808f88a5495b62c54c3cdb7e47aee2d946eceabd36e310479dadb7
        FIND_PACKAGE_ARGUMENTS "CONFIG"
        OPTIONS
            "CPP_JWT_BUILD_EXAMPLES OFF"
            "CPP_JWT_BUILD_TESTS OFF"
            "CPP_JWT_USE_VENDORED_NLOHMANN_JSON OFF"
    )
endif()

# Opus
set(CPM_USE_LOCAL_PACKAGES ON)
CPMAddPackage(
    NAME Opus
    VERSION 1.3
    URL "https://github.com/xiph/opus/archive/5ded705cf4.zip"
    URL_HASH SHA512=0dc89e58ddda1f3bc6a7037963994770c5806c10e66f5cc55c59286fc76d0544fe4eca7626772b888fd719f434bc8a92f792bdb350c807968b2ac14cfc04b203
    FIND_PACKAGE_ARGUMENTS "MODULE"
    OPTIONS
        "OPUS_BUILD_TESTING OFF"
        "OPUS_BUILD_PROGRAMS OFF"
        "OPUS_INSTALL_PKG_CONFIG_MODULE OFF"
        "OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF"
)

# FFMpeg
if (YUZU_USE_BUNDLED_FFMPEG)
    add_subdirectory(ffmpeg)
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
endif()

# Vulkan-Headers
if (YUZU_USE_EXTERNAL_VULKAN_HEADERS)
    set(CPM_USE_LOCAL_PACKAGES OFF)
else()
    set(CPM_USE_LOCAL_PACKAGES ON)
endif()

# TODO(crueter): System vk-headers are too new for externals vk-util

CPMAddPackage(
    NAME VulkanHeaders
    VERSION 1.3.274
    URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/89268a6d17.zip"
    URL_HASH SHA512=3ab349f74298ba72cafb8561015690c0674d428a09fb91ccd3cd3daca83650d190d46d33fd97b0a8fd4223fe6df2bcabae89136fbbf7c0bfeb8776f9448304c8
)

# Vulkan-Utility-Libraries
if (YUZU_USE_EXTERNAL_VULKAN_UTILITY_LIBRARIES)
    set(CPM_USE_LOCAL_PACKAGES OFF)
else()
    set(CPM_USE_LOCAL_PACKAGES ON)
endif()

CPMAddPackage(
    NAME VulkanUtilityLibraries
    URL "https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/df2e358152.zip"
    URL_HASH SHA512=3e468c3d9ff93f6d418d71e5527abe0a12c8c7ab5b0b52278bbbee4d02bb87e99073906729b727e0147242b7e3fd5dedf68b803f1878cb4c0e4f730bc2238d79
)

# SPIRV-Tools
if (YUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS)
    CPMAddPackage(
        NAME SPIRV-Tools
        URL "https://github.com/KhronosGroup/SPIRV-Tools/archive/40eb301f32.zip"
        URL_HASH SHA512=58d0fb1047d69373cf24c73e6f78c73a72a6cca3b4df1d9f083b9dcc0962745ef154abf3dbe9b3623b835be20c6ec769431cf11733349f45e7568b3525f707aa
        OPTIONS
            "SPIRV_SKIP_EXECUTABLES ON"
    )
endif()

# Boost headers
add_subdirectory(boost-headers)

# TZDB (Time Zone Database)
add_subdirectory(nx_tzdb)

# VMA
if (NOT TARGET GPUOpen::VulkanMemoryAllocator)
    add_subdirectory(VulkanMemoryAllocator)
endif()

if (NOT TARGET LLVM::Demangle)
    add_library(demangle demangle/ItaniumDemangle.cpp)
    target_include_directories(demangle PUBLIC ./demangle)
    if (NOT MSVC)
        target_compile_options(demangle PRIVATE -Wno-deprecated-declarations) # std::is_pod
    endif()
    add_library(LLVM::Demangle ALIAS demangle)
endif()

add_library(stb stb/stb_dxt.cpp)
target_include_directories(stb PUBLIC ./stb)

if (NOT TARGET stb::headers)
    add_library(stb::headers ALIAS stb)
endif()

add_library(tz tz/tz/tz.cpp)
target_include_directories(tz PUBLIC ./tz)

add_library(bc_decoder bc_decoder/bc_decoder.cpp)
target_include_directories(bc_decoder PUBLIC ./bc_decoder)

if (NOT TARGET RenderDoc::API)
    add_library(renderdoc INTERFACE)
    target_include_directories(renderdoc SYSTEM INTERFACE ./renderdoc)
    add_library(RenderDoc::API ALIAS renderdoc)
endif()

if (ANDROID)
   if (ARCHITECTURE_arm64)
       add_subdirectory(libadrenotools)
   endif()
endif()

if ((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT TARGET gamemode::headers)
    add_library(gamemode INTERFACE)
    target_include_directories(gamemode INTERFACE gamemode)
    add_library(gamemode::headers ALIAS gamemode)
endif()

# Breakpad
# TODO(crueter): Breakpad needs additional fetches
# https://github.com/google/breakpad/blob/main/DEPS
# https://github.com/microsoft/vcpkg/blob/master/ports/breakpad/CMakeLists.txt
if (YUZU_CRASH_DUMPS AND NOT TARGET libbreakpad_client)
    set(BREAKPAD_WIN32_DEFINES
        NOMINMAX
        UNICODE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )

    set(CPM_USE_LOCAL_PACKAGES OFF)
    CPMAddPackage(
        NAME breakpad
        URL "https://github.com/google/breakpad/archive/f80f288803.zip"
        URL_HASH SHA512=4a87ee88cea99bd633d52a5b00135a649f1475e3b65db325a6df9c804ab82b054ad7e62419b35f6e22cc5dfbbb569214041d7ad5d10fab10106e700bb5050e1d
        DOWNLOAD_ONLY YES
    )

    # libbreakpad
    add_library(libbreakpad STATIC)
    file(GLOB_RECURSE LIBBREAKPAD_SOURCES ${breakpad_SOURCE_DIR}/src/processor/*.cc)
    file(GLOB_RECURSE LIBDISASM_SOURCES ${breakpad_SOURCE_DIR}/src/third_party/libdisasm/*.c)
    list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "_unittest|_selftest|synth_minidump|/tests|/testdata|/solaris|microdump_stackwalk|minidump_dump|minidump_stackwalk")
    if (WIN32)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/mac|/android")
        target_compile_definitions(libbreakpad PRIVATE ${BREAKPAD_WIN32_DEFINES})
        target_include_directories(libbreakpad PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
    elseif (APPLE)
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/linux|/windows|/android")
    else()
        list(FILTER LIBBREAKPAD_SOURCES EXCLUDE REGEX "/mac|/windows|/android")
    endif()
    target_sources(libbreakpad PRIVATE ${LIBBREAKPAD_SOURCES} ${LIBDISASM_SOURCES})
    target_include_directories(libbreakpad
        PUBLIC
            ${breakpad_SOURCE_DIR}/src
            ${breakpad_SOURCE_DIR}/src/third_party/libdisasm
    )

    # libbreakpad_client
    add_library(libbreakpad_client STATIC)
    file(GLOB LIBBREAKPAD_COMMON_SOURCES ${breakpad_SOURCE_DIR}/src/common/*.cc ${breakpad_SOURCE_DIR}/src/common/*.c ${breakpad_SOURCE_DIR}/src/client/*.cc)

    if (WIN32)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/windows/*.cc ${breakpad_SOURCE_DIR}/src/common/windows/*.cc)
        list(FILTER LIBBREAKPAD_COMMON_SOURCES EXCLUDE REGEX "language.cc|path_helper.cc|stabs_to_module.cc|stabs_reader.cc|minidump_file_writer.cc")
        target_include_directories(libbreakpad_client PRIVATE "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include")
        target_compile_definitions(libbreakpad_client PRIVATE ${BREAKPAD_WIN32_DEFINES})
    elseif (APPLE)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_MACH_O_NLIST_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/mac/*.cc ${breakpad_SOURCE_DIR}/src/common/mac/*.cc)
        list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/common/mac/MachIPC.mm)
    else()
        target_compile_definitions(libbreakpad_client PUBLIC -DHAVE_A_OUT_H)
        file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES ${breakpad_SOURCE_DIR}/src/client/linux/*.cc ${breakpad_SOURCE_DIR}/src/common/linux/*.cc)
    endif()
    list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${LIBBREAKPAD_COMMON_SOURCES})
    list(FILTER LIBBREAKPAD_CLIENT_SOURCES EXCLUDE REGEX "/sender|/tests|/unittests|/testcases|_unittest|_test")
    target_sources(libbreakpad_client PRIVATE ${LIBBREAKPAD_CLIENT_SOURCES})
    target_include_directories(libbreakpad_client PRIVATE ${breakpad_SOURCE_DIR}/src)

    if (WIN32)
        target_link_libraries(libbreakpad_client PRIVATE wininet.lib)
    elseif (APPLE)
        find_library(CoreFoundation_FRAMEWORK CoreFoundation)
        target_link_libraries(libbreakpad_client PRIVATE ${CoreFoundation_FRAMEWORK})
    else()
        find_library(PTHREAD_LIBRARIES pthread)
        target_compile_definitions(libbreakpad_client PRIVATE HAVE_GETCONTEXT=1)
        if (PTHREAD_LIBRARIES)
            target_link_libraries(libbreakpad_client PRIVATE ${PTHREAD_LIBRARIES})
        endif()
    endif()

    # Host tools for symbol processing
    if (LINUX)
        find_package(ZLIB REQUIRED)

        add_executable(minidump_stackwalk ${breakpad_SOURCE_DIR}/src/processor/minidump_stackwalk.cc)
        target_link_libraries(minidump_stackwalk PRIVATE libbreakpad libbreakpad_client)

        add_executable(dump_syms
            ${breakpad_SOURCE_DIR}/src/common/dwarf_cfi_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_cu_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_line_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf_range_list_handler.cc
            ${breakpad_SOURCE_DIR}/src/common/language.cc
            ${breakpad_SOURCE_DIR}/src/common/module.cc
            ${breakpad_SOURCE_DIR}/src/common/path_helper.cc
            ${breakpad_SOURCE_DIR}/src/common/stabs_reader.cc
            ${breakpad_SOURCE_DIR}/src/common/stabs_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/bytereader.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/dwarf2diehandler.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/dwarf2reader.cc
            ${breakpad_SOURCE_DIR}/src/common/dwarf/elf_reader.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/crc32.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/dump_symbols.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/elf_symbols_to_module.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/elfutils.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/file_id.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/linux_libc_support.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/memory_mapped_file.cc
            ${breakpad_SOURCE_DIR}/src/common/linux/safe_readlink.cc
            ${breakpad_SOURCE_DIR}/src/tools/linux/dump_syms/dump_syms.cc)
        target_link_libraries(dump_syms PRIVATE libbreakpad_client ZLIB::ZLIB)
    endif()
endif()

# SimpleIni
if (NOT TARGET SimpleIni::SimpleIni)
    add_subdirectory(simpleini)
endif()

# oboe
if (ANDROID)
    set(CPM_USE_LOCAL_PACKAGES ON)
    CPMAddPackage(
        NAME oboe
        URL "https://github.com/google/oboe/archive/2bc873e53c.zip"
        URL_HASH SHA512=02329058a7f9cf7d5039afaae5ab170d9f42f60f4c01e21eaf4f46073886922b057a9ae30eeac040b3ac182f51b9c1bfe9fe1050a2c9f6ce567a1a9a0ec2c768
        OPTIONS
            "SPIRV_SKIP_EXECUTABLES ON"
    )

    add_library(oboe::oboe ALIAS oboe)
endif()

# sse2neon
if (ARCHITECTURE_arm64 AND NOT TARGET sse2neon)
    add_library(sse2neon INTERFACE)
    target_include_directories(sse2neon INTERFACE sse2neon)
endif()

# QuaZip
if (ENABLE_QT)
    set(CPM_USE_LOCAL_PACKAGES ON)

    CPMAddPackage(
        NAME QuaZip-Qt6
        VERSION 1.3
        URL "https://github.com/crueter/quazip-qt6/archive/f838774d63.zip"
    )
endif()
