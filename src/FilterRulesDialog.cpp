#include "FilterRulesDialog.h"

#include "DarkMode.h"

#include <algorithm>
#include <windows.h>

namespace {
constexpr int IDC_CLOSE_BUTTON = 4002;
constexpr int IDC_RULES_SCROLLBAR = 4003;
constexpr int kMargin = 16;
constexpr int kScrollBarWidth = 14;
constexpr int kButtonWidth = 96;
constexpr int kButtonHeight = 34;
constexpr int kBottomAreaHeight = 56;

const wchar_t* kHelpText = LR"(ADB过滤规则
基本格式
<tag>:<priority>

多个规则直接用空格分开。
* 不是通配符匹配 tag 名称，而是“所有未单独指定的 tag”。

比如：
MyApp:D *:S

意思是：
- MyApp 显示 Debug+
- 其他所有 tag 全部静默

相当于“只看某个 tag”。

最常见用法
1. 只看错误以上：
*:E

2. 默认只看 Info+：
*:I

3. 只看某个 tag：
MyApp:D *:S

4. 只看两个 tag：
MyApp:D AndroidRuntime:E *:S

5. 看系统启动关键日志：
ActivityManager:I ActivityTaskManager:I PackageManager:I *:S

6. 看崩溃：
AndroidRuntime:E DEBUG:E libc:F *:S

关键词过滤规则
1. 支持多个关键字用 | 分隔。
2. 支持通配符 * 和 ?。
3. 不区分大小写，会在 message、tag、raw line 中匹配。
4. 示例：error|fatal
5. 示例：*timeout*|*crash*

排除关键词规则
1. 规则与关键词过滤完全一样。
2. 只要某行 log 匹配到排除规则，就不显示。
3. 示例：*chatty*|BufferQueue
4. 示例：Audio*|Surface*
)";

HBRUSH DialogBackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(DarkMode::kBackground);
    return brush;
}
}  // namespace

FilterRulesDialog::FilterRulesDialog(HINSTANCE instance)
    : m_instance(instance),
      m_hWnd(nullptr),
      m_font(nullptr),
      m_textRect{0, 0, 0, 0},
      m_scrollPos(0),
      m_contentHeight(0),
      m_done(false) {
}

void FilterRulesDialog::ShowModal(HWND parent) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = m_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"AndroidLogViewerFilterRulesDialog";
    RegisterClassW(&wc);

    RECT parentRect = {};
    GetWindowRect(parent, &parentRect);
    const int width = 840;
    const int height = 700;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;

    m_hWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, L"过滤规则说明",
                             WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE, x, y, width, height, parent, nullptr, m_instance, this);
    if (m_hWnd == nullptr) {
        return;
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
}

