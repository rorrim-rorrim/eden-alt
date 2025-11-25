// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "video_core/renderer_vulkan/vk_query_cache.h"

#include "common/thread.h"
#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {


void Scheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf,
                                         vk::CommandBuffer upload_cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf, upload_cmdbuf);
        command->~Command();
        command = next;
    }
    submit = false;
    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

Scheduler::Scheduler(const Device& device_, StateTracker& state_tracker_)
    : device{device_}, state_tracker{state_tracker_},
      master_semaphore{std::make_unique<MasterSemaphore>(device)},
      command_pool{std::make_unique<CommandPool>(*master_semaphore, device)} {
    AcquireNewChunk();
    AllocateWorkerCommandBuffer();
    worker_thread = std::jthread([this](std::stop_token token) { WorkerThread(token); });
}

Scheduler::~Scheduler() = default;

u64 Scheduler::Flush(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    // When flushing, we only send data to the worker thread; no waiting is necessary.
    const u64 signal_value = SubmitExecution(signal_semaphore, wait_semaphore);
    AllocateNewContext();
    return signal_value;
}

void Scheduler::Finish(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    // When finishing, we need to wait for the submission to have executed on the device.
    const u64 presubmit_tick = CurrentTick();
    SubmitExecution(signal_semaphore, wait_semaphore);
    Wait(presubmit_tick);
    AllocateNewContext();
}

void Scheduler::WaitWorker() {
    DispatchWork();

    // Ensure the queue is drained.
    {
        std::unique_lock ql{queue_mutex};
        event_cv.wait(ql, [this] { return work_queue.empty(); });
    }

    // Now wait for execution to finish.
    std::scoped_lock el{execution_mutex};
}

void Scheduler::DispatchWork() {
    if (chunk->Empty()) {
        return;
    }
    {
        std::scoped_lock ql{queue_mutex};
        work_queue.push(std::move(chunk));
    }
    event_cv.notify_all();
    AcquireNewChunk();
}

