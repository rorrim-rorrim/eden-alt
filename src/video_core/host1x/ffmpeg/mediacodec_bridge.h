// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>
#include <cstdint>

#ifdef __ANDROID__
#include <android/hardware_buffer.h>
#endif

namespace FFmpeg::MediaCodecBridge {

bool IsAvailable();

// Create a platform decoder for the given mime type ("video/avc", "video/x-vnd.on2.vp9", ...)
// Returns decoder id (>0) on success, or 0 on failure.
int CreateDecoder(const char* mime, int width, int height);
void DestroyDecoder(int id);

// Feed an encoded packet to the decoder. Returns true if accepted.
bool SendPacket(int id, const uint8_t* data, size_t size, int64_t pts);

// Pop a decoded NV12 frame. Returns std::nullopt if none available. On success, fills width,height,pts
std::optional<std::vector<uint8_t>> PopDecodedFrame(int id, int& width, int& height, int64_t& pts);

#ifdef __ANDROID__
// Pop a decoded AHardwareBuffer (zero-copy path). Returns std::nullopt if none available.
std::optional<AHardwareBuffer*> PopDecodedHardwareBuffer(int id, int& width, int& height, int64_t& pts);
#endif

#ifdef __ANDROID__
// Enqueue an AHardwareBuffer for presentation; PresentManager will consume these.
void EnqueueHardwareBufferForPresent(AHardwareBuffer* ahb, int width, int height, int64_t pts);

// Try to pop an AHardwareBuffer for presentation. Returns true if a buffer was popped.
bool TryPopHardwareBufferForPresent(AHardwareBuffer** out_ahb, int& out_width, int& out_height, int64_t& out_pts);
#endif
} // namespace FFmpeg::MediaCodecBridge
