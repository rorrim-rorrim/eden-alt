// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/core.h"
#include "video_core/dma_pusher.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/guest_memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/texture_cache/util.h"

namespace {

/// Constexpr check if method should execute immediately (replaces 8KB execution_mask bitset).
constexpr bool IsMethodExecutable(u32 method, Tegra::Engines::EngineTypes engine_type) {
    constexpr u32 MacroRegistersStart = 0xE00;
    if (method >= MacroRegistersStart) {
        return true;
    }

    using Tegra::Engines::EngineTypes;
    switch (engine_type) {
    case EngineTypes::Maxwell3D:
        switch (method) {
        case 0x0110: case 0x0114: case 0x0115: case 0x0117: case 0x0124:
        case 0x01B0: case 0x01B4: case 0x02C8:
        case 0x0D74: case 0x0D75: case 0x0DE0: case 0x0F74: case 0x0F7C:
        case 0x108B: case 0x1214: case 0x1218: case 0x1301:
        case 0x1530: case 0x1552: case 0x15E8: case 0x15ED:
        case 0x1614: case 0x1615: case 0x1970: case 0x19D0: case 0x1B03:
        case 0x17CD: case 0x17CE: case 0x17E4: case 0x17E8: case 0x17EC:
        case 0x17F0: case 0x17F4: case 0x17F8:
        case 0x2304:
        case 0x2384: case 0x2385: case 0x2386: case 0x2387:
        case 0x2388: case 0x2389: case 0x238A: case 0x238B:
        case 0x238C: case 0x238D: case 0x238E: case 0x238F:
        case 0x2390: case 0x2391: case 0x2392: case 0x2393:
        case 0x2404: case 0x240C: case 0x2414: case 0x241C: case 0x2424:
            return true;
        default:
            return false;
        }
    case EngineTypes::Fermi2D:
        return method == 0x22F;
    case EngineTypes::KeplerCompute:
        return method == 0x1B || method == 0x2B;
    case EngineTypes::KeplerMemory:
        return method == 0x1B;
    case EngineTypes::MaxwellDMA:
        return method == 0xC0;
    default:
        return false;
    }
}

} // anonymous namespace

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Tegra {

constexpr u32 MacroRegistersStart = 0xE00;
[[maybe_unused]] constexpr u32 ComputeInline = 0x6D;

DmaPusher::DmaPusher(Core::System& system_, GPU& gpu_, MemoryManager& memory_manager_,
                     Control::ChannelState& channel_state_)
    : gpu{gpu_}, system{system_}, memory_manager{memory_manager_}, puller{gpu_, memory_manager_,
                                                                          *this, channel_state_}, signal_sync{false}, synced{false} {}

DmaPusher::~DmaPusher() = default;

void DmaPusher::DispatchCalls() {

    dma_pushbuffer_subindex = 0;

    dma_state.is_last_call = true;

    while (system.IsPoweredOn()) {
        if (!Step()) {
            break;
        }
    }
    gpu.FlushCommands();
    gpu.OnCommandListEnd();
}

bool DmaPusher::Step() {
    if (!ib_enable || dma_pushbuffer.empty()) {
        return false;
    }

    CommandList& command_list = dma_pushbuffer.front();

    const size_t prefetch_size = command_list.prefetch_command_list.size();
    const size_t command_list_size = command_list.command_lists.size();

    if (prefetch_size == 0 && command_list_size == 0) {
        dma_pushbuffer.pop();
        dma_pushbuffer_subindex = 0;
        return true;
    }

    if (prefetch_size > 0) {
        ProcessCommands(command_list.prefetch_command_list);
        dma_pushbuffer.pop();
        return true;
    }

    auto& current_command = command_list.command_lists[dma_pushbuffer_subindex];
    const CommandListHeader& header = current_command;
    dma_state.dma_get = header.addr;

    if (signal_sync && !synced) {
        std::unique_lock lk(sync_mutex);
        sync_cv.wait(lk, [this]() { return synced; });
        signal_sync = false;
        synced = false;
    }

    if (header.size > 0 && dma_state.method >= MacroRegistersStart && subchannels[dma_state.subchannel]) {
        subchannels[dma_state.subchannel]->current_dirty = memory_manager.IsMemoryDirty(dma_state.dma_get, header.size * sizeof(u32));
    }

    if (header.size > 0) {
        if (Settings::IsDMALevelDefault() ? (Settings::IsGPULevelMedium() || Settings::IsGPULevelHigh()) : Settings::IsDMALevelSafe()) {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::SafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            ProcessCommands(headers);
        } else {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::UnsafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            ProcessCommands(headers);
        }
    }

    if (++dma_pushbuffer_subindex >= command_list_size) {
        dma_pushbuffer.pop();
        dma_pushbuffer_subindex = 0;
    } else {
        signal_sync = command_list.command_lists[dma_pushbuffer_subindex].sync && Settings::values.sync_memory_operations.GetValue();
    }

    if (signal_sync) {
        rasterizer->SignalFence([this]() {
            std::scoped_lock lk(sync_mutex);
            synced = true;
            sync_cv.notify_all();
        });
    }

    return true;
}

void DmaPusher::ProcessCommands(std::span<const CommandHeader> commands) {
    for (size_t index = 0; index < commands.size();) {
        // Data word of methods command
        if (dma_state.method_count && dma_state.non_incrementing) {
            auto const& command_header = commands[index]; //must ref (MUltiMethod re)
            dma_state.dma_word_offset = u32(index * sizeof(u32));
            const u32 max_write = u32(std::min<std::size_t>(index + dma_state.method_count, commands.size()) - index);
            CallMultiMethod(&command_header.argument, max_write);
            dma_state.method_count -= max_write;
            dma_state.is_last_call = true;
            index += max_write;
        } else if (dma_state.method_count) {
            auto const command_header = commands[index]; //can copy
            dma_state.dma_word_offset = u32(index * sizeof(u32));
            dma_state.is_last_call = dma_state.method_count <= 1;
            CallMethod(command_header.argument);
            dma_state.method += !dma_state.non_incrementing ? 1 : 0;
            dma_state.non_incrementing |= dma_increment_once;
            dma_state.method_count--;
            index++;
        } else {
            auto const command_header = commands[index]; //can copy
            // No command active - this is the first word of a new one
            switch (command_header.mode) {
            case SubmissionMode::Increasing:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = false;
                break;
            case SubmissionMode::NonIncreasing:
                SetState(command_header);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::Inline:
                dma_state.method = command_header.method;
                dma_state.subchannel = command_header.subchannel;
                dma_state.dma_word_offset = u64(-s64(dma_state.dma_get)); // negate to set address as 0
                CallMethod(command_header.arg_count);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::IncreaseOnce:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = true;
                break;
            default:
                break;
            }
            index++;
        }
    }
}

void DmaPusher::SetState(const CommandHeader& command_header) {
    dma_state.method = command_header.method;
    dma_state.subchannel = command_header.subchannel;
    dma_state.method_count = command_header.method_count;
}

void DmaPusher::CallMethod(u32 argument) const {
    if (dma_state.method < non_puller_methods) {
        puller.CallPullerMethod(Engines::Puller::MethodCall{
            dma_state.method,
            argument,
            dma_state.subchannel,
            dma_state.method_count,
        });
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (!subchannel->execution_mask[dma_state.method]) {
            subchannel->method_sink.emplace_back(dma_state.method, argument);
        } else {
            subchannel->ConsumeSink();
            subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
            subchannel->CallMethod(dma_state.method, argument, dma_state.is_last_call);
        }
    }
}

void DmaPusher::CallMultiMethod(const u32* base_start, u32 num_methods) const {
    if (dma_state.method < non_puller_methods) {
        puller.CallMultiMethod(dma_state.method, dma_state.subchannel, base_start, num_methods, dma_state.method_count);
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        subchannel->ConsumeSink();
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        subchannel->CallMultiMethod(dma_state.method, base_start, num_methods, dma_state.method_count);
    }
}

void DmaPusher::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
    puller.BindRasterizer(rasterizer);
}

} // namespace Tegra
