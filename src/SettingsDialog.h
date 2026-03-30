#pragma once

#include "darkui/button.h"
#include "darkui/edit.h"

#include <windows.h>

#include <string>

class SettingsDialog {
public:
    explicit SettingsDialog(HINSTANCE instance);
    bool ShowModal(HWND parent, std::wstring& exportDirectory);

private:
    static LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnCommand(WPARAM wParam);
    void OnBrowse();
    void End(bool accepted);

    HINSTANCE m_instance;
    HWND m_hWnd;
    darkui::Theme m_theme;
    darkui::Edit m_exportPathEdit;
    darkui::Button m_browseButton;
    darkui::Button m_saveButton;
    darkui::Button m_cancelButton;
    HFONT m_font;
    bool m_done;
    bool m_accepted;
    std::wstring m_result;
};
