// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2021 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <memory>
#include <mutex>

#include <boost/icl/interval_set.hpp>
#include "dynarmic/common/assert.h"
#include <mcl/scope_exit.hpp>
#include "dynarmic/common/common_types.h"

#include "dynarmic/backend/arm64/a32_address_space.h"
#include "dynarmic/backend/arm64/a32_core.h"
#include "dynarmic/backend/arm64/a32_jitstate.h"
#include "dynarmic/common/atomic.h"
#include "dynarmic/interface/A32/a32.h"

namespace Dynarmic::A32 {

using namespace Backend::Arm64;

struct Jit::Impl final {
    Impl(Jit* jit_interface, A32::UserConfig conf)
            : jit_interface(jit_interface)
            , conf(conf)
            , current_address_space(conf)
            , core(conf) {}

    HaltReason Run() {
        ASSERT(!jit_interface->is_executing);
        PerformRequestedCacheInvalidation(static_cast<HaltReason>(Atomic::Load(&halt_reason)));

        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Run(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation(hr);

        return hr;
    }

    HaltReason Step() {
        ASSERT(!jit_interface->is_executing);
        PerformRequestedCacheInvalidation(static_cast<HaltReason>(Atomic::Load(&halt_reason)));

        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Step(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation(hr);

        return hr;
    }

    void ClearCache() {
        invalidate_entire_cache = true;
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(std::uint32_t start_address, std::size_t length) {
        invalid_cache_ranges.add(boost::icl::discrete_interval<u32>::closed(start_address, static_cast<u32>(start_address + length - 1)));
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        current_state = {};
    }

    void HaltExecution(HaltReason hr) {
        Atomic::Or(&halt_reason, static_cast<u32>(hr));
        Atomic::Barrier();
    }

    void ClearHalt(HaltReason hr) {
        Atomic::And(&halt_reason, ~static_cast<u32>(hr));
        Atomic::Barrier();
    }

    std::array<std::uint32_t, 16>& Regs() {
        return current_state.regs;
    }

    const std::array<std::uint32_t, 16>& Regs() const {
        return current_state.regs;
    }

    std::array<std::uint32_t, 64>& ExtRegs() {
        return current_state.ext_regs;
    }

    const std::array<std::uint32_t, 64>& ExtRegs() const {
        return current_state.ext_regs;
    }

    std::uint32_t Cpsr() const {
        return current_state.Cpsr();
    }

    void SetCpsr(std::uint32_t value) {
        current_state.SetCpsr(value);
    }

    std::uint32_t Fpscr() const {
        return current_state.Fpscr();
    }

    void SetFpscr(std::uint32_t value) {
        current_state.SetFpscr(value);
    }

    void ClearExclusiveState() {
        current_state.exclusive_state = false;
    }

    std::string Disassemble() const {
        return {};
    }

private:
    void PerformRequestedCacheInvalidation(HaltReason hr) {
        if (Has(hr, HaltReason::CacheInvalidation)) {
            ClearHalt(HaltReason::CacheInvalidation);

            if (invalidate_entire_cache) {
                current_address_space.ClearCache();

                invalidate_entire_cache = false;
                invalid_cache_ranges.clear();
                return;
            }

            if (!invalid_cache_ranges.empty()) {
                current_address_space.InvalidateCacheRanges(invalid_cache_ranges);

                invalid_cache_ranges.clear();
                return;
            }
        }
    }

    Jit* jit_interface;
    A32::UserConfig conf;
    A32JitState current_state{};
    A32AddressSpace current_address_space;
    A32Core core;

    volatile u32 halt_reason = 0;
    boost::icl::interval_set<u32> invalid_cache_ranges;
    bool invalidate_entire_cache = false;
};
static_assert(sizeof(Jit::Impl) <= sizeof(Jit::impl_storage));

Jit::Jit(UserConfig conf) {
    new (&impl_storage[0]) Jit::Impl(this, conf);
}
Jit::~Jit() {
    reinterpret_cast<Jit::Impl*>(&impl_storage[0])->~Impl();
}
inline Jit::Impl* GetImpl(Jit& jit) noexcept {
    return reinterpret_cast<Jit::Impl*>(&jit.impl_storage[0]);
}
inline Jit::Impl const* GetImpl(Jit const& jit) noexcept {
    return reinterpret_cast<Jit::Impl const*>(&jit.impl_storage[0]);
}

HaltReason Jit::Run() {
    return GetImpl(*this)->Run();
}

HaltReason Jit::Step() {
    return GetImpl(*this)->Step();
}

void Jit::ClearCache() {
    GetImpl(*this)->ClearCache();
}

void Jit::InvalidateCacheRange(std::uint32_t start_address, std::size_t length) {
    GetImpl(*this)->InvalidateCacheRange(start_address, length);
}

void Jit::Reset() {
    GetImpl(*this)->Reset();
}

void Jit::HaltExecution(HaltReason hr) {
    GetImpl(*this)->HaltExecution(hr);
}

void Jit::ClearHalt(HaltReason hr) {
    GetImpl(*this)->ClearHalt(hr);
}

std::array<std::uint32_t, 16>& Jit::Regs() {
    return GetImpl(*this)->Regs();
}

const std::array<std::uint32_t, 16>& Jit::Regs() const {
    return GetImpl(*this)->Regs();
}

std::array<std::uint32_t, 64>& Jit::ExtRegs() {
    return GetImpl(*this)->ExtRegs();
}

const std::array<std::uint32_t, 64>& Jit::ExtRegs() const {
    return GetImpl(*this)->ExtRegs();
}

std::uint32_t Jit::Cpsr() const {
    return GetImpl(*this)->Cpsr();
}

void Jit::SetCpsr(std::uint32_t value) {
    GetImpl(*this)->SetCpsr(value);
}

std::uint32_t Jit::Fpscr() const {
    return GetImpl(*this)->Fpscr();
}

void Jit::SetFpscr(std::uint32_t value) {
    GetImpl(*this)->SetFpscr(value);
}

void Jit::ClearExclusiveState() {
    GetImpl(*this)->ClearExclusiveState();
}

std::string Jit::Disassemble() const {
    return GetImpl(*this)->Disassemble();
}

}  // namespace Dynarmic::A32
