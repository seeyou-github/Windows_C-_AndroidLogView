#pragma once

#include <string>
#include <unordered_map>

class Config {
public:
    explicit Config(const std::wstring& exeDir);

    bool Load();
    bool SaveIfChanged();

    std::wstring Get(const std::wstring& key, const std::wstring& defaultValue = L"") const;
    void Set(const std::wstring& key, const std::wstring& value);
    const std::wstring& Path() const;

private:
    static std::unordered_map<std::wstring, std::wstring> Parse(const std::wstring& content);
    static std::wstring Serialize(const std::unordered_map<std::wstring, std::wstring>& values);

    std::wstring m_path;
    std::unordered_map<std::wstring, std::wstring> m_values;
    std::wstring m_lastSerialized;
};
