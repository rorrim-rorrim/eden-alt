// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <memory>
#include <mutex>

#include <boost/icl/interval_set.hpp>
#include "dynarmic/common/assert.h"
#include <mcl/scope_exit.hpp>
#include "dynarmic/common/common_types.h"

#include "dynarmic/backend/arm64/a64_address_space.h"
#include "dynarmic/backend/arm64/a64_core.h"
#include "dynarmic/backend/arm64/a64_jitstate.h"
#include "dynarmic/common/atomic.h"
#include "dynarmic/interface/A64/a64.h"
#include "dynarmic/interface/A64/config.h"

namespace Dynarmic::A64 {

using namespace Backend::Arm64;

struct Jit::Impl final {
    Impl(Jit*, A64::UserConfig conf)
        : conf(conf)
        , current_address_space(conf)
        , core(conf) {}

    HaltReason Run() {
        ASSERT(!is_executing);
        PerformRequestedCacheInvalidation(HaltReason(Atomic::Load(&halt_reason)));

        is_executing = true;
        SCOPE_EXIT {
            is_executing = false;
        };

        HaltReason hr = core.Run(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation(hr);

        return hr;
    }

    HaltReason Step() {
        ASSERT(!is_executing);
        PerformRequestedCacheInvalidation(HaltReason(Atomic::Load(&halt_reason)));

        is_executing = true;
        SCOPE_EXIT {
            is_executing = false;
        };

        HaltReason hr = core.Step(current_address_space, current_state, &halt_reason);

        PerformRequestedCacheInvalidation(hr);

        return hr;
    }

    void ClearCache() {
        invalidate_entire_cache = true;
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void InvalidateCacheRange(std::uint64_t start_address, std::size_t length) {
        invalid_cache_ranges.add(boost::icl::discrete_interval<u64>::closed(start_address, start_address + length - 1));
        HaltExecution(HaltReason::CacheInvalidation);
    }

    void Reset() {
        current_state = {};
    }

    void HaltExecution(HaltReason hr) {
        Atomic::Or(&halt_reason, static_cast<u32>(hr));
    }

    void ClearHalt(HaltReason hr) {
        Atomic::And(&halt_reason, ~static_cast<u32>(hr));
    }

    std::uint64_t PC() const {
        return current_state.pc;
    }

    void SetPC(std::uint64_t value) {
        current_state.pc = value;
    }

    std::uint64_t SP() const {
        return current_state.sp;
    }

    void SetSP(std::uint64_t value) {
        current_state.sp = value;
    }

    std::array<std::uint64_t, 31>& Regs() {
        return current_state.reg;
    }

    const std::array<std::uint64_t, 31>& Regs() const {
        return current_state.reg;
    }

    std::array<std::uint64_t, 64>& VecRegs() {
        return current_state.vec;
    }

    const std::array<std::uint64_t, 64>& VecRegs() const {
        return current_state.vec;
    }

    std::uint32_t Fpcr() const {
        return current_state.fpcr;
    }

    void SetFpcr(std::uint32_t value) {
        current_state.fpcr = value;
    }

    std::uint32_t Fpsr() const {
        return current_state.fpsr;
    }

    void SetFpsr(std::uint32_t value) {
        current_state.fpsr = value;
    }

    std::uint32_t Pstate() const {
        return current_state.cpsr_nzcv;
    }

    void SetPstate(std::uint32_t value) {
        current_state.cpsr_nzcv = value;
    }

    void ClearExclusiveState() {
        current_state.exclusive_state = false;
    }

    bool IsExecuting() const {
        return is_executing;
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

    A64::UserConfig conf;
    A64JitState current_state{};
    A64AddressSpace current_address_space;
    A64Core core;

    volatile u32 halt_reason = 0;
    boost::icl::interval_set<u64> invalid_cache_ranges;
    bool invalidate_entire_cache = false;
    bool is_executing = false;
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

void Jit::InvalidateCacheRange(std::uint64_t start_address, std::size_t length) {
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

std::uint64_t Jit::GetSP() const {
    return GetImpl(*this)->SP();
}

void Jit::SetSP(std::uint64_t value) {
    GetImpl(*this)->SetSP(value);
}

std::uint64_t Jit::GetPC() const {
    return GetImpl(*this)->PC();
}

void Jit::SetPC(std::uint64_t value) {
    GetImpl(*this)->SetPC(value);
}

std::uint64_t Jit::GetRegister(std::size_t index) const {
    return GetImpl(*this)->Regs()[index];
}

void Jit::SetRegister(size_t index, std::uint64_t value) {
    GetImpl(*this)->Regs()[index] = value;
}

std::array<std::uint64_t, 31> Jit::GetRegisters() const {
    return GetImpl(*this)->Regs();
}

void Jit::SetRegisters(const std::array<std::uint64_t, 31>& value) {
    GetImpl(*this)->Regs() = value;
}

Vector Jit::GetVector(std::size_t index) const {
    auto& vec = GetImpl(*this)->VecRegs();
    return {vec[index * 2], vec[index * 2 + 1]};
}

void Jit::SetVector(std::size_t index, Vector value) {
    auto& vec = GetImpl(*this)->VecRegs();
    vec[index * 2] = value[0];
    vec[index * 2 + 1] = value[1];
}

std::array<Vector, 32> Jit::GetVectors() const {
    std::array<Vector, 32> ret;
    std::memcpy(ret.data(), GetImpl(*this)->VecRegs().data(), sizeof(ret));
    return ret;
}

void Jit::SetVectors(const std::array<Vector, 32>& value) {
    std::memcpy(GetImpl(*this)->VecRegs().data(), value.data(), sizeof(value));
}

std::uint32_t Jit::GetFpcr() const {
    return GetImpl(*this)->Fpcr();
}

void Jit::SetFpcr(std::uint32_t value) {
    GetImpl(*this)->SetFpcr(value);
}

std::uint32_t Jit::GetFpsr() const {
    return GetImpl(*this)->Fpsr();
}

void Jit::SetFpsr(std::uint32_t value) {
    GetImpl(*this)->SetFpsr(value);
}

std::uint32_t Jit::GetPstate() const {
    return GetImpl(*this)->Pstate();
}

void Jit::SetPstate(std::uint32_t value) {
    GetImpl(*this)->SetPstate(value);
}

void Jit::ClearExclusiveState() {
    GetImpl(*this)->ClearExclusiveState();
}

bool Jit::IsExecuting() const {
    return GetImpl(*this)->IsExecuting();
}

std::string Jit::Disassemble() const {
    return GetImpl(*this)->Disassemble();
}

}  // namespace Dynarmic::A64
