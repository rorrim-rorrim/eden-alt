# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

## DetectArchitecture ##
#[[
Does exactly as it sounds. Detects common symbols defined for different architectures and
adds compile definitions thereof. Namely:
- arm64
- arm
- x86_64
- x86
- ia64
- mips
- ppc64
- ppc
- riscv
- wasm

This file is based off of Yuzu and Dynarmic.
TODO: add SPARC
]]
include(CheckSymbolExists)

# multiarch builds are a special case and also very difficult
# this is what I have for now, but it's not ideal

# Do note that situations where multiple architectures are defined
# should NOT be too dependent on the architecture
# otherwise, you may end up with duplicate code
if (CMAKE_OSX_ARCHITECTURES)
    set(MULTIARCH_BUILD 1)
    set(ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")

    # hope and pray the architecture names match
    foreach(ARCH IN ${CMAKE_OSX_ARCHITECTURES})
        set(ARCHITECTURE_${ARCH} 1 PARENT_SCOPE)
        add_definitions(-DARCHITECTURE_${ARCH}=1)
    endforeach()

    return()
endif()

function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

detect_architecture("__ARM64__" arm64)
detect_architecture("__aarch64__" arm64)
detect_architecture("_M_ARM64" arm64)

detect_architecture("__arm__" arm)
detect_architecture("__TARGET_ARCH_ARM" arm)
detect_architecture("_M_ARM" arm)

detect_architecture("__x86_64" x86_64)
detect_architecture("__x86_64__" x86_64)
detect_architecture("__amd64" x86_64)
detect_architecture("_M_X64" x86_64)

detect_architecture("__i386" x86)
detect_architecture("__i386__" x86)
detect_architecture("_M_IX86" x86)

detect_architecture("__ia64" ia64)
detect_architecture("__ia64__" ia64)
detect_architecture("_M_IA64" ia64)

detect_architecture("__mips" mips)
detect_architecture("__mips__" mips)
detect_architecture("_M_MRX000" mips)

detect_architecture("__ppc64__" ppc64)
detect_architecture("__powerpc64__" ppc64)

detect_architecture("__ppc__" ppc)
detect_architecture("__ppc" ppc)
detect_architecture("__powerpc__" ppc)
detect_architecture("_ARCH_COM" ppc)
detect_architecture("_ARCH_PWR" ppc)
detect_architecture("_ARCH_PPC" ppc)
detect_architecture("_M_MPPC" ppc)
detect_architecture("_M_PPC" ppc)

detect_architecture("__riscv" riscv)

detect_architecture("__EMSCRIPTEN__" wasm)
