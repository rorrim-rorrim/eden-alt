# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Determine installation parameters for OS, architecture, and compiler
function(determine_qt_parameters target host_out type_out arch_out arch_path_out host_type_out host_arch_out host_arch_path_out)
    if (WIN32)
        set(host "windows")
        set(type "desktop")

        if (NOT tool)
            if (MINGW)
                set(arch "win64_mingw")
                set(arch_path "mingw_64")
            elseif (MSVC)
                if ("arm64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2022_arm64")
                elseif ("x86_64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2022_64")
                else()
                    message(FATAL_ERROR "Unsupported bundled Qt architecture. Disable YUZU_USE_BUNDLED_QT and provide your own.")
                endif()
                set(arch "win64_${arch_path}")

                if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64")
                    set(host_arch_path "msvc2022_64")
                elseif (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "ARM64")
                    set(host_arch_path "msvc2022_arm64")
                endif()
                set(host_arch "win64_${host_arch_path}")
            else()
                message(FATAL_ERROR "Unsupported bundled Qt toolchain. Disable YUZU_USE_BUNDLED_QT and provide your own.")
            endif()
        endif()
    elseif (APPLE)
        set(host "mac")
        set(type "desktop")
        set(arch "clang_64")
        set(arch_path "macos")
    else()
        set(host "linux")
        set(type "desktop")
        set(arch "linux_gcc_64")
        set(arch_path "gcc_64")
    endif()

    set(${host_out} "${host}" PARENT_SCOPE)
    set(${type_out} "${type}" PARENT_SCOPE)
    set(${arch_out} "${arch}" PARENT_SCOPE)
    set(${arch_path_out} "${arch_path}" PARENT_SCOPE)
    if (DEFINED host_type)
        set(${host_type_out} "${host_type}" PARENT_SCOPE)
    else()
        set(${host_type_out} "${type}" PARENT_SCOPE)
    endif()
    if (DEFINED host_arch)
        set(${host_arch_out} "${host_arch}" PARENT_SCOPE)
    else()
        set(${host_arch_out} "${arch}" PARENT_SCOPE)
    endif()
    if (DEFINED host_arch_path)
        set(${host_arch_path_out} "${host_arch_path}" PARENT_SCOPE)
    else()
        set(${host_arch_path_out} "${arch_path}" PARENT_SCOPE)
    endif()
endfunction()

