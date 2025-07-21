// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 color;

/*
The idea here is copy each byte of one texture to another independently of the format
because is an incompatible copy
*/
void main() {
    //Destination has 4 bytes per pixel so 4 pixel on origin makes 1 pixel in dest
    float xPixelCoord = gl_FragCoord.x * 4;
    //Yuzu calls it abgr8 but the the order is rgba, dunno if is LSB or MSB
    color = vec4(
        texelFetch(tex, ivec2(xPixelCoord, gl_FragCoord.y), 0).r,
        texelFetch(tex, ivec2(xPixelCoord + 1, gl_FragCoord.y), 0).r,
        texelFetch(tex, ivec2(xPixelCoord + 2, gl_FragCoord.y), 0).r,
        texelFetch(tex, ivec2(xPixelCoord + 3, gl_FragCoord.y), 0).r
    );
}
