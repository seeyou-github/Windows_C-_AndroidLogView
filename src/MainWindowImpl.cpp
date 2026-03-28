#include "MainWindow.h"

#include "DarkMode.h"
#include "LogParser.h"
#include "ResourceStrings.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <memory>

#include "../res/resource.h"

namespace {
constexpr int kToolbarHeight = 118;
constexpr int kControlHeight = 30;
constexpr int kMargin = 12;
constexpr UINT_PTR kFlushTimerId = 1001;
constexpr UINT kFlushIntervalMs = 50;
constexpr UINT kAppStatusMessage = WM_APP + 1;
constexpr std::size_t kMaxLogEntries = 200000;

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
}  // namespace

MainWindow::MainWindow(HINSTANCE instance)
    : m_instance(instance),
      m_hWnd(nullptr),
      m_hToolbarPanel(nullptr),
      m_hDeviceCombo(nullptr),
      m_hAdbFilterEdit(nullptr),
      m_hKeywordEdit(nullptr),
      m_hTagEdit(nullptr),
      m_hPidEdit(nullptr),
      m_hLevelCombo(nullptr),
      m_hStartButton(nullptr),
      m_hStopButton(nullptr),
      m_hPauseButton(nullptr),
      m_hExportButton(nullptr),
      m_hClearButton(nullptr),
      m_hListView(nullptr),
      m_hStatusLabel(nullptr),
      m_uiFont(nullptr),
      m_monoFont(nullptr),
      m_bgBrush(nullptr),
      m_surfaceBrush(nullptr),
      m_surfaceAltBrush(nullptr),
      m_borderPen(nullptr),
      m_focusPen(nullptr),
      m_config(GetExeDir()),
      m_logBuffer(kMaxLogEntries),
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
    case WM_COMMAND:
        window->OnCommand(wParam, lParam);
        return 0;
    case WM_NOTIFY:
        return window->HandleNotify(lParam);
    case WM_TIMER:
        window->OnTimer(wParam);
        return 0;
    case WM_DRAWITEM:
        return window->HandleDrawItem(wParam, lParam);
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
}

void MainWindow::CreateControls() {
    m_hToolbarPanel = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd, nullptr, m_instance, nullptr);
    m_hDeviceCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_DEVICE_COMBO), m_instance, nullptr);
    m_hAdbFilterEdit = CreateWindowExW(0, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                       reinterpret_cast<HMENU>(IDC_ADB_FILTER_EDIT), m_instance, nullptr);
    m_hKeywordEdit = CreateWindowExW(0, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_KEYWORD_EDIT), m_instance, nullptr);
    m_hTagEdit = CreateWindowExW(0, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_TAG_EDIT), m_instance, nullptr);
    m_hPidEdit = CreateWindowExW(0, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_PID_EDIT), m_instance, nullptr);
    m_hLevelCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, m_hWnd,
                                    reinterpret_cast<HMENU>(IDC_LEVEL_COMBO), m_instance, nullptr);
    m_hStartButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_START).c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_START_BUTTON),
                                     m_instance, nullptr);
    m_hStopButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_STOP).c_str(),
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_STOP_BUTTON),
                                    m_instance, nullptr);
    m_hPauseButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_PAUSE).c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_PAUSE_BUTTON),
                                     m_instance, nullptr);
    m_hExportButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_EXPORT).c_str(),
                                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_EXPORT_BUTTON),
                                      m_instance, nullptr);
    m_hClearButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_CLEAR).c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_CLEAR_BUTTON),
                                     m_instance, nullptr);
    m_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                                  0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_LOG_LIST), m_instance, nullptr);
    m_hStatusLabel = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_STATUS_LABEL), m_instance, nullptr);

    const HWND controls[] = {m_hDeviceCombo,  m_hAdbFilterEdit, m_hKeywordEdit, m_hTagEdit,    m_hPidEdit,    m_hLevelCombo,
                             m_hStartButton,  m_hStopButton,    m_hPauseButton, m_hExportButton, m_hClearButton, m_hListView,
                             m_hStatusLabel};
    for (HWND control : controls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(control == m_hListView ? m_monoFont : m_uiFont), TRUE);
    }

    auto adbFilterCue = ResourceStrings::Load(m_instance, IDS_HINT_ADB_FILTER);
    auto keywordCue = ResourceStrings::Load(m_instance, IDS_HINT_KEYWORD);
    auto tagCue = ResourceStrings::Load(m_instance, IDS_HINT_TAG);
    auto pidCue = ResourceStrings::Load(m_instance, IDS_HINT_PID);
    SendMessageW(m_hAdbFilterEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(adbFilterCue.c_str()));
    SendMessageW(m_hKeywordEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(keywordCue.c_str()));
    SendMessageW(m_hTagEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tagCue.c_str()));
    SendMessageW(m_hPidEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(pidCue.c_str()));

    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_ALL).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_VERBOSE).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_DEBUG).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_INFO).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_WARN).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_ERROR).c_str());
    ComboBox_AddString(m_hLevelCombo, ResourceStrings::Load(m_instance, IDS_LEVEL_FATAL).c_str());
    ComboBox_SetCurSel(m_hLevelCombo, 0);

    InitializeListView();
    ApplyThemeToChildren();
    RefreshDevices(false);
}

