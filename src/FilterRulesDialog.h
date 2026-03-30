#pragma once

#include "darkui/button.h"
#include "darkui/scrollbar.h"

#include <windows.h>

class FilterRulesDialog {
public:
    explicit FilterRulesDialog(HINSTANCE instance);
    void ShowModal(HWND parent);

private:
    static LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam);
    void OnScroll(WPARAM wParam, LPARAM lParam);
    void OnMouseWheel(WPARAM wParam);
    void UpdateScrollMetrics();
    void Paint(HDC hdc);
    void End();

    HINSTANCE m_instance;
    HWND m_hWnd;
    HFONT m_font;
    darkui::Theme m_theme;
    darkui::Button m_closeButton;
    darkui::ScrollBar m_scrollBar;
    RECT m_textRect;
    int m_scrollPos;
    int m_contentHeight;
    bool m_done;
};
