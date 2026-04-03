#include "MainWindow.h"

#include "AddDeviceDialog.h"
#include "DarkMode.h"
#include "FilterRulesDialog.h"
#include "LogParser.h"
#include "ResourceStrings.h"
#include "SettingsDialog.h"

#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <thread>

#include "../res/resource.h"

namespace {
constexpr int kToolbarHeight = 108;
constexpr int kControlHeight = 38;
constexpr int kMargin = 12;
constexpr UINT_PTR kFlushTimerId = 1001;
constexpr UINT kFlushIntervalMs = 50;
constexpr UINT kAppStatusMessage = WM_APP + 1;
constexpr UINT kDeviceConnectResultMessage = WM_APP + 2;
constexpr UINT kToolbarAdbCommandResultMessage = WM_APP + 3;
constexpr UINT kDeviceListResultMessage = WM_APP + 4;
constexpr std::size_t kMaxLogEntries = 200000;

struct DeviceConnectResult {
    std::wstring address;
    std::wstring status;
    bool success = false;
    bool timedOut = false;
};

struct ToolbarAdbCommandResult {
    int commandId = 0;
    std::wstring actionName;
    std::wstring arguments;
    std::wstring status;
    bool success = false;
};

struct DeviceListResult {
    std::vector<AdbClient::DeviceInfo> devices;
    std::wstring currentSelection;
    bool keepSelection = false;
};

std::wstring FormatStatusLine(const std::wstring& baseText, std::size_t totalCount, std::size_t visibleCount, bool paused) {
    return baseText + (paused ? L"   [PAUSED]" : L"") + L"   Total: " + std::to_wstring(totalCount) + L"   Visible: " +
           std::to_wstring(visibleCount);
}

std::wstring GetExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    const std::size_t pos = full.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : full.substr(0, pos);
}

RECT GetMonitorWorkAreaForWindow(HWND hWnd) {
    RECT workArea = {};
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo)) {
        workArea = monitorInfo.rcWork;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }
    return workArea;
}

bool IsOnlineDeviceLabel(const std::wstring& text) {
    return text.rfind(L"* ", 0) == 0;
}

bool IsSavedNetworkDeviceLabel(const std::wstring& text) {
    return text.find(L" [saved]") != std::wstring::npos;
}

bool LooksLikeNetworkDeviceSerial(const std::wstring& serial) {
    return serial.find(L':') != std::wstring::npos;
}

std::wstring ExtractDeviceSerialFromLabel(const std::wstring& text) {
    const std::size_t pos = text.find(L" [");
    std::wstring serial = pos == std::wstring::npos ? text : text.substr(0, pos);
    if (serial.rfind(L"* ", 0) == 0) {
        serial = serial.substr(2);
    }
    return serial;
}
}  // namespace

MainWindow::MainWindow(HINSTANCE instance)
    : m_instance(instance),
      m_hWnd(nullptr),
      m_hToolbarPanel(nullptr),
      m_hDeviceCombo(nullptr),
      m_hBufferCombo(nullptr),
      m_hAdbFilterEdit(nullptr),
      m_hApplyAdbFilterButton(nullptr),
      m_hKeywordEdit(nullptr),
      m_hTagEdit(nullptr),
      m_hPidEdit(nullptr),
      m_hExcludeKeywordEdit(nullptr),
      m_hLevelCombo(nullptr),
      m_hListView(nullptr),
      m_hListHeader(nullptr),
      m_hStatusLabel(nullptr),
      m_hPopupHost(nullptr),
      m_hPopupList(nullptr),
      m_hActivePicker(nullptr),
      m_uiFont(nullptr),
      m_monoFont(nullptr),
      m_bgBrush(nullptr),
      m_surfaceBrush(nullptr),
      m_surfaceAltBrush(nullptr),
      m_borderPen(nullptr),
      m_focusPen(nullptr),
      m_config(GetExeDir()),
      m_logBuffer(kMaxLogEntries),
      m_pauseToolbarIndex(-1),
      m_selectedDeviceIndex(0),
      m_selectedBufferIndex(0),
      m_selectedLevelIndex(0),
      m_lastToolbarSuccessCommandId(0),
      m_pendingWindowWidth(1380),
      m_pendingWindowHeight(860),
      m_exportDirectory(),
      m_startAfterDeviceConnect(false),
      m_loadingConfig(false),
      m_windowBoundsDirty(false),
      m_toolbarCommandInProgress(false),
      m_deviceConnectInProgress(false),
      m_paused(false) {
}

MainWindow::~MainWindow() {
    if (m_uiFont != nullptr) DeleteObject(m_uiFont);
    if (m_monoFont != nullptr) DeleteObject(m_monoFont);
    if (m_bgBrush != nullptr) DeleteObject(m_bgBrush);
    if (m_surfaceBrush != nullptr) DeleteObject(m_surfaceBrush);
    if (m_surfaceAltBrush != nullptr) DeleteObject(m_surfaceAltBrush);
    if (m_borderPen != nullptr) DeleteObject(m_borderPen);
    if (m_focusPen != nullptr) DeleteObject(m_focusPen);
}

bool MainWindow::Create(int x, int y, int width, int height, const wchar_t* title) {
    m_hWnd = CreateWindowExW(0, L"AndroidLogViewerMainWindow", title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, width, height, nullptr,
                             nullptr, m_instance, this);
    if (m_hWnd != nullptr) {
        DarkMode::ApplyWindowDarkMode(m_hWnd);
    }
    return m_hWnd != nullptr;
}

