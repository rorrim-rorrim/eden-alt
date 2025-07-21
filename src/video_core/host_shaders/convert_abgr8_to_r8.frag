// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out float color;

/*
The idea here is copy each byte of one texture to another independently of the format
because is an incompatible copy
*/
void main() {
    //Origin has 4 bytes per pixel
    float xPixelCoord = gl_FragCoord.x / 4;
    vec4 pixel = texelFetch(tex, ivec2(xPixelCoord, gl_FragCoord.y), 0);
    //Yuzu calls it abgr8 but the the order is rgba, dunno if is LSB or MSB
    switch(int(gl_FragCoord.x) % 4) {
        case 0:
            color = pixel.r;
            break;
        case 1:
            color = pixel.g;
            break;
        case 2:
            color = pixel.b;
            break;
        case 3:
            color = pixel.a;
            break;
    }
}
