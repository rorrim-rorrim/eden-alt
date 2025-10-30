// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/ir/location_descriptor.h"

#include <fmt/format.h>

namespace Dynarmic::IR {

std::string ToString(const LocationDescriptor& descriptor) {
    return fmt::format("{{{:016x}}}", descriptor.Value());
}

} // namespace Dynarmic::IR
