#include "FilterEngine.h"

#include <algorithm>
#include <cwctype>

namespace {
std::wstring ToLowerCopy(const std::wstring& text) {
    std::wstring value = text;
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

bool ContainsCaseInsensitive(const std::wstring& source, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    return ToLowerCopy(source).find(ToLowerCopy(needle)) != std::wstring::npos;
}
}  // namespace

bool FilterEngine::Matches(const LogEntry& entry, const FilterOptions& options) {
    if (entry.level != LogLevel::Unknown && entry.level < options.minimumLevel) {
        return false;
    }

    if (!options.pidText.empty()) {
        try {
            const auto pidValue = static_cast<std::uint32_t>(std::stoul(options.pidText));
            if (entry.pid != pidValue) {
                return false;
            }
        } catch (...) {
            return false;
        }
    }

    if (!ContainsCaseInsensitive(entry.tag, options.tag)) {
        return false;
    }

    if (options.keyword.empty()) {
        return true;
    }

    return ContainsCaseInsensitive(entry.message, options.keyword) ||
           ContainsCaseInsensitive(entry.rawLine, options.keyword) ||
           ContainsCaseInsensitive(entry.tag, options.keyword);
}

std::vector<std::size_t> FilterEngine::BuildVisibleIndexes(const LogBuffer& buffer, const FilterOptions& options) {
    std::vector<std::size_t> indexes;
    indexes.reserve(buffer.Size());

    for (std::size_t i = 0; i < buffer.Size(); ++i) {
        const LogEntry* entry = buffer.At(i);
        if (entry != nullptr && FilterEngine::Matches(*entry, options)) {
            indexes.push_back(i);
        }
    }
    return indexes;
}
