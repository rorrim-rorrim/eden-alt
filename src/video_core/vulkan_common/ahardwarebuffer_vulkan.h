// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __ANDROID__

#include <vulkan/vulkan.h>
#include <android/hardware_buffer.h>
#include "video_core/vulkan_common/vulkan_wrapper.h"

// Import an AHardwareBuffer into Vulkan as a VkImage with bound memory.
// On success returns true and fills outImage/outMemory/outFormat. Caller is responsible
// for destroying the VkImage and freeing the VkDeviceMemory when done. The AHardwareBuffer
// reference is not released by this function; caller should call AHardwareBuffer_release when
// done with the AHardwareBuffer.

bool ImportAHardwareBufferToVk(VkDevice device, VkPhysicalDevice physicalDevice, AHardwareBuffer* ahb,
                               VkImage* outImage, VkDeviceMemory* outMemory, VkFormat* outFormat);

// Overload that uses the dispatch loader for extension calls.
bool ImportAHardwareBufferToVk(const Vulkan::vk::DeviceDispatch& dld,
                               VkDevice device, VkPhysicalDevice physicalDevice, AHardwareBuffer* ahb,
                               VkImage* outImage, VkDeviceMemory* outMemory, VkFormat* outFormat);

#endif // __ANDROID__
