#pragma once

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
    HWND m_hIpEdit1;
    HWND m_hIpEdit2;
    HWND m_hIpEdit3;
    HWND m_hIpEdit4;
    HWND m_hPortEdit;
    HWND m_hOkButton;
    HWND m_hCancelButton;
    HFONT m_font;
    bool m_done;
    bool m_accepted;
    std::wstring m_result;
};
