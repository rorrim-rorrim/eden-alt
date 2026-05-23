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
void MicroSleep(const CPUCaps& caps, u64 cycles) {
    do {
        u64 start = FencedRDTSC();
        if (caps.waitpkg) {
            constexpr auto RequestC02State = 0U;
            _tpause(RequestC02State, start + cycles);
        } else if (caps.monitorx) {
            constexpr auto EnableWaitTimeFlag = 1U << 1;
            constexpr auto RequestC1State = 0U;
            // monitor_var should be aligned to a cache line.
            alignas(64) static const u64 monitor_var{};
            _mm_monitorx(const_cast<u64*>(std::addressof(monitor_var)), 0, 0);
            _mm_mwaitx(EnableWaitTimeFlag, RequestC1State, cycles);
        } else {
            std::this_thread::yield();
        }
        cycles -= FencedRDTSC() - start;
    } while (cycles > 0);
}

} // namespace Common::X64
