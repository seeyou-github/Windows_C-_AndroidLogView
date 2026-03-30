#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FilterPattern {
    std::wstring text;
    bool wildcard = false;
};

struct FilterExpression {
    std::vector<FilterPattern> patterns;
};

enum class LogLevel {
    Verbose = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Silent,
    Unknown
};

struct LogEntry {
    std::uint64_t seq = 0;
    std::wstring timestamp;
    std::uint32_t pid = 0;
    std::uint32_t tid = 0;
    LogLevel level = LogLevel::Unknown;
    std::wstring tag;
    std::wstring message;
    std::wstring rawLine;
    std::wstring pidText;
    std::wstring levelText;
    std::wstring tagLower;
    std::wstring messageLower;
    std::wstring rawLineLower;
};

struct FilterOptions {
    std::wstring keyword;
    std::wstring excludeKeyword;
    std::wstring tag;
    std::wstring pidText;
    LogLevel minimumLevel = LogLevel::Verbose;
    FilterExpression keywordExpression;
    FilterExpression excludeKeywordExpression;
    FilterExpression tagExpression;
    bool hasPidFilter = false;
    std::uint32_t pidValue = 0;
};

struct AdbLaunchOptions {
    std::wstring deviceSerial;
    std::wstring logBuffer = L"main";
    std::wstring adbPriorityFilter;
};
