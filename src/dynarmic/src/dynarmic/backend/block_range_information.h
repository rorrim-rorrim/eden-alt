// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <set>

#include <ankerl/unordered_dense.h>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>

#include "dynarmic/ir/location_descriptor.h"

namespace Dynarmic::Backend {

template <typename ProgramCounterType>
class BlockRangeInformation {
public:
    void AddRange(boost::icl::discrete_interval<ProgramCounterType> range,
                  IR::LocationDescriptor location);
    void ClearCache();
    ankerl::unordered_dense::set<IR::LocationDescriptor> InvalidateRanges(
        const boost::icl::interval_set<ProgramCounterType>& ranges);

private:
    boost::icl::interval_map<ProgramCounterType, std::set<IR::LocationDescriptor>> block_ranges;
};

} // namespace Dynarmic::Backend
