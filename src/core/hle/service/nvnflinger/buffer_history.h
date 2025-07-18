// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <deque>
#include <mutex>
#include <common/assert.h>

struct BufferHistoryEntry { s32 slot; s64 timestamp; s64 frame_number; };

class BufferHistoryManager { public: void PushEntry(s32 slot, s64 timestamp, s64 frame_number) { std::lock_guardstd::mutex lock(mutex_); if (entries_.size() >= kMaxHistorySize) entries_.pop_front();

entries_.emplace_back(BufferHistoryEntry{slot, timestamp, frame_number});
}

s32 GetHistory(std::span<BufferHistoryEntry> out_entries) {
    std::lock_guard<std::mutex> lock(mutex_);

    s32 count = std::min(static_cast<s32>(entries_.size()), static_cast<s32>(out_entries.size()));

    for (s32 i = 0; i < count; ++i)
        out_entries[i] = entries_[entries_.size() - count + i];

    return count;
}

private: static constexpr size_t kMaxHistorySize = 16; std::deque<BufferHistoryEntry> entries_; std::mutex mutex_; };