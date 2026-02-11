// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/core.h"
#include "video_core/dma_pusher.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/gpu.h"
#include "video_core/guest_memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/texture_cache/util.h"

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
        const bool use_safe_read = Settings::IsDMALevelDefault()
                                       ? (Settings::IsGPULevelMedium() || Settings::IsGPULevelHigh())
                                       : Settings::IsDMALevelSafe();
        constexpr u32 bulk_count = 32;
        const size_t total_size = static_cast<size_t>(header.size);
        const size_t total_bytes = total_size * sizeof(CommandHeader);
        if (use_safe_read) {
            memory_manager.FlushRegion(dma_state.dma_get, total_bytes);
        }

        const u8* direct_span = memory_manager.GetSpan(dma_state.dma_get, total_bytes);
        if (direct_span) {
            const auto* headers = reinterpret_cast<const CommandHeader*>(direct_span);
            for (u32 offset = 0; offset < header.size; offset += bulk_count) {
                const u32 count = (std::min)(bulk_count, header.size - offset);
                ProcessCommands(std::span<const CommandHeader>(headers + offset, count),
                                static_cast<u64>(offset) * sizeof(CommandHeader));
            }
        } else {
            for (u32 offset = 0; offset < header.size; offset += bulk_count) {
                const u32 count = (std::min)(bulk_count, header.size - offset);
                command_headers.resize_destructive(count);
                const GPUVAddr gpu_addr =
                    dma_state.dma_get + static_cast<GPUVAddr>(offset) * sizeof(CommandHeader);
                if (use_safe_read) {
                    memory_manager.ReadBlock(gpu_addr, command_headers.data(),
                                             count * sizeof(CommandHeader));
                } else {
                    memory_manager.ReadBlockUnsafe(gpu_addr, command_headers.data(),
                                                   count * sizeof(CommandHeader));
                }
                ProcessCommands(
                    std::span<const CommandHeader>(command_headers.data(), count),
                    static_cast<u64>(offset) * sizeof(CommandHeader));
            }
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

void DmaPusher::ProcessCommands(std::span<const CommandHeader> commands, u64 base_word_offset) {
    u32 method = dma_state.method;
    u32 method_count = dma_state.method_count;
    u32 subchannel = dma_state.subchannel;
    u64 dma_word_offset = dma_state.dma_word_offset;
    bool non_incrementing = dma_state.non_incrementing;
    bool is_last_call = dma_state.is_last_call;
    const u64 dma_get = dma_state.dma_get;

    const auto sync_state = [&]() {
        dma_state.method = method;
        dma_state.method_count = method_count;
        dma_state.subchannel = subchannel;
        dma_state.dma_word_offset = dma_word_offset;
        dma_state.non_incrementing = non_incrementing;
        dma_state.is_last_call = is_last_call;
    };

    for (size_t index = 0; index < commands.size();) {
        // Data word of methods command
        if (method_count && non_incrementing) {
            auto const& command_header = commands[index]; //must ref (MUltiMethod re)
            dma_word_offset = base_word_offset + u32(index * sizeof(u32));
            const u32 max_write =
                u32(std::min<std::size_t>(index + method_count, commands.size()) - index);
            sync_state();
            CallMultiMethod(&command_header.argument, max_write);
            method_count -= max_write;
            is_last_call = true;
            index += max_write;
        } else if (method_count) {
            auto const command_header = commands[index]; //can copy
            dma_word_offset = base_word_offset + u32(index * sizeof(u32));
            is_last_call = method_count <= 1;
            sync_state();
            CallMethod(command_header.argument);
            method += !non_incrementing ? 1 : 0;
            non_incrementing |= dma_increment_once;
            method_count--;
            index++;
        } else {
            auto const command_header = commands[index]; //can copy
            // No command active - this is the first word of a new one
            switch (command_header.mode) {
            case SubmissionMode::Increasing:
                method = command_header.method;
                subchannel = command_header.subchannel;
                method_count = command_header.method_count;
                non_incrementing = false;
                dma_increment_once = false;
                break;
            case SubmissionMode::NonIncreasing:
                method = command_header.method;
                subchannel = command_header.subchannel;
                method_count = command_header.method_count;
                non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::Inline:
                method = command_header.method;
                subchannel = command_header.subchannel;
                dma_word_offset = u64(-s64(dma_get)); // negate to set address as 0
                sync_state();
                CallMethod(command_header.arg_count);
                non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::IncreaseOnce:
                method = command_header.method;
                subchannel = command_header.subchannel;
                method_count = command_header.method_count;
                non_incrementing = false;
                dma_increment_once = true;
                break;
            default:
                break;
            }
            index++;
        }
    }

    sync_state();
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
        if (!subchannel) {
            return;
        }
        const u32 method = dma_state.method;
        const u32 word_index = method >> 6;
        const u64 bit_mask = 1ULL << (method & 63);
        const bool is_executable =
            word_index < subchannel->execution_mask_words.size() &&
            (subchannel->execution_mask_words[word_index] & bit_mask) != 0;
        if (!is_executable) {
            subchannel->method_sink.emplace_back(method, argument);
        } else {
            subchannel->ConsumeSink();
            subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
            switch (subchannel_type[dma_state.subchannel]) {
            case Engines::EngineTypes::Maxwell3D:
                static_cast<Engines::Maxwell3D*>(subchannel)->CallMethod(method, argument,
                                                                         dma_state.is_last_call);
                break;
            case Engines::EngineTypes::KeplerCompute:
                static_cast<Engines::KeplerCompute*>(subchannel)->CallMethod(
                    method, argument, dma_state.is_last_call);
                break;
            case Engines::EngineTypes::Fermi2D:
                static_cast<Engines::Fermi2D*>(subchannel)->CallMethod(method, argument,
                                                                       dma_state.is_last_call);
                break;
            case Engines::EngineTypes::MaxwellDMA:
                static_cast<Engines::MaxwellDMA*>(subchannel)->CallMethod(method, argument,
                                                                          dma_state.is_last_call);
                break;
            case Engines::EngineTypes::KeplerMemory:
                static_cast<Engines::KeplerMemory*>(subchannel)->CallMethod(
                    method, argument, dma_state.is_last_call);
                break;
            default:
                subchannel->CallMethod(method, argument, dma_state.is_last_call);
                break;
            }
        }
    }
}

