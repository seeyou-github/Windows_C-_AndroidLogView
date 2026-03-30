#include "DarkMode.h"
#include "MainWindow.h"
#include "ResourceStrings.h"

#include <commctrl.h>
#include <windows.h>

#include "../res/resource.h"

namespace {
HBRUSH g_classBrush = nullptr;

void ShowWindowWithoutWhiteFlash(HWND hWnd, int showCommand) {
    const LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);

    ShowWindow(hWnd, showCommand);
    UpdateWindow(hWnd);
    RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);

    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    g_classBrush = CreateSolidBrush(DarkMode::kBackground);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = instance;
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_classBrush;
    wc.lpszClassName = L"AndroidLogViewerMainWindow";

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    const int clientWidth = 1380;
    const int clientHeight = 860;
    RECT rect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE, 0);

    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int windowWidth = rect.right - rect.left;
    const int windowHeight = rect.bottom - rect.top;
    const int x = workArea.left + ((workArea.right - workArea.left) - windowWidth) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - windowHeight) / 2;

    MainWindow window(instance);
    const std::wstring title = ResourceStrings::Load(instance, IDS_APP_TITLE);
    if (!window.Create(x, y, windowWidth, windowHeight, title.c_str())) {
        DeleteObject(g_classBrush);
        return 1;
    }

    const HICON bigIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    const HICON smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (bigIcon != nullptr) {
        SendMessageW(window.Window(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    }
    if (smallIcon != nullptr) {
        SendMessageW(window.Window(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }

    ShowWindowWithoutWhiteFlash(window.Window(), showCommand);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_classBrush);
    return static_cast<int>(msg.wParam);
}
