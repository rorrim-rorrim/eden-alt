// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace AudioCore::Renderer {

/**
 * Input struct passed to a splitter destination in REV13.
 */
struct SplitterDestinationInParameter {
    /* 0x00 */ s32 magic;
    /* 0x04 */ s32 id;
    /* 0x08 */ bool reset_prev_volume;
    /* 0x09 */ u8 reserved[11];  // Padding to align to 0x14
};
static_assert(sizeof(SplitterDestinationInParameter) == 0x14,
              "SplitterDestinationInParameter must be 0x14 bytes.");

} // namespace AudioCore::Renderer
