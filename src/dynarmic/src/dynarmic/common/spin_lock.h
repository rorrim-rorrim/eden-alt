// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

namespace Dynarmic {

struct SpinLock {
    void Lock() noexcept;
    void Unlock() noexcept;
    volatile int storage = 0;
};

} // namespace Dynarmic
