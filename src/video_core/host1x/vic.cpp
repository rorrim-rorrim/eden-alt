// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <tuple>
#include <stdint.h>

extern "C" {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <libswscale/swscale.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

#include "common/alignment.h"
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "common/polyfill_thread.h"
#include "common/settings.h"

#include "video_core/engines/maxwell_3d.h"
#include "video_core/guest_memory.h"
#include "video_core/host1x/host1x.h"
#include "video_core/host1x/nvdec.h"
#include "video_core/host1x/vic.h"
#include "video_core/memory_manager.h"
#include "video_core/textures/decoders.h"

#if defined(ARCHITECTURE_x86_64)
#include "common/x64/cpu_detect.h"
#endif

namespace Tegra::Host1x {
namespace {

void SwizzleSurface(std::span<u8> output, u32 out_stride, std::span<const u8> input, u32 in_stride, u32 height) noexcept {
    //// Taken from https://github.com/averne/FFmpeg/blob/nvtegra/libavutil/hwcontext_nvtegra.c#L949
    /// Can only handle block height == 1.
    u32 const x_mask = 0xFFFFFFD2u, y_mask = 0x2Cu;
    u32 offs_x = 0, offs_y = 0;
    for (size_t y = 0; y < height; y += 2) {
        auto dst_line = output.data() + offs_y * 16;
        auto const src_line = input.data() + u32(y) * (in_stride / 16) * 16;

        auto offs_line = offs_x;
        for (u32 x = 0; x < in_stride; x += 16) {
            std::memcpy(&dst_line[offs_line * 16], &src_line[x], 16);
            std::memcpy(&dst_line[offs_line * 16 + 16], &src_line[x + in_stride], 16);
            offs_line = (offs_line - x_mask) & x_mask;
        }
        offs_y = (offs_y - y_mask) & y_mask;
        offs_x += offs_y ? 0 : out_stride; // Wrap into next tile row
    }
}

} // namespace

Vic::Vic(Host1x& host1x_, s32 id_, u32 syncpt, FrameQueue& frame_queue_) noexcept :
    CDmaPusher{host1x_, id_}
    , id{id_}
    , syncpoint{syncpt}
    , frame_queue{frame_queue_}
{
    LOG_INFO(HW_GPU, "Created vic {}", id);
}

Vic::~Vic() noexcept {
    LOG_INFO(HW_GPU, "Destroying vic {}", id);
    frame_queue.Close(id);
}

void Vic::ProcessMethod(u32 method, u32 arg) noexcept {
    LOG_TRACE(HW_GPU, "Vic {} method {:#X}", id, u32(method));
    regs.reg_array[method] = arg;
    switch (Method(method * sizeof(u32))) {
    case Method::Execute:
        Execute();
        break;
    default:
        break;
    }
}

void Vic::Execute() noexcept {
    ConfigStruct config{};
    memory_manager.ReadBlock(regs.config_struct_offset.Address(), &config, sizeof(ConfigStruct));

    auto output_width = config.output_surface_config.out_surface_width + 1;
    auto output_height = config.output_surface_config.out_surface_height + 1;
    output_surface.resize(output_width * output_height);

    if (Settings::values.nvdec_emulation.GetValue() != Settings::NvdecEmulation::Off) {
        for (size_t i = 0; i < config.slot_structs.size(); i++) {
            if (auto& slot_config = config.slot_structs[i]; slot_config.config.slot_enable) {
                auto const luma_offset = regs.surfaces[i][SurfaceIndex::Current].luma.Address();
                if (nvdec_id == -1)
                    nvdec_id = frame_queue.VicFindNvdecFdFromOffset(luma_offset);
                if (auto frame = frame_queue.GetFrame(nvdec_id, luma_offset); frame.get()) {
                    switch (frame->GetPixelFormat()) {
                    case AV_PIX_FMT_YUV420P:
                        ReadY8__V8U8_N420(slot_config, regs.surfaces[i], std::move(frame), true);
                        break;
                    case AV_PIX_FMT_NV12:
                        ReadY8__V8U8_N420(slot_config, regs.surfaces[i], std::move(frame), false);
                        break;
                    default:
                        UNIMPLEMENTED_MSG("Unimplemented slot pixel format {}", u32(slot_config.surface_config.slot_pixel_format.Value()));
                        break;
                    }
                    Blend(config, slot_config, config.output_surface_config.out_pixel_format);
                } else {
                    LOG_ERROR(HW_GPU, "Vic {} failed to get frame with offset {:#X}", id, luma_offset);
                }
            }
        }
    } else {
        // Fill the frame with black, as otherwise they can have random data and be very glitchy.
        std::fill(output_surface.begin(), output_surface.end(), Pixel{});
    }

    switch (config.output_surface_config.out_pixel_format) {
    case VideoPixelFormat::A8B8G8R8:
    case VideoPixelFormat::X8B8G8R8:
    case VideoPixelFormat::A8R8G8B8:
        WriteABGR(config.output_surface_config);
        break;
    case VideoPixelFormat::Y8__V8U8_N420:
        WriteY8__V8U8_N420(config.output_surface_config);
        break;
    default:
        UNIMPLEMENTED_MSG("Unknown video pixel format {}", config.output_surface_config.out_pixel_format.Value());
        break;
    }
}

void Vic::ReadProgressiveY8__V8U8_N420(const SlotStruct& slot, std::span<const PlaneOffsets> offsets, std::shared_ptr<const FFmpeg::Frame> frame, bool planar, bool interlaced) noexcept {
    auto const out_luma_width = slot.surface_config.slot_surface_width + 1;
    auto const out_luma_height = (slot.surface_config.slot_surface_height + 1) * (interlaced ? 2 : 1);
    auto const out_luma_stride = out_luma_width;

    const auto in_luma_width = (std::min)(frame->GetWidth(), s32(out_luma_width));
    const auto in_luma_height = (std::min)(frame->GetHeight(), s32(out_luma_height));
    const auto in_luma_stride = frame->GetStride(0);

    const auto in_chroma_stride{frame->GetStride(1)};

    const auto* luma_buffer{frame->GetPlane(0)};
    const auto* chroma_u_buffer{frame->GetPlane(1)};
    const auto* chroma_v_buffer{frame->GetPlane(2)};

    LOG_TRACE(HW_GPU,
        "Reading frame"
        "\ninput luma {}x{} stride {} chroma {}x{} stride {}\n"
        "output luma {}x{} stride {} chroma {}x{} stride {}",
        in_luma_width, in_luma_height, in_luma_stride, in_luma_width / 2, in_luma_height / 2,
        in_chroma_stride, out_luma_width, out_luma_height, out_luma_stride, out_luma_width,
        out_luma_height, out_luma_stride);

    slot_surface.resize_destructive(out_luma_width * out_luma_height);
    auto const alpha = u16(slot.config.planar_alpha.Value());
    for (size_t y = 0; y < size_t(in_luma_height); y++) {
        auto const src_luma = y * in_luma_stride;
        auto const src_chroma = (y / 2) * in_chroma_stride;
        auto const dst = y * out_luma_stride;
        for (size_t x = 0; x < size_t(in_luma_width); x++) {
            slot_surface[dst + x].r = u16(luma_buffer[src_luma + x] << 2);
            // Chroma samples are duplicated horizontally and vertically.
            if(planar) {
                slot_surface[dst + x].g = u16(chroma_u_buffer[src_chroma + x / 2] << 2);
                slot_surface[dst + x].b = u16(chroma_v_buffer[src_chroma + x / 2] << 2);
            } else {
                slot_surface[dst + x].g = u16(chroma_u_buffer[src_chroma + (x & ~1) + 0] << 2);
                slot_surface[dst + x].b = u16(chroma_u_buffer[src_chroma + (x & ~1) + 1] << 2);
            }
            slot_surface[dst + x].a = alpha;
        }
    }
}

void Vic::ReadInterlacedY8__V8U8_N420(const SlotStruct& slot, std::span<const PlaneOffsets> offsets, std::shared_ptr<const FFmpeg::Frame> frame, bool planar, bool top_field) noexcept {
    if(!planar) {
        ReadProgressiveY8__V8U8_N420(slot, offsets, std::move(frame), planar, true);
        return;
    }
    const auto out_luma_width{slot.surface_config.slot_surface_width + 1};
    const auto out_luma_height{(slot.surface_config.slot_surface_height + 1) * 2};
    const auto out_luma_stride{out_luma_width};

    slot_surface.resize(out_luma_width * out_luma_height);

    const auto in_luma_width{(std::min)(frame->GetWidth(), s32(out_luma_width))};
    const auto in_luma_height{(std::min)(frame->GetHeight(), s32(out_luma_height))};
    const auto in_luma_stride{frame->GetStride(0)};

    const auto in_chroma_width{(frame->GetWidth() + 1) / 2};
    const auto in_chroma_height{(frame->GetHeight() + 1) / 2};
    const auto in_chroma_stride{frame->GetStride(1)};

    const auto* luma_buffer{frame->GetPlane(0)};
    const auto* chroma_u_buffer{frame->GetPlane(1)};
    const auto* chroma_v_buffer{frame->GetPlane(2)};

    LOG_TRACE(HW_GPU, "Reading frame"
        "\ninput luma {}x{} stride {} chroma {}x{} stride {}\n"
        "output luma {}x{} stride {} chroma {}x{} stride {}",
        in_luma_width, in_luma_height, in_luma_stride, in_chroma_width, in_chroma_height,
        in_chroma_stride, out_luma_width, out_luma_height, out_luma_stride,
        out_luma_width / 2, out_luma_height / 2, out_luma_stride);

    auto DecodeBobField = [&]() {
        const auto alpha = u16(slot.config.planar_alpha.Value());
        for (s32 y = s32(top_field == false); y < in_chroma_height * 2; y += 2) {
            const auto src_luma{y * in_luma_stride};
            const auto src_chroma{(y / 2) * in_chroma_stride};
            const auto dst{y * out_luma_stride};
            for (s32 x = 0; x < in_luma_width; x++) {
                slot_surface[dst + x].r = u16(luma_buffer[src_luma + x] << 2);
                if(planar) {
                    slot_surface[dst + x].g = u16(chroma_u_buffer[src_chroma + x / 2] << 2);
                    slot_surface[dst + x].b = u16(chroma_v_buffer[src_chroma + x / 2] << 2);
                } else {
                    slot_surface[dst + x].g = u16(chroma_u_buffer[src_chroma + (x & ~1) + 0] << 2);
                    slot_surface[dst + x].b = u16(chroma_u_buffer[src_chroma + (x & ~1) + 1] << 2);
                }
                slot_surface[dst + x].a = alpha;
            }
            s32 other_line = (top_field ? y + 1 : y - 1) * out_luma_stride;
            std::memcpy(&slot_surface[other_line], &slot_surface[dst], out_luma_width * sizeof(Pixel));
        }
    };

    switch (slot.config.deinterlace_mode) {
    case DXVAHD_DEINTERLACE_MODE_PRIVATE::WEAVE:
        // Due to the fact that we do not write to memory in nvdec, we cannot use Weave as it
        // relies on the previous frame.
        DecodeBobField();
        break;
    case DXVAHD_DEINTERLACE_MODE_PRIVATE::BOB_FIELD:
        DecodeBobField();
        break;
    case DXVAHD_DEINTERLACE_MODE_PRIVATE::DISI1:
        // Due to the fact that we do not write to memory in nvdec, we cannot use DISI1 as it
        // relies on previous/next frames.
        DecodeBobField();
        break;
    default:
        LOG_ERROR(HW_GPU, "Deinterlace mode {} not implemented!", s32(slot.config.deinterlace_mode.Value()));
        break;
    }
}

void Vic::ReadY8__V8U8_N420(const SlotStruct& slot, std::span<const PlaneOffsets> offsets, std::shared_ptr<const FFmpeg::Frame> frame, bool planar) noexcept {
    switch (slot.config.frame_format) {
    case DXVAHD_FRAME_FORMAT::PROGRESSIVE:
        ReadProgressiveY8__V8U8_N420(slot, offsets, std::move(frame), planar, false);
        break;
    case DXVAHD_FRAME_FORMAT::TOP_FIELD:
        ReadInterlacedY8__V8U8_N420(slot, offsets, std::move(frame), planar, true);
        break;
    case DXVAHD_FRAME_FORMAT::BOTTOM_FIELD:
        ReadInterlacedY8__V8U8_N420(slot, offsets, std::move(frame), planar, false);
        break;
    default:
        LOG_ERROR(HW_GPU, "Unknown deinterlace format {}", s32(slot.config.frame_format.Value()));
        break;
    }
}

void Vic::Blend(const ConfigStruct& config, const SlotStruct& slot, VideoPixelFormat format) noexcept {
    auto const add_one = [](u32 const v) -> u32 {
        return v + u32(v > 0);
    };

    auto source_left = add_one(u32(slot.config.source_rect_left.Value()));
    auto source_right = add_one(u32(slot.config.source_rect_right.Value()));
    auto source_top = add_one(u32(slot.config.source_rect_top.Value()));
    auto source_bottom = add_one(u32(slot.config.source_rect_bottom.Value()));

    auto const dest_left = add_one(u32(slot.config.dest_rect_left.Value()));
    auto const dest_right = add_one(u32(slot.config.dest_rect_right.Value()));
    auto const dest_top = add_one(u32(slot.config.dest_rect_top.Value()));
    auto const dest_bottom = add_one(u32(slot.config.dest_rect_bottom.Value()));

    auto rect_left = add_one(u32(config.output_config.target_rect_left.Value()));
    auto rect_right = add_one(u32(config.output_config.target_rect_right.Value()));
    auto rect_top = add_one(u32(config.output_config.target_rect_top.Value()));
    auto rect_bottom = add_one(u32(config.output_config.target_rect_bottom.Value()));

    rect_left = (std::max)(rect_left, dest_left);
    rect_right = (std::min)(rect_right, dest_right);
    rect_top = (std::max)(rect_top, dest_top);
    rect_bottom = (std::min)(rect_bottom, dest_bottom);

    source_left = (std::max)(source_left, rect_left);
    source_right = (std::min)(source_right, rect_right);
    source_top = (std::max)(source_top, rect_top);
    source_bottom = (std::min)(source_bottom, rect_bottom);

    auto const out_surface_width = config.output_surface_config.out_surface_width + 1;
    auto const out_surface_height = config.output_surface_config.out_surface_height + 1;
    auto const in_surface_width = slot.surface_config.slot_surface_width + 1;

    source_bottom = (std::min)(source_bottom, out_surface_height);
    source_right = (std::min)(source_right, out_surface_width);

    auto const work_width = u32((std::max)(0, s32(source_right) - s32(source_left)));
    auto const work_height = u32((std::max)(0, s32(source_bottom) - s32(source_top)));

    // TODO Alpha blending. No games I've seen use more than a single surface or supply an alpha
    // below max, so it's ignored for now.
    if (slot.color_matrix.matrix_enable) {
        // clang-format off
        // Colour conversion is enabled, this is a 3x4 * 4x1 matrix multiplication, resulting in a 3x1 matrix.
        // | r0c0 r0c1 r0c2 r0c3 |   | R |   | R |
        // | r1c0 r1c1 r1c2 r1c3 | * | G | = | G |
        // | r2c0 r2c1 r2c2 r2c3 |   | B |   | B |
        //                           | 1 |
        auto const shift = s32(slot.color_matrix.matrix_r_shift.Value());

        struct AliasedMatrixType { u64 m[4]; };
        static_assert(sizeof(AliasedMatrixType) == sizeof(slot.color_matrix));
        u64 const mat_mask = (1 << 20) - 1;
        auto const* amt = reinterpret_cast<AliasedMatrixType const*>(&slot.color_matrix);

        constexpr s32 shifts[4] = { 0, 20, 40, 60 };
        s32 mr[4][4];
        for (u32 j = 0; j < 3; ++j)
            for (u32 i = 0; i < 4; ++i)
                mr[j][i] = s64(((amt->m[i] >> shifts[j]) & mat_mask) << (64 - 20)) >> (64 - 20);

        auto const clamp_min = s32(slot.config.soft_clamp_low.Value());
        auto const clamp_max = s32(slot.config.soft_clamp_high.Value());
        for (u32 y = 0; y < work_height; ++y) {
            auto const src = (y + source_top) * in_surface_width + source_left;
            auto const dst = (y + source_top) * out_surface_width + rect_left;
            for (u32 x = 0; x < work_width; ++x) {
                auto const& in_pixel = slot_surface[src + x];
                auto& out_pixel = output_surface[dst + x];
                s32 const mul_values[4] = {
                    in_pixel.r * mr[0][0] + in_pixel.g * mr[1][1] + in_pixel.b * mr[0][2],
                    in_pixel.r * mr[1][0] + in_pixel.g * mr[1][1] + in_pixel.b * mr[1][2],
                    in_pixel.r * mr[2][0] + in_pixel.g * mr[2][1] + in_pixel.b * mr[2][2],
                    s32(in_pixel.a)
                };
                s32 const mul_clamp[4] = {
                    std::clamp(((mul_values[0] >> shift) + mr[0][3]) >> 8, clamp_min, clamp_max),
                    std::clamp(((mul_values[1] >> shift) + mr[1][3]) >> 8, clamp_min, clamp_max),
                    std::clamp(((mul_values[2] >> shift) + mr[2][3]) >> 8, clamp_min, clamp_max),
                    std::clamp(mul_values[3], clamp_min, clamp_max)
                };
                out_pixel = format == VideoPixelFormat::A8R8G8B8
                    ? Pixel(u16(mul_clamp[2]), u16(mul_clamp[1]), u16(mul_clamp[0]), u16(mul_clamp[3]))
                    : Pixel(u16(mul_clamp[0]), u16(mul_clamp[1]), u16(mul_clamp[2]), u16(mul_clamp[3]));
            }
        }
    } else {
        auto const copy_width = (std::min)(source_right - source_left, rect_right - rect_left);
        for (u32 y = 0; y < work_height; y++) {
            auto const dst_line = (y + source_top) * out_surface_width;
            auto const src_line = (y + source_top) * in_surface_width;
            std::memcpy(&output_surface[dst_line + rect_left], &slot_surface[src_line + source_left], copy_width * sizeof(Pixel));
        }
    }
}

void Vic::WriteY8__V8U8_N420(const OutputSurfaceConfig& output_surface_config) noexcept {
    constexpr u32 BytesPerPixel = 1;

    auto surface_width{output_surface_config.out_surface_width + 1};
    auto surface_height{output_surface_config.out_surface_height + 1};
    const auto surface_stride{surface_width};

    const auto out_luma_width = output_surface_config.out_luma_width + 1;
    const auto out_luma_height = output_surface_config.out_luma_height + 1;
    const auto out_luma_stride = Common::AlignUp(out_luma_width * BytesPerPixel, 0x10);
    const auto out_luma_size = out_luma_height * out_luma_stride;

    const auto out_chroma_width = output_surface_config.out_chroma_width + 1;
    const auto out_chroma_height = output_surface_config.out_chroma_height + 1;
    const auto out_chroma_stride = Common::AlignUp(out_chroma_width * BytesPerPixel * 2, 0x10);
    const auto out_chroma_size = out_chroma_height * out_chroma_stride;

    surface_width = (std::min)(surface_width, out_luma_width);
    surface_height = (std::min)(surface_height, out_luma_height);

    auto Decode = [&](std::span<u8> out_luma, std::span<u8> out_chroma) {
        for (size_t y = 0; y < surface_height; ++y) {
            auto const src_luma = y * surface_stride;
            auto const dst_luma = y * out_luma_stride;
            auto const src_chroma = y * surface_stride;
            auto const dst_chroma = (y / 2) * out_chroma_stride;
            for (size_t x = 0; x < surface_width; x += 2) {
                out_luma[dst_luma + x + 0] = u8(output_surface[src_luma + x + 0].r >> 2);
                out_luma[dst_luma + x + 1] = u8(output_surface[src_luma + x + 1].r >> 2);
                out_chroma[dst_chroma + x + 0] = u8(output_surface[src_chroma + x].g >> 2);
                out_chroma[dst_chroma + x + 1] = u8(output_surface[src_chroma + x].b >> 2);
            }
        }
    };

    switch (output_surface_config.out_block_kind) {
    case BLK_KIND::GENERIC_16Bx2: {
        u32 const block_height = u32(output_surface_config.out_block_height);
        auto const out_luma_swizzle_size = Texture::CalculateSize(true, BytesPerPixel, out_luma_width, out_luma_height, 1, block_height, 0);
        auto const out_chroma_swizzle_size = Texture::CalculateSize(true, BytesPerPixel * 2, out_chroma_width, out_chroma_height, 1, block_height, 0);

        LOG_TRACE(HW_GPU, "Writing Y8__V8U8_N420 swizzled frame\n"
            "\tinput surface {}x{} stride {} size {:#X}\n"
            "\toutput   luma {}x{} stride {} size {:#X} block height {} swizzled size 0x{:X}\n",
            "\toutput chroma {}x{} stride {} size {:#X} block height {} swizzled size 0x{:X}",
            surface_width, surface_height, surface_stride * BytesPerPixel,
            surface_stride * surface_height * BytesPerPixel, out_luma_width, out_luma_height,
            out_luma_stride, out_luma_size, block_height, out_luma_swizzle_size, out_chroma_width,
            out_chroma_height, out_chroma_stride, out_chroma_size, block_height,
            out_chroma_swizzle_size);

        luma_scratch.resize_destructive(out_luma_size);
        chroma_scratch.resize_destructive(out_chroma_size);
        Decode(luma_scratch, chroma_scratch);

        Tegra::Memory::GpuGuestMemoryScoped<u8, Core::Memory::GuestMemoryFlags::SafeWrite> out_luma(memory_manager, regs.output_surface.luma.Address(), out_luma_swizzle_size, &swizzle_scratch);
        Tegra::Memory::GpuGuestMemoryScoped<u8, Core::Memory::GuestMemoryFlags::SafeWrite> out_chroma(memory_manager, regs.output_surface.chroma_u.Address(), out_chroma_swizzle_size, &swizzle_scratch);
        if (block_height == 1) {
            SwizzleSurface(out_luma, out_luma_stride, luma_scratch, out_luma_stride, out_luma_height);
            SwizzleSurface(out_chroma, out_chroma_stride, chroma_scratch, out_chroma_stride, out_chroma_height);
        } else {
            Texture::SwizzleTexture(out_luma, luma_scratch, BytesPerPixel, out_luma_width, out_luma_height, 1, block_height, 0, 1);
            Texture::SwizzleTexture(out_chroma, chroma_scratch, BytesPerPixel, out_chroma_width, out_chroma_height, 1, block_height, 0, 1);
        }
    } break;
    case BLK_KIND::PITCH: {
        LOG_TRACE(HW_GPU, "Writing Y8__V8U8_N420 swizzled frame\n"
            "\tinput surface {}x{} stride {} size {:#X}\n"
            "\toutput   luma {}x{} stride {} size {:#X} block height {} swizzled size 0x{:X}\n",
            "\toutput chroma {}x{} stride {} size {:#X} block height {} swizzled size 0x{:X}",
            surface_width, surface_height, surface_stride * BytesPerPixel,
            surface_stride * surface_height * BytesPerPixel, out_luma_width, out_luma_height,
            out_luma_stride, out_luma_size, out_chroma_width, out_chroma_height, out_chroma_stride,
            out_chroma_size);

        // Unfortunately due to a driver bug or game bug, the chroma address can be not
        // appropriately spaced from the luma, so the luma of size out_stride * height runs into the
        // top of the chroma buffer. Unfortunately that removes an optimisation here where we could
        // create guest spans and decode into game memory directly to avoid the memory copy from
        // scratch to game. Due to this bug, we must write the luma first, and then the chroma
        // afterwards to re-overwrite the luma being too large.
        luma_scratch.resize_destructive(out_luma_size);
        chroma_scratch.resize_destructive(out_chroma_size);
        Decode(luma_scratch, chroma_scratch);
        memory_manager.WriteBlock(regs.output_surface.luma.Address(), luma_scratch.data(), out_luma_size);
        memory_manager.WriteBlock(regs.output_surface.chroma_u.Address(), chroma_scratch.data(), out_chroma_size);
    } break;
    default:
        UNREACHABLE();
        break;
    }
}

void Vic::WriteABGR(const OutputSurfaceConfig& output_surface_config) noexcept {
    constexpr u32 BytesPerPixel = 4;

    auto surface_width = output_surface_config.out_surface_width + 1;
    auto surface_height = output_surface_config.out_surface_height + 1;
    auto const surface_stride = surface_width;

    const auto out_luma_width = output_surface_config.out_luma_width + 1;
    const auto out_luma_height = output_surface_config.out_luma_height + 1;
    const auto out_luma_stride = Common ::AlignUp(out_luma_width * BytesPerPixel, 0x10);
    const auto out_luma_size = out_luma_height * out_luma_stride;

    surface_width = (std::min)(surface_width, out_luma_width);
    surface_height = (std::min)(surface_height, out_luma_height);

    auto Decode = [&](std::span<u8> s1, std::span<Pixel> s2) {
        for (size_t y = 0; y < surface_height; ++y) {
            auto const src = y * surface_stride, dst = y * out_luma_stride;
            for (size_t x = 0; x < surface_width; ++x) {
                s1[dst + x * 4 + 0] = u8(s2[src + x].r >> 2);
                s1[dst + x * 4 + 1] = u8(s2[src + x].g >> 2);
                s1[dst + x * 4 + 2] = u8(s2[src + x].b >> 2);
                s1[dst + x * 4 + 3] = u8(s2[src + x].a >> 2);
            }
        }
    };

    switch (output_surface_config.out_block_kind) {
    case BLK_KIND::GENERIC_16Bx2: {
        const u32 block_height = u32(output_surface_config.out_block_height);
        const auto out_swizzle_size = Texture::CalculateSize(true, BytesPerPixel, out_luma_width, out_luma_height, 1, block_height, 0);
        LOG_TRACE(HW_GPU, "Writing ABGR swizzled frame\n"
            "\tinput surface {}x{} stride {} size {:#X}\n"
            "\toutput surface {}x{} stride {} size {:#X} block height {} swizzled size 0x{:X}",
            surface_width, surface_height, surface_stride * BytesPerPixel,
            surface_stride * surface_height * BytesPerPixel, out_luma_width, out_luma_height,
            out_luma_stride, out_luma_size, block_height, out_swizzle_size);
        luma_scratch.resize_destructive(out_luma_size);
        Decode(luma_scratch, output_surface);

        Tegra::Memory::GpuGuestMemoryScoped<u8, Core::Memory::GuestMemoryFlags::SafeWrite> out_luma(memory_manager, regs.output_surface.luma.Address(), out_swizzle_size, &swizzle_scratch);
        if (block_height == 1) {
            SwizzleSurface(out_luma, out_luma_stride, luma_scratch, out_luma_stride, out_luma_height);
        } else {
            Texture::SwizzleTexture(out_luma, luma_scratch, BytesPerPixel, out_luma_width, out_luma_height, 1, block_height, 0, 1);
        }
    } break;
    case BLK_KIND::PITCH: {
        LOG_TRACE(HW_GPU, "Writing ABGR pitch frame\n"
            "\tinput surface {}x{} stride {} size {:#X}"
            "\toutput surface {}x{} stride {} size {:#X}",
            surface_width, surface_height, surface_stride,
            surface_stride * surface_height * BytesPerPixel, out_luma_width, out_luma_height,
            out_luma_stride, out_luma_size);
        luma_scratch.resize_destructive(out_luma_size);
        Tegra::Memory::GpuGuestMemoryScoped<u8, Core::Memory::GuestMemoryFlags::SafeWrite> out_luma(memory_manager, regs.output_surface.luma.Address(), out_luma_size, &luma_scratch);
        Decode(out_luma, output_surface);
    } break;
    default:
        UNREACHABLE();
        break;
    }
}

} // namespace Tegra::Host1x
