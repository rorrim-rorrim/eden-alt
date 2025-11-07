#pragma once

#include <optional>
#include <vector>
#include <cstdint>

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

} // namespace FFmpeg::MediaCodecBridge