void Scheduler::RequestRenderpass(const Framebuffer* framebuffer) {
    const VkRenderPass renderpass = framebuffer->RenderPass();
    const VkFramebuffer framebuffer_handle = framebuffer->Handle();
    const VkExtent2D render_area = framebuffer->RenderArea();
    if (device.IsDynamicRenderingSupported()) {
        bool is_clear_pending = next_clear_state.depth || next_clear_state.stencil;
        for (bool c : next_clear_state.color) {
            is_clear_pending |= c;
        }

        if (!is_clear_pending && state.is_dynamic_rendering && framebuffer_handle == state.framebuffer &&
            render_area.width == state.render_area.width &&
            render_area.height == state.render_area.height) {
            return;
        }
        EndRenderPass();

        state.is_dynamic_rendering = true;
        state.framebuffer = framebuffer_handle;
        state.render_area = render_area;
        state.renderpass = nullptr;

        const u32 layers = framebuffer->Layers();
        const auto& image_views = framebuffer->ImageViews();
        const auto& image_ranges = framebuffer->ImageRanges();
        const auto& images = framebuffer->Images();
        const u32 num_images = framebuffer->NumImages();

        std::array<VkImageView, 8> attachments_views{};
        for (size_t i = 0; i < 8; ++i) {
            if (framebuffer->IsColorAttachmentValid(i)) {
                attachments_views[i] = image_views[framebuffer->GetImageIndex(i)];
            } else {
                attachments_views[i] = VK_NULL_HANDLE;
            }
        }

        VkImageView depth_view = VK_NULL_HANDLE;
        bool has_depth = framebuffer->HasAspectDepthBit();
        bool has_stencil = framebuffer->HasAspectStencilBit();
        if (has_depth || has_stencil) {
             depth_view = image_views[framebuffer->NumColorBuffers()];
        }

        // Determine initialization state for barriers
        // If we haven't seen this image before, we must assume it is UNDEFINED.
        // This prevents validation errors and artifacts on mobile when loading from UNDEFINED.
        std::array<bool, 9> is_first_use;
        for(size_t i=0; i<num_images; ++i) {
            if (images[i] != VK_NULL_HANDLE) {
                is_first_use[i] = initialized_images.find(images[i]) == initialized_images.end();
                if (is_first_use[i]) {
                    initialized_images.insert(images[i]);
                }
            } else {
                is_first_use[i] = false;
            }
        }

        // Add pre rendering barriers to transition images to optimal layouts
        Record([num_images, images, image_ranges, is_first_use](vk::CommandBuffer cmdbuf) {
            std::array<VkImageMemoryBarrier, 9> pre_barriers;
            for (size_t i = 0; i < num_images; ++i) {
                const VkImageSubresourceRange& range = image_ranges[i];
                const bool is_color = (range.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
                const bool is_depth_stencil = (range.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;

                VkImageLayout optimal_layout = VK_IMAGE_LAYOUT_GENERAL;
                if (is_color) {
                    optimal_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                } else if (is_depth_stencil) {
                    optimal_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                }

                // If first use, transition from UNDEFINED to discard garbage data safely.
                // Otherwise, transition from GENERAL to preserve existing data.
                const VkImageLayout old_layout = is_first_use[i] ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;

                // For first use (UNDEFINED), we don't need to wait for any previous access.
                const VkAccessFlags src_access_mask = is_first_use[i] ? 0 : (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

                pre_barriers[i] = VkImageMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = src_access_mask,
                    .dstAccessMask = is_color ? (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                                              : (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
                    .oldLayout = old_layout,
                    .newLayout = optimal_layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = images[i],
                    .subresourceRange = range,
                };
            }

            if (num_images > 0) {
                cmdbuf.PipelineBarrier(
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    0, nullptr, nullptr,
                    vk::Span(pre_barriers.data(), num_images)
                );
            }
        });

        // Capture the clear state and reset it for next time
        const ClearState current_clear_state = next_clear_state;
        // Reset for next renderpass
        next_clear_state = ClearState{};

        // Capture image indices to allow looking up is_first_use in the lambda
        std::array<size_t, 8> color_indices{};
        for (size_t i = 0; i < 8; ++i) {
            if (framebuffer->IsColorAttachmentValid(i)) {
                color_indices[i] = framebuffer->GetImageIndex(i);
            } else {
                color_indices[i] = 0;
            }
        }
        const size_t depth_index = framebuffer->NumColorBuffers();

        Record([render_area, layers, attachments_views, depth_view, has_depth, has_stencil, current_clear_state, is_first_use, color_indices, depth_index](vk::CommandBuffer cmdbuf) {
            std::array<VkRenderingAttachmentInfo, 8> color_attachments{};
            for (size_t i = 0; i < 8; ++i) {
                // Determine proper load operation per attachment:
                // - DONT_CARE: Null attachments (not used)
                // - CLEAR: When we're clearing this specific color attachment
                // - LOAD: Normal rendering (preserve existing content)
                VkAttachmentLoadOp load_op;
                if (attachments_views[i] == VK_NULL_HANDLE) {
                    load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                } else if (current_clear_state.color[i]) {
                    load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                } else {
                    // Spec Compliance & Turnip Optimization:
                    // If we transitioned from UNDEFINED (is_first_use), we should use DONT_CARE.
                    // This avoids loading garbage data and prevents tile loads on mobile GPUs.
                    // We use the captured image index to check the initialization state.
                    if (is_first_use[color_indices[i]]) {
                        load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    } else {
                        load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
                    }
                }

                color_attachments[i] = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = nullptr,
                    .imageView = attachments_views[i],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode = VK_RESOLVE_MODE_NONE,
                    .resolveImageView = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp = load_op,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = current_clear_state.clear_color},
                };
            }

            VkRenderingAttachmentInfo depth_attachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = depth_view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = current_clear_state.depth ? VK_ATTACHMENT_LOAD_OP_CLEAR
                          : (has_depth && is_first_use[depth_index] ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD),
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.depthStencil = current_clear_state.clear_depth_stencil},
            };

            VkRenderingAttachmentInfo stencil_attachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = depth_view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = current_clear_state.stencil ? VK_ATTACHMENT_LOAD_OP_CLEAR
                          : (has_stencil && is_first_use[depth_index] ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD),
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.depthStencil = current_clear_state.clear_depth_stencil},
            };

            const VkRenderingInfo rendering_info{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderArea = {
                    .offset = {0, 0},
                    .extent = render_area,
                },
                .layerCount = layers,
                .viewMask = 0,
                .colorAttachmentCount = 8,
                .pColorAttachments = color_attachments.data(),
                .pDepthAttachment = has_depth ? &depth_attachment : nullptr,
                .pStencilAttachment = has_stencil ? &stencil_attachment : nullptr,
            };

            cmdbuf.BeginRendering(rendering_info);
        });

        num_renderpass_images = framebuffer->NumImages();
        renderpass_images = framebuffer->Images();
        renderpass_image_ranges = framebuffer->ImageRanges();
        return;
    }

    if (renderpass == state.renderpass && framebuffer_handle == state.framebuffer &&
        render_area.width == state.render_area.width &&
        render_area.height == state.render_area.height) {
        return;
    }
    EndRenderPass();
    state.renderpass = renderpass;
    state.framebuffer = framebuffer_handle;
    state.render_area = render_area;

    Record([renderpass, framebuffer_handle, render_area](vk::CommandBuffer cmdbuf) {
        const VkRenderPassBeginInfo renderpass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .framebuffer = framebuffer_handle,
            .renderArea =
                {
                    .offset = {.x = 0, .y = 0},
                    .extent = render_area,
                },
            .clearValueCount = 0,
            .pClearValues = nullptr,
        };
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
    });
    num_renderpass_images = framebuffer->NumImages();
    renderpass_images = framebuffer->Images();
    renderpass_image_ranges = framebuffer->ImageRanges();
}

