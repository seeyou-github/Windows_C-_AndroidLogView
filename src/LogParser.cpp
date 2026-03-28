#include "LogParser.h"

#include <cwctype>

namespace {
std::size_t SkipSpaces(const std::wstring& text, std::size_t pos) {
    while (pos < text.size() && iswspace(text[pos])) {
        ++pos;
    }
    return pos;
}

std::size_t ReadTokenEnd(const std::wstring& text, std::size_t pos) {
    while (pos < text.size() && !iswspace(text[pos])) {
        ++pos;
    }
    return pos;
}
}  // namespace

LogEntry LogParser::ParseThreadTimeLine(const std::wstring& line, std::uint64_t seq) {
    LogEntry entry;
    entry.seq = seq;
    entry.rawLine = line;
    entry.message = line;

    if (line.size() < 19) {
        return entry;
    }

    entry.timestamp = line.substr(0, 18);

    std::size_t pos = 18;
    pos = SkipSpaces(line, pos);

    std::size_t pidEnd = ReadTokenEnd(line, pos);
    if (pidEnd <= pos) {
        return entry;
    }

    try {
        entry.pid = static_cast<std::uint32_t>(std::stoul(line.substr(pos, pidEnd - pos)));
    } catch (...) {
        return entry;
    }

    pos = SkipSpaces(line, pidEnd);
    std::size_t tidEnd = ReadTokenEnd(line, pos);
    if (tidEnd <= pos) {
        return entry;
    }

    try {
        entry.tid = static_cast<std::uint32_t>(std::stoul(line.substr(pos, tidEnd - pos)));
    } catch (...) {
        return entry;
    }

    pos = SkipSpaces(line, tidEnd);
    if (pos >= line.size()) {
        return entry;
    }

    entry.level = CharToLevel(line[pos]);
    ++pos;
    pos = SkipSpaces(line, pos);

    const std::size_t colonPos = line.find(L':', pos);
    if (colonPos == std::wstring::npos) {
        return entry;
    }

    std::size_t tagEnd = colonPos;
    while (tagEnd > pos && iswspace(line[tagEnd - 1])) {
        --tagEnd;
    }
    entry.tag = line.substr(pos, tagEnd - pos);

    std::size_t msgPos = colonPos + 1;
    if (msgPos < line.size() && line[msgPos] == L' ') {
        ++msgPos;
    }
    entry.message = line.substr(msgPos);
    return entry;
}

std::wstring LogParser::LevelToText(LogLevel level) {
    switch (level) {
    case LogLevel::Verbose:
        return L"V";
    case LogLevel::Debug:
        return L"D";
    case LogLevel::Info:
        return L"I";
    case LogLevel::Warn:
        return L"W";
    case LogLevel::Error:
        return L"E";
    case LogLevel::Fatal:
        return L"F";
    case LogLevel::Silent:
        return L"S";
    default:
        return L"?";
    }
}

LogLevel LogParser::CharToLevel(wchar_t value) {
    switch (value) {
    case L'V':
        return LogLevel::Verbose;
    case L'D':
        return LogLevel::Debug;
    case L'I':
        return LogLevel::Info;
    case L'W':
        return LogLevel::Warn;
    case L'E':
        return LogLevel::Error;
    case L'F':
        return LogLevel::Fatal;
    case L'S':
        return LogLevel::Silent;
    default:
        return LogLevel::Unknown;
    }
}
