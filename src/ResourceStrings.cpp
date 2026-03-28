#include "ResourceStrings.h"

std::wstring ResourceStrings::Load(HINSTANCE instance, UINT id) {
    wchar_t buffer[256] = {};
    const int length = LoadStringW(instance, id, buffer, static_cast<int>(std::size(buffer)));
    if (length <= 0) {
        return L"";
    }
    return std::wstring(buffer, static_cast<std::size_t>(length));
}
