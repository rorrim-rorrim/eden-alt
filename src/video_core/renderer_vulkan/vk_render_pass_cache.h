// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <unordered_map>

#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

struct RenderPassKey {
    bool operator==(const RenderPassKey&) const noexcept = default;

    std::array<VideoCore::Surface::PixelFormat, 8> color_formats;
    VideoCore::Surface::PixelFormat depth_format;
    VkSampleCountFlagBits samples;
    
    // TBDR optimization hints - only affect tile-based GPUs (Qualcomm, ARM, Imagination)
    // These flags indicate the expected usage pattern to optimize load/store operations
    bool tbdr_will_clear{false};  // Attachment will be cleared with vkCmdClearAttachments
    bool tbdr_discard_after{false};  // Attachment won't be read after render pass
};

} // namespace Vulkan

namespace std {
template <>
struct hash<Vulkan::RenderPassKey> {
    [[nodiscard]] size_t operator()(const Vulkan::RenderPassKey& key) const noexcept {
        size_t value = static_cast<size_t>(key.depth_format) << 48;
        value ^= static_cast<size_t>(key.samples) << 52;
        value ^= (static_cast<size_t>(key.tbdr_will_clear) << 56);
        value ^= (static_cast<size_t>(key.tbdr_discard_after) << 57);
        for (size_t i = 0; i < key.color_formats.size(); ++i) {
            value ^= static_cast<size_t>(key.color_formats[i]) << (i * 6);
        }
        return value;
    }
};
} // namespace std

namespace Vulkan {

class Device;

class RenderPassCache {
public:
    explicit RenderPassCache(const Device& device_);

    VkRenderPass Get(const RenderPassKey& key);

private:
    const Device* device{};
    std::unordered_map<RenderPassKey, vk::RenderPass> cache;
    std::mutex mutex;
};

} // namespace Vulkan
