#pragma once

#include "LogTypes.h"

#include <string>

class LogParser {
public:
    static LogEntry ParseThreadTimeLine(const std::wstring& line, std::uint64_t seq);
    static std::wstring LevelToText(LogLevel level);
    static LogLevel CharToLevel(wchar_t value);
};
