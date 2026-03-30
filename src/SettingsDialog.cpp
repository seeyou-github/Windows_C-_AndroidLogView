#include "SettingsDialog.h"

#include "DarkMode.h"

#include <shlobj.h>
#include <windows.h>

namespace {
constexpr int IDC_EXPORT_PATH_EDIT = 3001;
constexpr int IDC_BROWSE_BUTTON = 3002;
constexpr int IDC_SAVE_BUTTON = 3003;
constexpr int IDC_CANCEL_BUTTON = 3004;
constexpr int kMargin = 16;
constexpr int kEditHeight = 38;
constexpr int kButtonHeight = 34;

HBRUSH DialogBackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(DarkMode::kBackground);
    return brush;
}
}  // namespace

SettingsDialog::SettingsDialog(HINSTANCE instance)
    : m_instance(instance), m_hWnd(nullptr), m_font(nullptr), m_done(false), m_accepted(false) {
}

bool SettingsDialog::ShowModal(HWND parent, std::wstring& exportDirectory) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = m_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"AndroidLogViewerSettingsDialog";
    RegisterClassW(&wc);

    m_result = exportDirectory;

    RECT parentRect = {};
    GetWindowRect(parent, &parentRect);
    const int width = 700;
    const int height = 180;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;

    m_hWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, L"\x8BBE\x7F6E", WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
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
        exportDirectory = m_result;
    }
    return m_accepted;
}

LRESULT CALLBACK SettingsDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* dialog = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        dialog = static_cast<SettingsDialog*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
        dialog->m_hWnd = hWnd;
    } else {
        dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
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

void SettingsDialog::OnCreate() {
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

    CreateWindowExW(0, L"STATIC", L"\x5BFC\x51FAlog\x8DEF\x5F84\xFF1A", WS_CHILD | WS_VISIBLE, kMargin, 20, 120, 24, m_hWnd, nullptr,
                    m_instance, nullptr);
    m_exportPathEdit.Create(m_hWnd, IDC_EXPORT_PATH_EDIT, m_result, m_theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL);
    m_browseButton.Create(m_hWnd, IDC_BROWSE_BUTTON, L"\x6D4F\x89C8", m_theme);
    m_saveButton.Create(m_hWnd, IDC_SAVE_BUTTON, L"\x4FDD\x5B58", m_theme);
    m_cancelButton.Create(m_hWnd, IDC_CANCEL_BUTTON, L"\x53D6\x6D88", m_theme);

    m_exportPathEdit.SetCornerRadius(0);
    m_browseButton.SetCornerRadius(0);
    m_saveButton.SetCornerRadius(0);
    m_cancelButton.SetCornerRadius(0);
    m_browseButton.SetSurfaceColor(DarkMode::kBackground);
    m_saveButton.SetSurfaceColor(DarkMode::kBackground);
    m_cancelButton.SetSurfaceColor(DarkMode::kBackground);
    m_exportPathEdit.SetCueBanner(L"\x8BF7\x9009\x62E9\x5BFC\x51FA\x6587\x4EF6\x5939");

    const int clientWidth = 700;
    const int editWidth = 470;
    const int browseWidth = 90;
    const int gap = 10;
    const int buttonWidth = 92;

    MoveWindow(m_exportPathEdit.hwnd(), kMargin + 120, 14, editWidth, kEditHeight, TRUE);
    MoveWindow(m_browseButton.hwnd(), kMargin + 120 + editWidth + gap, 14, browseWidth, kButtonHeight, TRUE);
    MoveWindow(m_saveButton.hwnd(), clientWidth - kMargin - buttonWidth * 2 - gap, 96, buttonWidth, kButtonHeight, TRUE);
    MoveWindow(m_cancelButton.hwnd(), clientWidth - kMargin - buttonWidth, 96, buttonWidth, kButtonHeight, TRUE);
}

void SettingsDialog::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
    case IDC_BROWSE_BUTTON:
        OnBrowse();
        return;
    case IDC_SAVE_BUTTON:
        m_result = m_exportPathEdit.GetText();
        End(!m_result.empty());
        return;
    case IDC_CANCEL_BUTTON:
        End(false);
        return;
    default:
        return;
    }
}

void SettingsDialog::OnBrowse() {
    BROWSEINFOW info = {};
    info.hwndOwner = m_hWnd;
    info.lpszTitle = L"\x9009\x62E9\x5BFC\x51FAlog\x6587\x4EF6\x5939";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    PIDLIST_ABSOLUTE itemId = SHBrowseForFolderW(&info);
    if (itemId == nullptr) {
        return;
    }

    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(itemId, path)) {
        m_exportPathEdit.SetText(path);
    }
    CoTaskMemFree(itemId);
}

void SettingsDialog::End(bool accepted) {
    m_done = true;
    m_accepted = accepted;
    DestroyWindow(m_hWnd);
}
