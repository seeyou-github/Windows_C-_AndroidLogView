#pragma once

#include <cstdint>
#include <string>

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
};

struct FilterOptions {
    std::wstring keyword;
    std::wstring excludeKeyword;
    std::wstring tag;
    std::wstring pidText;
    LogLevel minimumLevel = LogLevel::Verbose;
};

struct AdbLaunchOptions {
    std::wstring deviceSerial;
    std::wstring adbPriorityFilter;
};
