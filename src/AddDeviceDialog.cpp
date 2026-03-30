#include "AddDeviceDialog.h"

#include "DarkMode.h"

#include <windows.h>

namespace {
constexpr int IDC_IP_EDIT_1 = 2001;
constexpr int IDC_IP_EDIT_2 = 2002;
constexpr int IDC_IP_EDIT_3 = 2003;
constexpr int IDC_IP_EDIT_4 = 2004;
constexpr int IDC_PORT_EDIT = 2005;
constexpr int IDC_OK_BUTTON = 2006;
constexpr int IDC_CANCEL_BUTTON = 2007;
constexpr int kMargin = 16;
constexpr int kEditHeight = 38;
constexpr int kButtonHeight = 34;

HBRUSH DialogBackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(DarkMode::kBackground);
    return brush;
}
}

AddDeviceDialog::AddDeviceDialog(HINSTANCE instance)
    : m_instance(instance), m_hWnd(nullptr), m_font(nullptr), m_done(false), m_accepted(false) {
}

bool AddDeviceDialog::ShowModal(HWND parent, std::wstring& outAddress) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = m_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"AddDeviceDialogWindow";
    RegisterClassW(&wc);

    RECT parentRect = {};
    GetWindowRect(parent, &parentRect);
    const int width = 520;
    const int height = 170;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;

    m_hWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, L"添加设备", WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
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
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), DarkMode::kText);
        SetBkColor(reinterpret_cast<HDC>(wParam), DarkMode::kBackground);
        return reinterpret_cast<LRESULT>(DialogBackgroundBrush());
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, DialogBackgroundBrush());
        return 1;
    }
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

    m_theme.background = DarkMode::kBackground;
    m_theme.panel = DarkMode::kSurface;
    m_theme.border = DarkMode::kBorder;
    m_theme.text = DarkMode::kText;
    m_theme.mutedText = DarkMode::kMutedText;
    m_theme.button = RGB(58, 64, 72);
    m_theme.buttonHover = RGB(72, 80, 90);
    m_theme.buttonHot = RGB(82, 92, 104);
    m_theme.buttonDisabled = RGB(46, 50, 56);
    m_theme.buttonDisabledText = RGB(128, 134, 142);
    m_theme.editBackground = DarkMode::kSurfaceAlt;
    m_theme.editText = DarkMode::kText;
    m_theme.editPlaceholder = RGB(142, 149, 160);
    m_theme.uiFont.family = metrics.lfMessageFont.lfFaceName;
    m_theme.uiFont.height = metrics.lfMessageFont.lfHeight;
    m_theme.uiFont.weight = metrics.lfMessageFont.lfWeight;
    m_theme.uiFont.italic = metrics.lfMessageFont.lfItalic != FALSE;

    m_ipEdit1.Create(m_hWnd, IDC_IP_EDIT_1, L"192", m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER);
    m_ipEdit2.Create(m_hWnd, IDC_IP_EDIT_2, L"168", m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER);
    m_ipEdit3.Create(m_hWnd, IDC_IP_EDIT_3, L"", m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER);
    m_ipEdit4.Create(m_hWnd, IDC_IP_EDIT_4, L"", m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER);
    m_portEdit.Create(m_hWnd, IDC_PORT_EDIT, L"", m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER);
    m_okButton.Create(m_hWnd, IDC_OK_BUTTON, L"连接", m_theme);
    m_cancelButton.Create(m_hWnd, IDC_CANCEL_BUTTON, L"取消", m_theme);

    m_ipEdit1.SetCornerRadius(0);
    m_ipEdit2.SetCornerRadius(0);
    m_ipEdit3.SetCornerRadius(0);
    m_ipEdit4.SetCornerRadius(0);
    m_portEdit.SetCornerRadius(0);
    m_okButton.SetCornerRadius(0);
    m_cancelButton.SetCornerRadius(0);
    m_okButton.SetSurfaceColor(DarkMode::kBackground);
    m_cancelButton.SetSurfaceColor(DarkMode::kBackground);
    m_portEdit.SetCueBanner(L"端口");

    const int clientWidth = 520;
    const int ipWidth = 64;
    const int dotWidth = 10;
    const int colonWidth = 12;
    const int gap = 8;
    const int portWidth = 86;
    const int row1Y = 20;
    const int row2Y = 86;
    const int buttonWidth = 92;

    int x = kMargin;
    MoveWindow(m_ipEdit1.hwnd(), x, row1Y, ipWidth, kEditHeight, TRUE);
    x += ipWidth;
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, x, row1Y + 8, dotWidth, 22, m_hWnd, nullptr, m_instance, nullptr);
    x += dotWidth + gap;
    MoveWindow(m_ipEdit2.hwnd(), x, row1Y, ipWidth, kEditHeight, TRUE);
    x += ipWidth;
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, x, row1Y + 8, dotWidth, 22, m_hWnd, nullptr, m_instance, nullptr);
    x += dotWidth + gap;
    MoveWindow(m_ipEdit3.hwnd(), x, row1Y, ipWidth, kEditHeight, TRUE);
    x += ipWidth;
    CreateWindowExW(0, L"STATIC", L".", WS_CHILD | WS_VISIBLE, x, row1Y + 8, dotWidth, 22, m_hWnd, nullptr, m_instance, nullptr);
    x += dotWidth + gap;
    MoveWindow(m_ipEdit4.hwnd(), x, row1Y, ipWidth, kEditHeight, TRUE);
    x += ipWidth + gap;
    CreateWindowExW(0, L"STATIC", L":", WS_CHILD | WS_VISIBLE, x, row1Y + 8, colonWidth, 22, m_hWnd, nullptr, m_instance, nullptr);
    x += colonWidth + gap;
    MoveWindow(m_portEdit.hwnd(), x, row1Y, portWidth, kEditHeight, TRUE);

    MoveWindow(m_okButton.hwnd(), clientWidth - kMargin - buttonWidth * 2 - gap, row2Y, buttonWidth, kButtonHeight, TRUE);
    MoveWindow(m_cancelButton.hwnd(), clientWidth - kMargin - buttonWidth, row2Y, buttonWidth, kButtonHeight, TRUE);

    SetFocus(m_ipEdit3.edit_hwnd());
}

void AddDeviceDialog::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
    case IDC_OK_BUTTON: {
        const std::wstring a = m_ipEdit1.GetText();
        const std::wstring b = m_ipEdit2.GetText();
        const std::wstring c = m_ipEdit3.GetText();
        const std::wstring d = m_ipEdit4.GetText();
        const std::wstring port = m_portEdit.GetText();
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
