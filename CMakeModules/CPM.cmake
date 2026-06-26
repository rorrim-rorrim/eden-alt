# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# CPM.cmake - CMake's missing package manager
# ===========================================
# See https://github.com/cpm-cmake/CPM.cmake for usage and update instructions.
#
# MIT License
# -----------
#[[
  Copyright (c) 2019-2023 Lars Melchior and contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
]]

cmake_minimum_required(VERSION 3.31 FATAL_ERROR)

# Initialize logging prefix
if(NOT CPM_INDENT)
  set(CPM_INDENT "CPM:" CACHE INTERNAL "")
endif()

get_property(CPM_INITIALIZED GLOBAL PROPERTY CPM_INITIALIZED)
if(CPM_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY CPM_INITIALIZED TRUE)

macro(cpm_set_policies)
  # the policy allows us to change options without caching
  cmake_policy(SET CMP0077 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

  # the policy allows us to change set(CACHE) without caching
  cmake_policy(SET CMP0126 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

  # The policy uses the download time for timestamp,
  # instead of the timestamp in the archive. This
  # allows for proper rebuilds when a projects url changes
  cmake_policy(SET CMP0135 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

  # treat relative git repository paths as
  # being relative to the parent project's remote
  cmake_policy(SET CMP0150 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0150 NEW)
endmacro()

cpm_set_policies()

option(CPM_DONT_UPDATE_MODULE_PATH "Don't update the module path to allow using find_package"
       $ENV{CPM_DONT_UPDATE_MODULE_PATH})

set(CPM_FILE
    ${CMAKE_CURRENT_LIST_FILE}
    CACHE INTERNAL "")

set(CPM_PACKAGES "" CACHE INTERNAL "")

include(FetchContent)

# compute a hash of all patch file contents
# if there are no patches, returns none
function(cpm_compute_patch_key patches patch_key_out)
  if(NOT patches)
    set("${patch_key_out}" "none" PARENT_SCOPE)
    return()
  endif()

  set(combined "")
  foreach(PATCH ${patches})
    file(READ "${PATCH}" contents)
    string(APPEND combined "${contents}")
  endforeach()

  string(SHA512 key "${combined}")
  set("${patch_key_out}" "${key}" PARENT_SCOPE)
endfunction()

# Create a custom FindXXX.cmake module for a CPM package.
# This prevents `find_package(NAME)` from finding the system library
function(cpm_create_module_file Name)
  if(NOT CPM_DONT_UPDATE_MODULE_PATH)
    # Redirect find_package calls to the CPM package.
    # This is what FetchContent does when you set
    # OVERRIDE_FIND_PACKAGE. The CMAKE_FIND_PACKAGE_REDIRECTS_DIR works for
    # find_package in CONFIG mode, unlike the Find${Name}.cmake fallback.
    # CMAKE_FIND_PACKAGE_REDIRECTS_DIR is not defined in script mode
    # https://cmake.org/cmake/help/latest/module/FetchContent.html#fetchcontent-find-package-integration-examples
    string(TOLOWER ${Name} NameLower)
    file(WRITE ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/${NameLower}-config.cmake
      "include(\"\${CMAKE_CURRENT_LIST_DIR}/${NameLower}-extra.cmake\" OPTIONAL)\n"
      "include(\"\${CMAKE_CURRENT_LIST_DIR}/${Name}Extra.cmake\" OPTIONAL)\n")

    file(WRITE
      ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/${NameLower}-config-version.cmake
      "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
      "set(PACKAGE_VERSION_EXACT TRUE)\n")
  endif()
endfunction()

# checks if a package has been added before
function(cpm_check_if_package_already_added CPM_ARGS_NAME CPM_ARGS_VERSION)
  if("${CPM_ARGS_NAME}" IN_LIST CPM_PACKAGES)
    CPMGetPackageVersion(${CPM_ARGS_NAME} CPM_PACKAGE_VERSION)
    if("${CPM_PACKAGE_VERSION}" VERSION_LESS "${CPM_ARGS_VERSION}")
      message(WARNING
        "${CPM_INDENT} Requires a newer version of ${CPM_ARGS_NAME} (${CPM_ARGS_VERSION}) than currently included (${CPM_PACKAGE_VERSION}).")
    endif()
    cpm_get_fetch_properties(${CPM_ARGS_NAME})
    set(${CPM_ARGS_NAME}_ADDED NO)
    set(CPM_PACKAGE_ALREADY_ADDED YES PARENT_SCOPE)
    cpm_export_variables(${CPM_ARGS_NAME})
  else()
    set(CPM_PACKAGE_ALREADY_ADDED NO PARENT_SCOPE)
  endif()
endfunction()

# Generate a PATCH_COMMAND for ExternalProject_Add() from a list of patch files. Each file is
# applied sequentially. Returns the command via a `PATCH_COMMAND` variable in the parent scope.
function(cpm_add_patches)
  # Return if no patch files are supplied.
  if(NOT ARGN)
    return()
  endif()

  # Find the patch program.
  find_program(PATCH_EXECUTABLE patch)
  if(CMAKE_HOST_WIN32 AND NOT PATCH_EXECUTABLE)
    # The Windows git executable is distributed with patch.exe.
    # Find the path to the executable, if it exists, then search
    # `../usr/bin` and `../../usr/bin` for patch.exe.
    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
      get_filename_component(extra_search_path
        ${GIT_EXECUTABLE} DIRECTORY)
      get_filename_component(extra_search_path_1up
        ${extra_search_path} DIRECTORY)
      get_filename_component(extra_search_path_2up
        ${extra_search_path_1up} DIRECTORY)
      find_program(PATCH_EXECUTABLE patch HINTS
        "${extra_search_path_1up}/usr/bin"
        "${extra_search_path_2up}/usr/bin")
    endif()
  endif()
  if(NOT PATCH_EXECUTABLE)
    message(FATAL_ERROR
      "Couldn't find `patch` executable to use with PATCHES keyword.")
  endif()

  # Ensure each file exists (or error out) and add it to the list.
  set(patch_args "")
  set(first_item True)
  foreach(PATCH_FILE ${ARGN})
    # Make sure the patch file exists, if we can't find it,
    # try again in the current directory.
    if(NOT EXISTS "${PATCH_FILE}")
      if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${PATCH_FILE}")
        message(FATAL_ERROR "Couldn't find patch file: '${PATCH_FILE}'")
      endif()
      set(PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/${PATCH_FILE}")
    endif()

    # Convert to absolute path for use with patch file command.
    get_filename_component(PATCH_FILE "${PATCH_FILE}" ABSOLUTE)

    # The first patch entry must be preceded by "PATCH_COMMAND"
    # while the following items are preceded by "COMMAND".
    if(first_item)
      set(first_item False)
      list(APPEND patch_args "PATCH_COMMAND")
    else()
      list(APPEND patch_args "COMMAND")
    endif()

    # Add the patch command to the list
    list(APPEND patch_args "${PATCH_EXECUTABLE}" "-p1" "-i" "${PATCH_FILE}")
  endforeach()

  set(PATCH_COMMAND ${patch_args} PARENT_SCOPE)
endfunction()

function(CPMAddPackage)
  set(oneValueArgs
      NAME
      FORCE
      VERSION
      DOWNLOAD_ONLY
      SOURCE_DIR
      SYSTEM
      EXCLUDE_FROM_ALL
      SOURCE_SUBDIR
      CUSTOM_CACHE_KEY
      URL_HASH)

  set(multiValueArgs URL OPTIONS PATCHES)

  cmake_parse_arguments(CPM_ARGS
    ""
    "${oneValueArgs}"
    "${multiValueArgs}"
    "${ARGN}")

  if(CPM_ARGS_DOWNLOAD_ONLY)
    set(DOWNLOAD_ONLY ${CPM_ARGS_DOWNLOAD_ONLY})
  else()
    set(DOWNLOAD_ONLY NO)
  endif()

  if(NOT DEFINED CPM_ARGS_NAME)
    message(FATAL_ERROR "${CPM_INDENT} NAME is required")
  endif()

  if(NOT DEFINED CPM_ARGS_VERSION AND NOT DEFINED CPM_ARGS_SOURCE_DIR)
    message(FATAL_ERROR "${CPM_INDENT} VERSION is required")
  endif()

  if(NOT DEFINED CPM_ARGS_CUSTOM_CACHE_KEY AND NOT DEFINED CPM_ARGS_SOURCE_DIR)
    message(FATAL_ERROR "${CPM_INDENT} CUSTOM_CACHE_KEY is required")
  endif()

  cpm_check_if_package_already_added(${CPM_ARGS_NAME} "${CPM_ARGS_VERSION}")
  if(CPM_PACKAGE_ALREADY_ADDED)
    cpm_export_variables(${CPM_ARGS_NAME})
    return()
  endif()

  if(NOT CPM_ARGS_FORCE AND NOT "${CPM_${CPM_ARGS_NAME}_SOURCE}" STREQUAL "")
    set(PACKAGE_SOURCE ${CPM_${CPM_ARGS_NAME}_SOURCE})
    set(CPM_${CPM_ARGS_NAME}_SOURCE "")
    CPMAddPackage(
      NAME "${CPM_ARGS_NAME}"
      VERSION "${CPM_ARGS_VERSION}"
      SOURCE_DIR "${PACKAGE_SOURCE}"
      EXCLUDE_FROM_ALL "${CPM_ARGS_EXCLUDE_FROM_ALL}"
      SYSTEM "${CPM_ARGS_SYSTEM}"
      PATCHES "${CPM_ARGS_PATCHES}"
      OPTIONS "${CPM_ARGS_OPTIONS}"
      SOURCE_SUBDIR "${CPM_ARGS_SOURCE_SUBDIR}"
      DOWNLOAD_ONLY "${DOWNLOAD_ONLY}"
      FORCE True)

    cpm_export_variables(${CPM_ARGS_NAME})
    return()
  endif()

  CPMRegisterPackage("${CPM_ARGS_NAME}" "${CPM_ARGS_VERSION}")

  if(DEFINED FETCHCONTENT_BASE_DIR)
    set(CPM_FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR})
  else()
    set(CPM_FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps)
  endif()

  # Build patch command if any patches are specified
  set(fetchContentPatchArgs "")
  if(CPM_ARGS_PATCHES)
    cpm_add_patches(${CPM_ARGS_PATCHES})
    if(DEFINED PATCH_COMMAND)
      list(APPEND fetchContentPatchArgs ${PATCH_COMMAND})
    endif()
  endif()

  string(TOLOWER ${CPM_ARGS_NAME} lower_case_name)

  if(DEFINED CPM_ARGS_SOURCE_DIR)
    set(fetch_source_dir ${CPM_ARGS_SOURCE_DIR})
    if(NOT IS_ABSOLUTE ${CPM_ARGS_SOURCE_DIR})
      get_filename_component(
        fetch_source_dir ${CPM_ARGS_SOURCE_DIR}
        REALPATH BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    if(NOT EXISTS ${fetch_source_dir})
      file(REMOVE_RECURSE
        "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-subbuild")
    endif()
  else()
    set(download_directory
        ${CPM_SOURCE_CACHE}/${lower_case_name}/${CPM_ARGS_CUSTOM_CACHE_KEY})
    get_filename_component(download_directory ${download_directory} ABSOLUTE)
    set(fetch_source_dir ${download_directory})
  endif()

  # compute expected patch key
  cpm_compute_patch_key("${CPM_ARGS_PATCHES}" expected_patch_key)
  set(patch_key_file "${download_directory}/.cpm_patch_key")

  # source is already fetched and verified
  if(NOT DEFINED CPM_ARGS_SOURCE_DIR AND EXISTS ${download_directory})
    # read current patch key from stamp
    if(EXISTS "${patch_key_file}")
      file(READ "${patch_key_file}" current_patch_key)
    else()
      set(current_patch_key "")
    endif()

    # if the patch key file is missing, either patches have changed or download
    # failed; in either case refetch
    if(NOT current_patch_key STREQUAL expected_patch_key)
      message(DEBUG "${CPM_INDENT} Package ${CPM_ARGS_NAME} missing "
        "or mismatched patch key, refetching")
      file(REMOVE_RECURSE "${download_directory}")
      file(REMOVE_RECURSE
        "${CMAKE_BINARY_DIR}/CMakeFiles/fc-stamp/${lower_case_name}")
    else()
      file(LOCK ${download_directory}/../cmake.lock RELEASE)

      cpm_store_fetch_properties(
        ${CPM_ARGS_NAME} "${download_directory}"
        "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-build")

      cpm_get_fetch_properties("${CPM_ARGS_NAME}")

      if(CPM_ARGS_OPTIONS AND NOT DOWNLOAD_ONLY)
        foreach(OPTION ${CPM_ARGS_OPTIONS})
          cpm_parse_option("${OPTION}")
          set(${OPTION_KEY} "${OPTION_VALUE}")
        endforeach()
      endif()

      if(NOT "${DOWNLOAD_ONLY}")
        cpm_create_module_file(${CPM_ARGS_NAME})
      endif()

      if(NOT DOWNLOAD_ONLY)
        set(source_subdir ${download_directory})

        if(DEFINED CPM_ARGS_SOURCE_SUBDIR)
          set(source_subdir ${source_subdir}/${CPM_ARGS_SOURCE_SUBDIR})
        endif()

        if(EXISTS ${source_subdir}/CMakeLists.txt)
          set(addSubdirectoryExtraArgs "")
          if(${CPM_ARGS_EXCLUDE_FROM_ALL})
            list(APPEND addSubdirectoryExtraArgs EXCLUDE_FROM_ALL)
          endif()

          if(CPM_ARGS_SYSTEM)
            list(APPEND addSubdirectoryExtraArgs SYSTEM)
          endif()

          add_subdirectory(
            ${source_subdir}
            ${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-build
            ${addSubdirectoryExtraArgs})
        endif()
      endif()

      set(${CPM_ARGS_NAME}_ADDED YES)
      cpm_export_variables(${CPM_ARGS_NAME})
      return()
    endif()
  endif()

  set(fetchContentDeclareExtraArgs "")
  if(${CPM_ARGS_EXCLUDE_FROM_ALL})
    list(APPEND fetchContentDeclareExtraArgs EXCLUDE_FROM_ALL)
  endif()

  if(${CPM_ARGS_SYSTEM})
    list(APPEND fetchContentDeclareExtraArgs SYSTEM)
  endif()

  if(DEFINED CPM_ARGS_SOURCE_SUBDIR)
    list(APPEND fetchContentDeclareExtraArgs
      SOURCE_SUBDIR ${CPM_ARGS_SOURCE_SUBDIR})
  endif()

  if(CPM_ARGS_OPTIONS AND NOT DOWNLOAD_ONLY)
    foreach(OPTION ${CPM_ARGS_OPTIONS})
      cpm_parse_option("${OPTION}")
      set(${OPTION_KEY} "${OPTION_VALUE}")
    endforeach()
  endif()

  set(fetchContentURLArgs "")
  if(DEFINED CPM_ARGS_URL)
    list(APPEND fetchContentURLArgs URL ${CPM_ARGS_URL})
  endif()

  if(DEFINED CPM_ARGS_URL_HASH)
    list(APPEND fetchContentURLArgs URL_HASH ${CPM_ARGS_URL_HASH})
  endif()

  if(NOT DEFINED CPM_ARGS_SOURCE_DIR)
    file(REMOVE_RECURSE
      ${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-subbuild)
    file(LOCK ${download_directory}/../cmake.lock)
  endif()

  if(NOT "${DOWNLOAD_ONLY}")
    cpm_create_module_file(${CPM_ARGS_NAME})
  endif()

  FetchContent_Declare(${CPM_ARGS_NAME}
    ${fetchContentDeclareExtraArgs}
    ${fetchContentURLArgs}
    ${fetchContentPatchArgs}
    SOURCE_DIR ${fetch_source_dir}
    DOWNLOAD_NO_PROGRESS TRUE)

  if(DOWNLOAD_ONLY)
    FetchContent_Populate(${CPM_ARGS_NAME}
      ${fetchContentURLArgs}
      SOURCE_DIR "${fetch_source_dir}"
      BINARY_DIR "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-build"
      SUBBUILD_DIR "${CPM_FETCHCONTENT_BASE_DIR}/${lower_case_name}-subbuild"
      DOWNLOAD_NO_PROGRESS TRUE)
  else()
    FetchContent_MakeAvailable(${CPM_ARGS_NAME})
  endif()

  # Write patch key.
  if(NOT DEFINED CPM_ARGS_SOURCE_DIR AND EXISTS ${download_directory})
    file(WRITE "${patch_key_file}" "${expected_patch_key}")
  endif()

  if(NOT DEFINED CPM_ARGS_SOURCE_DIR)
    file(LOCK ${download_directory}/../cmake.lock RELEASE)
  endif()

  FetchContent_GetProperties(${CPM_ARGS_NAME})
  cpm_store_fetch_properties(
    ${CPM_ARGS_NAME}
    ${${lower_case_name}_SOURCE_DIR}
    ${${lower_case_name}_BINARY_DIR})

  cpm_get_fetch_properties("${CPM_ARGS_NAME}")

  set(${CPM_ARGS_NAME}_ADDED YES)
  cpm_export_variables("${CPM_ARGS_NAME}")
endfunction()

# export variables available to the caller to the
# parent scope expects ${CPM_ARGS_NAME} to be set
macro(cpm_export_variables name)
  set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
  set(${name}_BINARY_DIR "${${name}_BINARY_DIR}" PARENT_SCOPE)
  set(${name}_ADDED "${${name}_ADDED}" PARENT_SCOPE)
  set(CPM_LAST_PACKAGE_NAME "${name}" PARENT_SCOPE)
endmacro()

# registers a package that has been added to CPM
function(CPMRegisterPackage PACKAGE VERSION)
  list(APPEND CPM_PACKAGES ${PACKAGE})
  set(CPM_PACKAGES
      ${CPM_PACKAGES}
      CACHE INTERNAL ""
  )
  set("CPM_PACKAGE_${PACKAGE}_VERSION"
      ${VERSION}
      CACHE INTERNAL ""
  )
endfunction()

# retrieve the current version of the package to ${OUTPUT}
function(CPMGetPackageVersion PACKAGE OUTPUT)
  set(${OUTPUT}
      "${CPM_PACKAGE_${PACKAGE}_VERSION}"
      PARENT_SCOPE
  )
endfunction()


function(cpm_get_fetch_properties PACKAGE)
  set(${PACKAGE}_SOURCE_DIR
      "${CPM_PACKAGE_${PACKAGE}_SOURCE_DIR}"
      PARENT_SCOPE
  )
  set(${PACKAGE}_BINARY_DIR
      "${CPM_PACKAGE_${PACKAGE}_BINARY_DIR}"
      PARENT_SCOPE
  )
endfunction()

function(cpm_store_fetch_properties PACKAGE source_dir binary_dir)
  set(CPM_PACKAGE_${PACKAGE}_SOURCE_DIR
      "${source_dir}"
      CACHE INTERNAL ""
  )
  set(CPM_PACKAGE_${PACKAGE}_BINARY_DIR
      "${binary_dir}"
      CACHE INTERNAL ""
  )
endfunction()

# splits a package option
function(cpm_parse_option OPTION)
  string(REGEX MATCH "^[^ ]+" OPTION_KEY "${OPTION}")
  string(LENGTH "${OPTION}" OPTION_LENGTH)
  string(LENGTH "${OPTION_KEY}" OPTION_KEY_LENGTH)
  if(OPTION_KEY_LENGTH STREQUAL OPTION_LENGTH)
    # no value for key provided, assume user wants to set option to "ON"
    set(OPTION_VALUE "ON")
  else()
    math(EXPR OPTION_KEY_LENGTH "${OPTION_KEY_LENGTH}+1")
    string(SUBSTRING "${OPTION}" "${OPTION_KEY_LENGTH}" "-1" OPTION_VALUE)
  endif()
  set(OPTION_KEY
      "${OPTION_KEY}"
      PARENT_SCOPE
  )
  set(OPTION_VALUE
      "${OPTION_VALUE}"
      PARENT_SCOPE
  )
endfunction()