HWND MainWindow::Window() const {
    return m_hWnd;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        window = static_cast<MainWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hWnd = hWnd;
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (window == nullptr) {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        window->OnCreate();
        return 0;
    case WM_SIZE:
        window->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_EXITSIZEMOVE:
        window->UpdatePendingWindowBounds();
        return 0;
    case WM_COMMAND:
        window->OnCommand(wParam, lParam);
        return 0;
    case WM_NOTIFY:
        return window->HandleNotify(lParam);
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            window->HidePickerPopup();
        }
        return 0;
    case WM_TIMER:
        window->OnTimer(wParam);
        return 0;
    case WM_DRAWITEM:
        return window->HandleDrawItem(wParam, lParam);
    case WM_MEASUREITEM:
        return window->HandleMeasureItem(lParam);
    case WM_CONTEXTMENU:
        return window->HandleContextMenu(wParam, lParam);
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hWnd, &ps);
        window->PaintCustomChrome(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return window->HandleControlColor(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
    case WM_ERASEBKGND: {
        RECT rc = {};
        GetClientRect(hWnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, window->m_bgBrush);
        return 1;
    }
    case kAppStatusMessage:
        return window->HandleAppStatusMessage(lParam);
    case kDeviceListResultMessage:
        return window->HandleDeviceListResultMessage(lParam);
    case kDeviceConnectResultMessage:
        return window->HandleDeviceConnectResultMessage(lParam);
    case kToolbarAdbCommandResultMessage:
        return window->HandleToolbarAdbCommandResultMessage(lParam);
    case WM_DESTROY:
        window->OnDestroy();
        return 0;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

void MainWindow::InitThemeResources() {
    m_bgBrush = CreateSolidBrush(DarkMode::kBackground);
    m_surfaceBrush = CreateSolidBrush(DarkMode::kSurface);
    m_surfaceAltBrush = CreateSolidBrush(DarkMode::kSurfaceAlt);
    m_borderPen = CreatePen(PS_SOLID, 1, DarkMode::kBorder);
    m_focusPen = CreatePen(PS_SOLID, 1, RGB(87, 138, 220));

    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);

    m_uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
    LOGFONTW mono = metrics.lfMessageFont;
    wcscpy_s(mono.lfFaceName, L"Consolas");
    mono.lfHeight = -16;
    m_monoFont = CreateFontIndirectW(&mono);

    m_darkTheme.background = DarkMode::kBackground;
    m_darkTheme.panel = DarkMode::kSurface;
    m_darkTheme.border = DarkMode::kBorder;
    m_darkTheme.text = DarkMode::kText;
    m_darkTheme.mutedText = DarkMode::kMutedText;
    m_darkTheme.button = RGB(56, 60, 66);
    m_darkTheme.buttonHover = RGB(68, 74, 82);
    m_darkTheme.buttonHot = RGB(78, 86, 98);
    m_darkTheme.buttonDisabled = RGB(46, 50, 56);
    m_darkTheme.buttonDisabledText = RGB(128, 134, 142);
    m_darkTheme.editBackground = DarkMode::kSurfaceAlt;
    m_darkTheme.editText = DarkMode::kText;
    m_darkTheme.editPlaceholder = RGB(142, 149, 160);
    m_darkTheme.arrow = DarkMode::kMutedText;
    m_darkTheme.popupItem = RGB(48, 52, 58);
    m_darkTheme.popupItemHot = RGB(70, 78, 92);
    m_darkTheme.popupAccentItem = RGB(40, 72, 124);
    m_darkTheme.popupAccentItemHot = RGB(56, 92, 148);
    m_darkTheme.toolbarBackground = DarkMode::kSurface;
    m_darkTheme.toolbarItem = RGB(66, 72, 80);
    m_darkTheme.toolbarItemHot = RGB(68, 74, 82);
    m_darkTheme.toolbarItemActive = RGB(54, 128, 74);
    m_darkTheme.toolbarText = DarkMode::kText;
    m_darkTheme.toolbarTextActive = RGB(245, 247, 250);
    m_darkTheme.toolbarSeparator = DarkMode::kBorder;
    m_darkTheme.uiFont.family = metrics.lfMessageFont.lfFaceName;
    m_darkTheme.uiFont.height = metrics.lfMessageFont.lfHeight;
    m_darkTheme.uiFont.weight = metrics.lfMessageFont.lfWeight;
    m_darkTheme.uiFont.italic = metrics.lfMessageFont.lfItalic != FALSE;
    m_darkTheme.itemHeight = 28;
    m_darkTheme.toolbarHeight = 38;
    m_darkTheme.textPadding = 12;
    m_darkTheme.popupOffsetY = 2;
}

void MainWindow::CreateControls() {
    m_hToolbarPanel = nullptr;
    m_deviceComboControl.Create(m_hWnd, IDC_DEVICE_COMBO, m_darkTheme);
    m_hDeviceCombo = m_deviceComboControl.hwnd();
    m_bufferComboControl.Create(m_hWnd, IDC_BUFFER_COMBO, m_darkTheme);
    m_hBufferCombo = m_bufferComboControl.hwnd();
    m_adbFilterEditControl.Create(m_hWnd, IDC_ADB_FILTER_EDIT, L"", m_darkTheme);
    m_hApplyAdbFilterButton =
        CreateWindowExW(0, L"BUTTON", L"\u5e94\u7528", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd,
                        reinterpret_cast<HMENU>(IDC_APPLY_ADB_FILTER_BUTTON), m_instance, nullptr);
    m_keywordEditControl.Create(m_hWnd, IDC_KEYWORD_EDIT, L"", m_darkTheme);
    m_tagEditControl.Create(m_hWnd, IDC_TAG_EDIT, L"", m_darkTheme);
    m_pidEditControl.Create(m_hWnd, IDC_PID_EDIT, L"", m_darkTheme);
    m_excludeKeywordEditControl.Create(m_hWnd, IDC_EXCLUDE_KEYWORD_EDIT, L"", m_darkTheme);
    m_adbFilterEditControl.SetCueBanner(ResourceStrings::Load(m_instance, IDS_HINT_ADB_FILTER));
    m_keywordEditControl.SetCueBanner(L"\x5173\x952E\x8BCD\x8FC7\x6EE4\xFF1A\x652F\x6301 * \x548C |");
    m_tagEditControl.SetCueBanner(L"Tag\x8FC7\x6EE4");
    m_pidEditControl.SetCueBanner(L"Pid\x8FC7\x6EE4");
    m_excludeKeywordEditControl.SetCueBanner(L"\x6392\x9664\x5173\x952E\x8BCD\xFF1A\x652F\x6301 * \x548C |");
    m_deviceComboControl.SetTheme(m_darkTheme);
    m_bufferComboControl.SetTheme(m_darkTheme);
    m_adbFilterEditControl.SetCornerRadius(0);
    m_keywordEditControl.SetCornerRadius(0);
    m_tagEditControl.SetCornerRadius(0);
    m_pidEditControl.SetCornerRadius(0);
    m_excludeKeywordEditControl.SetCornerRadius(0);
    m_hAdbFilterEdit = m_adbFilterEditControl.hwnd();
    m_hKeywordEdit = m_keywordEditControl.hwnd();
    m_hTagEdit = m_tagEditControl.hwnd();
    m_hPidEdit = m_pidEditControl.hwnd();
    m_hExcludeKeywordEdit = m_excludeKeywordEditControl.hwnd();
    m_levelComboControl.Create(m_hWnd, IDC_LEVEL_COMBO, m_darkTheme);
    m_hLevelCombo = m_levelComboControl.hwnd();
    if (m_actionToolbar.Create(m_hWnd, 1100, m_darkTheme)) {
        m_actionToolbar.SetItems({
            {ResourceStrings::Load(m_instance, IDS_BUTTON_START), IDC_START_BUTTON},
            {ResourceStrings::Load(m_instance, IDS_BUTTON_STOP), IDC_STOP_BUTTON},
            {ResourceStrings::Load(m_instance, IDS_BUTTON_PAUSE), IDC_PAUSE_BUTTON},
            {ResourceStrings::Load(m_instance, IDS_BUTTON_CLEAR), IDC_CLEAR_BUTTON},
            {L"", 0, nullptr, nullptr, 0, true},
            {ResourceStrings::Load(m_instance, IDS_BUTTON_EXPORT), IDC_EXPORT_BUTTON},
            {L"\u6253\u5f00log\u6587\u4ef6\u5939", IDC_OPEN_LOG_FOLDER_BUTTON},
            {L"", 0, nullptr, nullptr, 0, true},
            {L"adb exit", IDC_ADB_KILL_SERVER_BUTTON},
            {L"", 0, nullptr, nullptr, 0, true},
            {L"adb restart", IDC_ADB_RESTART_SERVER_BUTTON},
            {L"logcat -c", IDC_ADB_LOGCAT_CLEAR_BUTTON}
        });
    }
    m_pauseToolbarIndex = 2;
    if (m_actionToolbar.hwnd() != nullptr) {
        m_actionToolbar.AddItem({L"\x8BBE\x7F6E", IDC_SETTINGS_BUTTON, nullptr, nullptr, 0, false, false, false, true});
        m_actionToolbar.AddItem({L"\u8fc7\u6ee4\u89c4\u5219\u8bf4\u660e", IDC_FILTER_RULES_BUTTON, nullptr, nullptr, 0, false, false, false, true});
    }
    m_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
                                  0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_LOG_LIST), m_instance, nullptr);
    m_hStatusLabel = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_STATUS_LABEL), m_instance, nullptr);
    m_hPopupHost = nullptr;
    m_hPopupList = nullptr;
    m_hActivePicker = nullptr;
    m_hListHeader = ListView_GetHeader(m_hListView);
    DarkMode::ApplyControlTheme(m_hListView);
    ListView_SetBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextColor(m_hListView, DarkMode::kText);

    const HWND controls[] = {m_hListView, m_hStatusLabel, m_hApplyAdbFilterButton};
    for (HWND control : controls) {
        if (control != nullptr) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(control == m_hListView ? m_monoFont : m_uiFont), TRUE);
        }
    }
    SetWindowSubclass(m_hListView, ListSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
    if (m_hListHeader != nullptr) {
        SetWindowTheme(m_hListHeader, L"", L"");
        SetWindowSubclass(m_hListHeader, HeaderSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
    }
    m_levelItems = {ResourceStrings::Load(m_instance, IDS_LEVEL_ALL),   ResourceStrings::Load(m_instance, IDS_LEVEL_VERBOSE),
                    ResourceStrings::Load(m_instance, IDS_LEVEL_DEBUG), ResourceStrings::Load(m_instance, IDS_LEVEL_INFO),
                    ResourceStrings::Load(m_instance, IDS_LEVEL_WARN),  ResourceStrings::Load(m_instance, IDS_LEVEL_ERROR),
                    ResourceStrings::Load(m_instance, IDS_LEVEL_FATAL)};
    m_levelComboItems.clear();
    m_bufferItems = {L"main", L"system", L"radio", L"crash", L"all"};
    m_bufferComboItems.clear();
    for (std::size_t i = 0; i < m_bufferItems.size(); ++i) {
        m_bufferComboItems.push_back({m_bufferItems[i], i, false});
    }
    m_bufferComboControl.SetItems(m_bufferComboItems);
    m_bufferComboControl.SetSelection(0, false);
    for (std::size_t i = 0; i < m_levelItems.size(); ++i) {
        m_levelComboItems.push_back({m_levelItems[i], i, false});
    }
    m_levelComboControl.SetItems(m_levelComboItems);
    m_levelComboControl.SetSelection(0, false);

    InitializeListView();
    ApplyThemeToChildren();
    UpdateActionToolbarState();
}

void MainWindow::InitializeListView() {
    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextColor(m_hListView, DarkMode::kText);

    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    auto timeLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_TIME);
    column.pszText = const_cast<wchar_t*>(timeLabel.c_str());
    column.cx = 182;
    ListView_InsertColumn(m_hListView, 0, &column);

    auto levelLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_LEVEL);
    column.pszText = const_cast<wchar_t*>(levelLabel.c_str());
    column.cx = 70;
    column.iSubItem = 1;
    ListView_InsertColumn(m_hListView, 1, &column);

    auto pidLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_PID);
    column.pszText = const_cast<wchar_t*>(pidLabel.c_str());
    column.cx = 80;
    column.iSubItem = 2;
    ListView_InsertColumn(m_hListView, 2, &column);

    auto tagLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_TAG);
    column.pszText = const_cast<wchar_t*>(tagLabel.c_str());
    column.cx = 180;
    column.iSubItem = 3;
    ListView_InsertColumn(m_hListView, 3, &column);

    auto msgLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_MESSAGE);
    column.pszText = const_cast<wchar_t*>(msgLabel.c_str());
    column.cx = 900;
    column.iSubItem = 4;
    ListView_InsertColumn(m_hListView, 4, &column);
}

void MainWindow::UpdateColumnWidths(int listWidth) {
    const int innerWidth = std::max(400, listWidth - 4);
    const int timeWidth = 182;
    const int levelWidth = 70;
    const int pidWidth = 80;
    const int tagWidth = 180;
    const int messageWidth = std::max(200, innerWidth - timeWidth - levelWidth - pidWidth - tagWidth - 24);
    ListView_SetColumnWidth(m_hListView, 0, timeWidth);
    ListView_SetColumnWidth(m_hListView, 1, levelWidth);
    ListView_SetColumnWidth(m_hListView, 2, pidWidth);
    ListView_SetColumnWidth(m_hListView, 3, tagWidth);
    ListView_SetColumnWidth(m_hListView, 4, messageWidth);
}

void MainWindow::ApplyThemeToChildren() {
    DarkMode::ApplyWindowDarkMode(m_hWnd);
    const HWND controls[] = {m_hListView};
    for (HWND control : controls) {
        if (control != nullptr) {
            DarkMode::ApplyControlTheme(control);
        }
    }
}

