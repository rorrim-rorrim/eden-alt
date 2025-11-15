// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450

// VK_QCOM_render_pass_shader_resolve fragment shader
// Resolves MSAA attachment to single-sample within render pass
// Requires VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM in subpass flags

// Use combined image sampler for MSAA texture instead of input attachment
// This allows us to sample MSAA textures from previous rendering
layout(set = 0, binding = 0) uniform sampler2DMS msaa_texture;

layout(location = 0) out vec4 color_output;

layout(push_constant) uniform PushConstants {
    vec2 tex_scale;
    vec2 tex_offset;
} push_constants;

// Custom MSAA resolve using box filter (simple average)
// Assumes 4x MSAA (can be extended with push constant for dynamic sample count)
void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    ivec2 tex_size = textureSize(msaa_texture);
    
    // Clamp coordinates to texture bounds
    coord = clamp(coord, ivec2(0), tex_size - ivec2(1));
    
    vec4 accumulated_color = vec4(0.0);
    int sample_count = 4; // Adreno typically uses 4x MSAA max
    
    // Box filter: simple average of all MSAA samples
    for (int i = 0; i < sample_count; i++) {
        accumulated_color += texelFetch(msaa_texture, coord, i);
    }
    
    color_output = accumulated_color / float(sample_count);
}
