// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "common/x64/cpu_detect.h"

namespace Common::X64 {

void MicroSleep(const CPUCaps& caps, u64 cycles);

} // namespace Common::X64
