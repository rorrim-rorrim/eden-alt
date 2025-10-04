// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "video_core/host1x/host1x.h"
#include "video_core/host1x/nvdec.h"
#include "video_core/host1x/vic.h"

namespace Tegra::Host1x {

Host1x::Host1x(Core::System& system_)
    : system{system_}, syncpoint_manager{},
      memory_manager(system.DeviceMemory()), gmmu_manager{system, memory_manager, 32, 0, 12},
      allocator{std::make_unique<Common::FlatAllocator<u32, 0, 32>>(1 << 12)} {}

Host1x::~Host1x() = default;

void Host1x::StartDevice(s32 fd, ChannelType type, u32 syncpt) {
    switch (type) {
    case ChannelType::NvDec:
        std::call_once(nvdec_first_init, []() {std::this_thread::sleep_for(std::chrono::milliseconds{500});}); // HACK: For Astroneer
        devices[fd] = std::make_unique<Tegra::Host1x::Nvdec>(*this, fd, syncpt, frame_queue);
        break;
    case ChannelType::VIC:
        std::call_once(vic_first_init, []() {std::this_thread::sleep_for(std::chrono::milliseconds{500});}); // HACK: For Astroneer
        devices[fd] = std::make_unique<Tegra::Host1x::Vic>(*this, fd, syncpt, frame_queue);
        break;
    default:
        LOG_ERROR(HW_GPU, "Unimplemented host1x device {}", static_cast<u32>(type));
        break;
    }
}

void Host1x::StopDevice(s32 fd, ChannelType type) {
    devices.erase(fd);
}

} // namespace Tegra::Host1x
