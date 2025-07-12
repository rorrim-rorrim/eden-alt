// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "common/logging/log.h"
#include <ranges>
#include "video_core/vulkan_common/vma.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
    namespace {

// Helpers translating MemoryUsage to flags/usage

        [[maybe_unused]] VkMemoryPropertyFlags MemoryUsagePropertyFlags(MemoryUsage usage) {
            switch (usage) {
                case MemoryUsage::DeviceLocal:
                    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                case MemoryUsage::Upload:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                case MemoryUsage::Download:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                case MemoryUsage::Stream:
                    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
            ASSERT_MSG(false, "Invalid memory usage={}", usage);
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        [[nodiscard]] VkMemoryPropertyFlags MemoryUsagePreferredVmaFlags(MemoryUsage usage) {
            if (usage == MemoryUsage::Download) {
                return VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
            return usage != MemoryUsage::DeviceLocal ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                     : VkMemoryPropertyFlagBits{};
        }

        [[nodiscard]] VmaAllocationCreateFlags MemoryUsageVmaFlags(MemoryUsage usage) {
            switch (usage) {
                case MemoryUsage::Upload:
                case MemoryUsage::Stream:
                    return VMA_ALLOCATION_CREATE_MAPPED_BIT |
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                case MemoryUsage::Download:
                    return VMA_ALLOCATION_CREATE_MAPPED_BIT |
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                case MemoryUsage::DeviceLocal:
                    return {};
            }
            return {};
        }

        [[nodiscard]] VmaMemoryUsage MemoryUsageVma(MemoryUsage usage) {
            switch (usage) {
                case MemoryUsage::DeviceLocal:
                case MemoryUsage::Stream:
                    return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                case MemoryUsage::Upload:
                case MemoryUsage::Download:
                    return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            }
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        }


// This avoids calling vkGetBufferMemoryRequirements* directly.
        template<typename T>
        static VkBuffer GetVkHandleFromBuffer(const T &buf) {
            if constexpr (requires { static_cast<VkBuffer>(buf); }) {
                return static_cast<VkBuffer>(buf);
            } else if constexpr (requires {{ buf.GetHandle() } -> std::convertible_to<VkBuffer>; }) {
                return buf.GetHandle();
            } else if constexpr (requires {{ buf.Handle() } -> std::convertible_to<VkBuffer>; }) {
                return buf.Handle();
            } else if constexpr (requires {{ buf.vk_handle() } -> std::convertible_to<VkBuffer>; }) {
                return buf.vk_handle();
            } else {
                static_assert(sizeof(T) == 0, "Cannot extract VkBuffer handle from vk::Buffer");
                return VK_NULL_HANDLE;
            }
        }

    } // namespace

//MemoryCommit is now VMA-backed
    MemoryCommit::MemoryCommit(VmaAllocator alloc, VmaAllocation a,
                               const VmaAllocationInfo &info) noexcept
            : allocator{alloc}, allocation{a}, memory{info.deviceMemory},
              offset{info.offset}, size{info.size}, mapped_ptr{info.pMappedData} {}

    MemoryCommit::~MemoryCommit() { Release(); }

    MemoryCommit::MemoryCommit(MemoryCommit &&rhs) noexcept
            : allocator{std::exchange(rhs.allocator, nullptr)},
              allocation{std::exchange(rhs.allocation, nullptr)},
              memory{std::exchange(rhs.memory, VK_NULL_HANDLE)},
              offset{std::exchange(rhs.offset, 0)},
              size{std::exchange(rhs.size, 0)},
              mapped_ptr{std::exchange(rhs.mapped_ptr, nullptr)} {}

    MemoryCommit &MemoryCommit::operator=(MemoryCommit &&rhs) noexcept {
        if (this != &rhs) {
            Release();
            allocator = std::exchange(rhs.allocator, nullptr);
            allocation = std::exchange(rhs.allocation, nullptr);
            memory = std::exchange(rhs.memory, VK_NULL_HANDLE);
            offset = std::exchange(rhs.offset, 0);
            size = std::exchange(rhs.size, 0);
            mapped_ptr = std::exchange(rhs.mapped_ptr, nullptr);
        }
        return *this;
    }

    std::span<u8> MemoryCommit::Map()
    {
        if (!allocation) return {};
        if (!mapped_ptr) {
            if (vmaMapMemory(allocator, allocation, &mapped_ptr) != VK_SUCCESS) return {};
        }
        const size_t n = static_cast<size_t>(std::min<VkDeviceSize>(size,
                                                                    (std::numeric_limits<size_t>::max)()));
        return std::span<u8>{static_cast<u8 *>(mapped_ptr), n};
    }

    std::span<const u8> MemoryCommit::Map() const
    {
        if (!allocation) return {};
        if (!mapped_ptr) {
            void *p = nullptr;
            if (vmaMapMemory(allocator, allocation, &p) != VK_SUCCESS) return {};
            const_cast<MemoryCommit *>(this)->mapped_ptr = p;
        }
        const size_t n = static_cast<size_t>(std::min<VkDeviceSize>(size,
                                                                    (std::numeric_limits<size_t>::max)()));
        return std::span<const u8>{static_cast<const u8 *>(mapped_ptr), n};
    }

    void MemoryCommit::Unmap()
    {
        if (allocation && mapped_ptr) {
            vmaUnmapMemory(allocator, allocation);
            mapped_ptr = nullptr;
        }
    }

    void MemoryCommit::Release() {
        if (allocation && allocator) {
            if (mapped_ptr) {
                vmaUnmapMemory(allocator, allocation);
                mapped_ptr = nullptr;
            }
            vmaFreeMemory(allocator, allocation);
        }
        allocation = nullptr;
        allocator = nullptr;
        memory = VK_NULL_HANDLE;
        offset = 0;
        size = 0;
    }

    MemoryAllocator::MemoryAllocator(const Device &device_)
            : device{device_}, allocator{device.GetAllocator()},
              properties{device_.GetPhysical().GetMemoryProperties().memoryProperties},
              buffer_image_granularity{
                      device_.GetPhysical().GetProperties().limits.bufferImageGranularity} {

        // Preserve the previous "RenderDoc small heap" trimming behavior that we had in original vma minus the heap bug
        if (device.HasDebuggingToolAttached())
        {
            using namespace Common::Literals;
            ForEachDeviceLocalHostVisibleHeap(device, [this](size_t heap_idx, VkMemoryHeap &heap) {
                if (heap.size <= 256_MiB) {
                    for (u32 t = 0; t < properties.memoryTypeCount; ++t) {
                        if (properties.memoryTypes[t].heapIndex == heap_idx) {
                            valid_memory_types &= ~(1u << t);
                        }
                    }
                }
            });
        }
    }

    MemoryAllocator::~MemoryAllocator() = default;

    vk::Image MemoryAllocator::CreateImage(const VkImageCreateInfo &ci) const
    {
        const VmaAllocationCreateInfo alloc_ci = {
                .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                .requiredFlags = 0,
                .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .memoryTypeBits = 0,
                .pool = VK_NULL_HANDLE,
                .pUserData = nullptr,
                .priority = 0.f,
        };

        VkImage handle{};
        VmaAllocation allocation{};
        vk::Check(vmaCreateImage(allocator, &ci, &alloc_ci, &handle, &allocation, nullptr));
        return vk::Image(handle, ci.usage, *device.GetLogical(), allocator, allocation,
                         device.GetDispatchLoader());
    }

    vk::Buffer
    MemoryAllocator::CreateBuffer(const VkBufferCreateInfo &ci, MemoryUsage usage) const
    {
        const VmaAllocationCreateInfo alloc_ci = {
                .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | MemoryUsageVmaFlags(usage),
                .usage = MemoryUsageVma(usage),
                .requiredFlags = 0,
                .preferredFlags = MemoryUsagePreferredVmaFlags(usage),
                .memoryTypeBits = usage == MemoryUsage::Stream ? 0u : valid_memory_types,
                .pool = VK_NULL_HANDLE,
                .pUserData = nullptr,
                .priority = 0.f,
        };

        VkBuffer handle{};
        VmaAllocationInfo alloc_info{};
        VmaAllocation allocation{};
        VkMemoryPropertyFlags property_flags{};

        vk::Check(vmaCreateBuffer(allocator, &ci, &alloc_ci, &handle, &allocation, &alloc_info));
        vmaGetAllocationMemoryProperties(allocator, allocation, &property_flags);

        u8 *data = reinterpret_cast<u8 *>(alloc_info.pMappedData);
        const std::span<u8> mapped_data = data ? std::span<u8>{data, ci.size} : std::span<u8>{};
        const bool is_coherent = (property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        return vk::Buffer(handle, *device.GetLogical(), allocator, allocation, mapped_data,
                          is_coherent,
                          device.GetDispatchLoader());
    }

    MemoryCommit MemoryAllocator::Commit(const VkMemoryRequirements &reqs, MemoryUsage usage)
    {
        const auto vma_usage = MemoryUsageVma(usage);
        VmaAllocationCreateInfo ci{};
        ci.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | MemoryUsageVmaFlags(usage);
        ci.usage = vma_usage;
        ci.memoryTypeBits = reqs.memoryTypeBits & valid_memory_types;
        ci.requiredFlags = 0;
        ci.preferredFlags = MemoryUsagePreferredVmaFlags(usage);

        VmaAllocation a{};
        VmaAllocationInfo info{};

        VkResult res = vmaAllocateMemory(allocator, &reqs, &ci, &a, &info);

        if (res != VK_SUCCESS) {
            // Relax 1: drop budget constraint
            auto ci2 = ci;
            ci2.flags &= ~VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            res = vmaAllocateMemory(allocator, &reqs, &ci2, &a, &info);

            // Relax 2: if we preferred DEVICE_LOCAL, drop that preference
            if (res != VK_SUCCESS && (ci.preferredFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                auto ci3 = ci2;
                ci3.preferredFlags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                res = vmaAllocateMemory(allocator, &reqs, &ci3, &a, &info);
            }
        }

        vk::Check(res);
        return MemoryCommit(allocator, a, info);
    }

    MemoryCommit MemoryAllocator::Commit(const vk::Buffer &buffer, MemoryUsage usage) {
        // Allocate memory appropriate for this buffer automatically
        const auto vma_usage = MemoryUsageVma(usage);

        VmaAllocationCreateInfo ci{};
        ci.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | MemoryUsageVmaFlags(usage);
        ci.usage = vma_usage;
        ci.requiredFlags = 0;
        ci.preferredFlags = MemoryUsagePreferredVmaFlags(usage);
        ci.pool = VK_NULL_HANDLE;
        ci.pUserData = nullptr;
        ci.priority = 0.0f;

        const VkBuffer raw = *buffer;

        VmaAllocation a{};
        VmaAllocationInfo info{};

        // Let VMA infer memory requirements from the buffer
        VkResult res = vmaAllocateMemoryForBuffer(allocator, raw, &ci, &a, &info);

        if (res != VK_SUCCESS) {
            auto ci2 = ci;
            ci2.flags &= ~VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
            res = vmaAllocateMemoryForBuffer(allocator, raw, &ci2, &a, &info);

            if (res != VK_SUCCESS && (ci.preferredFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                auto ci3 = ci2;
                ci3.preferredFlags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                res = vmaAllocateMemoryForBuffer(allocator, raw, &ci3, &a, &info);
            }
        }

        vk::Check(res);
        vk::Check(vmaBindBufferMemory2(allocator, a, 0, raw, nullptr));
        return MemoryCommit(allocator, a, info);
    }

    MemoryAllocator* const allocator;           ///< Parent memory allocation.
    const vk::DeviceMemory memory;              ///< Vulkan memory allocation handler.
    const u64 allocation_size;                  ///< Size of this allocation.
    const VkMemoryPropertyFlags property_flags; ///< Vulkan memory property flags.
    const u32 shifted_memory_type;              ///< Shifted Vulkan memory type.
    std::vector<Range> commits;                 ///< All commit ranges done from this allocation.
    std::span<u8> memory_mapped_span; ///< Memory mapped span. Empty if not queried before.
};

MemoryCommit::MemoryCommit(MemoryAllocation* allocation_, VkDeviceMemory memory_, u64 begin_,
                           u64 end_) noexcept
    : allocation{allocation_}, memory{memory_}, begin{begin_}, end{end_} {}

MemoryCommit::~MemoryCommit() {
    Release();
}

MemoryCommit& MemoryCommit::operator=(MemoryCommit&& rhs) noexcept {
    Release();
    allocation = std::exchange(rhs.allocation, nullptr);
    memory = rhs.memory;
    begin = rhs.begin;
    end = rhs.end;
    span = std::exchange(rhs.span, std::span<u8>{});
    return *this;
}

MemoryCommit::MemoryCommit(MemoryCommit&& rhs) noexcept
    : allocation{std::exchange(rhs.allocation, nullptr)}, memory{rhs.memory}, begin{rhs.begin},
      end{rhs.end}, span{std::exchange(rhs.span, std::span<u8>{})} {}

std::span<u8> MemoryCommit::Map() {
    if (span.empty()) {
        span = allocation->Map().subspan(begin, end - begin);
    }
    return span;
}

void MemoryCommit::Release() {
    if (allocation) {
        allocation->Free(begin);
    }
}

MemoryAllocator::MemoryAllocator(const Device& device_)
    : device{device_}, allocator{device.GetAllocator()},
      properties{device_.GetPhysical().GetMemoryProperties().memoryProperties},
      buffer_image_granularity{
          device_.GetPhysical().GetProperties().limits.bufferImageGranularity} {
    // GPUs not supporting rebar may only have a region with less than 256MB host visible/device
    // local memory. In that case, opening 2 RenderDoc captures side-by-side is not possible due to
    // the heap running out of memory. With RenderDoc attached and only a small host/device region,
    // only allow the stream buffer in this memory heap.
    if (device.HasDebuggingToolAttached()) {
        using namespace Common::Literals;
        ForEachDeviceLocalHostVisibleHeap(device, [this](size_t index, VkMemoryHeap& heap) {
            if (heap.size <= 256_MiB) {
                valid_memory_types &= ~(1u << index);
            }
        });
    }
}

MemoryAllocator::~MemoryAllocator() = default;

vk::Image MemoryAllocator::CreateImage(const VkImageCreateInfo& ci) const {
    const VmaAllocationCreateInfo alloc_ci = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .memoryTypeBits = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = 0.f,
    };

    VkImage handle{};
    VmaAllocation allocation{};

    vk::Check(vmaCreateImage(allocator, &ci, &alloc_ci, &handle, &allocation, nullptr));

    return vk::Image(handle, ci.usage, *device.GetLogical(), allocator, allocation,
                     device.GetDispatchLoader());
}

vk::Buffer MemoryAllocator::CreateBuffer(const VkBufferCreateInfo& ci, MemoryUsage usage) const {
    const VmaAllocationCreateInfo alloc_ci = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT | MemoryUsageVmaFlags(usage),
        .usage = MemoryUsageVma(usage),
        .requiredFlags = 0,
        .preferredFlags = MemoryUsagePreferredVmaFlags(usage),
        .memoryTypeBits = usage == MemoryUsage::Stream ? 0u : valid_memory_types,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
        .priority = 0.f,
    };

    VkBuffer handle{};
    VmaAllocationInfo alloc_info{};
    VmaAllocation allocation{};
    VkMemoryPropertyFlags property_flags{};

    VkResult result = vmaCreateBuffer(allocator, &ci, &alloc_ci, &handle, &allocation, &alloc_info);
    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        LOG_ERROR(Render_Vulkan, "Out of memory creating buffer (size: {})", ci.size);
    }
    vmaGetAllocationMemoryProperties(allocator, allocation, &property_flags);

    u8* data = reinterpret_cast<u8*>(alloc_info.pMappedData);
    const std::span<u8> mapped_data = data ? std::span<u8>{data, ci.size} : std::span<u8>{};
    const bool is_coherent = property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    return vk::Buffer(handle, *device.GetLogical(), allocator, allocation, mapped_data, is_coherent,
                      device.GetDispatchLoader());
}

MemoryCommit MemoryAllocator::Commit(const VkMemoryRequirements& requirements, MemoryUsage usage) {
        // Find the fastest memory flags we can afford with the current requirements
        const u32 type_mask = requirements.memoryTypeBits;
        const VkMemoryPropertyFlags usage_flags = MemoryUsagePropertyFlags(usage);
        const VkMemoryPropertyFlags flags = MemoryPropertyFlags(type_mask, usage_flags);
        if (std::optional<MemoryCommit> commit = TryCommit(requirements, flags)) {
            return std::move(*commit);
        }
        // Commit has failed, allocate more memory.
        const u64 chunk_size = AllocationChunkSize(requirements.size);
        if (!TryAllocMemory(flags, type_mask, chunk_size)) {
            // TODO(Rodrigo): Handle out of memory situations in some way like flushing to guest memory.
            throw vk::Exception(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        }
        // Commit again, this time it won't fail since there's a fresh allocation above.
        // If it does, there's a bug.
        return TryCommit(requirements, flags).value();
    }

bool MemoryAllocator::TryAllocMemory(VkMemoryPropertyFlags flags, u32 type_mask, u64 size) {
    const auto type_opt = FindType(flags, type_mask);
    if (!type_opt) {
        return false;
    }

    // Adreno stands firm
    const u64 aligned_size = (device.GetDriverID() == VK_DRIVER_ID_QUALCOMM_PROPRIETARY) ?
                            Common::AlignUp(size, 4096) :
                            size;

    vk::DeviceMemory memory = device.GetLogical().TryAllocateMemory({
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = aligned_size,
        .memoryTypeIndex = *type_opt,
    });

    if (!memory) {
        return false;
    }

    allocations.push_back(
        std::make_unique<MemoryAllocation>(this, std::move(memory), flags, aligned_size, *type_opt));
    return true;
}

void MemoryAllocator::ReleaseMemory(MemoryAllocation* alloc) {
    const auto it = std::ranges::find(allocations, alloc, &std::unique_ptr<MemoryAllocation>::get);
    ASSERT(it != allocations.end());
    allocations.erase(it);
}

std::optional<MemoryCommit> MemoryAllocator::TryCommit(const VkMemoryRequirements& requirements,
                                                       VkMemoryPropertyFlags flags) {
    // Conservative, spec-compliant alignment for suballocation
    VkDeviceSize eff_align = requirements.alignment;
    const auto& limits = device.GetPhysical().GetProperties().limits;
    if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        !(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        // Non-coherent memory must be invalidated on atom boundary
        if (limits.nonCoherentAtomSize > eff_align) eff_align = limits.nonCoherentAtomSize;
    }
    // Separate buffers to avoid stalls on tilers
    if (buffer_image_granularity > eff_align) {
        eff_align = buffer_image_granularity;
    }
    eff_align = std::bit_ceil(eff_align);

    for (auto& allocation : allocations) {
        if (!allocation->IsCompatible(flags, requirements.memoryTypeBits)) {
            continue;
        }
        if (auto commit = allocation->Commit(requirements.size, eff_align)) {
            return commit;
        }
    }
    if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
        // Look for non device local commits on failure
        return TryCommit(requirements, flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    return std::nullopt;
}

VkMemoryPropertyFlags MemoryAllocator::MemoryPropertyFlags(u32 type_mask,
                                                           VkMemoryPropertyFlags flags) const {
    if (FindType(flags, type_mask)) {
        // Found a memory type with those requirements
        return flags;
    }
    if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) {
        // Remove host cached bit in case it's not supported
        return MemoryPropertyFlags(type_mask, flags & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    }
    if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
        // Remove device local, if it's not supported by the requested resource
        return MemoryPropertyFlags(type_mask, flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    ASSERT_MSG(false, "No compatible memory types found");
    return 0;
}

std::optional<u32> MemoryAllocator::FindType(VkMemoryPropertyFlags flags, u32 type_mask) const {
    for (u32 type_index = 0; type_index < properties.memoryTypeCount; ++type_index) {
        const VkMemoryPropertyFlags type_flags = properties.memoryTypes[type_index].propertyFlags;
        if ((type_mask & (1U << type_index)) != 0 && (type_flags & flags) == flags) {
            // The type matches in type and in the wanted properties.
            return type_index;
        }
    }
    // Failed to find index
    return std::nullopt;
}

} // namespace Vulkan
