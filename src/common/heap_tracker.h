// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <shared_mutex>
#include <set>

#include "common/host_memory.h"

namespace Common {

struct SeparateHeapMap {
    VAddr vaddr{};
    PAddr paddr{};
    size_t size{};
    size_t tick{};
    MemoryPermission perm{};
    bool is_resident{};
};

struct SeparateHeapMapAddrComparator {
    constexpr bool operator()(const SeparateHeapMap& lhs, const SeparateHeapMap& rhs) const noexcept {
        return lhs.vaddr < rhs.vaddr;
    }
};
struct SeparateHeapMapTickComparator {
    constexpr bool operator()(const SeparateHeapMap& lhs, const SeparateHeapMap& rhs) const noexcept {
        if (lhs.tick != rhs.tick)
            return lhs.tick < rhs.tick;
        return SeparateHeapMapAddrComparator()(lhs, rhs);
    }
};

class HeapTracker {
public:
    explicit HeapTracker(Common::HostMemory& buffer);
    ~HeapTracker();

    void Map(size_t virtual_offset, size_t host_offset, size_t length, MemoryPermission perm,
             bool is_separate_heap);
    void Unmap(size_t virtual_offset, size_t size, bool is_separate_heap);
    void Protect(size_t virtual_offset, size_t length, MemoryPermission perm);
    inline u8* VirtualBasePointer() noexcept {
        return m_buffer.VirtualBasePointer();
    }
private:
    void SplitHeapMap(VAddr offset, size_t size);
    void SplitHeapMapLocked(VAddr offset);
    void RebuildSeparateHeapAddressSpace();
    std::set<SeparateHeapMap, SeparateHeapMapAddrComparator> m_mappings{};
    std::set<SeparateHeapMap, SeparateHeapMapTickComparator> m_resident_mappings{};
    Common::HostMemory& m_buffer;
    const s64 m_max_resident_map_count;
    std::shared_mutex m_rebuild_lock{};
    std::mutex m_lock{};
    s64 m_map_count{};
    s64 m_resident_map_count{};
    size_t m_tick{};
};

} // namespace Common
