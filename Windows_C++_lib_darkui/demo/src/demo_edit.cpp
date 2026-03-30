#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiEditDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui edit demo";

enum ControlId {
    ID_EDIT_PRIMARY = 9001,
    ID_EDIT_SECONDARY = 9002,
    ID_BUTTON_APPLY = 9003
};

struct DemoState {
    darkui::Theme theme;
    darkui::Edit primaryEdit;
    darkui::Edit secondaryEdit;
    darkui::Button applyButton;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    std::wstring status = L"Ready";
};

void CleanupState(DemoState* state) {
    if (!state) return;
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(30, 33, 38);
    theme.border = RGB(58, 64, 74);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.editBackground = RGB(43, 47, 54);
    theme.editText = RGB(236, 239, 244);
    theme.editPlaceholder = RGB(128, 137, 150);
    theme.button = RGB(69, 95, 148);
    theme.buttonHover = RGB(82, 110, 168);
    theme.buttonHot = RGB(92, 122, 184);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.textPadding = 10;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);

    const int left = 32;
    const int width = client.right - 64;
    MoveWindow(state->primaryEdit.hwnd(), left, 104, width, 42, TRUE);
    MoveWindow(state->secondaryEdit.hwnd(), left, 164, width, 42, TRUE);
    MoveWindow(state->applyButton.hwnd(), left, 228, 132, 40, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->theme = MakeTheme();
        created->brushBackground = CreateSolidBrush(created->theme.background);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -30;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);
        created->textFont = darkui::CreateFont(created->theme.uiFont);
        if (!created->brushBackground || !created->titleFont || !created->textFont) {
            CleanupState(created);
            return -1;
        }

        if (!created->primaryEdit.Create(window, ID_EDIT_PRIMARY, L"A borderless dark edit control", created->theme) ||
            !created->secondaryEdit.Create(window, ID_EDIT_SECONDARY, L"", created->theme) ||
            !created->applyButton.Create(window, ID_BUTTON_APPLY, L"Apply", created->theme)) {
            CleanupState(created);
            return -1;
        }

        created->secondaryEdit.SetCueBanner(L"Type here. No border line should be visible.");
        created->primaryEdit.SetCornerRadius(16);
        created->secondaryEdit.SetCornerRadius(16);
        created->applyButton.SetCornerRadius(14);
        created->applyButton.SetSurfaceColor(created->theme.background);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == ID_BUTTON_APPLY) {
            state->status = L"Primary: " + state->primaryEdit.GetText() + L" | Secondary: " + state->secondaryEdit.GetText();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_PRIMARY && HIWORD(wParam) == EN_CHANGE) {
            state->status = L"Primary edit changed";
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_SECONDARY && HIWORD(wParam) == EN_CHANGE) {
            state->status = L"Secondary edit changed";
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if (state) {
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, state->brushBackground);
            return 1;
        }
        break;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            RECT client{};
            GetClientRect(window, &client);
            FillRect(dc, &client, state->brushBackground);

            RECT titleRect{32, 24, client.right - 32, 58};
            RECT descRect{32, 60, client.right - 32, 90};
            RECT noteRect{32, 288, client.right - 32, 350};
            RECT statusRect{32, client.bottom - 56, client.right - 32, client.bottom - 28};

            DrawLine(dc, state->titleFont, state->theme.text, titleRect, L"Dark Edit Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->textFont, state->theme.mutedText, descRect, L"Two rounded dark input fields with no visible native border line. Placeholder text color is theme-driven and remains available through the custom fallback overlay.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"Try focus changes, typing, selection, and IME input.\nThe control keeps native text behavior while removing the thin system border and showing a custom placeholder when empty.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            DrawLine(dc, state->textFont, state->theme.text, statusRect, state->status.c_str(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        CleanupState(state);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kDemoClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  820,
                                  420,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (!window) {
        return 0;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
