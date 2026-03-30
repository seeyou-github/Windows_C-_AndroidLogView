#include "FilterEngine.h"

#include <algorithm>
#include <cwctype>
#include <vector>

namespace {
std::wstring ToLowerCopy(const std::wstring& text) {
    std::wstring value = text;
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::wstring TrimCopy(const std::wstring& text) {
    std::size_t start = 0;
    while (start < text.size() && iswspace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(start, end - start);
}

bool WildcardMatch(const std::wstring& text, const std::wstring& pattern) {
    std::size_t textIndex = 0;
    std::size_t patternIndex = 0;
    std::size_t starIndex = std::wstring::npos;
    std::size_t matchIndex = 0;

    while (textIndex < text.size()) {
        if (patternIndex < pattern.size() && (pattern[patternIndex] == L'?' || pattern[patternIndex] == text[textIndex])) {
            ++textIndex;
            ++patternIndex;
        } else if (patternIndex < pattern.size() && pattern[patternIndex] == L'*') {
            starIndex = patternIndex++;
            matchIndex = textIndex;
        } else if (starIndex != std::wstring::npos) {
            patternIndex = starIndex + 1;
            textIndex = ++matchIndex;
        } else {
            return false;
        }
    }

    while (patternIndex < pattern.size() && pattern[patternIndex] == L'*') {
        ++patternIndex;
    }
    return patternIndex == pattern.size();
}

std::vector<std::wstring> SplitPatterns(const std::wstring& expression) {
    std::vector<std::wstring> patterns;
    std::size_t start = 0;
    while (start <= expression.size()) {
        const std::size_t end = expression.find(L'|', start);
        const std::wstring token = TrimCopy(expression.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
        if (!token.empty()) {
            patterns.push_back(ToLowerCopy(token));
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }
    return patterns;
}

bool ExpressionMatches(const std::wstring& loweredSource, const FilterExpression& expression) {
    if (expression.patterns.empty()) {
        return true;
    }

    for (const auto& pattern : expression.patterns) {
        if (pattern.wildcard) {
            if (WildcardMatch(loweredSource, pattern.text)) {
                return true;
            }
            continue;
        }
        if (loweredSource.find(pattern.text) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

bool EntryMatchesExpression(const LogEntry& entry, const FilterExpression& expression) {
    return ExpressionMatches(entry.messageLower, expression) ||
           ExpressionMatches(entry.rawLineLower, expression) ||
           ExpressionMatches(entry.tagLower, expression);
}
}  // namespace

FilterExpression FilterEngine::CompileExpression(const std::wstring& expression) {
    FilterExpression compiled;
    for (const auto& pattern : SplitPatterns(expression)) {
        compiled.patterns.push_back({pattern, pattern.find_first_of(L"*?") != std::wstring::npos});
    }
    return compiled;
}

bool FilterEngine::Matches(const LogEntry& entry, const FilterOptions& options) {
    if (entry.level != LogLevel::Unknown && entry.level < options.minimumLevel) {
        return false;
    }

    if (!options.pidText.empty()) {
        if (!options.hasPidFilter || entry.pid != options.pidValue) {
            return false;
        }
    }

    if (!ExpressionMatches(entry.tagLower, options.tagExpression)) {
        return false;
    }

    if (!options.keyword.empty() && !EntryMatchesExpression(entry, options.keywordExpression)) {
        return false;
    }

    if (!options.excludeKeyword.empty() && EntryMatchesExpression(entry, options.excludeKeywordExpression)) {
        return false;
    }

    return true;
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
