// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <list>

#include <boost/container/container_fwd.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include "dynarmic/common/common_types.h"

#include "dynarmic/ir/location_descriptor.h"
#include "dynarmic/ir/microinstruction.h"
#include "dynarmic/ir/terminal.h"
#include "dynarmic/ir/value.h"

namespace Dynarmic::IR {

enum class Cond;
enum class Opcode;

/// A basic block. It consists of zero or more instructions followed by exactly one terminal.
/// Note that this is a linear IR and not a pure tree-based IR: i.e.: there is an ordering to
/// the microinstructions. This only matters before chaining is done in order to correctly
/// order memory accesses.
class Block final {
public:
    using instruction_list_type = boost::container::stable_vector<Inst>;
    using iterator = instruction_list_type::iterator;
    using const_iterator = instruction_list_type::const_iterator;
    using reverse_iterator = instruction_list_type::reverse_iterator;

    Block(LocationDescriptor location) noexcept;
    ~Block() = default;
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    Block(Block&&) = default;
    Block& operator=(Block&&) = default;

    [[nodiscard]] iterator PrependNewInst(const_iterator insertion_point, Opcode op, std::initializer_list<Value> args) noexcept;
    void Reset(LocationDescriptor location_) noexcept;

    /// Determines whether or not a predicated condition failure block is present.
    [[nodiscard]] inline bool HasConditionFailedLocation() const noexcept {
        return cond_failed.Value() > 0;
    }

    /// Determines whether or not this basic block has a terminal instruction.
    [[nodiscard]] inline bool HasTerminal() const noexcept {
        return terminal.which() != 0;
    }

    /// List of instructions in this block.
    instruction_list_type instructions;
    /// Block to execute next if `cond` did not pass.
    LocationDescriptor cond_failed;
    /// Description of the starting location of this block
    LocationDescriptor location;
    /// Description of the end location of this block
    LocationDescriptor end_location;
    /// Conditional to pass in order to execute this block
    Cond cond = Cond::AL;
    /// Terminal instruction of this block.
    Terminal terminal = Term::Invalid{};
    /// Number of cycles this block takes to execute if the conditional fails.
    size_t cond_failed_cycle_count = 0;
    /// Number of cycles this block takes to execute.
    size_t cycle_count = 0;
};
//static_assert(sizeof(Block) == 120);

/// Returns a string representation of the contents of block. Intended for debugging.
std::string DumpBlock(const IR::Block& block) noexcept;

}  // namespace Dynarmic::IR