# Download Qt binaries for a specific configuration.
function(download_qt_configuration prefix_out target host type arch arch_path base_path)
    if (target MATCHES "tools_.*")
        set(tool ON)
    else()
        set(tool OFF)
    endif()

    set(install_args -c "${CURRENT_MODULE_DIR}/aqt_config.ini")
    if (tool)
        set(prefix "${base_path}/Tools")
        list(APPEND install_args install-tool --outputdir "${base_path}" "${host}" desktop "${target}")
    else()
        set(prefix "${base_path}/${target}/${arch_path}")
        list(APPEND install_args install-qt --outputdir "${base_path}" "${host}" "${type}" "${target}" "${arch}" -m qt_base)

        if (YUZU_USE_QT_MULTIMEDIA)
            list(APPEND install_args qtmultimedia)
        endif()

        if (YUZU_USE_QT_WEB_ENGINE)
            list(APPEND install_args qtpositioning qtwebchannel qtwebengine)
        endif()

        if (NOT "${YUZU_QT_MIRROR}" STREQUAL "")
            message(STATUS "Using Qt mirror ${YUZU_QT_MIRROR}")
            list(APPEND install_args -b "${YUZU_QT_MIRROR}")
        endif()
    endif()

    message(STATUS "Install Args: ${install_args}")

    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading Qt binaries for ${target}:${host}:${type}:${arch}:${arch_path}")
        set(AQT_PREBUILD_BASE_URL "https://github.com/miurahr/aqtinstall/releases/download/v3.3.0")
        if (WIN32)
            set(aqt_path "${base_path}/aqt.exe")
            if (NOT EXISTS "${aqt_path}")
                file(DOWNLOAD "${AQT_PREBUILD_BASE_URL}/aqt.exe" "${aqt_path}" SHOW_PROGRESS)
            endif()
            execute_process(COMMAND "${aqt_path}" ${install_args}
                WORKING_DIRECTORY "${base_path}"
                RESULT_VARIABLE aqt_res
                OUTPUT_VARIABLE aqt_out
                ERROR_VARIABLE aqt_err)
            if (NOT aqt_res EQUAL 0)
                message(FATAL_ERROR "aqt.exe failed: ${aqt_err}")
            endif()
        elseif (APPLE)
            set(aqt_path "${base_path}/aqt-macos")
            if (NOT EXISTS "${aqt_path}")
                file(DOWNLOAD "${AQT_PREBUILD_BASE_URL}/aqt-macos" "${aqt_path}" SHOW_PROGRESS)
            endif()
            execute_process(COMMAND chmod +x "${aqt_path}")
            execute_process(COMMAND "${aqt_path}" ${install_args}
                WORKING_DIRECTORY "${base_path}"
                RESULT_VARIABLE aqt_res
                ERROR_VARIABLE aqt_err)
            if (NOT aqt_res EQUAL 0)
                message(FATAL_ERROR "aqt-macos failed: ${aqt_err}")
            endif()
        else()
            find_program(PYTHON3_EXECUTABLE python3)
            if (NOT PYTHON3_EXECUTABLE)
                message(FATAL_ERROR "python3 is required to install Qt using aqt (pip mode).")
            endif()
            set(aqt_install_path "${base_path}/aqt")
            file(MAKE_DIRECTORY "${aqt_install_path}")

            execute_process(COMMAND "${PYTHON3_EXECUTABLE}" -m pip install --target="${aqt_install_path}" aqtinstall
                WORKING_DIRECTORY "${base_path}"
                RESULT_VARIABLE pip_res
                ERROR_VARIABLE pip_err)
            if (NOT pip_res EQUAL 0)
                message(FATAL_ERROR "pip install aqtinstall failed: ${pip_err}")
            endif()

            execute_process(COMMAND "${CMAKE_COMMAND}" -E env PYTHONPATH="${aqt_install_path}" "${PYTHON3_EXECUTABLE}" -m aqt ${install_args}
                WORKING_DIRECTORY "${base_path}"
                RESULT_VARIABLE aqt_res
                ERROR_VARIABLE aqt_err)
            if (NOT aqt_res EQUAL 0)
                message(FATAL_ERROR "aqt (python) failed: ${aqt_err}")
            endif()
        endif()

        message(STATUS "Downloaded Qt binaries for ${target}:${host}:${type}:${arch}:${arch_path} to ${prefix}")
    endif()

    set(${prefix_out} "${prefix}" PARENT_SCOPE)
endfunction()

# This function downloads Qt using aqt.
# The path of the downloaded content will be added to the CMAKE_PREFIX_PATH.
# QT_TARGET_PATH is set to the Qt for the compile target platform.
# QT_HOST_PATH is set to a host-compatible Qt, for running tools.
# Params:
#   target: Qt dependency to install. Specify a version number to download Qt, or "tools_(name)" for a specific build tool.
function(download_qt target)
    determine_qt_parameters("${target}" host type arch arch_path host_type host_arch host_arch_path)

    set(base_path "${CMAKE_BINARY_DIR}/externals/qt")
    file(MAKE_DIRECTORY "${base_path}")

    download_qt_configuration(prefix "${target}" "${host}" "${type}" "${arch}" "${arch_path}" "${base_path}")
    if (DEFINED host_arch_path AND NOT "${host_arch_path}" STREQUAL "${arch_path}")
        download_qt_configuration(host_prefix "${target}" "${host}" "${host_type}" "${host_arch}" "${host_arch_path}" "${base_path}")
    else()
        set(host_prefix "${prefix}")
    endif()

    set(QT_TARGET_PATH "${prefix}" CACHE STRING "")
    set(QT_HOST_PATH "${host_prefix}" CACHE STRING "")

    list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
endfunction()
