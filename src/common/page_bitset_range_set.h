// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/common_types.h"
#include "common/div_ceil.h"

namespace Common {

template <typename AddressType, u32 PageBits, u64 MaxAddress>
class PageBitsetRangeSet {
public:
    PageBitsetRangeSet() {
        static_assert((MaxAddress % PAGE_SIZE) == 0, "MaxAddress must be page-aligned.");
    }

    void Add(AddressType base_address, size_t size) {
        Modify(base_address, size, true);
    }

    void Subtract(AddressType base_address, size_t size) {
        Modify(base_address, size, false);
    }

    void Clear() {
        std::fill(m_words.begin(), m_words.end(), 0);
    }

    bool Empty() const {
        return std::none_of(m_words.begin(), m_words.end(), [](u64 word) { return word != 0; });
    }

    template <typename Func>
    void ForEachInRange(AddressType base_address, size_t size, Func&& func) const {
        if (size == 0 || m_words.empty()) {
            return;
        }
        const AddressType end_address = base_address + AddressType(size);
        const u64 start_page = u64(base_address) >> PageBits;
        const u64 end_page = Common::DivCeil(u64(end_address), PAGE_SIZE);
        const u64 clamped_start = (std::min)(start_page, PAGE_COUNT);
        const u64 clamped_end = (std::min)(end_page, PAGE_COUNT);
        if (clamped_start >= clamped_end) {
            return;
        }

        bool in_run = false;
        u64 run_start_page = 0;
        for (u64 page = clamped_start; page < clamped_end; ++page) {
            if (Test(page)) {
                if (!in_run) {
                    in_run = true;
                    run_start_page = page;
                }
                continue;
            }
            if (in_run) {
                EmitRun(run_start_page, page, base_address, end_address, func);
                in_run = false;
            }
        }
        if (in_run) {
            EmitRun(run_start_page, clamped_end, base_address, end_address, func);
        }
    }

private:
    static constexpr u64 PAGE_SIZE = u64{1} << PageBits;
    static constexpr u64 PAGE_COUNT = MaxAddress / PAGE_SIZE;

    void Modify(AddressType base_address, size_t size, bool set_bits) {
        if (size == 0 || m_words.empty()) {
            return;
        }
        const AddressType end_address = base_address + static_cast<AddressType>(size);
        const u64 start_page = static_cast<u64>(base_address) >> PageBits;
        const u64 end_page = Common::DivCeil(static_cast<u64>(end_address), PAGE_SIZE);
        const u64 clamped_start = (std::min)(start_page, PAGE_COUNT);
        const u64 clamped_end = (std::min)(end_page, PAGE_COUNT);
        if (clamped_start >= clamped_end) {
            return;
        }
        const u64 start_word = clamped_start / 64;
        const u64 end_word = (clamped_end - 1) / 64;
        const u64 start_mask = ~0ULL << (clamped_start % 64);
        const u64 end_mask = (clamped_end % 64) == 0 ? ~0ULL : ((1ULL << (clamped_end % 64)) - 1);

        if (start_word == end_word) {
            const u64 mask = start_mask & end_mask;
            if (set_bits) {
                m_words[start_word] |= mask;
            } else {
                m_words[start_word] &= ~mask;
            }
            return;
        }

        if (set_bits) {
            m_words[start_word] |= start_mask;
            m_words[end_word] |= end_mask;
            std::fill(m_words.begin() + std::ptrdiff_t(start_word + 1), m_words.begin() + std::ptrdiff_t(end_word), ~0ULL);
        } else {
            m_words[start_word] &= ~start_mask;
            m_words[end_word] &= ~end_mask;
            std::fill(m_words.begin() + std::ptrdiff_t(start_word + 1), m_words.begin() + std::ptrdiff_t(end_word), 0ULL);
        }
    }

    bool Test(u64 page) const {
        return m_words[page / 64].test(page % 64);
    }

    template <typename Func>
    static void EmitRun(u64 run_start_page, u64 run_end_page, AddressType base_address, AddressType end_address, Func&& func) {
        AddressType run_start = run_start_page * PAGE_SIZE;
        AddressType run_end = run_end_page * PAGE_SIZE;
        if (run_start < base_address) {
            run_start = base_address;
        }
        if (run_end > end_address) {
            run_end = end_address;
        }
        if (run_start < run_end) {
            func(run_start, run_end);
        }
    }

    std::array<std::bitset<64>, (PAGE_COUNT + 63) / 64> m_words;
};

} // namespace Common
