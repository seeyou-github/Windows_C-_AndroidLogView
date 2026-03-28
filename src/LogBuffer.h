#pragma once

#include "LogTypes.h"

#include <cstddef>
#include <deque>
#include <vector>

class LogBuffer {
public:
    explicit LogBuffer(std::size_t maxEntries);

    void Clear();
    std::size_t AppendBatch(const std::vector<LogEntry>& batch);
    std::size_t Size() const;
    const LogEntry* At(std::size_t index) const;

private:
    std::size_t m_maxEntries;
    std::deque<LogEntry> m_entries;
};
