/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <boost/variant/get.hpp>
#include <mcl/stdint.hpp>

#include "dynarmic/frontend/A64/a64_location_descriptor.h"
#include "dynarmic/frontend/A64/translate/a64_translate.h"
#include "dynarmic/interface/A64/config.h"
#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/opt/passes.h"

namespace Dynarmic::Optimization {

// blind peepholes
void X64Peepholes(IR::Block& block) {
    for (auto iter = block.begin(); iter != block.end(); iter++) {
        
    }
}

}  // namespace Dynarmic::Optimization