void MainWindow::LayoutControls(int width, int height) {
    const int row1Y = 14;
    const int row2Y = 58;
    const int statusHeight = 24;
    const int rowGap = 10;
    const int deviceWidth = 300;
    const int bufferWidth = 110;
    const int toolbarX = kMargin + deviceWidth + rowGap + bufferWidth + rowGap;
    const int toolbarWidth = std::max(220, width - toolbarX - kMargin);
    const int levelWidth = 130;
    const int pidWidth = 110;
    const int tagWidth = 170;
    const int excludeKeywordWidth = 240;
    const int applyButtonWidth = 78;
    const int adbFilterWidth = 220;
    const int keywordWidth =
        std::max(220, width - kMargin * 2 - pidWidth - levelWidth - tagWidth - adbFilterWidth - applyButtonWidth - excludeKeywordWidth -
                           rowGap * 6);

    if (m_hDeviceCombo != nullptr) MoveWindow(m_hDeviceCombo, kMargin, row1Y, deviceWidth, kControlHeight, TRUE);
    if (m_hBufferCombo != nullptr) MoveWindow(m_hBufferCombo, kMargin + deviceWidth + rowGap, row1Y, bufferWidth, kControlHeight, TRUE);
    if (m_actionToolbar.hwnd() != nullptr) {
        MoveWindow(m_actionToolbar.hwnd(), toolbarX, row1Y, toolbarWidth, m_darkTheme.toolbarHeight, TRUE);
    }

    int x = kMargin;
    MoveWindow(m_hKeywordEdit, x, row2Y, keywordWidth, kControlHeight, TRUE);
    x += keywordWidth + rowGap;
    MoveWindow(m_hTagEdit, x, row2Y, tagWidth, kControlHeight, TRUE);
    x += tagWidth + rowGap;
    MoveWindow(m_hPidEdit, x, row2Y, pidWidth, kControlHeight, TRUE);
    x += pidWidth + rowGap;
    if (m_hLevelCombo != nullptr) MoveWindow(m_hLevelCombo, x, row2Y, levelWidth, kControlHeight, TRUE);
    x += levelWidth + rowGap;
    MoveWindow(m_hAdbFilterEdit, x, row2Y, adbFilterWidth, kControlHeight, TRUE);
    x += adbFilterWidth + rowGap;
    if (m_hApplyAdbFilterButton != nullptr) MoveWindow(m_hApplyAdbFilterButton, x, row2Y, applyButtonWidth, kControlHeight, TRUE);
    x += applyButtonWidth + rowGap;
    if (m_hExcludeKeywordEdit != nullptr) MoveWindow(m_hExcludeKeywordEdit, x, row2Y, excludeKeywordWidth, kControlHeight, TRUE);

    MoveWindow(m_hListView, kMargin, kToolbarHeight, width - kMargin * 2, height - kToolbarHeight - statusHeight - kMargin, TRUE);
    MoveWindow(m_hStatusLabel, kMargin, height - statusHeight - 4, width - kMargin * 2, statusHeight, TRUE);
    UpdateColumnWidths(width - kMargin * 2);
}

void MainWindow::OnCreate() {
    InitThemeResources();
    CreateControls();
    m_config.Load();
    LoadKnownDevices();
    BeginRefreshDevices(false);
    LoadConfig();
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_READY));
    SetTimer(m_hWnd, kFlushTimerId, kFlushIntervalMs, nullptr);
    RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
}

void MainWindow::OnSize(int width, int height) {
    HidePickerPopup();
    LayoutControls(width, height);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM) {
    const WORD controlId = LOWORD(wParam);
    const WORD notifyCode = HIWORD(wParam);

    if (controlId == IDC_START_BUTTON) {
        OnStart();
        return;
    }
    if (controlId == IDC_STOP_BUTTON) {
        OnStop();
        return;
    }
    if (controlId == IDC_PAUSE_BUTTON) {
        OnPauseResume();
        return;
    }
    if (controlId == IDC_EXPORT_BUTTON) {
        OnExport();
        return;
    }
    if (controlId == IDC_APPLY_ADB_FILTER_BUTTON) {
        if (m_adbClient.IsRunning()) {
            RestartAdbCaptureForSelection();
        }
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_OPEN_LOG_FOLDER_BUTTON) {
        OnOpenLogFolder();
        return;
    }
    if (controlId == IDC_CLEAR_BUTTON) {
        OnClear();
        return;
    }
    if (controlId == IDC_ADB_KILL_SERVER_BUTTON) {
        BeginToolbarAdbCommand(controlId, L"kill-server", L"ADB exit");
        return;
    }
    if (controlId == IDC_ADB_RESTART_SERVER_BUTTON) {
        BeginToolbarAdbCommand(controlId, L"kill-server && adb start-server", L"ADB restart");
        return;
    }
    if (controlId == IDC_ADB_LOGCAT_CLEAR_BUTTON) {
        BeginToolbarAdbCommand(controlId, L"logcat -c", L"logcat -c");
        return;
    }
    if (controlId == IDC_SETTINGS_BUTTON) {
        OnSettings();
        return;
    }
    if (controlId == IDC_FILTER_RULES_BUTTON) {
        OnFilterRulesHelp();
        return;
    }

    if ((controlId == IDC_KEYWORD_EDIT || controlId == IDC_TAG_EDIT || controlId == IDC_PID_EDIT || controlId == IDC_EXCLUDE_KEYWORD_EDIT) &&
        notifyCode == EN_CHANGE) {
        if (m_loadingConfig) {
            return;
        }
        OnFilterChanged();
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_ADB_FILTER_EDIT && notifyCode == EN_CHANGE) {
        if (m_loadingConfig) {
            return;
        }
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_LEVEL_COMBO && notifyCode == CBN_SELCHANGE) {
        if (m_loadingConfig) {
            return;
        }
        m_selectedLevelIndex = std::max(0, m_levelComboControl.GetSelection());
        OnFilterChanged();
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_BUFFER_COMBO && notifyCode == CBN_SELCHANGE) {
        if (m_loadingConfig) {
            return;
        }
        m_selectedBufferIndex = std::max(0, m_bufferComboControl.GetSelection());
        if (m_adbClient.IsRunning()) {
            RestartAdbCaptureForSelection();
        }
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_DEVICE_COMBO && notifyCode == CBN_SELCHANGE) {
        const int selected = m_deviceComboControl.GetSelection();
        if (selected < 0) {
            return;
        }
        if (selected == static_cast<int>(m_deviceItems.size()) - 1) {
            AddDeviceDialog dialog(m_instance);
            std::wstring address;
            if (dialog.ShowModal(m_hWnd, address)) {
                BeginConnectDevice(address);
            } else {
                m_deviceComboControl.SetSelection(m_selectedDeviceIndex, false);
            }
        } else {
            const std::wstring selectedText = m_deviceItems[static_cast<std::size_t>(selected)];
            m_selectedDeviceIndex = selected;
            if (IsSavedNetworkDeviceLabel(selectedText)) {
                BeginConnectDevice(ExtractDeviceSerialFromLabel(selectedText));
                return;
            }
            if (IsOnlineDeviceLabel(selectedText) && m_adbClient.IsRunning()) {
                RestartAdbCaptureForSelection();
            }
            SaveConfigIfNeeded();
        }
        return;
    }
}

void MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == kFlushTimerId && !m_paused) {
        FlushPendingLogs();
    }
}

void MainWindow::OnDestroy() {
    KillTimer(m_hWnd, kFlushTimerId);
    SaveConfigIfNeeded();
    m_adbClient.Stop();
    PostQuitMessage(0);
}

void MainWindow::OnFilterChanged() {
    m_filters = ReadFilterOptions();
    RebuildVisibleIndexes(!IsNearBottom());
}

void MainWindow::OnStart() {
    if (m_adbClient.IsRunning()) {
        return;
    }

    const std::wstring selectedText = GetSelectedDeviceText();
    const std::wstring selectedSerial = ExtractDeviceSerialFromLabel(selectedText);
    if (IsSavedNetworkDeviceLabel(selectedText) || (IsOnlineDeviceLabel(selectedText) && LooksLikeNetworkDeviceSerial(selectedSerial))) {
        m_startAfterDeviceConnect = true;
        BeginConnectDevice(selectedSerial);
        return;
    }

    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STARTING));
    m_paused = false;
    UpdateActionToolbarState();
    SaveConfigIfNeeded();

    m_adbClient.Start(
        ReadAdbLaunchOptions(),
        [this](std::vector<LogEntry> batch) {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            m_pendingLogs.insert(m_pendingLogs.end(), batch.begin(), batch.end());
        },
        [this](const std::wstring& status) {
            PostMessageW(m_hWnd, kAppStatusMessage, 0, reinterpret_cast<LPARAM>(new std::wstring(status)));
        });
    UpdateActionToolbarState();
}

void MainWindow::OnStop() {
    if (!m_adbClient.IsRunning()) {
        return;
    }
    m_adbClient.Stop();
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STOPPED));
    UpdateActionToolbarState();
}

void MainWindow::OnClear() {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingLogs.clear();
    m_logBuffer.Clear();
    m_visibleIndexes.clear();
    ListView_SetItemCountEx(m_hListView, 0, LVSICF_NOSCROLL);
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_CLEARED));
    UpdateActionToolbarState();
}

void MainWindow::BeginToolbarAdbCommand(int commandId, const std::wstring& arguments, const std::wstring& actionName) {
    if (m_toolbarCommandInProgress) {
        return;
    }

    SetToolbarCommandInProgress(true);
    SetStatus(actionName + L" running...");

    const HWND hwnd = m_hWnd;
    std::thread([hwnd, commandId, arguments, actionName]() {
        auto* result = new ToolbarAdbCommandResult();
        result->commandId = commandId;
        result->actionName = actionName;
        result->arguments = arguments;
        if (commandId == IDC_ADB_RESTART_SERVER_BUTTON) {
            std::wstring killStatus;
            std::wstring startStatus;
            const bool killed = AdbClient::RunAdbCommand(L"kill-server", killStatus, 5000);
            const bool started = killed && AdbClient::RunAdbCommand(L"start-server", startStatus, 5000);
            result->success = killed && started;
            result->status = result->success ? startStatus : (started ? startStatus : (startStatus.empty() ? killStatus : killStatus + L"\r\n" + startStatus));
        } else {
            result->success = AdbClient::RunAdbCommand(arguments, result->status, 5000);
        }
        if (!PostMessageW(hwnd, kToolbarAdbCommandResultMessage, 0, reinterpret_cast<LPARAM>(result))) {
            delete result;
        }
    }).detach();
}

