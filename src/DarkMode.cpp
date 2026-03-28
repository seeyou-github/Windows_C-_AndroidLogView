#include "DarkMode.h"

#include <dwmapi.h>
#include <uxtheme.h>

namespace DarkMode {
void ApplyWindowDarkMode(HWND hWnd) {
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hWnd, 19, &enabled, sizeof(enabled));
    const COLORREF darkCaption = RGB(18, 19, 21);
    DwmSetWindowAttribute(hWnd, 35, &darkCaption, sizeof(darkCaption));
}

void ApplyControlTheme(HWND hWnd) {
    SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
}

COLORREF GetLevelColor(int levelValue) {
    switch (static_cast<LogLevel>(levelValue)) {
    case LogLevel::Verbose:
        return RGB(148, 154, 162);
    case LogLevel::Debug:
        return RGB(114, 169, 255);
    case LogLevel::Info:
        return RGB(220, 223, 228);
    case LogLevel::Warn:
        return RGB(232, 186, 84);
    case LogLevel::Error:
        return RGB(241, 113, 113);
    case LogLevel::Fatal:
        return RGB(255, 96, 128);
    default:
        return kText;
    }
}
}  // namespace
