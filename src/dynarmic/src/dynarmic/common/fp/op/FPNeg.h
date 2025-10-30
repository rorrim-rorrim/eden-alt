// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include "dynarmic/common/fp/info.h"

namespace Dynarmic::FP {

template <typename FPT>
constexpr FPT FPNeg(FPT op) {
    return op ^ FPInfo<FPT>::sign_mask;
}

} // namespace Dynarmic::FP
