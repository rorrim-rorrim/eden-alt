// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out float color;

void main() {
    vec2 coord = gl_FragCoord.xy / vec2(textureSize(tex, 0));
    float red = texture(tex, coord).r;
    color = red;
}