void MainWindow::InitializeListView() {
    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ListView_SetBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextColor(m_hListView, DarkMode::kText);

    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    auto timeLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_TIME);
    column.pszText = const_cast<wchar_t*>(timeLabel.c_str());
    column.cx = 150;
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

void MainWindow::ApplyThemeToChildren() {
    DarkMode::ApplyWindowDarkMode(m_hWnd);
    const HWND controls[] = {m_hDeviceCombo, m_hAdbFilterEdit, m_hKeywordEdit, m_hTagEdit, m_hPidEdit, m_hLevelCombo, m_hListView};
    for (HWND control : controls) {
        DarkMode::ApplyControlTheme(control);
    }
}

void MainWindow::LayoutControls(int width, int height) {
    const int row1Y = 14;
    const int row2Y = 54;
    const int statusHeight = 24;
    const int buttonWidth = 82;
    const int actionX = width - kMargin - (buttonWidth + 8) * 5;
    const int leftWidth = actionX - (kMargin * 2);
    const int rowGap = 10;
    const int deviceWidth = 230;
    const int adbFilterWidth = std::max(260, leftWidth - deviceWidth - rowGap);
    const int keywordWidth = 260;
    const int tagWidth = 170;
    const int pidWidth = 90;
    const int levelWidth = 110;

    MoveWindow(m_hToolbarPanel, 0, 0, width, kToolbarHeight, TRUE);
    MoveWindow(m_hDeviceCombo, kMargin, row1Y, deviceWidth, kControlHeight + 200, TRUE);
    MoveWindow(m_hAdbFilterEdit, kMargin + deviceWidth + rowGap, row1Y, adbFilterWidth, kControlHeight, TRUE);

    int x = kMargin;
    MoveWindow(m_hKeywordEdit, x, row2Y, keywordWidth, kControlHeight, TRUE);
    x += keywordWidth + rowGap;
    MoveWindow(m_hTagEdit, x, row2Y, tagWidth, kControlHeight, TRUE);
    x += tagWidth + rowGap;
    MoveWindow(m_hPidEdit, x, row2Y, pidWidth, kControlHeight, TRUE);
    x += pidWidth + rowGap;
    MoveWindow(m_hLevelCombo, x, row2Y, levelWidth, kControlHeight + 180, TRUE);

    int buttonX = actionX;
    MoveWindow(m_hStartButton, buttonX, row1Y, buttonWidth, kControlHeight, TRUE);
    buttonX += buttonWidth + 8;
    MoveWindow(m_hStopButton, buttonX, row1Y, buttonWidth, kControlHeight, TRUE);
    buttonX += buttonWidth + 8;
    MoveWindow(m_hPauseButton, buttonX, row1Y, buttonWidth, kControlHeight, TRUE);
    buttonX += buttonWidth + 8;
    MoveWindow(m_hExportButton, buttonX, row1Y, buttonWidth, kControlHeight, TRUE);
    buttonX += buttonWidth + 8;
    MoveWindow(m_hClearButton, buttonX, row1Y, buttonWidth, kControlHeight, TRUE);

    MoveWindow(m_hListView, kMargin, kToolbarHeight, width - kMargin * 2, height - kToolbarHeight - statusHeight - kMargin, TRUE);
    MoveWindow(m_hStatusLabel, kMargin, height - statusHeight - 4, width - kMargin * 2, statusHeight, TRUE);
}

