/* This file is part of the dynarmic project.
 * Copyright (c) 2020 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <vector>

#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/opcodes.h"
#include "dynarmic/ir/opt/passes.h"

namespace Dynarmic::Optimization {

void IdentityRemovalPass(IR::Block& block) {
    std::vector<IR::Inst*> to_invalidate;
    for (auto iter = block.begin(); iter != block.end(); ) {
        IR::Inst& inst = *iter;
        const size_t num_args = inst.NumArgs();
        for (size_t i = 0; i < num_args; i++) {
            while (true) {
                IR::Value arg = inst.GetArg(i);
                if (!arg.IsIdentity())
                    break;
                inst.SetArg(i, arg.GetInst()->GetArg(0));
            }
        }
        if (inst.GetOpcode() == IR::Opcode::Identity || inst.GetOpcode() == IR::Opcode::Void) {
            iter = block.Instructions().erase(iter);
            to_invalidate.push_back(&inst);
        } else {
            ++iter;
        }
    }
    for (auto* inst : to_invalidate)
        inst->Invalidate();
}

}  // namespace Dynarmic::Optimization
