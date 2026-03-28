#pragma once

#include "LogTypes.h"

#include <windows.h>

namespace DarkMode {
constexpr COLORREF kBackground = RGB(24, 26, 28);
constexpr COLORREF kSurface = RGB(34, 37, 41);
constexpr COLORREF kSurfaceAlt = RGB(42, 45, 49);
constexpr COLORREF kText = RGB(225, 228, 232);
constexpr COLORREF kMutedText = RGB(160, 166, 173);
constexpr COLORREF kBorder = RGB(58, 62, 67);

void ApplyWindowDarkMode(HWND hWnd);
void ApplyControlTheme(HWND hWnd);
COLORREF GetLevelColor(int levelValue);
}  // namespace
