// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>
#include <vector>

#include "qt_common/qt_common.h"

#include "common/dynamic_library.h"
#include "common/logging/log.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "vulkan/vulkan_core.h"
#include "yuzu/vk_device_info.h"

class QWindow;

namespace VkDeviceInfo {
Record::Record(std::string_view name_, const std::vector<VkPresentModeKHR>& vsync_modes_,
               bool has_broken_compute_)
    : name{name_}, vsync_support{vsync_modes_}, has_broken_compute{has_broken_compute_} {}

Record::~Record() = default;

void PopulateRecords(std::vector<Record>& records, QWindow* window) try {
    using namespace Vulkan;

    // Create a test window with a Vulkan surface type for checking present modes.
    QWindow test_window(window);
    test_window.setSurfaceType(QWindow::VulkanSurface);
    test_window.create();
    auto wsi = QtCommon::GetWindowSystemInfo(&test_window);

    vk::InstanceDispatch dld;
    const auto library = OpenLibrary();
    const vk::Instance instance = CreateInstance(*library, dld, VK_API_VERSION_1_1, wsi.type);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();
    vk::SurfaceKHR surface = CreateSurface(instance, wsi);

    records.clear();
    records.reserve(physical_devices.size());
    for (const VkPhysicalDevice device : physical_devices) {
        const auto physical_device = vk::PhysicalDevice(device, dld);
        std::string name = physical_device.GetProperties().deviceName;

        const std::vector<VkPresentModeKHR> present_modes =
            physical_device.GetSurfacePresentModesKHR(*surface);

        VkPhysicalDeviceDriverProperties driver_properties{};
        driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        driver_properties.pNext = nullptr;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        properties.pNext = &driver_properties;
        dld.vkGetPhysicalDeviceProperties2(physical_device, &properties);

        const auto driverID = driver_properties.driverID;

        bool has_broken_compute{Vulkan::Device::CheckBrokenCompute(
            driverID, properties.properties.driverVersion)};

        std::string driver_string{};

        // TODO: This can be moved to a utility function but I'm lazy.
        switch (driverID) {
        case VK_DRIVER_ID_MESA_DOZEN:
            driver_string = "Dozen";
            break;
        case VK_DRIVER_ID_MOLTENVK:
            driver_string = "MoltenVK";
            break;
        case VK_DRIVER_ID_AMD_OPEN_SOURCE:
            driver_string = "AMDVLK";
            break;
        case VK_DRIVER_ID_GOOGLE_SWIFTSHADER:
            driver_string = "SwiftShader";
            break;
        case VK_DRIVER_ID_MESA_LLVMPIPE:
            driver_string = "llvmpipe";
            break;
        case VK_DRIVER_ID_SAMSUNG_PROPRIETARY:
            driver_string = "Samsung";
            break;
        case VK_DRIVER_ID_COREAVI_PROPRIETARY:
            driver_string = "Coreavi";
            break;
        case VK_DRIVER_ID_JUICE_PROPRIETARY:
        case VK_DRIVER_ID_MESA_VENUS:
            driver_string = "Virtualized";
            break;
        case VK_DRIVER_ID_VERISILICON_PROPRIETARY:
            driver_string = "Verisilicon";
            break;
        case VK_DRIVER_ID_AMD_PROPRIETARY:
            driver_string = "AMD";
            break;
        case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
            driver_string = "Nvidia";
            break;
        case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
            driver_string = "Intel";
            break;
        case VK_DRIVER_ID_IMAGINATION_PROPRIETARY:
            driver_string = "Imagination";
            break;
        case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:
            driver_string = "Qualcomm";
            break;
        case VK_DRIVER_ID_ARM_PROPRIETARY:
            driver_string = "ARM";
            break;
        // ?
        case VK_DRIVER_ID_GGP_PROPRIETARY:
            driver_string = "GGP";
            break;
        case VK_DRIVER_ID_BROADCOM_PROPRIETARY:
            driver_string = "Broadcom";
            break;
        case VK_DRIVER_ID_MESA_NVK:
            driver_string = "Nouveau";
            break;
        case VK_DRIVER_ID_MESA_TURNIP:
            driver_string = "Turnip";
            break;
        case VK_DRIVER_ID_MESA_PANVK:
            driver_string = "Panfrost";
            break;
        case VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA:
            driver_string = "PowerVR MESA";
            break;
        case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
            driver_string = "ANV";
            break;
        case VK_DRIVER_ID_MESA_RADV:
            driver_string = "RADV";
            break;
        case VK_DRIVER_ID_MESA_V3DV:
            driver_string = "V3DV";
            break;
        case VK_DRIVER_ID_MESA_HONEYKRISP:
            driver_string = "HoneyKrisp";
            break;
        case VK_DRIVER_ID_MESA_KOSMICKRISP:
            driver_string = "KosmicKrisp";
            break;
        case VK_DRIVER_ID_MAX_ENUM:
        case VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN:
        default:
            break;
        }

        if (!driver_string.empty()) {
            name = fmt::format("{} ({})", name, driver_string);
        }

        records.push_back(VkDeviceInfo::Record(name, present_modes, has_broken_compute));
    }
} catch (const Vulkan::vk::Exception& exception) {
    LOG_ERROR(Frontend, "Failed to enumerate devices with error: {}", exception.what());
}
} // namespace VkDeviceInfo