void MainWindow::SetToolbarCommandInProgress(bool inProgress) {
    m_toolbarCommandInProgress = inProgress;
    UpdateActionToolbarState();
}

void MainWindow::RestartAdbCaptureForSelection() {
    if (!m_adbClient.IsRunning()) {
        return;
    }

    m_adbClient.Stop();
    SetStatus(L"Switching device...");
    m_paused = false;
    m_adbClient.Start(
        ReadAdbLaunchOptions(),
        [this](std::vector<LogEntry> batch) {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            m_pendingLogs.insert(m_pendingLogs.end(), batch.begin(), batch.end());
        },
        [this](const std::wstring& status) {
            PostMessageW(m_hWnd, kAppStatusMessage, 0, reinterpret_cast<LPARAM>(new std::wstring(status)));
        });
    UpdateActionToolbarState();
}

void MainWindow::BeginConnectDevice(const std::wstring& address) {
    if (address.empty() || m_deviceConnectInProgress) {
        return;
    }

    SetDeviceConnectInProgress(true);
    SetStatus(L"Connecting to " + address + L"...");

    const HWND hwnd = m_hWnd;
    std::thread([hwnd, address]() {
        auto* result = new DeviceConnectResult();
        result->address = address;
        result->success = AdbClient::ConnectNetworkDevice(address, result->status, &result->timedOut);
        if (!PostMessageW(hwnd, kDeviceConnectResultMessage, 0, reinterpret_cast<LPARAM>(result))) {
            delete result;
        }
    }).detach();
}

void MainWindow::SetDeviceConnectInProgress(bool inProgress) {
    m_deviceConnectInProgress = inProgress;
    if (m_hDeviceCombo != nullptr) {
        EnableWindow(m_hDeviceCombo, !inProgress);
    }
}

void MainWindow::OnPauseResume() {
    m_paused = !m_paused;
    UpdateActionToolbarState();
    if (m_paused) {
        SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_PAUSED));
    } else {
        FlushPendingLogs();
        SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_READY));
    }
}

void MainWindow::OnExport() {
    if (m_exportDirectory.empty()) {
        OnSettings();
        if (m_exportDirectory.empty()) {
            return;
        }
    }

    DWORD attributes = GetFileAttributesW(m_exportDirectory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        MessageBoxW(m_hWnd, L"\x5BFC\x51FA\x76EE\x5F55\x4E0D\x5B58\x5728\xFF0C\x8BF7\x5148\x5728\x8BBE\x7F6E\x91CC\x91CD\x65B0\x914D\x7F6E\x3002",
                    L"\x5BFC\x51FA\x5931\x8D25", MB_OK | MB_ICONERROR);
        OnSettings();
        if (m_exportDirectory.empty()) {
            return;
        }
        attributes = GetFileAttributesW(m_exportDirectory.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return;
        }
    }

    const std::wstring path = m_exportDirectory + L"\\logs_export.txt";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxW(m_hWnd, L"\x65E0\x6CD5\x521B\x5EFA\x5BFC\x51FA\x6587\x4EF6\x3002", L"\x5BFC\x51FA\x5931\x8D25", MB_OK | MB_ICONERROR);
        SetStatus(L"Export failed.");
        return;
    }

    const char bom[] = "\xEF\xBB\xBF";
    DWORD written = 0;
    WriteFile(file, bom, 3, &written, nullptr);
    std::string utf8Chunk;
    utf8Chunk.reserve(256 * 1024);
    auto flushChunk = [&]() -> bool {
        if (utf8Chunk.empty()) {
            return true;
        }
        const bool ok = WriteFile(file, utf8Chunk.data(), static_cast<DWORD>(utf8Chunk.size()), &written, nullptr) != FALSE;
        if (ok) {
            utf8Chunk.clear();
        }
        return ok;
    };
    for (std::size_t index : m_visibleIndexes) {
        const LogEntry* entry = m_logBuffer.At(index);
        if (entry == nullptr) continue;
        std::wstring line = entry->rawLine + L"\r\n";
        int bytes = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), utf8.data(), bytes, nullptr, nullptr);
        if (utf8Chunk.size() + utf8.size() > 256 * 1024 && !flushChunk()) {
            CloseHandle(file);
            MessageBoxW(m_hWnd, L"\u5199\u5165\u5bfc\u51fa\u6587\u4ef6\u5931\u8d25\u3002", L"\x5BFC\x51FA\x5931\x8D25", MB_OK | MB_ICONERROR);
            SetStatus(L"Export failed.");
            return;
        }
        utf8Chunk.append(utf8);
    }
    if (!flushChunk()) {
        CloseHandle(file);
        MessageBoxW(m_hWnd, L"\u5199\u5165\u5bfc\u51fa\u6587\u4ef6\u5931\u8d25\u3002", L"\x5BFC\x51FA\x5931\x8D25", MB_OK | MB_ICONERROR);
        SetStatus(L"Export failed.");
        return;
    }
    CloseHandle(file);
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_EXPORTED) + L": " + path);
}

void MainWindow::OnOpenLogFolder() {
    if (m_exportDirectory.empty()) {
        OnSettings();
        if (m_exportDirectory.empty()) {
            return;
        }
    }

    DWORD attributes = GetFileAttributesW(m_exportDirectory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        MessageBoxW(m_hWnd, L"\u5bfc\u51falog\u76ee\u5f55\u4e0d\u5b58\u5728\uff0c\u8bf7\u5148\u5728\u8bbe\u7f6e\u91cc\u91cd\u65b0\u914d\u7f6e\u3002",
                    L"\u6253\u5f00log\u6587\u4ef6\u5939", MB_OK | MB_ICONERROR);
        OnSettings();
        if (m_exportDirectory.empty()) {
            return;
        }
    }

    const HINSTANCE opened = ShellExecuteW(m_hWnd, L"open", m_exportDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(opened) <= 32) {
        MessageBoxW(m_hWnd, L"\u65e0\u6cd5\u6253\u5f00log\u8f93\u51fa\u6587\u4ef6\u5939\u3002", L"\u6253\u5f00log\u6587\u4ef6\u5939",
                    MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"\u5df2\u6253\u5f00log\u8f93\u51fa\u6587\u4ef6\u5939\u3002");
}

void MainWindow::OnSettings() {
    SettingsDialog dialog(m_instance);
    std::wstring exportDirectory = m_exportDirectory;
    if (!dialog.ShowModal(m_hWnd, exportDirectory)) {
        return;
    }

    m_exportDirectory = exportDirectory;
    SaveExportDirectory();
    SetStatus(L"\x5BFC\x51FA\x76EE\x5F55\x5DF2\x4FDD\x5B58\x3002");
}

void MainWindow::OnFilterRulesHelp() {
    FilterRulesDialog dialog(m_instance);
    dialog.ShowModal(m_hWnd);
}

void MainWindow::BeginRefreshDevices(bool keepSelection) {
    const HWND hwnd = m_hWnd;
    const std::wstring currentSelection = keepSelection ? GetSelectedDeviceText() : L"";
    std::thread([hwnd, keepSelection, currentSelection]() {
        auto* result = new DeviceListResult();
        result->keepSelection = keepSelection;
        result->currentSelection = currentSelection;
        result->devices = AdbClient::ListDevices();
        if (!PostMessageW(hwnd, kDeviceListResultMessage, 0, reinterpret_cast<LPARAM>(result))) {
            delete result;
        }
    }).detach();
}

void MainWindow::RefreshDevices(bool keepSelection) {
    const std::wstring currentSelection = keepSelection ? GetSelectedDeviceText() : L"";
    m_deviceItems.clear();
    m_deviceComboItems.clear();
    m_deviceItems.push_back(L"Android Devices");
    m_deviceComboItems.push_back({m_deviceItems.back(), 0, false});
    std::vector<std::wstring> added;
    auto devices = AdbClient::ListDevices();
    int selected = 0;
    bool matchedCurrentSelection = false;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const std::wstring label = L"* " + devices[i].serial + L" [" + devices[i].state + L"]";
        m_deviceItems.push_back(label);
        m_deviceComboItems.push_back({label, i + 1, true});
        added.push_back(devices[i].serial);
        if (!currentSelection.empty() && currentSelection == label) {
            selected = static_cast<int>(m_deviceItems.size() - 1);
            matchedCurrentSelection = true;
        }
    }
    for (const auto& device : m_knownDevices) {
        if (std::find(added.begin(), added.end(), device) != added.end()) {
            continue;
        }
        const std::wstring label = device + L" [saved]";
        m_deviceItems.push_back(label);
        m_deviceComboItems.push_back({label, m_deviceItems.size() - 1, false});
        if (!currentSelection.empty() && currentSelection == label) {
            selected = static_cast<int>(m_deviceItems.size() - 1);
            matchedCurrentSelection = true;
        }
    }
    m_deviceItems.push_back(L"\x6DFB\x52A0\x8BBE\x5907");
    m_deviceComboItems.push_back({m_deviceItems.back(), m_deviceItems.size() - 1, true});
    if (!matchedCurrentSelection && !devices.empty()) {
        selected = 1;
    }
    m_selectedDeviceIndex = selected;
    m_deviceComboControl.SetItems(m_deviceComboItems);
    m_deviceComboControl.SetSelection(m_selectedDeviceIndex, false);
}

