#include "LogBuffer.h"

LogBuffer::LogBuffer(std::size_t maxEntries) : m_maxEntries(maxEntries) {
}

void LogBuffer::Clear() {
    m_entries.clear();
}

std::size_t LogBuffer::AppendBatch(const std::vector<LogEntry>& batch) {
    for (const auto& item : batch) {
        m_entries.push_back(item);
    }

    std::size_t dropped = 0;
    while (m_entries.size() > m_maxEntries) {
        m_entries.pop_front();
        ++dropped;
    }
    return dropped;
}

std::size_t LogBuffer::Size() const {
    return m_entries.size();
}

const LogEntry* LogBuffer::At(std::size_t index) const {
    if (index >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries[index];
}