void MainWindow::OnCreate() {
    InitThemeResources();
    CreateControls();
    LoadConfig();
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_READY));
    SetTimer(m_hWnd, kFlushTimerId, kFlushIntervalMs, nullptr);
}

void MainWindow::OnSize(int width, int height) {
    LayoutControls(width, height);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM) {
    const WORD controlId = LOWORD(wParam);
    const WORD notifyCode = HIWORD(wParam);

    if (controlId == IDC_START_BUTTON && notifyCode == BN_CLICKED) {
        OnStart();
        return;
    }
    if (controlId == IDC_STOP_BUTTON && notifyCode == BN_CLICKED) {
        OnStop();
        return;
    }
    if (controlId == IDC_PAUSE_BUTTON && notifyCode == BN_CLICKED) {
        OnPauseResume();
        return;
    }
    if (controlId == IDC_EXPORT_BUTTON && notifyCode == BN_CLICKED) {
        OnExport();
        return;
    }
    if (controlId == IDC_CLEAR_BUTTON && notifyCode == BN_CLICKED) {
        OnClear();
        return;
    }

    if ((controlId == IDC_KEYWORD_EDIT || controlId == IDC_TAG_EDIT || controlId == IDC_PID_EDIT) && notifyCode == EN_CHANGE) {
        OnFilterChanged();
        SaveConfigIfNeeded();
        return;
    }
    if (controlId == IDC_ADB_FILTER_EDIT && notifyCode == EN_CHANGE) {
        SaveConfigIfNeeded();
        return;
    }
    if ((controlId == IDC_LEVEL_COMBO || controlId == IDC_DEVICE_COMBO) && notifyCode == CBN_SELCHANGE) {
        if (controlId == IDC_LEVEL_COMBO) {
            OnFilterChanged();
        }
        SaveConfigIfNeeded();
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

    RefreshDevices(true);
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STARTING));
    m_paused = false;
    SetWindowTextW(m_hPauseButton, ResourceStrings::Load(m_instance, IDS_BUTTON_PAUSE).c_str());
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
}

void MainWindow::OnStop() {
    if (!m_adbClient.IsRunning()) {
        return;
    }
    m_adbClient.Stop();
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STOPPED));
}

void MainWindow::OnClear() {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingLogs.clear();
    m_logBuffer.Clear();
    m_visibleIndexes.clear();
    ListView_SetItemCountEx(m_hListView, 0, LVSICF_NOSCROLL);
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_CLEARED));
}

void MainWindow::OnPauseResume() {
    m_paused = !m_paused;
    SetWindowTextW(m_hPauseButton, ResourceStrings::Load(m_instance, m_paused ? IDS_BUTTON_RESUME : IDS_BUTTON_PAUSE).c_str());
    if (m_paused) {
        SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_PAUSED));
    } else {
        FlushPendingLogs();
        SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_READY));
    }
    InvalidateRect(m_hPauseButton, nullptr, TRUE);
}

