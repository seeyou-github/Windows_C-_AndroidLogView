#include "Config.h"

#include <windows.h>

#include <algorithm>
#include <sstream>
#include <vector>

namespace {
std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return L"";
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return "";
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return "";
    }
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}

std::wstring ReadFileText(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return L"";
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) {
        CloseHandle(file);
        return L"";
    }

    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD readBytes = 0;
    const BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &readBytes, nullptr);
    CloseHandle(file);
    if (!ok) {
        return L"";
    }

    if (readBytes >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    } else if (readBytes < bytes.size()) {
        bytes.resize(readBytes);
    }

    return Utf8ToWide(bytes);
}

bool WriteFileText(const std::wstring& path, const std::wstring& content) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const std::string utf8 = WideToUtf8(content);
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    BOOL ok = WriteFile(file, bom, 3, &written, nullptr);
    if (ok && !utf8.empty()) {
        ok = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
    CloseHandle(file);
    return ok == TRUE;
}
}  // namespace

Config::Config(const std::wstring& exeDir) : m_path(exeDir + L"\\AndroidLogViewer.ini") {
}

bool Config::Load() {
    m_lastSerialized = ReadFileText(m_path);
    m_values = Parse(m_lastSerialized);
    return true;
}

bool Config::SaveIfChanged() {
    const std::wstring serialized = Serialize(m_values);
    if (serialized == m_lastSerialized) {
        return true;
    }
    if (!WriteFileText(m_path, serialized)) {
        return false;
    }
    m_lastSerialized = serialized;
    return true;
}

std::wstring Config::Get(const std::wstring& key, const std::wstring& defaultValue) const {
    const auto it = m_values.find(key);
    return it == m_values.end() ? defaultValue : it->second;
}

void Config::Set(const std::wstring& key, const std::wstring& value) {
    m_values[key] = value;
}

const std::wstring& Config::Path() const {
    return m_path;
}

std::unordered_map<std::wstring, std::wstring> Config::Parse(const std::wstring& content) {
    std::unordered_map<std::wstring, std::wstring> values;
    std::wstringstream stream(content);
    std::wstring line;
    while (std::getline(stream, line)) {
        const std::size_t pos = line.find(L'=');
        if (pos == std::wstring::npos) {
            continue;
        }
        values[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return values;
}

std::wstring Config::Serialize(const std::unordered_map<std::wstring, std::wstring>& values) {
    std::wstring output;
    std::vector<std::wstring> emittedKeys;
    emittedKeys.reserve(values.size());

    for (const auto& key : OrderedKeys()) {
        const auto it = values.find(key);
        if (it == values.end()) {
            continue;
        }
        output += it->first + L"=" + it->second + L"\n";
        emittedKeys.push_back(it->first);
    }

    std::vector<std::wstring> remainingKeys;
    remainingKeys.reserve(values.size());
    for (const auto& item : values) {
        if (std::find(emittedKeys.begin(), emittedKeys.end(), item.first) == emittedKeys.end()) {
            remainingKeys.push_back(item.first);
        }
    }
    std::sort(remainingKeys.begin(), remainingKeys.end());
    for (const auto& key : remainingKeys) {
        const auto it = values.find(key);
        if (it != values.end()) {
            output += it->first + L"=" + it->second + L"\n";
        }
    }

    return output;
}

const std::vector<std::wstring>& Config::OrderedKeys() {
    static const std::vector<std::wstring> keys = {
        L"window_width",
        L"window_height",
        L"adb_filter",
        L"keyword",
        L"tag",
        L"pid",
        L"exclude_keyword",
        L"level",
        L"export_dir",
        L"known_devices",
    };
    return keys;
}