void Scheduler::RequestOutsideRenderPassOperationContext() {
    EndRenderPass();
}

bool Scheduler::UpdateGraphicsPipeline(GraphicsPipeline* pipeline) {
    if (state.graphics_pipeline == pipeline) {
        return false;
    }
    state.graphics_pipeline = pipeline;
    return true;
}

bool Scheduler::UpdateRescaling(bool is_rescaling) {
    if (state.rescaling_defined && is_rescaling == state.is_rescaling) {
        return false;
    }
    state.rescaling_defined = true;
    state.is_rescaling = is_rescaling;
    return true;
}

void Scheduler::WorkerThread(std::stop_token stop_token) {
    Common::SetCurrentThreadName("VulkanWorker");

    const auto TryPopQueue{[this](auto& work) -> bool {
        if (work_queue.empty()) {
            return false;
        }

        work = std::move(work_queue.front());
        work_queue.pop();
        event_cv.notify_all();
        return true;
    }};

    while (!stop_token.stop_requested()) {
        std::unique_ptr<CommandChunk> work;

        {
            std::unique_lock lk{queue_mutex};

            // Wait for work.
            event_cv.wait(lk, stop_token, [&] { return TryPopQueue(work); });

            // If we've been asked to stop, we're done.
            if (stop_token.stop_requested()) {
                return;
            }

            // Exchange lock ownership so that we take the execution lock before
            // the queue lock goes out of scope. This allows us to force execution
            // to complete in the next step.
            std::exchange(lk, std::unique_lock{execution_mutex});

            // Perform the work, tracking whether the chunk was a submission
            // before executing.
            const bool has_submit = work->HasSubmit();
            work->ExecuteAll(current_cmdbuf, current_upload_cmdbuf);

            // If the chunk was a submission, reallocate the command buffer.
            if (has_submit) {
                AllocateWorkerCommandBuffer();
            }
        }

        {
            std::scoped_lock rl{reserve_mutex};

            // Recycle the chunk back to the reserve.
            chunk_reserve.emplace_back(std::move(work));
        }
    }
}

void Scheduler::AllocateWorkerCommandBuffer() {
    current_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });
    current_upload_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
    current_upload_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });
}

