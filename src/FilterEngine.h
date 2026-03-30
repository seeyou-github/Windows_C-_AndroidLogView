#pragma once

#include "LogBuffer.h"

#include <vector>

class FilterEngine {
public:
    static FilterExpression CompileExpression(const std::wstring& expression);
    static bool Matches(const LogEntry& entry, const FilterOptions& options);
    static std::vector<std::size_t> BuildVisibleIndexes(const LogBuffer& buffer, const FilterOptions& options);
};
