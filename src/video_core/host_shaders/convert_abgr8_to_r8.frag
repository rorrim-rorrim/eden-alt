// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out float color;

layout(push_constant) uniform PushConstants {
    uniform vec2 tex_scale;
    uniform vec2 tex_offset;
};

void main() {
    vec2 coord = gl_FragCoord.xy / tex_scale * textureSize(tex, 0);
    vec4 fetch = texelFetch(tex, ivec2(coord), 0);
    color = fetch.r;
}
