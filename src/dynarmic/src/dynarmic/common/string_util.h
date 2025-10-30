// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

namespace Dynarmic::Common {

template <typename T>
constexpr char SignToChar(T value) {
    return value >= 0 ? '+' : '-';
}

} // namespace Dynarmic::Common
