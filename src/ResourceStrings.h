#pragma once

#include <windows.h>

#include <string>

class ResourceStrings {
public:
    static std::wstring Load(HINSTANCE instance, UINT id);
};
