// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */
#pragma once

#include "dynarmic/common/assert.h"
#include "dynarmic/common/common_types.h"
#include <xbyak/xbyak.h>

namespace Dynarmic::Backend::X64 {

// Our static vector will contain 32 elements, stt. an uint16_t will fill up 64 bytes
// (an entire cache line). Thanks.
enum class HostLoc : uint16_t {
    // Ordering of the registers is intentional. See also: HostLocToX64.
    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    XMM0,
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7,
    XMM8,
    XMM9,
    XMM10,
    XMM11,
    XMM12,
    XMM13,
    XMM14,
    XMM15,
    CF,
    PF,
    AF,
    ZF,
    SF,
    OF,
    FirstSpill,
};

constexpr size_t NonSpillHostLocCount = static_cast<size_t>(HostLoc::FirstSpill);

inline bool HostLocIsGPR(HostLoc reg) {
    return reg >= HostLoc::RAX && reg <= HostLoc::R15;
}

inline bool HostLocIsXMM(HostLoc reg) {
    return reg >= HostLoc::XMM0 && reg <= HostLoc::XMM15;
}

inline bool HostLocIsRegister(HostLoc reg) {
    return HostLocIsGPR(reg) || HostLocIsXMM(reg);
}

inline bool HostLocIsFlag(HostLoc reg) {
    return reg >= HostLoc::CF && reg <= HostLoc::OF;
}

inline HostLoc HostLocRegIdx(int idx) {
    ASSERT(idx >= 0 && idx <= 15);
    return HostLoc(idx);
}

inline HostLoc HostLocXmmIdx(int idx) {
    ASSERT(idx >= 0 && idx <= 15);
    return HostLoc(size_t(HostLoc::XMM0) + idx);
}

inline HostLoc HostLocSpill(size_t i) {
    return HostLoc(size_t(HostLoc::FirstSpill) + i);
}

inline bool HostLocIsSpill(HostLoc reg) {
    return reg >= HostLoc::FirstSpill;
}

inline size_t HostLocBitWidth(HostLoc loc) {
    if (HostLocIsGPR(loc))
        return 64;
    if (HostLocIsXMM(loc))
        return 128;
    if (HostLocIsSpill(loc))
        return 128;
    if (HostLocIsFlag(loc))
        return 1;
    UNREACHABLE();
}

using HostLocList = std::initializer_list<HostLoc>;

// RSP is preserved for function calls
// R13 contains fastmem pointer if any
// R14 contains the pagetable pointer
// R15 contains the JitState pointer
const HostLocList any_gpr = {
    HostLoc::RAX,
    HostLoc::RBX,
    HostLoc::RCX,
    HostLoc::RDX,
    HostLoc::RSI,
    HostLoc::RDI,
    HostLoc::RBP,
    HostLoc::R8,
    HostLoc::R9,
    HostLoc::R10,
    HostLoc::R11,
    HostLoc::R12,
    HostLoc::R13,
    HostLoc::R14,
    //HostLoc::R15,
};

// XMM0 is reserved for use by instructions that implicitly use it as an argument
// XMM1 is used by 128 mem accessors
// XMM2 is also used by that (and other stuff)
// Basically dont use either XMM0, XMM1 or XMM2 ever; they're left for the regsel
const HostLocList any_xmm = {
    //HostLoc::XMM1,
    //HostLoc::XMM2,
    HostLoc::XMM3,
    HostLoc::XMM4,
    HostLoc::XMM5,
    HostLoc::XMM6,
    HostLoc::XMM7,
    HostLoc::XMM8,
    HostLoc::XMM9,
    HostLoc::XMM10,
    HostLoc::XMM11,
    HostLoc::XMM12,
    HostLoc::XMM13,
    HostLoc::XMM14,
    HostLoc::XMM15,
};

Xbyak::Reg64 HostLocToReg64(HostLoc loc);
Xbyak::Xmm HostLocToXmm(HostLoc loc);

}  // namespace Dynarmic::Backend::X64
