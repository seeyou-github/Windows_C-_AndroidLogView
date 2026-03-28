#include "AddDeviceDialog.h"

#include <windows.h>

namespace {
constexpr int IDC_IP_EDIT_1 = 2001;
constexpr int IDC_IP_EDIT_2 = 2002;
constexpr int IDC_IP_EDIT_3 = 2003;
constexpr int IDC_IP_EDIT_4 = 2004;
constexpr int IDC_OK_BUTTON = 2005;
constexpr int IDC_CANCEL_BUTTON = 2006;
}

AddDeviceDialog::AddDeviceDialog(HINSTANCE instance)
    : m_instance(instance), m_hWnd(nullptr), m_hIpEdit1(nullptr), m_hIpEdit2(nullptr), m_hIpEdit3(nullptr), m_hIpEdit4(nullptr),
      m_hPortEdit(nullptr), m_hOkButton(nullptr), m_hCancelButton(nullptr),
      m_font(nullptr), m_done(false), m_accepted(false) {
}

bool AddDeviceDialog::ShowModal(HWND parent, std::wstring& outAddress) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = m_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AddDeviceDialogWindow";
    RegisterClassW(&wc);

    RECT parentRect = {};
    GetWindowRect(parent, &parentRect);
    const int width = 440;
    const int height = 200;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;

    m_hWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, L"Add Network Device", WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
                             x, y, width, height, parent, nullptr, m_instance, this);
    if (m_hWnd == nullptr) {
        return false;
    }

    EnableWindow(parent, FALSE);
    MSG msg = {};
    while (!m_done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(m_hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);

    if (m_font != nullptr) {
        DeleteObject(m_font);
        m_font = nullptr;
    }

    if (m_accepted) {
        outAddress = m_result;
    }
    return m_accepted;
}

LRESULT CALLBACK AddDeviceDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AddDeviceDialog* dialog = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        dialog = static_cast<AddDeviceDialog*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
        dialog->m_hWnd = hWnd;
    } else {
        dialog = reinterpret_cast<AddDeviceDialog*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (dialog == nullptr) {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        dialog->OnCreate();
        return 0;
    case WM_COMMAND:
        dialog->OnCommand(wParam);
        return 0;
    case WM_CLOSE:
        dialog->End(false);
        return 0;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

void AddDeviceDialog::OnCreate() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    m_font = CreateFontIndirectW(&metrics.lfMessageFont);

    CreateWindowExW(0, L"STATIC", L"IP Address:", WS_CHILD | WS_VISIBLE, 16, 18, 90, 22, m_hWnd, nullptr, m_instance, nullptr);
    m_hIpEdit1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"192", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 112, 16, 48, 24, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_IP_EDIT_1), m_instance, nullptr);
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, 165, 18, 8, 22, m_hWnd, nullptr, m_instance, nullptr);
    m_hIpEdit2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"168", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 178, 16, 48, 24, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_IP_EDIT_2), m_instance, nullptr);
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, 231, 18, 8, 22, m_hWnd, nullptr, m_instance, nullptr);
    m_hIpEdit3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 244, 16, 48, 24, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_IP_EDIT_3), m_instance, nullptr);
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, 297, 18, 8, 22, m_hWnd, nullptr, m_instance, nullptr);
    m_hIpEdit4 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 310, 16, 48, 24, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_IP_EDIT_4), m_instance, nullptr);
    CreateWindowExW(0, L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE, 16, 58, 90, 22, m_hWnd, nullptr, m_instance, nullptr);
    m_hPortEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"5555", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 112, 56, 90, 24, m_hWnd,
                                  reinterpret_cast<HMENU>(2100), m_instance, nullptr);
    m_hOkButton = CreateWindowExW(0, L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 234, 126, 80, 28, m_hWnd,
                                  reinterpret_cast<HMENU>(IDC_OK_BUTTON), m_instance, nullptr);
    m_hCancelButton = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 324, 126, 80, 28, m_hWnd,
                                      reinterpret_cast<HMENU>(IDC_CANCEL_BUTTON), m_instance, nullptr);

    const HWND controls[] = {m_hIpEdit1, m_hIpEdit2, m_hIpEdit3, m_hIpEdit4, m_hPortEdit, m_hOkButton, m_hCancelButton};
    for (HWND control : controls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    }
    SetFocus(m_hIpEdit3);
}

void AddDeviceDialog::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
    case IDC_OK_BUTTON: {
        const std::wstring a = ReadText(m_hIpEdit1);
        const std::wstring b = ReadText(m_hIpEdit2);
        const std::wstring c = ReadText(m_hIpEdit3);
        const std::wstring d = ReadText(m_hIpEdit4);
        const std::wstring port = ReadText(m_hPortEdit);
        if (!a.empty() && !b.empty() && !c.empty() && !d.empty() && !port.empty()) {
            m_result = a + L"." + b + L"." + c + L"." + d + L":" + port;
            End(true);
        }
        return;
    }
    case IDC_CANCEL_BUTTON:
        End(false);
        return;
    default:
        return;
    }
}

void AddDeviceDialog::End(bool accepted) {
    m_done = true;
    m_accepted = accepted;
    DestroyWindow(m_hWnd);
}

std::wstring AddDeviceDialog::ReadText(HWND hWnd) const {
    const int length = GetWindowTextLengthW(hWnd);
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    GetWindowTextW(hWnd, text.data(), length + 1);
    return text;
}