void MainWindow::FlushPendingLogs() {
    std::vector<LogEntry> batch;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_pendingLogs.empty()) return;
        batch.swap(m_pendingLogs);
    }

    const bool followBottom = IsNearBottom();
    const std::size_t oldSize = m_logBuffer.Size();
    const std::size_t oldVisibleSize = m_visibleIndexes.size();
    const std::size_t dropped = m_logBuffer.AppendBatch(batch);
    const std::size_t oldEntriesRetained = dropped >= oldSize ? 0 : (oldSize - dropped);
    const std::size_t newSize = m_logBuffer.Size();
    bool listContentChanged = false;

    if (dropped > 0) {
        std::size_t writeIndex = 0;
        for (std::size_t readIndex = 0; readIndex < m_visibleIndexes.size(); ++readIndex) {
            const std::size_t index = m_visibleIndexes[readIndex];
            if (index >= dropped) {
                m_visibleIndexes[writeIndex++] = index - dropped;
            }
        }
        if (writeIndex != m_visibleIndexes.size()) {
            m_visibleIndexes.resize(writeIndex);
        }
        listContentChanged = true;
    }

    for (std::size_t i = oldEntriesRetained; i < newSize; ++i) {
        const LogEntry* entry = m_logBuffer.At(i);
        if (entry != nullptr && FilterEngine::Matches(*entry, m_filters)) {
            m_visibleIndexes.push_back(i);
        }
    }

    if (m_visibleIndexes.size() != oldVisibleSize) {
        listContentChanged = true;
    }

    if (listContentChanged) {
        ListView_SetItemCountEx(m_hListView, static_cast<int>(m_visibleIndexes.size()), LVSICF_NOSCROLL);
        InvalidateRect(m_hListView, nullptr, FALSE);
    }

    if (followBottom) ScrollToBottom();
    UpdateStatusText();
}

void MainWindow::RebuildVisibleIndexes(bool keepScrollPosition) {
    const int topIndex = ListView_GetTopIndex(m_hListView);
    m_visibleIndexes = FilterEngine::BuildVisibleIndexes(m_logBuffer, m_filters);
    ListView_SetItemCountEx(m_hListView, static_cast<int>(m_visibleIndexes.size()), LVSICF_NOSCROLL);
    InvalidateRect(m_hListView, nullptr, FALSE);
    if (keepScrollPosition && topIndex >= 0 && topIndex < static_cast<int>(m_visibleIndexes.size())) {
        ListView_EnsureVisible(m_hListView, topIndex, FALSE);
    } else if (!keepScrollPosition) {
        ScrollToBottom();
    }
    UpdateStatusText();
}

void MainWindow::UpdateStatusText() {
    SetWindowTextW(m_hStatusLabel, FormatStatusLine(m_statusText, m_logBuffer.Size(), m_visibleIndexes.size(), m_paused).c_str());
}

void MainWindow::UpdateActionToolbarState() {
    if (m_actionToolbar.hwnd() == nullptr) {
        return;
    }

    if (m_pauseToolbarIndex >= 0) {
        darkui::ToolbarItem pauseItem = m_actionToolbar.GetItem(m_pauseToolbarIndex);
        pauseItem.text = m_paused ? L"\x7EE7\x7EED\x663E\x793A" : L"\x6682\x505C\x663E\x793A";
        pauseItem.checked = m_paused;
        pauseItem.disabled = !m_adbClient.IsRunning() || m_toolbarCommandInProgress;
        m_actionToolbar.SetItem(m_pauseToolbarIndex, pauseItem);
    }

    const int startIndex = FindToolbarIndexByCommandId(IDC_START_BUTTON);
    const int stopIndex = FindToolbarIndexByCommandId(IDC_STOP_BUTTON);
    const int exportIndex = FindToolbarIndexByCommandId(IDC_EXPORT_BUTTON);
    const int openFolderIndex = FindToolbarIndexByCommandId(IDC_OPEN_LOG_FOLDER_BUTTON);
    const int clearIndex = FindToolbarIndexByCommandId(IDC_CLEAR_BUTTON);
    const int killIndex = FindToolbarIndexByCommandId(IDC_ADB_KILL_SERVER_BUTTON);
    const int restartIndex = FindToolbarIndexByCommandId(IDC_ADB_RESTART_SERVER_BUTTON);
    const int logcatClearIndex = FindToolbarIndexByCommandId(IDC_ADB_LOGCAT_CLEAR_BUTTON);
    const int rulesIndex = FindToolbarIndexByCommandId(IDC_FILTER_RULES_BUTTON);
    const int settingsIndex = FindToolbarIndexByCommandId(IDC_SETTINGS_BUTTON);

    if (stopIndex >= 0) {
        darkui::ToolbarItem stopItem = m_actionToolbar.GetItem(stopIndex);
        stopItem.text = L"\x505C\x6B62\x91C7\x96C6";
        m_actionToolbar.SetItem(stopIndex, stopItem);
    }

    if (startIndex >= 0) m_actionToolbar.SetDisabled(startIndex, m_adbClient.IsRunning() || m_toolbarCommandInProgress);
    if (stopIndex >= 0) m_actionToolbar.SetDisabled(stopIndex, !m_adbClient.IsRunning() || m_toolbarCommandInProgress);
    if (exportIndex >= 0) m_actionToolbar.SetDisabled(exportIndex, m_toolbarCommandInProgress);
    if (openFolderIndex >= 0) m_actionToolbar.SetDisabled(openFolderIndex, m_toolbarCommandInProgress);
    if (clearIndex >= 0) m_actionToolbar.SetDisabled(clearIndex, m_toolbarCommandInProgress);
    if (killIndex >= 0) m_actionToolbar.SetDisabled(killIndex, m_toolbarCommandInProgress);
    if (restartIndex >= 0) m_actionToolbar.SetDisabled(restartIndex, m_toolbarCommandInProgress);
    if (logcatClearIndex >= 0) m_actionToolbar.SetDisabled(logcatClearIndex, m_toolbarCommandInProgress);
    if (rulesIndex >= 0) m_actionToolbar.SetDisabled(rulesIndex, m_toolbarCommandInProgress);
    if (settingsIndex >= 0) m_actionToolbar.SetDisabled(settingsIndex, m_toolbarCommandInProgress);

    const int successCommands[] = {
        IDC_ADB_KILL_SERVER_BUTTON,
        IDC_ADB_RESTART_SERVER_BUTTON,
        IDC_ADB_LOGCAT_CLEAR_BUTTON
    };
    for (int commandId : successCommands) {
        const int index = FindToolbarIndexByCommandId(commandId);
        if (index >= 0) {
            m_actionToolbar.SetChecked(index, m_lastToolbarSuccessCommandId == commandId);
        }
    }
}

void MainWindow::SetStatus(const std::wstring& text) {
    m_statusText = text;
    UpdateStatusText();
}

FilterOptions MainWindow::ReadFilterOptions() const {
    FilterOptions options;
    options.keyword = m_keywordEditControl.GetText();
    options.excludeKeyword = m_excludeKeywordEditControl.GetText();
    options.tag = m_tagEditControl.GetText();
    options.pidText = m_pidEditControl.GetText();
    options.keywordExpression = FilterEngine::CompileExpression(options.keyword);
    options.excludeKeywordExpression = FilterEngine::CompileExpression(options.excludeKeyword);
    options.tagExpression = FilterEngine::CompileExpression(options.tag);
    if (!options.pidText.empty()) {
        try {
            options.pidValue = static_cast<std::uint32_t>(std::stoul(options.pidText));
            options.hasPidFilter = true;
        } catch (...) {
            options.hasPidFilter = false;
        }
    }
    switch (m_selectedLevelIndex) {
    case 2:
        options.minimumLevel = LogLevel::Debug;
        break;
    case 3:
        options.minimumLevel = LogLevel::Info;
        break;
    case 4:
        options.minimumLevel = LogLevel::Warn;
        break;
    case 5:
        options.minimumLevel = LogLevel::Error;
        break;
    case 6:
        options.minimumLevel = LogLevel::Fatal;
        break;
    default:
        options.minimumLevel = LogLevel::Verbose;
        break;
    }
    return options;
}

AdbLaunchOptions MainWindow::ReadAdbLaunchOptions() const {
    AdbLaunchOptions options;
    const std::wstring text = GetSelectedDeviceText();
    if (m_selectedDeviceIndex > 0 && m_selectedDeviceIndex < static_cast<int>(m_deviceItems.size()) - 1) {
        const std::size_t pos = text.find(L" [");
        const std::wstring serial = pos == std::wstring::npos ? text : text.substr(0, pos);
        options.deviceSerial = serial.rfind(L"* ", 0) == 0 ? serial.substr(2) : serial;
    }
    options.logBuffer = GetSelectedBufferText();
    options.adbPriorityFilter = m_adbFilterEditControl.GetText();
    return options;
}

const LogEntry* MainWindow::GetVisibleEntry(int itemIndex) const {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_visibleIndexes.size())) return nullptr;
    return m_logBuffer.At(m_visibleIndexes[static_cast<std::size_t>(itemIndex)]);
}

bool MainWindow::IsNearBottom() const {
    const int countPerPage = ListView_GetCountPerPage(m_hListView);
    const int topIndex = ListView_GetTopIndex(m_hListView);
    const int visibleCount = static_cast<int>(m_visibleIndexes.size());
    return visibleCount == 0 || topIndex + countPerPage >= visibleCount - 2;
}

void MainWindow::ScrollToBottom() {
    const int visibleCount = static_cast<int>(m_visibleIndexes.size());
    if (visibleCount > 0) {
        ListView_EnsureVisible(m_hListView, visibleCount - 1, FALSE);
    }
}

std::wstring MainWindow::GetWindowTextString(HWND hWnd) const {
    const int length = GetWindowTextLengthW(hWnd);
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    GetWindowTextW(hWnd, text.data(), length + 1);
    return text;
}

