// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450
#extension GL_ARB_shader_stencil_export : require

layout(binding = 0) uniform sampler2D color_texture;

// See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
vec4 SrgbToLinear(vec4 srgba) {
    return mix(pow((srgba + 0.055) * (1.0 / 1.055), vec3(2.4)), srgba * (1.0/12.92), lessThanEqual(srgba, vec3(0.04045)));
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    uvec4 color = uvec4(texelFetch(color_texture, coord, 0).abgr * (exp2(8) - 1.0f));
    uvec4 srgb_color = uvec4(SrgbToLinear(vec4(color) / 255.f) * 255.f); // Is this even needed... ?ploo
    uvec4 bytes = srgb_color << uvec4(24, 16, 8, 0);
    uint depth_stencil_unorm = bytes.x | bytes.y | bytes.z | bytes.w;
    gl_FragDepth = float(depth_stencil_unorm & 0x00FFFFFFu) / (exp2(24.0) - 1.0f);
    gl_FragStencilRefARB = int(depth_stencil_unorm >> 24);
}
