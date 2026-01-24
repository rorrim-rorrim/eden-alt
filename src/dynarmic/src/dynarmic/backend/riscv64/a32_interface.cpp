// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2024 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <memory>
#include <mutex>

#include <boost/icl/interval_set.hpp>
#include "dynarmic/common/assert.h"
#include <mcl/scope_exit.hpp>
#include "dynarmic/common/common_types.h"

#include "dynarmic/backend/riscv64/a32_address_space.h"
#include "dynarmic/backend/riscv64/a32_core.h"
#include "dynarmic/backend/riscv64/a32_jitstate.h"
#include "dynarmic/common/atomic.h"
#include "dynarmic/interface/A32/a32.h"

namespace Dynarmic::A32 {

using namespace Backend::RV64;

struct Jit::Impl final {
    Impl(Jit* jit_interface, A32::UserConfig conf)
            : jit_interface(jit_interface)
            , conf(conf)
            , current_address_space(conf)
            , core(conf) {}

    HaltReason Run() {
        ASSERT(!jit_interface->is_executing);
        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        HaltReason hr = core.Run(current_address_space, current_state, &halt_reason);

        RequestCacheInvalidation();

        return hr;
    }

    HaltReason Step() {
        ASSERT(!jit_interface->is_executing);
        jit_interface->is_executing = true;
        SCOPE_EXIT {
            jit_interface->is_executing = false;
        };

        UNIMPLEMENTED();

        RequestCacheInvalidation();

        return HaltReason{};
    }

    void ClearCache() {
        invalidate_entire_cache = true;
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(u32 start_address, size_t length) {
        invalid_cache_ranges.add(boost::icl::discrete_interval<u32>::closed(start_address, static_cast<u32>(start_address + length - 1)));
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        current_state = {};
    }

    void HaltExecution(HaltReason hr) {
        Atomic::Or(&halt_reason, ~static_cast<u32>(hr));
    }

    void ClearHalt(HaltReason hr) {
        Atomic::And(&halt_reason, ~static_cast<u32>(hr));
    }

    std::array<u32, 16>& Regs() {
        return current_state.regs;
    }

    const std::array<u32, 16>& Regs() const {
        return current_state.regs;
    }

    std::array<u32, 64>& ExtRegs() {
        return current_state.ext_regs;
    }

    const std::array<u32, 64>& ExtRegs() const {
        return current_state.ext_regs;
    }

    u32 Cpsr() const {
        return current_state.Cpsr();
    }

    void SetCpsr(u32 value) {
        current_state.SetCpsr(value);
    }

    u32 Fpscr() const {
        return current_state.Fpscr();
    }

    void SetFpscr(u32 value) {
        current_state.SetFpscr(value);
    }

    void ClearExclusiveState() {
        current_state.exclusive_state = false;
    }

private:
    void RequestCacheInvalidation() {
        // UNREACHABLE();

        invalidate_entire_cache = false;
        invalid_cache_ranges.clear();
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

void Jit::InvalidateCacheRange(u32 start_address, std::size_t length) {
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

std::array<u32, 16>& Jit::Regs() {
    return GetImpl(*this)->Regs();
}

const std::array<u32, 16>& Jit::Regs() const {
    return GetImpl(*this)->Regs();
}

std::array<u32, 64>& Jit::ExtRegs() {
    return GetImpl(*this)->ExtRegs();
}

const std::array<u32, 64>& Jit::ExtRegs() const {
    return GetImpl(*this)->ExtRegs();
}

u32 Jit::Cpsr() const {
    return GetImpl(*this)->Cpsr();
}

void Jit::SetCpsr(u32 value) {
    GetImpl(*this)->SetCpsr(value);
}

u32 Jit::Fpscr() const {
    return GetImpl(*this)->Fpscr();
}

void Jit::SetFpscr(u32 value) {
    GetImpl(*this)->SetFpscr(value);
}

void Jit::ClearExclusiveState() {
    GetImpl(*this)->ClearExclusiveState();
}

}  // namespace Dynarmic::A32
