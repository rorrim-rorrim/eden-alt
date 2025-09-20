// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// https://en.wikipedia.org/wiki/Lanczos_resampling

#version 460 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;
layout (binding = 0) uniform sampler2D color_texture;

// precomputed kernel
const float w_kernel[49] = float[] (
    -0.238811f, 0.531959f, 0.961865f, 1.000000f, 0.961865f, 0.531959f, -0.238811f, 0.531959f, 0.957419f, 0.313883f, -0.000000f, 0.313883f, 0.957419f, 0.531959f, 0.961865f, 0.313883f, -0.322602f, 0.000000f, -0.322602f, 0.313883f, 0.961865f, 1.000000f, -0.000000f, 0.000000f, 1.000000f, 0.000000f, -0.000000f, 1.000000f, 0.961865f, 0.313883f, -0.322602f, 0.000000f, -0.322602f, 0.313883f, 0.961865f, 0.531959f, 0.957419f, 0.313883f, -0.000000f, 0.313883f, 0.957419f, 0.531959f, -0.238811f, 0.531959f, 0.961865f, 1.000000f, 0.961865f, 0.531959f, -0.238811f
);
const vec2 w_pos[49] = vec2[] (
    vec2(-0.750000f, -0.750000f), vec2(-0.750000f, -0.500000f), vec2(-0.750000f, -0.250000f), vec2(-0.750000f, 0.000000f), vec2(-0.750000f, 0.250000f), vec2(-0.750000f, 0.500000f), vec2(-0.750000f, 0.750000f), vec2(-0.500000f, -0.750000f), vec2(-0.500000f, -0.500000f), vec2(-0.500000f, -0.250000f), vec2(-0.500000f, 0.000000f), vec2(-0.500000f, 0.250000f), vec2(-0.500000f, 0.500000f), vec2(-0.500000f, 0.750000f), vec2(-0.250000f, -0.750000f), vec2(-0.250000f, -0.500000f), vec2(-0.250000f, -0.250000f), vec2(-0.250000f, 0.000000f), vec2(-0.250000f, 0.250000f), vec2(-0.250000f, 0.500000f), vec2(-0.250000f, 0.750000f), vec2(0.000000f, -0.750000f), vec2(0.000000f, -0.500000f), vec2(0.000000f, -0.250000f), vec2(0.000000f, 0.000000f), vec2(0.000000f, 0.250000f), vec2(0.000000f, 0.500000f), vec2(0.000000f, 0.750000f), vec2(0.250000f, -0.750000f), vec2(0.250000f, -0.500000f), vec2(0.250000f, -0.250000f), vec2(0.250000f, 0.000000f), vec2(0.250000f, 0.250000f), vec2(0.250000f, 0.500000f), vec2(0.250000f, 0.750000f), vec2(0.500000f, -0.750000f), vec2(0.500000f, -0.500000f), vec2(0.500000f, -0.250000f), vec2(0.500000f, 0.000000f), vec2(0.500000f, 0.250000f), vec2(0.500000f, 0.500000f), vec2(0.500000f, 0.750000f), vec2(0.750000f, -0.750000f), vec2(0.750000f, -0.500000f), vec2(0.750000f, -0.250000f), vec2(0.750000f, 0.000000f), vec2(0.750000f, 0.250000f), vec2(0.750000f, 0.500000f), vec2(0.750000f, 0.750000f)
);
const float w_sum = 21.045683f;
vec4 textureLanczos(sampler2D textureSampler, vec2 p) {
    vec3 c_sum = vec3(0.0f);
    vec2 res = vec2(textureSize(textureSampler, 0));
    vec2 cc = floor(p * res) / res;
    for (int i = 0; i < 49; i++) { // kernel size = (r * 2 + 1) ^ 2
        vec2 kp = w_pos[i] / res;
        vec2 uv = cc + kp;
        c_sum += w_kernel[i] * texture(textureSampler, p + kp).rgb;
    }
    return vec4(c_sum / w_sum, 1.0f);
}

void main() {
    color = textureLanczos(color_texture, frag_tex_coord);
}
