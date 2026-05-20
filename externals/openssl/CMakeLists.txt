# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

if (YUZU_USE_BUNDLED_OPENSSL)
    set(BUILD_SHARED_LIBS OFF)
    AddJsonPackage(openssl-ci)
    if (OpenSSL_ADDED)
        add_compile_definitions(YUZU_BUNDLED_OPENSSL)
    endif()
    # openssl funniness
    find_package(OpenSSL 3 COMPONENTS SSL Crypto REQUIRED)
else()
    message(STATUS "Using OpenSSL from externals")
    AddJsonPackage(openssl)
    set(OpenSSL_PREFIX ${OpenSSL_SOURCE_DIR})
    set(OpenSSL_BUILD_DIR ${OpenSSL_BINARY_DIR})
    set(OpenSSL_MAKEFILE ${OpenSSL_BUILD_DIR}/Makefile)
    set(OpenSSL_BUILD_LIBRARIES "")
    list(APPEND OpenSSL_BUILD_LIBRARIES
        "${OpenSSL_BUILD_DIR}/libssl.a"
        "${OpenSSL_BUILD_DIR}/libcrypto.a")

    make_directory(${OpenSSL_BUILD_DIR})

    # The ABSOLUTE best language, hands down, better than bash by a MILE
    find_program(PERL_PROGRAM perl)
    add_custom_command(
        OUTPUT
            ${OpenSSL_MAKEFILE}
        COMMAND
            ${PERL_PROGRAM} ${OpenSSL_PREFIX}/Configure
                shared
                no-makedepend
                --release
                threads
                no-tests
			    no-docs
                enable-camellia
                enable-ec
                enable-ec2m
                enable-sm2
                enable-srp
                enable-idea
                enable-mdc2
                enable-rc5
                enable-rfc3779
                enable-asm
			    enable-quic
                enable-fips
        WORKING_DIRECTORY
            ${OpenSSL_BUILD_DIR}
    )

    # Workaround for Ubuntu 18.04's older version of make not being able to call make as a child
    # with context of the jobserver. Also helps ninja users.
    execute_process(COMMAND nproc OUTPUT_VARIABLE SYSTEM_THREADS)
    # BSD make or Solaris make don't support ffmpeg make-j8
    set(OpenSSL_MAKE_ARGS "")
    if (PLATFORM_LINUX OR ANDROID OR APPLE OR WIN32 OR PLATFORM_FREEBSD)
        set(OpenSSL_MAKE_ARGS -j${SYSTEM_THREADS})
    endif()

    add_custom_command(
        OUTPUT
            ${OpenSSL_BUILD_LIBRARIES}
        COMMAND
            gmake ${OpenSSL_MAKE_ARGS}
        WORKING_DIRECTORY
            ${OpenSSL_BUILD_DIR}
    )
    add_custom_target(openssl-configure ALL DEPENDS ${OpenSSL_MAKEFILE})
    add_custom_target(openssl-build ALL DEPENDS ${OpenSSL_BUILD_LIBRARIES} openssl-configure)

    add_library(SSL INTERFACE)
    target_include_directories(SSL INTERFACE ${OpenSSL_SOURCE_DIR}/include)
    target_link_libraries(SSL INTERFACE openssl-build)

    add_library(Crypto INTERFACE)
    target_include_directories(Crypto INTERFACE ${OpenSSL_SOURCE_DIR}/include)
    target_link_libraries(Crypto INTERFACE openssl-build)

    add_library(OpenSSL::SSL ALIAS SSL)
    add_library(OpenSSL::Crypto ALIAS Crypto)

    # evil haxx for httplib https://cmake.org/cmake/help/latest/module/FindOpenSSL.html
    # fuck httplib
    set(OpenSSL_FOUND TRUE PARENT_SCOPE)
    set(OpenSSL_VERSION "4.0.0" PARENT_SCOPE)
    # Deprecated but some people run older cmake
    set(OPENSSL_FOUND TRUE PARENT_SCOPE)
    set(OPENSSL_VERSION "4.0.0" PARENT_SCOPE)

    set(OPENSSL_INCLUDE_DIR ${OpenSSL_SOURCE_DIR}/include PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARY ${OpenSSL_BUILD_DIR}/libcrypto.a PARENT_SCOPE)
    set(OPENSSL_CRYPTO_LIBRARIES ${OpenSSL_BUILD_DIR}/libcrypto.a PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARY ${OpenSSL_BUILD_DIR}/libssl.a PARENT_SCOPE)
    set(OPENSSL_SSL_LIBRARIES ${OpenSSL_BUILD_DIR}/libssl.a PARENT_SCOPE)
    set(OPENSSL_LIBRARIES "${OpenSSL_BUILD_DIR}/libssl.a;${OpenSSL_BUILD_DIR}/libcrypto.a" PARENT_SCOPE)

    set(OPENSSL_ROOT_DIR ${OpenSSL_BUILD_DIR} PARENT_SCOPE)
    set(OPENSSL_USE_STATIC_LIBS TRUE PARENT_SCOPE)
endif()
