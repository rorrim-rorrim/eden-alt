// SPDX-FileCopyrightText: Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#version 460 core

precision highp float;
precision highp int;

// Operation modes: RGBA -> 1, RGBY -> 3, LERP -> 4
#define OPERATION_MODE 1
#define EDGE_THRESHOLD (8.0 / 255.0)

layout( push_constant ) uniform constants {
    vec4 viewport[1];
    vec2 resize_factor;
    float edge_sharpness;
};
layout(set = 0, binding = 0) uniform sampler2D sampler0;
layout(location=0) in vec2 texcoord;
layout(location=0) out vec4 frag_color;

vec4 weightY(vec4 dx, vec4 dy, vec4 std) {
    vec4 x = ((dx * dx) + (dy * dy)) * 0.55f + std;
    return (x - 1.f) * (x - 4.f) * 3.8125f; // approx. of (x - 1) * (x - 4)^3
}

void main() {
    vec4 color = textureLod(sampler0, texcoord.xy, 0.0f);
    // image coord
    vec2 icoord = ((texcoord.xy * viewport[0].zw) + vec2(-0.5f, 0.5f));
    vec2 icoord_pixel = floor(icoord);
    vec2 coord = icoord_pixel * viewport[0].xy;
    vec2 pl = icoord - icoord_pixel;
    vec4 left = textureGather(sampler0, coord, 1);
    float edgeVote = abs(left.z - left.y) + abs(color[1] - left.y)  + abs(color[1] - left.z);
    if (edgeVote > EDGE_THRESHOLD) {
        coord.x += viewport[0].x;
        vec4 right = textureGather(sampler0, coord + vec2(viewport[0].x, 0.0f), 1);
        vec4 upDown = vec4(
            textureGather(sampler0, coord + vec2(0.0f, -viewport[0].y), 1).wz,
            textureGather(sampler0, coord + vec2(0.0f, +viewport[0].y), 1).yx
        );
        float mean = (left.y + left.z + right.x + right.w) * 0.25f;
        left = left - vec4(mean);
        right = right - vec4(mean);
        upDown = upDown - vec4(mean);
        color.w = color[1] - mean;

        vec4 sum = abs(left) + abs(right) + abs(upDown);
        float std = 2.181818f / (sum.x + sum.y + sum.z + sum.w);

        mat2x4 w = mat2x4(
            weightY(
                pl.xxxx + vec4(+0.0f, -1.0f, -1.0f, +0.0f),
                pl.yyyy + vec4(+1.0f, +1.0f, -2.0f, -2.0f),
                clamp(abs(upDown) * std, 0.0f, 1.0f)
            ) + weightY(
                pl.xxxx + vec4(+1.0f, +0.0f, +0.0f, +1.0f),
                pl.yyyy + vec4(-1.0f, -1.0f, +0.0f, +0.0f),
                clamp(abs(left) * std, 0.0f, 1.0f)
            ) + weightY(
                pl.xxxx + vec4(-1.0f, -2.0f, -2.0f, -1.0f),
                pl.yyyy + vec4(-1.0f, -1.0f, +0.0f, +0.0f),
                clamp(abs(right) * std, 0.0f, 1.0f)
            ),
            upDown + left + right
        );
        // compute final y with bounds
        vec2 yb = vec2(
            min(min(left.y, left.z), min(right.x, right.w)), // min
            max(max(left.y, left.z), max(right.x, right.w))  // max
        );
        vec2 fvy = vec2(
            w[0].x + w[0].y + w[0].z + w[0].w,
            w[1].x + w[1].y + w[1].z + w[1].w
        );
        float fy = clamp((fvy.y / fvy.x) * edge_sharpness, yb[0], yb[1]);
        // Smooth high contrast input
        float dy = clamp(fy - color.w, -23.0f / 255.0f, 23.0f / 255.0f);
        color = clamp(color + dy, 0.0f, 1.0f);
    }
    color.w = 1.0f;  //assume alpha channel is not used
    frag_color.xyzw = color;
}