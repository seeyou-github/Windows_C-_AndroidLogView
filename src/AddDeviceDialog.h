#pragma once

#include "darkui/button.h"
#include "darkui/edit.h"

#include <windows.h>

#include <string>

class AddDeviceDialog {
public:
    explicit AddDeviceDialog(HINSTANCE instance);
    bool ShowModal(HWND parent, std::wstring& outAddress);

private:
    static LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnCommand(WPARAM wParam);
    void End(bool accepted);
    std::wstring ReadText(HWND hWnd) const;

    HINSTANCE m_instance;
    HWND m_hWnd;
    darkui::Theme m_theme;
    darkui::Edit m_ipEdit1;
    darkui::Edit m_ipEdit2;
    darkui::Edit m_ipEdit3;
    darkui::Edit m_ipEdit4;
    darkui::Edit m_portEdit;
    darkui::Button m_okButton;
    darkui::Button m_cancelButton;
    HFONT m_font;
    bool m_done;
    bool m_accepted;
    std::wstring m_result;
};