void MainWindow::LoadConfig() {
    m_loadingConfig = true;
    const int savedWidth = _wtoi(m_config.Get(L"window_width", L"1380").c_str());
    const int savedHeight = _wtoi(m_config.Get(L"window_height", L"860").c_str());
    m_exportDirectory = m_config.Get(L"export_dir");
    m_pendingWindowWidth = savedWidth > 640 ? savedWidth : 1380;
    m_pendingWindowHeight = savedHeight > 480 ? savedHeight : 860;
    if (savedWidth > 640 && savedHeight > 480) {
        const RECT workArea = GetMonitorWorkAreaForWindow(m_hWnd);
        const int x = workArea.left + ((workArea.right - workArea.left) - savedWidth) / 2;
        const int y = workArea.top + ((workArea.bottom - workArea.top) - savedHeight) / 2;
        SetWindowPos(m_hWnd, nullptr, x, y, savedWidth, savedHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    m_adbFilterEditControl.SetText(m_config.Get(L"adb_filter"));
    m_keywordEditControl.SetText(m_config.Get(L"keyword"));
    m_tagEditControl.SetText(m_config.Get(L"tag"));
    m_pidEditControl.SetText(m_config.Get(L"pid"));
    m_excludeKeywordEditControl.SetText(m_config.Get(L"exclude_keyword"));
    const std::wstring logBuffer = m_config.Get(L"log_buffer", L"main");
    auto bufferIt = std::find(m_bufferItems.begin(), m_bufferItems.end(), logBuffer);
    m_selectedBufferIndex = bufferIt == m_bufferItems.end() ? 0 : static_cast<int>(bufferIt - m_bufferItems.begin());
    m_bufferComboControl.SetSelection(m_selectedBufferIndex, false);
    const int level = _wtoi(m_config.Get(L"level", L"0").c_str());
    m_selectedLevelIndex = std::clamp(level, 0, static_cast<int>(m_levelItems.size()) - 1);
    m_levelComboControl.SetSelection(m_selectedLevelIndex, false);
    m_filters = ReadFilterOptions();
    m_loadingConfig = false;
}

void MainWindow::SaveConfigIfNeeded() {
    if (m_windowBoundsDirty) {
        m_config.Set(L"window_width", std::to_wstring(m_pendingWindowWidth));
        m_config.Set(L"window_height", std::to_wstring(m_pendingWindowHeight));
        m_windowBoundsDirty = false;
    }
    m_config.Set(L"adb_filter", m_adbFilterEditControl.GetText());
    m_config.Set(L"keyword", m_keywordEditControl.GetText());
    m_config.Set(L"tag", m_tagEditControl.GetText());
    m_config.Set(L"pid", m_pidEditControl.GetText());
    m_config.Set(L"exclude_keyword", m_excludeKeywordEditControl.GetText());
    m_config.Set(L"log_buffer", GetSelectedBufferText());
    m_config.Set(L"level", std::to_wstring(m_selectedLevelIndex));
    m_config.Set(L"export_dir", m_exportDirectory);
    m_config.SaveIfChanged();
}

void MainWindow::SaveExportDirectory() {
    m_config.Set(L"export_dir", m_exportDirectory);
    m_config.SaveIfChanged();
}

void MainWindow::UpdatePendingWindowBounds() {
    if (m_hWnd == nullptr || IsZoomed(m_hWnd) || IsIconic(m_hWnd)) {
        return;
    }

    RECT rc = {};
    GetWindowRect(m_hWnd, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 640 || height <= 480) {
        return;
    }
    if (width == m_pendingWindowWidth && height == m_pendingWindowHeight) {
        return;
    }

    m_pendingWindowWidth = width;
    m_pendingWindowHeight = height;
    m_windowBoundsDirty = true;
}

void MainWindow::LoadKnownDevices() {
    m_knownDevices.clear();
    std::wstring known = m_config.Get(L"known_devices");
    std::wstringstream ss(known);
    std::wstring item;
    while (std::getline(ss, item, L';')) {
        if (!item.empty()) {
            m_knownDevices.push_back(item);
        }
    }
}

void MainWindow::SaveKnownDevice(const std::wstring& address) {
    if (address.empty()) {
        return;
    }
    if (std::find(m_knownDevices.begin(), m_knownDevices.end(), address) == m_knownDevices.end()) {
        m_knownDevices.push_back(address);
    }
    std::wstring known;
    for (std::size_t i = 0; i < m_knownDevices.size(); ++i) {
        if (i > 0) {
            known += L";";
        }
        known += m_knownDevices[i];
    }
    m_config.Set(L"known_devices", known);
    SaveConfigIfNeeded();
}

void MainWindow::RemoveKnownDevice(const std::wstring& address) {
    if (address.empty()) {
        return;
    }

    const auto oldSize = m_knownDevices.size();
    m_knownDevices.erase(std::remove(m_knownDevices.begin(), m_knownDevices.end(), address), m_knownDevices.end());
    if (m_knownDevices.size() == oldSize) {
        return;
    }

    std::wstring known;
    for (std::size_t i = 0; i < m_knownDevices.size(); ++i) {
        if (i > 0) {
            known += L";";
        }
        known += m_knownDevices[i];
    }
    m_config.Set(L"known_devices", known);
    SaveConfigIfNeeded();
}

int MainWindow::FindToolbarIndexByCommandId(int commandId) const {
    for (int i = 0; i < static_cast<int>(m_actionToolbar.GetCount()); ++i) {
        if (m_actionToolbar.GetItem(i).commandId == commandId) {
            return i;
        }
    }
    return -1;
}

int MainWindow::FindDeviceIndexBySerial(const std::wstring& serial) const {
    if (serial.empty()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(m_deviceItems.size()); ++i) {
        if (ExtractDeviceSerialFromLabel(m_deviceItems[static_cast<std::size_t>(i)]) == serial) {
            return i;
        }
    }
    return -1;
}

void MainWindow::PaintCustomChrome(HDC hdc) {
    RECT client = {};
    GetClientRect(m_hWnd, &client);
    RECT topBar = {0, 0, client.right, kToolbarHeight};
    FillRect(hdc, &topBar, m_bgBrush);

    DrawControlBorder(hdc, m_hDeviceCombo, GetFocus() == m_hDeviceCombo);
    DrawControlBorder(hdc, m_hBufferCombo, GetFocus() == m_hBufferCombo);
    DrawControlBorder(hdc, m_hLevelCombo, GetFocus() == m_hLevelCombo);
    DrawControlBorder(hdc, m_hListView, GetFocus() == m_hListView);
}

void MainWindow::DrawButton(LPDRAWITEMSTRUCT drawInfo) {
    const bool selected = (drawInfo->itemState & ODS_SELECTED) != 0;
    HBRUSH brush = CreateSolidBrush(selected ? RGB(54, 59, 66) : DarkMode::kSurfaceAlt);
    FillRect(drawInfo->hDC, &drawInfo->rcItem, brush);
    DeleteObject(brush);

    HPEN oldPen = static_cast<HPEN>(SelectObject(drawInfo->hDC, m_borderPen));
    HGDIOBJ oldBrush = SelectObject(drawInfo->hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(drawInfo->hDC, drawInfo->rcItem.left, drawInfo->rcItem.top, drawInfo->rcItem.right, drawInfo->rcItem.bottom);
    SelectObject(drawInfo->hDC, oldBrush);
    SelectObject(drawInfo->hDC, oldPen);

    wchar_t text[64] = {};
    GetWindowTextW(drawInfo->hwndItem, text, 64);
    SetBkMode(drawInfo->hDC, TRANSPARENT);
    SetTextColor(drawInfo->hDC, DarkMode::kText);
    SelectObject(drawInfo->hDC, m_uiFont);
    DrawTextW(drawInfo->hDC, text, -1, &drawInfo->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::DrawDropdownButton(LPDRAWITEMSTRUCT drawInfo) {
    const bool selected = (drawInfo->itemState & ODS_SELECTED) != 0;
    HBRUSH brush = CreateSolidBrush(selected ? RGB(70, 78, 92) : RGB(56, 60, 66));
    FillRect(drawInfo->hDC, &drawInfo->rcItem, brush);
    DeleteObject(brush);

    HPEN oldPen = static_cast<HPEN>(SelectObject(drawInfo->hDC, m_borderPen));
    HGDIOBJ oldBrush = SelectObject(drawInfo->hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(drawInfo->hDC, drawInfo->rcItem.left, drawInfo->rcItem.top, drawInfo->rcItem.right, drawInfo->rcItem.bottom);
    SelectObject(drawInfo->hDC, oldBrush);
    SelectObject(drawInfo->hDC, oldPen);

    std::wstring text;
    switch (drawInfo->CtlID) {
    case IDC_DEVICE_COMBO:
        text = GetSelectedDeviceText();
        break;
    case IDC_BUFFER_COMBO:
        text = GetSelectedBufferText();
        break;
    default:
        text = GetSelectedLevelText();
        break;
    }
    SetBkMode(drawInfo->hDC, TRANSPARENT);
    SetTextColor(drawInfo->hDC, DarkMode::kText);
    SelectObject(drawInfo->hDC, m_uiFont);
    RECT rc = drawInfo->rcItem;
    rc.left += 8;
    rc.right -= 24;
    DrawTextW(drawInfo->hDC, text.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS);
    RECT arrowRect = drawInfo->rcItem;
    arrowRect.left = arrowRect.right - 20;
    DrawTextW(drawInfo->hDC, L"v", -1, &arrowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::DrawPopupListItem(LPDRAWITEMSTRUCT drawInfo) {
    if (drawInfo->itemID == static_cast<UINT>(-1)) {
        return;
    }
    const bool selected = (drawInfo->itemState & ODS_SELECTED) != 0;
    HBRUSH brush = CreateSolidBrush(selected ? RGB(70, 78, 92) : RGB(48, 52, 58));
    FillRect(drawInfo->hDC, &drawInfo->rcItem, brush);
    DeleteObject(brush);

    wchar_t text[256] = {};
    SendMessageW(drawInfo->hwndItem, LB_GETTEXT, drawInfo->itemID, reinterpret_cast<LPARAM>(text));
    SetBkMode(drawInfo->hDC, TRANSPARENT);
    SetTextColor(drawInfo->hDC, DarkMode::kText);
    SelectObject(drawInfo->hDC, m_uiFont);
    RECT rc = drawInfo->rcItem;
    rc.left += 8;
    DrawTextW(drawInfo->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void MainWindow::DrawControlBorder(HDC hdc, HWND control, bool focused) {
    if (control == nullptr) {
        return;
    }
    RECT rc = {};
    GetWindowRect(control, &rc);
    MapWindowPoints(HWND_DESKTOP, m_hWnd, reinterpret_cast<LPPOINT>(&rc), 2);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, focused ? m_focusPen : m_borderPen));
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

void MainWindow::DrawEditCueBanner(HWND control, HDC hdc) {
    if (GetFocus() == control || GetWindowTextLengthW(control) > 0) {
        return;
    }
    const auto it = m_editCueTexts.find(control);
    if (it == m_editCueTexts.end()) {
        return;
    }
    RECT rc = {};
    GetClientRect(control, &rc);
    rc.left += 6;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kMutedText);
    SelectObject(hdc, m_uiFont);
    DrawTextW(hdc, it->second.c_str(), -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::CopySelectedLogs() {
    int index = -1;
    std::wstring text;
    while ((index = ListView_GetNextItem(m_hListView, index, LVNI_SELECTED)) != -1) {
        const LogEntry* entry = GetVisibleEntry(index);
        if (entry == nullptr) {
            continue;
        }
        if (!text.empty()) {
            text += L"\r\n";
        }
        text += entry->rawLine;
    }
    if (text.empty()) {
        return;
    }
    if (!OpenClipboard(m_hWnd)) {
        return;
    }
    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr) {
        void* dest = GlobalLock(memory);
        memcpy(dest, text.c_str(), bytes);
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
    }
    CloseClipboard();
}

LRESULT MainWindow::HandleNotify(LPARAM lParam) {
    auto* hdr = reinterpret_cast<NMHDR*>(lParam);
    if (hdr->hwndFrom == m_hListView) {
        if (hdr->code == LVN_GETDISPINFOW) {
            auto* dispInfo = reinterpret_cast<NMLVDISPINFOW*>(lParam);
            const LogEntry* entry = GetVisibleEntry(dispInfo->item.iItem);
            if (entry == nullptr) return 0;

            static thread_local std::wstring textBuffer;
            if ((dispInfo->item.mask & LVIF_TEXT) != 0) {
                switch (dispInfo->item.iSubItem) {
                case 0: textBuffer = entry->timestamp; break;
                case 1: textBuffer = entry->levelText; break;
                case 2: textBuffer = entry->pidText; break;
                case 3: textBuffer = entry->tag; break;
                default: textBuffer = entry->message; break;
                }
                wcsncpy_s(dispInfo->item.pszText, dispInfo->item.cchTextMax, textBuffer.c_str(), _TRUNCATE);
            }
            return 0;
        }
        if (hdr->code == NM_CUSTOMDRAW) {
            return HandleCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(lParam));
        }
    }
    return 0;
}

LRESULT MainWindow::HandleCustomDraw(NMLVCUSTOMDRAW* drawInfo) {
    switch (drawInfo->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        const LogEntry* entry = GetVisibleEntry(static_cast<int>(drawInfo->nmcd.dwItemSpec));
        drawInfo->clrText = entry != nullptr ? DarkMode::GetLevelColor(static_cast<int>(entry->level)) : DarkMode::kText;
        drawInfo->clrTextBk = DarkMode::kSurface;
        return CDRF_NEWFONT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

void MainWindow::PaintHeader(HWND hWnd, HDC hdc) {
    RECT client = {};
    GetClientRect(hWnd, &client);
    FillRect(hdc, &client, m_surfaceAltBrush);

    const int itemCount = Header_GetItemCount(hWnd);
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_uiFont));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kText);

    for (int i = 0; i < itemCount; ++i) {
        RECT itemRect = {};
        if (!Header_GetItemRect(hWnd, i, &itemRect)) {
            continue;
        }

        HDITEMW item = {};
        wchar_t text[128] = {};
        item.mask = HDI_TEXT | HDI_FORMAT;
        item.pszText = text;
        item.cchTextMax = static_cast<int>(sizeof(text) / sizeof(text[0]));
        Header_GetItem(hWnd, i, &item);

        RECT textRect = itemRect;
        textRect.left += 10;
        textRect.right -= 10;

        const bool sortUp = (item.fmt & HDF_SORTUP) != 0;
        const bool sortDown = (item.fmt & HDF_SORTDOWN) != 0;
        if (sortUp || sortDown) {
            RECT arrowRect = itemRect;
            arrowRect.right -= 8;
            arrowRect.left = arrowRect.right - 10;
            arrowRect.top += 8;
            arrowRect.bottom -= 8;

            POINT arrow[3] = {};
            if (sortUp) {
                arrow[0] = {arrowRect.left + 5, arrowRect.top};
                arrow[1] = {arrowRect.left, arrowRect.bottom};
                arrow[2] = {arrowRect.right, arrowRect.bottom};
            } else {
                arrow[0] = {arrowRect.left, arrowRect.top};
                arrow[1] = {arrowRect.right, arrowRect.top};
                arrow[2] = {arrowRect.left + 5, arrowRect.bottom};
            }
            HBRUSH arrowBrush = CreateSolidBrush(DarkMode::kMutedText);
            HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, arrowBrush));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Polygon(hdc, arrow, 3);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(arrowBrush);
            textRect.right = arrowRect.left - 6;
        }

        UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
        if ((item.fmt & HDF_CENTER) != 0) {
            format |= DT_CENTER;
        } else if ((item.fmt & HDF_RIGHT) != 0) {
            format |= DT_RIGHT;
        } else {
            format |= DT_LEFT;
        }
        DrawTextW(hdc, text, -1, &textRect, format);

        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, m_borderPen));
        MoveToEx(hdc, itemRect.right - 1, itemRect.top + 6, nullptr);
        LineTo(hdc, itemRect.right - 1, itemRect.bottom - 1);
        MoveToEx(hdc, itemRect.left, itemRect.bottom - 1, nullptr);
        LineTo(hdc, itemRect.right, itemRect.bottom - 1);
        SelectObject(hdc, oldPen);
    }

    SelectObject(hdc, oldFont);
}

INT_PTR MainWindow::HandleControlColor(HDC hdc, HWND control) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kText);
    if (control == m_hStatusLabel || control == m_hToolbarPanel) {
        SetBkColor(hdc, DarkMode::kBackground);
        return reinterpret_cast<INT_PTR>(m_bgBrush);
    }
    if (control == m_hAdbFilterEdit || control == m_hKeywordEdit || control == m_hTagEdit || control == m_hPidEdit) {
        SetBkColor(hdc, DarkMode::kSurfaceAlt);
        return reinterpret_cast<INT_PTR>(m_surfaceAltBrush);
    }
    SetBkColor(hdc, DarkMode::kSurface);
    return reinterpret_cast<INT_PTR>(m_surfaceBrush);
}

LRESULT MainWindow::HandleAppStatusMessage(LPARAM lParam) {
    std::unique_ptr<std::wstring> message(reinterpret_cast<std::wstring*>(lParam));
    if (message) SetStatus(*message);
    return 0;
}

LRESULT MainWindow::HandleDeviceListResultMessage(LPARAM lParam) {
    std::unique_ptr<DeviceListResult> result(reinterpret_cast<DeviceListResult*>(lParam));
    if (!result) {
        return 0;
    }

    const std::wstring currentSelection = result->keepSelection ? result->currentSelection : L"";
    m_deviceItems.clear();
    m_deviceComboItems.clear();
    m_deviceItems.push_back(L"Android Devices");
    m_deviceComboItems.push_back({m_deviceItems.back(), 0, false});
    std::vector<std::wstring> added;
    int selected = 0;
    bool matchedCurrentSelection = false;
    for (std::size_t i = 0; i < result->devices.size(); ++i) {
        const std::wstring label = L"* " + result->devices[i].serial + L" [" + result->devices[i].state + L"]";
        m_deviceItems.push_back(label);
        m_deviceComboItems.push_back({label, i + 1, true});
        added.push_back(result->devices[i].serial);
        if (!currentSelection.empty() && currentSelection == label) {
            selected = static_cast<int>(m_deviceItems.size() - 1);
            matchedCurrentSelection = true;
        }
    }
    for (const auto& device : m_knownDevices) {
        if (std::find(added.begin(), added.end(), device) != added.end()) {
            continue;
        }
        const std::wstring label = device + L" [saved]";
        m_deviceItems.push_back(label);
        m_deviceComboItems.push_back({label, m_deviceItems.size() - 1, false});
        if (!currentSelection.empty() && currentSelection == label) {
            selected = static_cast<int>(m_deviceItems.size() - 1);
            matchedCurrentSelection = true;
        }
    }
    m_deviceItems.push_back(L"\x6DFB\x52A0\x8BBE\x5907");
    m_deviceComboItems.push_back({m_deviceItems.back(), m_deviceItems.size() - 1, true});
    if (!matchedCurrentSelection && !result->devices.empty()) {
        selected = 1;
    }
    m_selectedDeviceIndex = selected;
    m_deviceComboControl.SetItems(m_deviceComboItems);
    m_deviceComboControl.SetSelection(m_selectedDeviceIndex, false);
    return 0;
}

LRESULT MainWindow::HandleDeviceConnectResultMessage(LPARAM lParam) {
    std::unique_ptr<DeviceConnectResult> result(reinterpret_cast<DeviceConnectResult*>(lParam));
    SetDeviceConnectInProgress(false);
    if (!result) {
        return 0;
    }

    if (result->success) {
        SaveKnownDevice(result->address);
        BeginRefreshDevices(false);
        const int connectedIndex = FindDeviceIndexBySerial(result->address);
        if (connectedIndex >= 0) {
            m_selectedDeviceIndex = connectedIndex;
        }
        m_deviceComboControl.SetSelection(m_selectedDeviceIndex, false);
        if (m_startAfterDeviceConnect) {
            m_startAfterDeviceConnect = false;
            SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STARTING));
            m_paused = false;
            UpdateActionToolbarState();
            SaveConfigIfNeeded();
            m_adbClient.Start(
                ReadAdbLaunchOptions(),
                [this](std::vector<LogEntry> batch) {
                    std::lock_guard<std::mutex> lock(m_pendingMutex);
                    m_pendingLogs.insert(m_pendingLogs.end(), batch.begin(), batch.end());
                },
                [this](const std::wstring& status) {
                    PostMessageW(m_hWnd, kAppStatusMessage, 0, reinterpret_cast<LPARAM>(new std::wstring(status)));
                });
            UpdateActionToolbarState();
        } else if (m_adbClient.IsRunning()) {
            RestartAdbCaptureForSelection();
        }
    } else {
        RemoveKnownDevice(result->address);
        m_startAfterDeviceConnect = false;
        BeginRefreshDevices(false);
        m_deviceComboControl.SetSelection(m_selectedDeviceIndex, false);
        MessageBoxW(m_hWnd, result->status.c_str(), L"\x8FDE\x63A5\x8BBE\x5907\x5931\x8D25", MB_OK | MB_ICONERROR);
    }
    SetStatus(result->status);
    return 0;
}