u64 Scheduler::SubmitExecution(VkSemaphore signal_semaphore, VkSemaphore wait_semaphore) {
    EndPendingOperations();
    InvalidateState();

    const u64 signal_value = master_semaphore->NextTick();
    RecordWithUploadBuffer([signal_semaphore, wait_semaphore, signal_value,
                            this](vk::CommandBuffer cmdbuf, vk::CommandBuffer upload_cmdbuf) {
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        };
        upload_cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, WRITE_BARRIER);
        upload_cmdbuf.End();
        cmdbuf.End();

        if (on_submit) {
            on_submit();
        }

        std::scoped_lock lock{submit_mutex};
        switch (const VkResult result = master_semaphore->SubmitQueue(
                    cmdbuf, upload_cmdbuf, signal_semaphore, wait_semaphore, signal_value)) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss();
            [[fallthrough]];
        default:
            vk::Check(result);
            break;
        }
    });
    chunk->MarkSubmit();
    DispatchWork();
    return signal_value;
}

void Scheduler::AllocateNewContext() {
    // Enable counters once again. These are disabled when a command buffer is finished.
}

void Scheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state.rescaling_defined = false;
    state.is_dynamic_rendering = false;
    state_tracker.InvalidateCommandBufferState();
}

void Scheduler::EndPendingOperations() {
    query_cache->CounterReset(VideoCommon::QueryType::ZPassPixelCount64);
    EndRenderPass();
}

    void Scheduler::EndRenderPass() {
        if (!state.renderpass && !state.is_dynamic_rendering) {
            return;
        }

        query_cache->CounterEnable(VideoCommon::QueryType::ZPassPixelCount64, false);
        query_cache->NotifySegment(false);

        const bool is_dynamic_rendering = state.is_dynamic_rendering;
        Record([num_images = num_renderpass_images,
                       images = renderpass_images,
                       ranges = renderpass_image_ranges,
                       is_dynamic_rendering](vk::CommandBuffer cmdbuf) {
            std::array<VkImageMemoryBarrier, 9> barriers;
            VkPipelineStageFlags src_stages = 0;

            for (size_t i = 0; i < num_images; ++i) {
                const VkImageSubresourceRange& range = ranges[i];
                const bool is_color = (range.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
                const bool is_depth_stencil = (range.aspectMask
                                              & (VK_IMAGE_ASPECT_DEPTH_BIT
                                                 | VK_IMAGE_ASPECT_STENCIL_BIT)) !=0;

                VkAccessFlags src_access = 0;
                VkPipelineStageFlags this_stage = 0;
                VkImageLayout old_layout = VK_IMAGE_LAYOUT_GENERAL;

                if (is_color) {
                    src_access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    this_stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    if (is_dynamic_rendering) {
                        old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    }
                }

                if (is_depth_stencil) {
                    src_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    this_stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    if (is_dynamic_rendering) {
                        old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    }
                }

                src_stages |= this_stage;

                barriers[i] = VkImageMemoryBarrier{
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext = nullptr,
                        .srcAccessMask = src_access,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                                         | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                         | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                         | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        .oldLayout = old_layout,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = images[i],
                        .subresourceRange = range,
                };
            }

            // Graft: ensure explicit fragment tests + color output stages are always synchronized (AMD/Windows fix)
            src_stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if (is_dynamic_rendering) {
                cmdbuf.EndRendering();
            } else {
                cmdbuf.EndRenderPass();
            }

            cmdbuf.PipelineBarrier(src_stages,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   0,
                                   nullptr,
                                   nullptr,
                                   vk::Span(barriers.data(), num_images)  // Batched image barriers
            );
        });

        state.renderpass = nullptr;
        state.is_dynamic_rendering = false;
        num_renderpass_images = 0;
    }
}

void Scheduler::UnregisterImage(VkImage image) {
    initialized_images.erase(image);
}

void Scheduler::AcquireNewChunk() {
    std::scoped_lock rl{reserve_mutex};

    if (chunk_reserve.empty()) {
        // If we don't have anything reserved, we need to make a new chunk.
        chunk = std::make_unique<CommandChunk>();
    } else {
        // Otherwise, we can just take from the reserve.
        chunk = std::move(chunk_reserve.back());
        chunk_reserve.pop_back();
    }
}

} // namespace Vulkan
