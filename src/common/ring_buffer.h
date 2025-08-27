// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <vector>

namespace Common {

/// SPSC ring buffer
/// @tparam T            Element type
/// @tparam capacity     Number of slots in ring buffer
template <typename T, std::size_t capacity>
class RingBuffer {
    /// A "slot" is made of a single `T`.
    static constexpr std::size_t slot_size = sizeof(T);
    // T must be safely memcpy-able and have a trivial default constructor.
    static_assert(std::is_trivial_v<T>);
    // Ensure capacity is sensible.
    static_assert(capacity < std::numeric_limits<std::size_t>::max() / 2);
    static_assert((capacity & (capacity - 1)) == 0, "capacity must be a power of two");
    // Ensure lock-free.
    static_assert(std::atomic_size_t::is_always_lock_free);

public:
    /// Pushes slots into the ring buffer
    /// @param new_slots   Pointer to the slots to push
    /// @param slot_count  Number of slots to push
    /// @returns The number of slots actually pushed
    std::size_t Push(const void* new_slots, std::size_t slot_count) {
        std::lock_guard lock(rb_mutex);

        const std::size_t slots_free = capacity + read_index - write_index;
        const std::size_t push_count = std::min(slot_count, slots_free);
        const std::size_t pos = write_index % capacity;
        const std::size_t first_copy = std::min(capacity - pos, push_count);
        const std::size_t second_copy = push_count - first_copy;

        const char* in = static_cast<const char*>(new_slots);
        std::memcpy(m_data.data() + pos, in, first_copy * slot_size);
        in += first_copy * slot_size;
        std::memcpy(m_data.data(), in, second_copy * slot_size);

        write_index = write_index + push_count;
        return push_count;
    }

    std::size_t Push(std::span<const T> input) {
        return Push(input.data(), input.size());
    }

    /// Pops slots from the ring buffer
    /// @param output     Where to store the popped slots
    /// @param max_slots  Maximum number of slots to pop
    /// @returns The number of slots actually popped
    std::size_t Pop(void* output, std::size_t max_slots = ~std::size_t(0)) {
        std::lock_guard lock(rb_mutex);

        const std::size_t slots_filled = write_index - read_index;
        const std::size_t pop_count = std::min(slots_filled, max_slots);
        const std::size_t pos = read_index % capacity;
        const std::size_t first_copy = std::min(capacity - pos, pop_count);
        const std::size_t second_copy = pop_count - first_copy;

        char* out = static_cast<char*>(output);
        std::memcpy(out, m_data.data() + pos, first_copy * slot_size);
        out += first_copy * slot_size;
        std::memcpy(out, m_data.data(), second_copy * slot_size);

        read_index = read_index + pop_count;
        return pop_count;
    }

    std::vector<T> Pop(std::size_t max_slots = ~std::size_t(0)) {
        std::vector<T> out(std::min(max_slots, capacity));
        const std::size_t count = Pop(out.data(), out.size());
        out.resize(count);
        return out;
    }

    /// @returns Number of slots used
    [[nodiscard]] inline std::size_t Size() const {
        return write_index - read_index;
    }

    /// @returns Maximum size of ring buffer
    [[nodiscard]] consteval std::size_t Capacity() const {
        return capacity;
    }

private:
    std::array<T, capacity> m_data;
    // This is wrong, a thread-safe ringbuffer is impossible.
    std::size_t read_index{0};
    std::size_t write_index{0};
    std::mutex rb_mutex;
};

} // namespace Common