void DmaPusher::CallMultiMethod(const u32* base_start, u32 num_methods) const {
    if (dma_state.method < non_puller_methods) {
        puller.CallMultiMethod(dma_state.method, dma_state.subchannel, base_start, num_methods, dma_state.method_count);
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (!subchannel) {
            return;
        }
        subchannel->ConsumeSink();
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        switch (subchannel_type[dma_state.subchannel]) {
        case Engines::EngineTypes::Maxwell3D:
            static_cast<Engines::Maxwell3D*>(subchannel)->CallMultiMethod(
                dma_state.method, base_start, num_methods, dma_state.method_count);
            break;
        case Engines::EngineTypes::KeplerCompute:
            static_cast<Engines::KeplerCompute*>(subchannel)->CallMultiMethod(
                dma_state.method, base_start, num_methods, dma_state.method_count);
            break;
        case Engines::EngineTypes::Fermi2D:
            static_cast<Engines::Fermi2D*>(subchannel)->CallMultiMethod(
                dma_state.method, base_start, num_methods, dma_state.method_count);
            break;
        case Engines::EngineTypes::MaxwellDMA:
            static_cast<Engines::MaxwellDMA*>(subchannel)->CallMultiMethod(
                dma_state.method, base_start, num_methods, dma_state.method_count);
            break;
        case Engines::EngineTypes::KeplerMemory:
            static_cast<Engines::KeplerMemory*>(subchannel)->CallMultiMethod(
                dma_state.method, base_start, num_methods, dma_state.method_count);
            break;
        default:
            subchannel->CallMultiMethod(dma_state.method, base_start, num_methods,
                                        dma_state.method_count);
            break;
        }
    }
}

void DmaPusher::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
    puller.BindRasterizer(rasterizer);
}

} // namespace Tegra
