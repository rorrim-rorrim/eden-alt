// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <limits>
#include <vector>

#include "common/common_types.h"

namespace Tegra::Engines {

enum class EngineTypes : u32 {
    KeplerCompute,
    Maxwell3D,
    Fermi2D,
    MaxwellDMA,
    KeplerMemory,
};

class EngineInterface {
public:
    virtual ~EngineInterface() = default;

    static constexpr size_t ExecutionMaskBits = (std::numeric_limits<u16>::max)();
    static constexpr size_t ExecutionMaskWordBits = 64;
    static constexpr size_t ExecutionMaskWords =
        (ExecutionMaskBits + ExecutionMaskWordBits - 1) / ExecutionMaskWordBits;

    /// Write the value to the register identified by method.
    virtual void CallMethod(u32 method, u32 method_argument, bool is_last_call) = 0;

    /// Write multiple values to the register identified by method.
    virtual void CallMultiMethod(u32 method, const u32* base_start, u32 amount,
                                 u32 methods_pending) = 0;

    void ConsumeSink() {
        if (method_sink.empty()) {
            return;
        }
        ConsumeSinkImpl();
    }

    void ClearExecutionMask() {
        execution_mask_words.fill(0);
    }

    void SetExecutionMaskBit(u32 method, bool value = true) {
        if (method >= ExecutionMaskBits) {
            return;
        }
        const size_t word_index = method >> 6;
        const u64 bit_mask = 1ULL << (method & 63);
        if (value) {
            execution_mask_words[word_index] |= bit_mask;
        } else {
            execution_mask_words[word_index] &= ~bit_mask;
        }
    }

    bool IsExecutionMaskSet(u32 method) const {
        if (method >= ExecutionMaskBits) {
            return false;
        }
        const size_t word_index = method >> 6;
        const u64 bit_mask = 1ULL << (method & 63);
        return (execution_mask_words[word_index] & bit_mask) != 0;
    }

    std::array<u64, ExecutionMaskWords> execution_mask_words{};
    std::vector<std::pair<u32, u32>> method_sink{};
    bool current_dirty{};
    GPUVAddr current_dma_segment;

protected:
    virtual void ConsumeSinkImpl() {
        for (auto [method, value] : method_sink) {
            CallMethod(method, value, true);
        }
        method_sink.clear();
    }
};

} // namespace Tegra::Engines
