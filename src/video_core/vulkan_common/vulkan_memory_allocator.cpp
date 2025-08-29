// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bit>
#include <optional>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/polyfill_ranges.h"
#include "video_core/vulkan_common/vma.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
struct Range {
    u64 begin;
    u64 end;

    [[nodiscard]] bool Contains(u64 iterator, u64 size) const noexcept {
        return iterator < end && begin < iterator + size;
    }
};

[[nodiscard]] u64 AllocationChunkSize(u64 required_size) {
    static constexpr std::array sizes{
        0x1000ULL << 10,  0x1400ULL << 10,  0x1800ULL << 10,  0x1c00ULL << 10, 0x2000ULL << 10,
        0x3200ULL << 10,  0x4000ULL << 10,  0x6000ULL << 10,  0x8000ULL << 10, 0xA000ULL << 10,
        0x10000ULL << 10, 0x18000ULL << 10, 0x20000ULL << 10,
    };
    static_assert(std::is_sorted(sizes.begin(), sizes.end()));

    const auto it = std::ranges::lower_bound(sizes, required_size);
    return it != sizes.end() ? *it : Common::AlignUp(required_size, 4ULL << 20);
}

[[nodiscard]] VkMemoryPropertyFlags MemoryUsagePropertyFlags(MemoryUsage usage) {
    switch (usage) {
    case MemoryUsage::DeviceLocal:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    case MemoryUsage::Upload:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    case MemoryUsage::Download:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    case MemoryUsage::Stream:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    ASSERT_MSG(false, "Invalid memory usage={}", usage);
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

[[nodiscard]] VkMemoryPropertyFlags MemoryUsagePreferredVmaFlags(MemoryUsage usage) {
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
        return VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
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

} // Anonymous namespace

class MemoryAllocation {
public:
    explicit MemoryAllocation(MemoryAllocator* const allocator_, vk::DeviceMemory memory_,
                              VkMemoryPropertyFlags properties, u64 allocation_size_, u32 type)
        : allocator{allocator_}, memory{std::move(memory_)}, allocation_size{allocation_size_},
                  property_flags{properties}, shifted_memory_type{1U << type}
    {
        // create the virtual block
        VmaVirtualBlockCreateInfo vbci{};
        vbci.size = allocation_size;
        vbci.flags = 0;
        vk::Check(vmaCreateVirtualBlock(&vbci, &vblock));
    }

    ~MemoryAllocation() {
        if (vblock) vmaDestroyVirtualBlock(vblock);
        // vk::DeviceMemory releases itself via RAII
    }

    MemoryAllocation& operator=(const MemoryAllocation&) = delete;
    MemoryAllocation(const MemoryAllocation&) = delete;
    MemoryAllocation& operator=(MemoryAllocation&&) = delete;
    MemoryAllocation(MemoryAllocation&&) = delete;

    [[nodiscard]] std::optional<MemoryCommit> Commit(VkDeviceSize size, VkDeviceSize alignment) {
        VmaVirtualAllocationCreateInfo aci{};
        aci.size      = size;
        aci.alignment = alignment;
        aci.flags     = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
        VmaVirtualAllocation h{};
        VkDeviceSize offset = 0;
        if (vmaVirtualAllocate(vblock, &aci, &h, &offset) != VK_SUCCESS) {
            return std::nullopt;
        }
        // Track the handle by its offset so our existing MemoryCommit interface can stay unchanged.
        handles.emplace(offset, h);
        return std::make_optional<MemoryCommit>(this, *memory, offset, offset + size);
    }

    void Free(u64 begin) {
        const auto it = handles.find(begin);
        ASSERT_MSG(it != handles.end(), "Invalid commit");
        vmaVirtualFree(vblock, it->second);
        handles.erase(it);
        if (handles.empty()) {
            // Do not touch 'this' after this call, it may delete us.
            allocator->ReleaseMemory(this);
        }
    }

    [[nodiscard]] std::span<u8> Map() {
        if (memory_mapped_span.empty()) {
            u8* const raw_pointer = memory.Map(0, allocation_size);
            memory_mapped_span = std::span<u8>(raw_pointer, allocation_size);
        }
        return memory_mapped_span;
    }

    [[nodiscard]] bool IsCompatible(VkMemoryPropertyFlags flags, u32 type_mask) const {
        return (flags & property_flags) == flags && (type_mask & shifted_memory_type) != 0;
    }

    [[nodiscard]] bool IsUnoccupied() const { return handles.empty(); }
    [[nodiscard]] u64 GetSize() const { return allocation_size; }
    [[nodiscard]] u32 GetMemoryType() const { return std::countr_zero(shifted_memory_type); }

private:
    static constexpr u32 ShiftType(u32 type) { return 1U << type; }

    MemoryAllocator* const allocator{};
    vk::DeviceMemory memory{};
    const u64 allocation_size{};
    const VkMemoryPropertyFlags property_flags{};
    const u32 shifted_memory_type{};
    std::span<u8> memory_mapped_span{};

    // the virtual block and per-suballoc handles
    VmaVirtualBlock vblock{VK_NULL_HANDLE};
    std::unordered_map<u64, VmaVirtualAllocation> handles;
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
        ForEachDeviceLocalHostVisibleHeap(device, [this](size_t heap_idx, VkMemoryHeap& heap) {
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

    vk::Check(vmaCreateBuffer(allocator, &ci, &alloc_ci, &handle, &allocation, &alloc_info));
    vmaGetAllocationMemoryProperties(allocator, allocation, &property_flags);

    u8* data = reinterpret_cast<u8*>(alloc_info.pMappedData);
    const std::span<u8> mapped_data = data ? std::span<u8>{data, ci.size} : std::span<u8>{};
    const bool is_coherent = property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    return vk::Buffer(handle, *device.GetLogical(), allocator, allocation, mapped_data, is_coherent,
                      device.GetDispatchLoader());
}

bool MemoryAllocator::TryReclaimVmaMemory(u32 heap_index, VkDeviceSize needed_size) {
        VkDeviceSize reclaimed = 0;
        std::vector<MemoryAllocation*> to_release;
        for (const auto& alloc : allocations) {
        if (alloc->IsUnoccupied() &&
            properties.memoryTypes[alloc->GetMemoryType()].heapIndex == heap_index) {
            to_release.push_back(alloc.get());
            reclaimed += alloc->GetSize();
            if (reclaimed >= needed_size) break;
        }
    }
    for (auto* alloc : to_release) {
        ReleaseMemory(alloc);
    }
        return reclaimed >= needed_size;
}

void MemoryAllocator::FreeEmptyAllocations() {
    //let VMA handle it
        std::vector<MemoryAllocation*> to_release;
        for (const auto& alloc : allocations) {
            if (alloc->IsUnoccupied()) {
                to_release.push_back(alloc.get());
            }
        }
        for (auto* alloc : to_release) {
            ReleaseMemory(alloc);
        }
}

MemoryCommit MemoryAllocator::Commit(const VkMemoryRequirements& requirements, MemoryUsage usage) {
        const u32 type_mask = requirements.memoryTypeBits;
        const VkMemoryPropertyFlags usage_flags = MemoryUsagePropertyFlags(usage);
        VkMemoryPropertyFlags flags = MemoryPropertyFlags(type_mask, usage_flags);

        // First attempt with preferred flags
        if (std::optional<MemoryCommit> commit = TryCommit(requirements, flags)) {
            return std::move(*commit);
        }

        // Allocate new chunk
        const u64 chunk_size = AllocationChunkSize(requirements.size);
        if (TryAllocMemory(flags, type_mask, chunk_size)) {
            if (auto commit = TryCommit(requirements, flags)) {
                return std::move(*commit);
            }
        }

        // Free empty allocations and retry
        FreeEmptyAllocations();
        if (TryAllocMemory(flags, type_mask, chunk_size)) {
            if (auto commit = TryCommit(requirements, flags)) {
                return std::move(*commit);
            }
        }

        // Relax memory requirements if possible
        if (usage == MemoryUsage::DeviceLocal && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            flags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (auto commit = TryCommit(requirements, flags)) {
                return std::move(*commit);
            }

            // Try allocating with relaxed flags
            if (TryAllocMemory(flags, type_mask, chunk_size)) {
                if (auto commit = TryCommit(requirements, flags)) {
                    return std::move(*commit);
                }
            }
        }

        // Last resort: try minimum size allocation
        const u64 min_chunk = Common::AlignUp(requirements.size, 1ULL << 20);
        if (min_chunk < chunk_size && TryAllocMemory(flags, type_mask, min_chunk)) {
            if (auto commit = TryCommit(requirements, flags)) {
                return std::move(*commit);
            }
        }

    throw vk::Exception(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    }

bool MemoryAllocator::TryAllocMemory(VkMemoryPropertyFlags flags, u32 type_mask, u64 size) {
    const auto type_opt = FindType(flags, type_mask);
    if (!type_opt) {
        return false;
    }

    // Check VMA budget before attempting allocation
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator, budgets);
    const u32 heap_index = properties.memoryTypes[*type_opt].heapIndex;
    const VkDeviceSize available = budgets[heap_index].budget - budgets[heap_index].usage;
    if (available < size) {
        LOG_WARNING(Render_Vulkan, "Heap {} has insufficient budget: {} < {}",
                    heap_index, available, size);
        // Try to free VMA allocations first
        if (!TryReclaimVmaMemory(heap_index, size)) {
                return false;
        }
    }

    const u64 aligned_size = (device.GetDriverID() == VK_DRIVER_ID_QUALCOMM_PROPRIETARY) ?
                             Common::AlignUp(size, 4096) : size;

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
