#include "Config.h"

#include <windows.h>

#include <fstream>
#include <locale>
#include <sstream>

namespace {
std::wstring ReadFileText(const std::wstring& path) {
    std::wifstream input(path.c_str());
    input.imbue(std::locale(".UTF-8"));
    if (!input) {
        return L"";
    }

    std::wstringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool WriteFileText(const std::wstring& path, const std::wstring& content) {
    std::wofstream output(path.c_str(), std::ios::trunc);
    output.imbue(std::locale(".UTF-8"));
    if (!output) {
        return false;
    }
    output << content;
    return output.good();
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
    for (const auto& item : values) {
        output += item.first + L"=" + item.second + L"\n";
    }
    return output;
}