void MainWindow::OnExport() {
    const std::wstring path = GetExeDir() + L"\\logs_export.txt";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetStatus(L"Export failed.");
        return;
    }

    const char bom[] = "\xEF\xBB\xBF";
    DWORD written = 0;
    WriteFile(file, bom, 3, &written, nullptr);
    for (std::size_t index : m_visibleIndexes) {
        const LogEntry* entry = m_logBuffer.At(index);
        if (entry == nullptr) continue;
        std::wstring line = entry->rawLine + L"\r\n";
        int bytes = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), utf8.data(), bytes, nullptr, nullptr);
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
    CloseHandle(file);
    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_EXPORTED) + L": " + path);
}

void MainWindow::RefreshDevices(bool keepSelection) {
    const std::wstring currentSelection = keepSelection ? GetWindowTextString(m_hDeviceCombo) : L"";
    ComboBox_ResetContent(m_hDeviceCombo);
    ComboBox_AddString(m_hDeviceCombo, ResourceStrings::Load(m_instance, IDS_DEVICE_AUTO).c_str());
    auto devices = AdbClient::ListDevices();
    int selected = 0;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const std::wstring label = devices[i].serial + L" [" + devices[i].state + L"]";
        ComboBox_AddString(m_hDeviceCombo, label.c_str());
        if (!currentSelection.empty() && currentSelection == label) {
            selected = static_cast<int>(i + 1);
        }
    }
    ComboBox_SetCurSel(m_hDeviceCombo, selected);
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
    const std::size_t dropped = m_logBuffer.AppendBatch(batch);

    if (dropped > 0) {
        RebuildVisibleIndexes(!followBottom);
    } else {
        const std::size_t newSize = m_logBuffer.Size();
        for (std::size_t i = oldSize; i < newSize; ++i) {
            const LogEntry* entry = m_logBuffer.At(i);
            if (entry != nullptr && FilterEngine::Matches(*entry, m_filters)) {
                m_visibleIndexes.push_back(i);
            }
        }
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

void MainWindow::SetStatus(const std::wstring& text) {
    m_statusText = text;
    UpdateStatusText();
}

FilterOptions MainWindow::ReadFilterOptions() const {
    FilterOptions options;
    options.keyword = GetWindowTextString(m_hKeywordEdit);
    options.tag = GetWindowTextString(m_hTagEdit);
    options.pidText = GetWindowTextString(m_hPidEdit);

    switch (ComboBox_GetCurSel(m_hLevelCombo)) {
    case 2: options.minimumLevel = LogLevel::Debug; break;
    case 3: options.minimumLevel = LogLevel::Info; break;
    case 4: options.minimumLevel = LogLevel::Warn; break;
    case 5: options.minimumLevel = LogLevel::Error; break;
    case 6: options.minimumLevel = LogLevel::Fatal; break;
    default: options.minimumLevel = LogLevel::Verbose; break;
    }
    return options;
}

AdbLaunchOptions MainWindow::ReadAdbLaunchOptions() const {
    AdbLaunchOptions options;
    const int selected = ComboBox_GetCurSel(m_hDeviceCombo);
    const std::wstring text = GetWindowTextString(m_hDeviceCombo);
    if (selected > 0) {
        const std::size_t pos = text.find(L" [");
        options.deviceSerial = pos == std::wstring::npos ? text : text.substr(0, pos);
    }
    options.adbPriorityFilter = GetWindowTextString(m_hAdbFilterEdit);
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
    m_config.Load();
    SetWindowTextW(m_hAdbFilterEdit, m_config.Get(L"adb_filter").c_str());
    SetWindowTextW(m_hKeywordEdit, m_config.Get(L"keyword").c_str());
    SetWindowTextW(m_hTagEdit, m_config.Get(L"tag").c_str());
    SetWindowTextW(m_hPidEdit, m_config.Get(L"pid").c_str());
    const int level = _wtoi(m_config.Get(L"level", L"0").c_str());
    ComboBox_SetCurSel(m_hLevelCombo, std::clamp(level, 0, 6));

    RefreshDevices(false);
    const std::wstring device = m_config.Get(L"device");
    if (!device.empty()) {
        const int count = ComboBox_GetCount(m_hDeviceCombo);
        for (int i = 0; i < count; ++i) {
            wchar_t buffer[256] = {};
            ComboBox_GetLBText(m_hDeviceCombo, i, buffer);
            if (device == buffer) {
                ComboBox_SetCurSel(m_hDeviceCombo, i);
                break;
            }
        }
    }
    m_filters = ReadFilterOptions();
}

void MainWindow::SaveConfigIfNeeded() {
    m_config.Set(L"adb_filter", GetWindowTextString(m_hAdbFilterEdit));
    m_config.Set(L"keyword", GetWindowTextString(m_hKeywordEdit));
    m_config.Set(L"tag", GetWindowTextString(m_hTagEdit));
    m_config.Set(L"pid", GetWindowTextString(m_hPidEdit));
    m_config.Set(L"level", std::to_wstring(ComboBox_GetCurSel(m_hLevelCombo)));
    m_config.Set(L"device", GetWindowTextString(m_hDeviceCombo));
    m_config.SaveIfChanged();
}

void MainWindow::PaintCustomChrome(HDC hdc) {
    RECT client = {};
    GetClientRect(m_hWnd, &client);
    RECT topBar = {0, 0, client.right, kToolbarHeight};
    FillRect(hdc, &topBar, m_bgBrush);

    SelectObject(hdc, m_uiFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kMutedText);
    const std::wstring title = ResourceStrings::Load(m_instance, IDS_APP_TITLE);
    TextOutW(hdc, kMargin, 6, title.c_str(), static_cast<int>(title.size()));

    DrawControlBorder(hdc, m_hDeviceCombo, GetFocus() == m_hDeviceCombo);
    DrawControlBorder(hdc, m_hAdbFilterEdit, GetFocus() == m_hAdbFilterEdit);
    DrawControlBorder(hdc, m_hKeywordEdit, GetFocus() == m_hKeywordEdit);
    DrawControlBorder(hdc, m_hTagEdit, GetFocus() == m_hTagEdit);
    DrawControlBorder(hdc, m_hPidEdit, GetFocus() == m_hPidEdit);
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

void MainWindow::DrawControlBorder(HDC hdc, HWND control, bool focused) {
    RECT rc = {};
    GetWindowRect(control, &rc);
    MapWindowPoints(HWND_DESKTOP, m_hWnd, reinterpret_cast<LPPOINT>(&rc), 2);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, focused ? m_focusPen : m_borderPen));
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
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
                case 1: textBuffer = LogParser::LevelToText(entry->level); break;
                case 2: textBuffer = std::to_wstring(entry->pid); break;
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

INT_PTR MainWindow::HandleControlColor(HDC hdc, HWND control) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, DarkMode::kText);
    if (control == m_hStatusLabel || control == m_hToolbarPanel) {
        SetBkColor(hdc, DarkMode::kBackground);
        return reinterpret_cast<INT_PTR>(m_bgBrush);
    }
    SetBkColor(hdc, DarkMode::kSurface);
    return reinterpret_cast<INT_PTR>(m_surfaceBrush);
}

LRESULT MainWindow::HandleAppStatusMessage(LPARAM lParam) {
    std::unique_ptr<std::wstring> message(reinterpret_cast<std::wstring*>(lParam));
    if (message) SetStatus(*message);
    return 0;
}

LRESULT MainWindow::HandleDrawItem(WPARAM, LPARAM lParam) {
    auto* drawInfo = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
    switch (drawInfo->CtlID) {
    case IDC_START_BUTTON:
    case IDC_STOP_BUTTON:
    case IDC_PAUSE_BUTTON:
    case IDC_EXPORT_BUTTON:
    case IDC_CLEAR_BUTTON:
        DrawButton(drawInfo);
        return TRUE;
    default:
        return FALSE;
    }
}
