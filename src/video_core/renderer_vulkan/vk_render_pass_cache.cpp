// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <unordered_map>

#include <boost/container/static_vector.hpp>

#include "common/logging/log.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceType;

        // Check if the driver uses tile-based deferred rendering (TBDR) architecture
        // These GPUs benefit from optimized load/store operations to keep data on-chip
        // 
        // TBDR GPUs supported in Eden:
        // - Qualcomm Adreno (Snapdragon): Most Android flagship/midrange devices
        // - ARM Mali: Android devices (Samsung Exynos, MediaTek, etc.)
        // - Imagination PowerVR: Older iOS devices, some Android tablets
        // - Samsung Xclipse: Galaxy S22+ (AMD RDNA2-based, but uses TBDR mode)
        // - Broadcom VideoCore: Raspberry Pi
        [[nodiscard]] constexpr bool IsTBDRGPU(VkDriverId driver_id) {
            return driver_id == VK_DRIVER_ID_QUALCOMM_PROPRIETARY ||
                   driver_id == VK_DRIVER_ID_ARM_PROPRIETARY ||
                   driver_id == VK_DRIVER_ID_IMAGINATION_PROPRIETARY ||
                   driver_id == VK_DRIVER_ID_SAMSUNG_PROPRIETARY ||
                   driver_id == VK_DRIVER_ID_BROADCOM_PROPRIETARY;
        }

        constexpr SurfaceType GetSurfaceType(PixelFormat format) {
            switch (format) {
                // Depth formats
                case PixelFormat::D16_UNORM:
                case PixelFormat::D32_FLOAT:
                case PixelFormat::X8_D24_UNORM:
                    return SurfaceType::Depth;

                    // Stencil formats
                case PixelFormat::S8_UINT:
                    return SurfaceType::Stencil;

                    // Depth+Stencil formats
                case PixelFormat::D24_UNORM_S8_UINT:
                case PixelFormat::S8_UINT_D24_UNORM:
                case PixelFormat::D32_FLOAT_S8_UINT:
                    return SurfaceType::DepthStencil;

                    // Everything else is a color texture
                default:
                    return SurfaceType::ColorTexture;
            }
        }

        VkAttachmentDescription AttachmentDescription(const Device& device, PixelFormat format,
                                                      VkSampleCountFlagBits samples,
                                                      bool tbdr_will_clear,
                                                      bool tbdr_discard_after) {
            using MaxwellToVK::SurfaceFormat;

            const SurfaceType surface_type = GetSurfaceType(format);
            const bool has_stencil = surface_type == SurfaceType::DepthStencil ||
                                     surface_type == SurfaceType::Stencil;

            // TBDR optimization: Apply hints only on tile-based GPUs
            // Desktop GPUs (NVIDIA/AMD/Intel) ignore these hints and use standard behavior
            const bool is_tbdr = IsTBDRGPU(device.GetDriverID());
            
            // On TBDR: Use DONT_CARE if clear is guaranteed (avoids loading from main memory)
            // On Desktop: Always LOAD to preserve existing content (safer default)
            VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            if (is_tbdr && tbdr_will_clear) {
                load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }

            // On TBDR: Use DONT_CARE if content won't be read (avoids storing to main memory)
            // On Desktop: Always STORE (safer default)
            VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
            if (is_tbdr && tbdr_discard_after) {
                store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }

            // Stencil operations follow same logic
            VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            if (has_stencil) {
                stencil_load_op = (is_tbdr && tbdr_will_clear) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                : VK_ATTACHMENT_LOAD_OP_LOAD;
                stencil_store_op = (is_tbdr && tbdr_discard_after) ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                                                    : VK_ATTACHMENT_STORE_OP_STORE;
            }

            return {
                .flags = {},
                .format = SurfaceFormat(device, FormatType::Optimal, true, format).format,
                .samples = samples,
                .loadOp = load_op,
                .storeOp = store_op,
                .stencilLoadOp = stencil_load_op,
                .stencilStoreOp = stencil_store_op,
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
        }
    } // Anonymous namespace

RenderPassCache::RenderPassCache(const Device& device_) : device{&device_} {}

VkRenderPass RenderPassCache::Get(const RenderPassKey& key) {
    std::scoped_lock lock{mutex};
    const auto [pair, is_new] = cache.try_emplace(key);
    if (!is_new) {
        return *pair->second;
    }

    const bool is_tbdr = IsTBDRGPU(device->GetDriverID());
    if (is_tbdr && (key.tbdr_will_clear || key.tbdr_discard_after)) {
        LOG_DEBUG(Render_Vulkan, "Creating TBDR-optimized render pass (driver={}, clear={}, discard={})",
                  static_cast<u32>(device->GetDriverID()), key.tbdr_will_clear, key.tbdr_discard_after);
    }

    boost::container::static_vector<VkAttachmentDescription, 9> descriptions;
    std::array<VkAttachmentReference, 8> references{};
    u32 num_attachments{};
    u32 num_colors{};
    for (size_t index = 0; index < key.color_formats.size(); ++index) {
        const PixelFormat format{key.color_formats[index]};
        const bool is_valid{format != PixelFormat::Invalid};
        references[index] = VkAttachmentReference{
            .attachment = is_valid ? num_colors : VK_ATTACHMENT_UNUSED,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        if (is_valid) {
            descriptions.push_back(AttachmentDescription(*device, format, key.samples,
                                                         key.tbdr_will_clear, key.tbdr_discard_after));
            num_attachments = static_cast<u32>(index + 1);
            ++num_colors;
        }
    }
    const bool has_depth{key.depth_format != PixelFormat::Invalid};
    VkAttachmentReference depth_reference{};
    if (key.depth_format != PixelFormat::Invalid) {
        depth_reference = VkAttachmentReference{
            .attachment = num_colors,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        descriptions.push_back(AttachmentDescription(*device, key.depth_format, key.samples,
                                                     key.tbdr_will_clear, key.tbdr_discard_after));
    }
    VkSubpassDescriptionFlags subpass_flags = 0;
    if (key.qcom_shader_resolve) {
        // VK_QCOM_render_pass_shader_resolve: enables custom shader resolve in fragment shader
        // This must be the last subpass in the dependency chain
        subpass_flags |= 0x00000004; // VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM
    }
    
    const VkSubpassDescription subpass{
        .flags = subpass_flags,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = num_attachments,
        .pColorAttachments = references.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = has_depth ? &depth_reference : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    const VkSubpassDependency dependency{
            .srcSubpass = 0,  // Current subpass
            .dstSubpass = 0,  // Same subpass (self-dependency)
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    };
    pair->second = device->GetLogical().CreateRenderPass({
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<u32>(descriptions.size()),
        .pAttachments = descriptions.empty() ? nullptr : descriptions.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    });
    return *pair->second;
}

} // namespace Vulkan
