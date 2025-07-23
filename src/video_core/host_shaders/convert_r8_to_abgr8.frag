// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstants {
    uniform vec2 tex_scale;
    uniform vec2 tex_offset;
};

layout(location = 0) out vec4 color;

void main() {
    vec2 coord = gl_FragCoord.xy / tex_scale * textureSize(tex, 0);
    float red = texelFetch(tex, ivec2(coord), 0).r;
    color = vec4(red, red, red, 1.0);
}