LRESULT MainWindow::HandleToolbarAdbCommandResultMessage(LPARAM lParam) {
    std::unique_ptr<ToolbarAdbCommandResult> result(reinterpret_cast<ToolbarAdbCommandResult*>(lParam));
    SetToolbarCommandInProgress(false);
    if (!result) {
        return 0;
    }

    if (result->commandId == IDC_ADB_KILL_SERVER_BUTTON || result->commandId == IDC_ADB_RESTART_SERVER_BUTTON) {
        if (m_adbClient.IsRunning()) {
            m_adbClient.Stop();
        }
    }

    if (result->success) {
        m_lastToolbarSuccessCommandId = result->commandId;
        SetStatus(result->status);
    } else {
        m_lastToolbarSuccessCommandId = 0;
        SetStatus(result->status);
        MessageBoxW(m_hWnd, result->status.c_str(), result->actionName.c_str(), MB_OK | MB_ICONERROR);
    }
    UpdateActionToolbarState();
    return 0;
}

LRESULT MainWindow::HandleDrawItem(WPARAM, LPARAM lParam) {
    auto* drawInfo = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
    switch (drawInfo->CtlID) {
    case 5001:
        DrawPopupListItem(drawInfo);
        return TRUE;
    case IDC_DEVICE_COMBO:
    case IDC_BUFFER_COMBO:
    case IDC_LEVEL_COMBO:
        DrawDropdownButton(drawInfo);
        return TRUE;
    case IDC_START_BUTTON:
    case IDC_STOP_BUTTON:
    case IDC_PAUSE_BUTTON:
    case IDC_EXPORT_BUTTON:
    case IDC_CLEAR_BUTTON:
    case IDC_APPLY_ADB_FILTER_BUTTON:
        DrawButton(drawInfo);
        return TRUE;
    default:
        return FALSE;
    }
}

