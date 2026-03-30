#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiToolbarDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui toolbar demo";

enum ControlId {
    ID_TOOLBAR = 8001,
    ID_CMD_NEW = 8101,
    ID_CMD_OPEN,
    ID_CMD_SHARE,
    ID_CMD_LAYOUT,
    ID_CMD_EXPORT
};

struct DemoState {
    darkui::Theme theme;
    darkui::Toolbar toolbar;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    std::wstring status = L"Ready";
    HICON iconNew = nullptr;
    HICON iconOpen = nullptr;
    HICON iconShare = nullptr;
    HICON iconLayout = nullptr;
    HICON iconExport = nullptr;
    HICON iconSearch = nullptr;
    HICON iconPin = nullptr;
    HMENU layoutMenu = nullptr;
};

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    if (state->layoutMenu) DestroyMenu(state->layoutMenu);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(33, 36, 41);
    theme.border = RGB(60, 66, 76);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(142, 149, 160);
    theme.toolbarBackground = RGB(25, 28, 32);
    theme.toolbarItem = RGB(46, 51, 58);
    theme.toolbarItemHot = RGB(64, 71, 82);
    theme.toolbarItemActive = RGB(78, 120, 184);
    theme.toolbarText = RGB(228, 232, 238);
    theme.toolbarTextActive = RGB(248, 250, 252);
    theme.toolbarSeparator = RGB(70, 76, 86);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -19;
    theme.toolbarHeight = 44;
    theme.textPadding = 12;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->toolbar.hwnd(), 28, 92, client.right - 56, state->theme.toolbarHeight + 12, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
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

        if (!created->toolbar.Create(window, ID_TOOLBAR, created->theme)) {
            CleanupState(created);
            return -1;
        }
        created->iconNew = LoadIconW(nullptr, IDI_APPLICATION);
        created->iconOpen = LoadIconW(nullptr, IDI_INFORMATION);
        created->iconShare = LoadIconW(nullptr, IDI_QUESTION);
        created->iconLayout = LoadIconW(nullptr, IDI_WARNING);
        created->iconExport = LoadIconW(nullptr, IDI_ERROR);
        created->iconSearch = LoadIconW(nullptr, IDI_INFORMATION);
        created->iconPin = LoadIconW(nullptr, IDI_SHIELD);
        created->layoutMenu = CreatePopupMenu();
        if (created->layoutMenu) {
            AppendMenuW(created->layoutMenu, MF_STRING, ID_CMD_LAYOUT + 100, L"Grid Layout");
            AppendMenuW(created->layoutMenu, MF_STRING, ID_CMD_LAYOUT + 101, L"Focus Layout");
            AppendMenuW(created->layoutMenu, MF_STRING, ID_CMD_LAYOUT + 102, L"Review Layout");
        }
        created->toolbar.SetItems({
            {L"New", ID_CMD_NEW, created->iconNew, nullptr, 0, false, true, false, false, false, false},
            {L"Open", ID_CMD_OPEN, created->iconOpen, nullptr, 0, false, false, false, false, false, false},
            {L"", 0, nullptr, nullptr, 0, true, false, false, false, false, false},
            {L"Search", 8201, created->iconSearch, nullptr, 0, false, false, false, false, true, false},
            {L"Pin", 8202, created->iconPin, nullptr, 0, false, false, false, false, true, false},
            {L"Share", ID_CMD_SHARE, created->iconShare, nullptr, 0, false, false, false, true, false, false},
            {L"Layout", ID_CMD_LAYOUT, created->iconLayout, created->layoutMenu, 0, false, false, false, true, false, true},
            {L"", 0, nullptr, nullptr, 0, true, false, false, true, false, false},
            {L"Export", ID_CMD_EXPORT, created->iconExport, nullptr, 0, false, false, true, true, false, false}
        });

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
        if (state) {
            switch (LOWORD(wParam)) {
            case ID_CMD_NEW:
                state->status = L"New session created";
                state->toolbar.SetChecked(0, true);
                state->toolbar.SetChecked(1, false);
                return 0;
            case ID_CMD_OPEN:
                state->status = L"Library opened";
                state->toolbar.SetChecked(0, false);
                state->toolbar.SetChecked(1, true);
                return 0;
            case ID_CMD_SHARE:
                state->status = L"Share sheet opened";
                return 0;
            case ID_CMD_LAYOUT:
                state->status = L"Layout menu opened";
                return 0;
            case ID_CMD_LAYOUT + 100:
                state->status = L"Grid layout selected";
                return 0;
            case ID_CMD_LAYOUT + 101:
                state->status = L"Focus layout selected";
                return 0;
            case ID_CMD_LAYOUT + 102:
                state->status = L"Review layout selected";
                return 0;
            case 8201:
                state->status = L"Search activated";
                return 0;
            case 8202:
                state->status = L"Pinned current view";
                return 0;
            default:
                break;
            }
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

            RECT titleRect{28, 24, client.right - 28, 56};
            RECT descRect{28, 58, client.right - 28, 82};
            RECT noteRect{28, 164, client.right - 28, 280};
            RECT statusRect{28, client.bottom - 56, client.right - 28, client.bottom - 28};

            DrawLine(dc, state->titleFont, state->theme.text, titleRect, L"Dark Toolbar Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->textFont, state->theme.mutedText, descRect, L"Custom dark toolbar with icons, icon-only tools, a drop-down button, a right-aligned tool group, and overflow handling.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"Try the toolbar buttons above:\n- Left group holds primary actions.\n- The two center square buttons are icon-only items.\n- Layout opens a drop-down menu.\n- Resize the window narrower to force overflow into the ... button on the right edge.",
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
                                  900,
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
