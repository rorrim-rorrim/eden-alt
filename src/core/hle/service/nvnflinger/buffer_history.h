#pragma once

#include <deque>
#include <mutex>
#include <common/assert.h>

struct BufferHistoryEntry {
    s32 slot;
    s64 timestamp;
    s64 frame_number;
};

class BufferHistoryManager {
public:
    void PushEntry(s32 slot, s64 timestamp, s64 frame_number) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (history_entries.size() >= kMaxHistorySize) {
            history_entries.pop_front();
        }
        history_entries.emplace_back(BufferHistoryEntry{slot, timestamp, frame_number});
    }

    s32 GetHistory(s32 wantedEntries, std::span<BufferHistoryEntry>& entries) {
        std::lock_guard<std::mutex> lock(mutex_);
        s32 countToCopy = std::min<s32>(wantedEntries, static_cast<s32>(history_entries.size()));
        auto out_entries = std::vector<BufferHistoryEntry>(countToCopy);
        for (s32 i = 0; i < countToCopy; ++i) {
            out_entries[i] = history_entries[history_entries.size() - countToCopy + i];
        }
        entries = std::span(out_entries);
        return countToCopy;
    }

private:
    static constexpr size_t kMaxHistorySize = 16;
    std::deque<BufferHistoryEntry> history_entries;
    std::mutex mutex_;
};