LRESULT MainWindow::HandleMeasureItem(LPARAM lParam) {
    auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
    if (measure && measure->CtlID == 5001) {
        measure->itemHeight = 24;
        return TRUE;
    }
    return FALSE;
}

LRESULT MainWindow::HandleContextMenu(WPARAM wParam, LPARAM lParam) {
    if (reinterpret_cast<HWND>(wParam) != m_hListView) {
        return DefWindowProcW(m_hWnd, WM_CONTEXTMENU, wParam, lParam);
    }

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Copy");
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, nullptr);
    DestroyMenu(menu);
    if (cmd == 1) {
        CopySelectedLogs();
    }
    return 0;
}

LRESULT MainWindow::HandleListKeyDown(MSG* msg) {
    if (msg->hwnd == m_hListView && msg->message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && msg->wParam == 'C') {
        CopySelectedLogs();
        return 0;
    }
    return 1;
}

void MainWindow::ShowPickerPopup(HWND picker) {
    if (picker == nullptr || m_hPopupHost == nullptr || m_hPopupList == nullptr) {
        return;
    }
    m_hActivePicker = picker;
    const auto& items = (picker == m_hDeviceCombo) ? m_deviceItems : m_levelItems;
    const int selected = (picker == m_hDeviceCombo) ? m_selectedDeviceIndex : m_selectedLevelIndex;
    SendMessageW(m_hPopupList, LB_RESETCONTENT, 0, 0);
    for (const auto& item : items) {
        SendMessageW(m_hPopupList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    }
    if (selected >= 0) {
        SendMessageW(m_hPopupList, LB_SETCURSEL, selected, 0);
        SendMessageW(m_hPopupList, LB_SETTOPINDEX, selected, 0);
    }

    RECT rc = {};
    GetWindowRect(picker, &rc);
    const int itemCount = static_cast<int>(items.size());
    const int listHeight = std::max(28, std::min(itemCount, 12) * 24);
    const int hostWidth = std::max(120, static_cast<int>(rc.right - rc.left));
    const int hostHeight = listHeight + 2;

    int x = rc.left;
    int y = rc.bottom + 2;
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo)) {
        const RECT work = monitorInfo.rcWork;
        const int availableBelow = work.bottom - rc.bottom - 8;
        if (availableBelow < hostHeight) {
            y = rc.top - hostHeight - 2;
        }
        if (y < work.top + 4) {
            y = work.top + 4;
        }
        // Keep left aligned to picker whenever possible; only clamp when out of monitor work area.
        if (x + hostWidth > work.right - 4) {
            x = work.right - 4 - hostWidth;
        }
        if (x < work.left + 4) {
            x = work.left + 4;
        }
    }

    SetWindowPos(m_hPopupHost, HWND_TOPMOST, x, y, hostWidth, hostHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    MoveWindow(m_hPopupList, 1, 1, hostWidth - 2, hostHeight - 2, TRUE);
    ShowWindow(m_hPopupList, SW_SHOWNA);
    RedrawWindow(m_hPopupHost, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
    SetFocus(m_hPopupList);
}

void MainWindow::HidePickerPopup() {
    if (m_hPopupList != nullptr) {
        ShowWindow(m_hPopupList, SW_HIDE);
    }
    if (m_hPopupHost != nullptr) {
        ShowWindow(m_hPopupHost, SW_HIDE);
    }
    m_hActivePicker = nullptr;
}

void MainWindow::ApplyPopupSelection() {
    if (m_hActivePicker == nullptr) {
        return;
    }
    const int selected = static_cast<int>(SendMessageW(m_hPopupList, LB_GETCURSEL, 0, 0));
    if (selected < 0) {
        HidePickerPopup();
        return;
    }

    if (m_hActivePicker == m_hDeviceCombo) {
        if (selected == static_cast<int>(m_deviceItems.size()) - 1) {
            HidePickerPopup();
            AddDeviceDialog dialog(m_instance);
            std::wstring address;
            if (dialog.ShowModal(m_hWnd, address)) {
                BeginConnectDevice(address);
            }
            return;
        }
        m_selectedDeviceIndex = selected;
        SetWindowTextW(m_hDeviceCombo, GetSelectedDeviceText().c_str());
    } else if (m_hActivePicker == m_hLevelCombo) {
        m_selectedLevelIndex = selected;
        SetWindowTextW(m_hLevelCombo, GetSelectedLevelText().c_str());
        OnFilterChanged();
    }
    SaveConfigIfNeeded();
    HidePickerPopup();
}

std::wstring MainWindow::GetSelectedDeviceText() const {
    if (m_selectedDeviceIndex >= 0 && m_selectedDeviceIndex < static_cast<int>(m_deviceItems.size())) {
        return m_deviceItems[static_cast<std::size_t>(m_selectedDeviceIndex)];
    }
    return ResourceStrings::Load(m_instance, IDS_DEVICE_AUTO);
}

std::wstring MainWindow::GetSelectedBufferText() const {
    if (m_selectedBufferIndex >= 0 && m_selectedBufferIndex < static_cast<int>(m_bufferItems.size())) {
        return m_bufferItems[static_cast<std::size_t>(m_selectedBufferIndex)];
    }
    return L"main";
}

std::wstring MainWindow::GetSelectedLevelText() const {
    if (m_selectedLevelIndex >= 0 && m_selectedLevelIndex < static_cast<int>(m_levelItems.size())) {
        return m_levelItems[static_cast<std::size_t>(m_selectedLevelIndex)];
    }
    return m_levelItems.empty() ? L"" : m_levelItems.front();
}

LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* window = reinterpret_cast<MainWindow*>(refData);
    if (message == WM_PAINT) {
        LRESULT result = DefSubclassProc(hWnd, message, wParam, lParam);
        HDC hdc = GetDC(hWnd);
        window->DrawEditCueBanner(hWnd, hdc);
        ReleaseDC(hWnd, hdc);
        return result;
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_SETTEXT) {
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    return DefSubclassProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::ListSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* window = reinterpret_cast<MainWindow*>(refData);
    if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'C') {
        window->CopySelectedLogs();
        return 0;
    }
    return DefSubclassProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::HeaderSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* window = reinterpret_cast<MainWindow*>(refData);
    switch (message) {
    case WM_THEMECHANGED:
        SetWindowTheme(hWnd, L"", L"");
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hWnd, &ps);
        window->PaintHeader(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    default:
        return DefSubclassProc(hWnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK MainWindow::PopupListSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* window = reinterpret_cast<MainWindow*>(refData);
    switch (message) {
    case WM_KILLFOCUS: {
        HWND next = reinterpret_cast<HWND>(wParam);
        if (next != window->m_hDeviceCombo && next != window->m_hLevelCombo) {
            window->HidePickerPopup();
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            window->ApplyPopupSelection();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            window->HidePickerPopup();
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        window->ApplyPopupSelection();
        return 0;
    default:
        break;
    }
    return DefSubclassProc(hWnd, message, wParam, lParam);
}
