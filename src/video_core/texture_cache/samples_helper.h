// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

constexpr inline std::size_t MaxSampleLocationSlots = 16;

[[nodiscard]] inline std::pair<int, int> SamplesLog2(int num_samples) {
    switch (num_samples) {
    case 1:
        return {0, 0};
    case 2:
        return {1, 0};
    case 4:
        return {1, 1};
    case 8:
        return {2, 1};
    case 16:
        return {2, 2};
    }
    ASSERT_MSG(false, "Invalid number of samples={}", num_samples);
    return {0, 0};
}

[[nodiscard]] inline int NumSamples(Tegra::Texture::MsaaMode msaa_mode) {
    using Tegra::Texture::MsaaMode;
    switch (msaa_mode) {
    case MsaaMode::Msaa1x1:
        return 1;
    case MsaaMode::Msaa2x1:
    case MsaaMode::Msaa2x1_D3D:
        return 2;
    case MsaaMode::Msaa2x2:
    case MsaaMode::Msaa2x2_VC4:
    case MsaaMode::Msaa2x2_VC12:
        return 4;
    case MsaaMode::Msaa4x2:
    case MsaaMode::Msaa4x2_D3D:
    case MsaaMode::Msaa4x2_VC8:
    case MsaaMode::Msaa4x2_VC24:
        return 8;
    case MsaaMode::Msaa4x4:
        return 16;
    }
    ASSERT_MSG(false, "Invalid MSAA mode={}", static_cast<int>(msaa_mode));
    return 1;
}

[[nodiscard]] inline int NumSamplesX(Tegra::Texture::MsaaMode msaa_mode) {
    using Tegra::Texture::MsaaMode;
    switch (msaa_mode) {
    case MsaaMode::Msaa1x1:
        return 1;
    case MsaaMode::Msaa2x1:
    case MsaaMode::Msaa2x1_D3D:
    case MsaaMode::Msaa2x2:
    case MsaaMode::Msaa2x2_VC4:
    case MsaaMode::Msaa2x2_VC12:
        return 2;
    case MsaaMode::Msaa4x2:
    case MsaaMode::Msaa4x2_D3D:
    case MsaaMode::Msaa4x2_VC8:
    case MsaaMode::Msaa4x2_VC24:
    case MsaaMode::Msaa4x4:
        return 4;
    }
    ASSERT_MSG(false, "Invalid MSAA mode={}", static_cast<int>(msaa_mode));
    return 1;
}

[[nodiscard]] inline int NumSamplesY(Tegra::Texture::MsaaMode msaa_mode) {
    using Tegra::Texture::MsaaMode;
    switch (msaa_mode) {
    case MsaaMode::Msaa1x1:
    case MsaaMode::Msaa2x1:
    case MsaaMode::Msaa2x1_D3D:
        return 1;
    case MsaaMode::Msaa2x2:
    case MsaaMode::Msaa2x2_VC4:
    case MsaaMode::Msaa2x2_VC12:
    case MsaaMode::Msaa4x2:
    case MsaaMode::Msaa4x2_D3D:
    case MsaaMode::Msaa4x2_VC8:
    case MsaaMode::Msaa4x2_VC24:
        return 2;
    case MsaaMode::Msaa4x4:
        return 4;
    }
    ASSERT_MSG(false, "Invalid MSAA mode={}", static_cast<int>(msaa_mode));
    return 1;
}

// NVN guarantees up to sixteen programmable sample slots shared across a repeating pixel grid
// (per the 0.7.0 addon release notes), so the grid dimensions shrink as MSAA increases.
[[nodiscard]] inline std::pair<u32, u32> SampleLocationGridSize(Tegra::Texture::MsaaMode msaa_mode) {
    const int samples = NumSamples(msaa_mode);
    switch (samples) {
    case 1:
        return {4u, 4u};
    case 2:
        return {4u, 2u};
    case 4:
        return {2u, 2u};
    case 8:
        return {2u, 1u};
    case 16:
        return {1u, 1u};
    }
    ASSERT_MSG(false, "Unsupported sample count for grid size={}", samples);
    return {1u, 1u};
}

} // namespace VideoCommon
