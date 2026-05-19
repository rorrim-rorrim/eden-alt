// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <thread>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "common/x64/cpu_detect.h"
#include "common/x64/cpu_wait.h"
#include "common/x64/rdtsc.h"

namespace Common::X64 {

#ifdef __clang__
__attribute__((target("waitpkg,mwaitx")))
#elif defined(__GNUC__)
#pragma GCC target("waitpkg")
#pragma GCC target("mwaitx")
#endif
void MicroSleep(u64 rem) {
    // 100,000 cycles is a reasonable amount of time to wait to save on CPU resources.
    // For reference:
    // At 1 GHz, 100K cycles is 100us
    // At 2 GHz, 100K cycles is 50us
    // At 4 GHz, 100K cycles is 25us
    auto& caps = GetCPUCaps();
    u32 cycles = caps.invariant_tsc
        ? 1'000'000U
        : rem * (caps.tsc_frequency / 1000000ULL);
    if (caps.waitpkg) {
        constexpr auto RequestC02State = 0U;
        _tpause(RequestC02State, FencedRDTSC() + cycles);
    } else if (caps.monitorx) {
        constexpr auto EnableWaitTimeFlag = 1U << 1;
        constexpr auto RequestC1State = 0U;
        // monitor_var should be aligned to a cache line.
        alignas(64) u64 monitor_var{};
        _mm_monitorx(&monitor_var, 0, 0);
        _mm_mwaitx(EnableWaitTimeFlag, RequestC1State, cycles);
    } else {
        std::this_thread::yield();
    }
}

} // namespace Common::X64
