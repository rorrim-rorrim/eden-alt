// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string_view>
#include "common/common_types.h"

namespace Common {

/// x86/x64 CPU capabilities that may be detected by this module
struct CPUCaps {
    char brand_string[13];

    char cpu_string[48];

    u32 base_frequency;
    u32 max_frequency;
    u32 bus_frequency;

    u32 tsc_crystal_ratio_denominator;
    u32 tsc_crystal_ratio_numerator;
    u32 crystal_frequency;
    u64 tsc_frequency; // Derived from the above three values

#define CPU_CAPS_LIST \
    CPU_CAPS_ELEM(sse) \
    CPU_CAPS_ELEM(sse2) \
    CPU_CAPS_ELEM(sse3) \
    CPU_CAPS_ELEM(ssse3) \
    CPU_CAPS_ELEM(sse4_1) \
    CPU_CAPS_ELEM(sse4_2) \
    CPU_CAPS_ELEM(avx) \
    CPU_CAPS_ELEM(avx_vnni) \
    CPU_CAPS_ELEM(avx2) \
    CPU_CAPS_ELEM(avx512f) \
    CPU_CAPS_ELEM(avx512dq) \
    CPU_CAPS_ELEM(avx512cd) \
    CPU_CAPS_ELEM(avx512bw) \
    CPU_CAPS_ELEM(avx512vl) \
    CPU_CAPS_ELEM(avx512vbmi) \
    CPU_CAPS_ELEM(avx512vbmi2) \
    CPU_CAPS_ELEM(avx512vnni) \
    CPU_CAPS_ELEM(avx512bitalg) \
    CPU_CAPS_ELEM(avx512popcntq) \
    CPU_CAPS_ELEM(avx512bf16) \
    CPU_CAPS_ELEM(aes) \
    CPU_CAPS_ELEM(bmi1) \
    CPU_CAPS_ELEM(bmi2) \
    CPU_CAPS_ELEM(f16c) \
    CPU_CAPS_ELEM(fma) \
    CPU_CAPS_ELEM(fma4) \
    CPU_CAPS_ELEM(gfni) \
    CPU_CAPS_ELEM(invariant_tsc) \
    CPU_CAPS_ELEM(lzcnt) \
    CPU_CAPS_ELEM(monitorx) \
    CPU_CAPS_ELEM(movbe) \
    CPU_CAPS_ELEM(pclmulqdq) \
    CPU_CAPS_ELEM(popcnt) \
    CPU_CAPS_ELEM(sha) \
    CPU_CAPS_ELEM(waitpkg)
#define CPU_CAPS_ELEM(n) bool n : 1;
    CPU_CAPS_LIST
#undef CPU_CAPS_ELEM
};

/// Gets the supported capabilities of the host CPU
/// @return Reference to a CPUCaps struct with the detected host CPU capabilities
const CPUCaps& GetCPUCaps();

/// Detects CPU core count
std::optional<int> GetProcessorCount();

} // namespace Common