LRESULT CALLBACK FilterRulesDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    FilterRulesDialog* dialog = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        dialog = static_cast<FilterRulesDialog*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
        dialog->m_hWnd = hWnd;
    } else {
        dialog = reinterpret_cast<FilterRulesDialog*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (dialog == nullptr) {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        dialog->OnCreate();
        return 0;
    case WM_SIZE:
        dialog->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_VSCROLL:
        dialog->OnScroll(wParam, lParam);
        return 0;
    case WM_MOUSEWHEEL:
        dialog->OnMouseWheel(wParam);
        return 0;
    case WM_COMMAND:
        dialog->OnCommand(wParam);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hWnd, &ps);
        dialog->Paint(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        dialog->End();
        return 0;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

void FilterRulesDialog::OnCreate() {
    DarkMode::ApplyWindowDarkMode(m_hWnd);

    LOGFONTW fontSpec = {};
    wcscpy_s(fontSpec.lfFaceName, L"Segoe UI");
    fontSpec.lfHeight = -28;
    fontSpec.lfWeight = FW_NORMAL;
    m_font = CreateFontIndirectW(&fontSpec);

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
    m_theme.scrollBarBackground = DarkMode::kBackground;
    m_theme.scrollBarTrack = RGB(52, 56, 62);
    m_theme.scrollBarThumb = RGB(120, 128, 140);
    m_theme.scrollBarThumbHot = RGB(150, 160, 174);
    m_theme.uiFont.family = L"Segoe UI";
    m_theme.uiFont.height = -18;
    m_theme.uiFont.weight = FW_NORMAL;

    m_closeButton.Create(m_hWnd, IDC_CLOSE_BUTTON, L"关闭", m_theme);
    m_closeButton.SetCornerRadius(0);
    m_closeButton.SetSurfaceColor(DarkMode::kBackground);
    m_scrollBar.Create(m_hWnd, IDC_RULES_SCROLLBAR, true, m_theme);

    RECT client = {};
    GetClientRect(m_hWnd, &client);
    OnSize(client.right - client.left, client.bottom - client.top);
}

void FilterRulesDialog::OnSize(int width, int height) {
    const int textBottom = height - kBottomAreaHeight;
    m_textRect.left = kMargin;
    m_textRect.top = kMargin;
    m_textRect.right = width - kMargin - kScrollBarWidth - 8;
    m_textRect.bottom = textBottom;

    if (m_scrollBar.hwnd() != nullptr) {
        MoveWindow(m_scrollBar.hwnd(), m_textRect.right + 8, m_textRect.top, kScrollBarWidth, m_textRect.bottom - m_textRect.top, TRUE);
    }
    if (m_closeButton.hwnd() != nullptr) {
        MoveWindow(m_closeButton.hwnd(), width - kMargin - kButtonWidth, height - kMargin - kButtonHeight, kButtonWidth, kButtonHeight, TRUE);
    }

    UpdateScrollMetrics();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void FilterRulesDialog::OnCommand(WPARAM wParam) {
    if (LOWORD(wParam) == IDC_CLOSE_BUTTON) {
        End();
    }
}

void FilterRulesDialog::OnScroll(WPARAM, LPARAM lParam) {
    if (reinterpret_cast<HWND>(lParam) != m_scrollBar.hwnd()) {
        return;
    }
    m_scrollPos = m_scrollBar.GetValue();
    InvalidateRect(m_hWnd, &m_textRect, FALSE);
}

void FilterRulesDialog::OnMouseWheel(WPARAM wParam) {
    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta == 0) {
        return;
    }
    const int lineStep = 48;
    const int next = m_scrollPos - (delta / WHEEL_DELTA) * lineStep;
    m_scrollBar.SetValue(next, false);
    m_scrollPos = m_scrollBar.GetValue();
    InvalidateRect(m_hWnd, &m_textRect, FALSE);
}

void FilterRulesDialog::UpdateScrollMetrics() {
    HDC hdc = GetDC(m_hWnd);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font));
    RECT calcRect = {0, 0, std::max(100, static_cast<int>(m_textRect.right - m_textRect.left - 16)), 0};
    DrawTextW(hdc, kHelpText, -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(hdc, oldFont);
    ReleaseDC(m_hWnd, hdc);

    m_contentHeight = calcRect.bottom - calcRect.top + 16;
    const int visibleHeight = std::max(1, static_cast<int>(m_textRect.bottom - m_textRect.top));
    m_scrollBar.SetRange(0, std::max(visibleHeight, m_contentHeight));
    m_scrollBar.SetPageSize(visibleHeight);
    m_scrollBar.SetValue(m_scrollPos, false);
    m_scrollPos = m_scrollBar.GetValue();
}

void FilterRulesDialog::Paint(HDC hdc) {
    RECT client = {};
    GetClientRect(m_hWnd, &client);
    FillRect(hdc, &client, DialogBackgroundBrush());

    HBRUSH panelBrush = CreateSolidBrush(DarkMode::kSurface);
    FillRect(hdc, &m_textRect, panelBrush);
    DeleteObject(panelBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, DarkMode::kBorder);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, m_textRect.left - 1, m_textRect.top - 1, m_textRect.right + 1, m_textRect.bottom + 1);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    HRGN clip = CreateRectRgn(m_textRect.left, m_textRect.top, m_textRect.right, m_textRect.bottom);
    SelectClipRgn(hdc, clip);
    DeleteObject(clip);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kText);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font));
    RECT drawRect = m_textRect;
    drawRect.left += 10;
    drawRect.right -= 10;
    drawRect.top = m_textRect.top + 10 - m_scrollPos;
    DrawTextW(hdc, kHelpText, -1, &drawRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    SelectObject(hdc, oldFont);
    SelectClipRgn(hdc, nullptr);
}

void FilterRulesDialog::End() {
    m_done = true;
    DestroyWindow(m_hWnd);
}
