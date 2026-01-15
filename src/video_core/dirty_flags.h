// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"

namespace VideoCommon::Dirty {

enum : u8 {
    NullEntry = 0,

    Descriptors,

    RenderTargets,
    RenderTargetControl,
    ColorBuffer0,
    ColorBuffer1,
    ColorBuffer2,
    ColorBuffer3,
    ColorBuffer4,
    ColorBuffer5,
    ColorBuffer6,
    ColorBuffer7,
    ZetaBuffer,
    RescaleViewports,
    RescaleScissors,

    VertexBuffers,
    VertexBuffer0,
    VertexBuffer31 = VertexBuffer0 + 31,

    IndexBuffer,

    Shaders,

    // Special entries
    DepthBiasGlobal,

    LastCommonEntry,
};

constexpr std::pair<u8, u8> GetDirtyFlagsForMethod(u32 method) {
    // All offsets are register indices (byte_offset / 4)
    constexpr u32 OFF_RT = 0x0200;                    // rt: 0x0800 / 4
    constexpr u32 OFF_ZETA = 0x03F8;                  // zeta: 0x0FE0 / 4
    constexpr u32 OFF_SURFACE_CLIP = 0x03FD;          // surface_clip: 0x0FF4 / 4
    constexpr u32 OFF_RT_CONTROL = 0x0487;            // rt_control: 0x121C / 4
    constexpr u32 OFF_ZETA_SIZE_WIDTH = 0x048A;       // zeta_size.width: 0x1228 / 4
    constexpr u32 OFF_ZETA_SIZE_HEIGHT = 0x048B;      // zeta_size.height: 0x122C / 4
    constexpr u32 OFF_ZETA_ENABLE = 0x054E;           // zeta_enable: 0x1538 / 4
    constexpr u32 OFF_TEX_SAMPLER = 0x0557;           // tex_sampler: 0x155C / 4
    constexpr u32 OFF_TEX_HEADER = 0x055D;            // tex_header: 0x1574 / 4
    constexpr u32 OFF_INDEX_BUFFER = 0x05F2;          // index_buffer: 0x17C8 / 4
    constexpr u32 OFF_VERTEX_STREAMS = 0x0700;        // vertex_streams: 0x1C00 / 4
    constexpr u32 OFF_VERTEX_STREAM_LIMITS = 0x07C0;  // vertex_stream_limits: 0x1F00 / 4
    constexpr u32 OFF_PIPELINES = 0x0800;             // pipelines: 0x2000 / 4

    // Render targets: 8 RTs × 8 dwords each = 64 entries
    if (method >= OFF_RT && method < OFF_RT + 64) {
        const u32 rt_idx = (method - OFF_RT) / 8;
        return {static_cast<u8>(ColorBuffer0 + rt_idx), RenderTargets};
    }

    // Zeta buffer: 8 dwords
    if (method >= OFF_ZETA && method < OFF_ZETA + 8) {
        return {ZetaBuffer, RenderTargets};
    }

    // Surface clip: 4 dwords
    if (method >= OFF_SURFACE_CLIP && method < OFF_SURFACE_CLIP + 4) {
        return {RenderTargets, NullEntry};
    }

    // RT control: single register
    if (method == OFF_RT_CONTROL) {
        return {RenderTargets, RenderTargetControl};
    }

    // Zeta size and enable
    if (method == OFF_ZETA_ENABLE || method == OFF_ZETA_SIZE_WIDTH || method == OFF_ZETA_SIZE_HEIGHT) {
        return {ZetaBuffer, RenderTargets};
    }

    // Texture sampler: 64 entries (0x40)
    if (method >= OFF_TEX_SAMPLER && method < OFF_TEX_SAMPLER + 0x40) {
        return {Descriptors, NullEntry};
    }

    // Texture header: 64 entries (0x40)
    if (method >= OFF_TEX_HEADER && method < OFF_TEX_HEADER + 0x40) {
        return {Descriptors, NullEntry};
    }

    // Index buffer: 7 dwords
    if (method >= OFF_INDEX_BUFFER && method < OFF_INDEX_BUFFER + 7) {
        return {IndexBuffer, NullEntry};
    }

    // Vertex streams: 32 buffers × 4 dwords = 128 entries
    if (method >= OFF_VERTEX_STREAMS && method < OFF_VERTEX_STREAMS + 128) {
        const u32 buffer_idx = (method - OFF_VERTEX_STREAMS) / 4;
        return {static_cast<u8>(VertexBuffer0 + buffer_idx), VertexBuffers};
    }

    // Vertex stream limits: 32 buffers × 2 dwords = 64 entries
    if (method >= OFF_VERTEX_STREAM_LIMITS && method < OFF_VERTEX_STREAM_LIMITS + 64) {
        const u32 buffer_idx = (method - OFF_VERTEX_STREAM_LIMITS) / 2;
        return {static_cast<u8>(VertexBuffer0 + buffer_idx), VertexBuffers};
    }

    // Pipelines/Shaders: 6 programs × 0x40 dwords = 384 entries
    if (method >= OFF_PIPELINES && method < OFF_PIPELINES + 384) {
        return {Shaders, NullEntry};
    }

    return {NullEntry, NullEntry};
}

template <typename Integer>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Table& table, std::size_t begin,
               std::size_t num, Integer dirty_index) {
    const auto it = std::begin(table) + begin;
    std::fill(it, it + num, static_cast<u8>(dirty_index));
}

template <typename Integer1, typename Integer2>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables, std::size_t begin,
               std::size_t num, Integer1 index_a, Integer2 index_b) {
    FillBlock(tables[0], begin, num, index_a);
    FillBlock(tables[1], begin, num, index_b);
}

void SetupDirtyFlags(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables);

} // namespace VideoCommon::Dirty
