// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef __ANDROID__

#include "ahardwarebuffer_vulkan.h"
#include <vulkan/vulkan.h>
#include <vulkan/vk_android_external_memory_android_hardware_buffer.h>
#include <android/hardware_buffer.h>
#include <cstring>

namespace {

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                                    VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    // Fallback: return first matching bit
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if (typeBits & (1u << i)) return i;
    }
    return 0;
}

} // anonymous

static bool ImportAHardwareBufferInternal(PFN_vkGetAndroidHardwareBufferPropertiesANDROID get_props,
                                          VkDevice device, VkPhysicalDevice physicalDevice, AHardwareBuffer* ahb,
                                          VkImage* outImage, VkDeviceMemory* outMemory, VkFormat* outFormat) {
    if (!device || !physicalDevice || !ahb || !outImage || !outMemory || !get_props) return false;

    VkAndroidHardwareBufferPropertiesANDROID ahb_props{};
    ahb_props.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    VkResult r = get_props(device, ahb, &ahb_props);
    if (r != VK_SUCCESS) return false;

    // Determine VkFormat to use
    VkFormat fmt = static_cast<VkFormat>(ahb_props.format);
    if (outFormat) *outFormat = fmt;

    // Create image with external memory
    VkExternalMemoryImageCreateInfo external_info{};
    external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = &external_info;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = fmt;
    image_info.extent.width = ahb_props.allocationSize ? 0 : 0; // placeholder; we'll query via AHardwareBuffer_describe

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);
    image_info.extent.width = desc.width;
    image_info.extent.height = desc.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Use typical usage bits; adjust as needed by consumer
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    r = vkCreateImage(device, &image_info, nullptr, outImage);
    if (r != VK_SUCCESS) return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, *outImage, &memReq);

    // Prepare import struct
    VkImportAndroidHardwareBufferInfoANDROID import_info{};
    import_info.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    import_info.buffer = ahb;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &import_info;
    alloc_info.allocationSize = memReq.size;
    alloc_info.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    r = vkAllocateMemory(device, &alloc_info, nullptr, outMemory);
    if (r != VK_SUCCESS) {
        vkDestroyImage(device, *outImage, nullptr);
        return false;
    }

    r = vkBindImageMemory(device, *outImage, *outMemory, 0);
    if (r != VK_SUCCESS) {
        vkDestroyImage(device, *outImage, nullptr);
        vkFreeMemory(device, *outMemory, nullptr);
        return false;
    }

    return true;
}

bool ImportAHardwareBufferToVk(VkDevice device, VkPhysicalDevice physicalDevice, AHardwareBuffer* ahb,
                               VkImage* outImage, VkDeviceMemory* outMemory, VkFormat* outFormat) {
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID fpGetAHBProps =
        (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)vkGetDeviceProcAddr(device, "vkGetAndroidHardwareBufferPropertiesANDROID");
    return ImportAHardwareBufferInternal(fpGetAHBProps, device, physicalDevice, ahb, outImage, outMemory, outFormat);
}

bool ImportAHardwareBufferToVk(const Vulkan::vk::DeviceDispatch& dld,
                               VkDevice device, VkPhysicalDevice physicalDevice, AHardwareBuffer* ahb,
                               VkImage* outImage, VkDeviceMemory* outMemory, VkFormat* outFormat) {
    return ImportAHardwareBufferInternal(dld.vkGetAndroidHardwareBufferPropertiesANDROID, device, physicalDevice, ahb,
                                         outImage, outMemory, outFormat);
}

#endif // __ANDROID__
