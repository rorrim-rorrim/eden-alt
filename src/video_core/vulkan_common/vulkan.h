// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__HAIKU__)
#define VK_USE_PLATFORM_XCB_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include <vulkan/vulkan.h>

// Define maintenance 7-9 extension names (not yet in official Vulkan headers)
#ifndef VK_KHR_MAINTENANCE_7_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_7_EXTENSION_NAME "VK_KHR_maintenance7"
#endif
#ifndef VK_KHR_MAINTENANCE_8_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_8_EXTENSION_NAME "VK_KHR_maintenance8"
#endif
#ifndef VK_KHR_MAINTENANCE_9_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_9_EXTENSION_NAME "VK_KHR_maintenance9"
#endif
#ifndef VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME
#define VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME "VK_QCOM_tile_properties"
#define VK_QCOM_TILE_PROPERTIES_SPEC_VERSION 1
#endif

#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM ((VkStructureType)1000484000)
#define VK_STRUCTURE_TYPE_TILE_PROPERTIES_QCOM ((VkStructureType)1000484001)

typedef struct VkPhysicalDeviceTilePropertiesFeaturesQCOM {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           tileProperties;
} VkPhysicalDeviceTilePropertiesFeaturesQCOM;

typedef struct VkTilePropertiesQCOM {
    VkStructureType    sType;
    void*              pNext;
    VkExtent3D         tileSize;
    VkExtent2D         apronSize;
    VkOffset2D         origin;
} VkTilePropertiesQCOM;
#endif
#ifndef VK_QCOM_RENDER_PASS_TRANSFORM_EXTENSION_NAME
#define VK_QCOM_RENDER_PASS_TRANSFORM_EXTENSION_NAME "VK_QCOM_render_pass_transform"
#define VK_QCOM_RENDER_PASS_TRANSFORM_SPEC_VERSION 4
#endif
#ifndef VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM
#define VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM ((VkStructureType)1000282000)
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM ((VkStructureType)1000282001)

typedef struct VkRenderPassTransformBeginInfoQCOM {
    VkStructureType                  sType;
    void*                            pNext;
    VkSurfaceTransformFlagBitsKHR    transform;
} VkRenderPassTransformBeginInfoQCOM;

typedef struct VkCommandBufferInheritanceRenderPassTransformInfoQCOM {
    VkStructureType                  sType;
    void*                            pNext;
    VkSurfaceTransformFlagBitsKHR    transform;
    VkRect2D                         renderArea;
} VkCommandBufferInheritanceRenderPassTransformInfoQCOM;
#endif
#ifndef VK_QCOM_RENDER_PASS_STORE_OPS_EXTENSION_NAME
#define VK_QCOM_RENDER_PASS_STORE_OPS_EXTENSION_NAME "VK_QCOM_render_pass_store_ops"
#define VK_QCOM_RENDER_PASS_STORE_OPS_SPEC_VERSION 2
#endif
#ifndef VK_ATTACHMENT_STORE_OP_NONE_QCOM
#define VK_ATTACHMENT_STORE_OP_NONE_QCOM ((VkAttachmentStoreOp)1000301000)
#endif

// Sanitize macros
#undef CreateEvent
#undef CreateSemaphore
#undef Always
#undef False
#undef None
#undef True
