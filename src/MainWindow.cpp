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
constexpr int kToolbarHeight = 84;
constexpr int kControlHeight = 30;
constexpr int kMargin = 12;
constexpr UINT_PTR kFlushTimerId = 1001;
constexpr UINT kFlushIntervalMs = 50;
constexpr UINT kAppStatusMessage = WM_APP + 1;
constexpr std::size_t kMaxLogEntries = 200000;

std::wstring FormatStatusLine(const std::wstring& baseText, std::size_t totalCount, std::size_t visibleCount) {
    return baseText + L"   Total: " + std::to_wstring(totalCount) + L"   Visible: " + std::to_wstring(visibleCount);
}
}  // namespace

MainWindow::MainWindow(HINSTANCE instance)
    : m_instance(instance),
      m_hWnd(nullptr),
      m_hToolbarPanel(nullptr),
      m_hKeywordEdit(nullptr),
      m_hTagEdit(nullptr),
      m_hPidEdit(nullptr),
      m_hLevelCombo(nullptr),
      m_hStartButton(nullptr),
      m_hStopButton(nullptr),
      m_hClearButton(nullptr),
      m_hListView(nullptr),
      m_hStatusLabel(nullptr),
      m_uiFont(nullptr),
      m_monoFont(nullptr),
      m_bgBrush(nullptr),
      m_surfaceBrush(nullptr),
      m_surfaceAltBrush(nullptr),
      m_logBuffer(kMaxLogEntries) {
}

MainWindow::~MainWindow() {
    if (m_uiFont != nullptr) {
        DeleteObject(m_uiFont);
    }
    if (m_monoFont != nullptr) {
        DeleteObject(m_monoFont);
    }
    if (m_bgBrush != nullptr) {
        DeleteObject(m_bgBrush);
    }
    if (m_surfaceBrush != nullptr) {
        DeleteObject(m_surfaceBrush);
    }
    if (m_surfaceAltBrush != nullptr) {
        DeleteObject(m_surfaceAltBrush);
    }
}

bool MainWindow::Create(int x, int y, int width, int height, const wchar_t* title) {
    m_hWnd = CreateWindowExW(
        0,
        L"AndroidLogViewerMainWindow",
        title,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        m_instance,
        this);
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
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
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
    m_hKeywordEdit = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_KEYWORD_EDIT), m_instance, nullptr);
    m_hTagEdit = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_TAG_EDIT), m_instance, nullptr);
    m_hPidEdit = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, m_hWnd,
                                 reinterpret_cast<HMENU>(IDC_PID_EDIT), m_instance, nullptr);
    m_hLevelCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, m_hWnd,
                                    reinterpret_cast<HMENU>(IDC_LEVEL_COMBO), m_instance, nullptr);
    m_hStartButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_START).c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_START_BUTTON),
                                     m_instance, nullptr);
    m_hStopButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_STOP).c_str(),
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_STOP_BUTTON),
                                    m_instance, nullptr);
    m_hClearButton = CreateWindowExW(0, WC_BUTTONW, ResourceStrings::Load(m_instance, IDS_BUTTON_CLEAR).c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_CLEAR_BUTTON),
                                     m_instance, nullptr);
    m_hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                  WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                                  0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDC_LOG_LIST), m_instance, nullptr);
    m_hStatusLabel = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd,
                                     reinterpret_cast<HMENU>(IDC_STATUS_LABEL), m_instance, nullptr);

    SendMessageW(m_hKeywordEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hTagEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hPidEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hLevelCombo, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hStartButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hStopButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hClearButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);
    SendMessageW(m_hListView, WM_SETFONT, reinterpret_cast<WPARAM>(m_monoFont), TRUE);
    SendMessageW(m_hStatusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_uiFont), TRUE);

    auto keywordCue = ResourceStrings::Load(m_instance, IDS_HINT_KEYWORD);
    auto tagCue = ResourceStrings::Load(m_instance, IDS_HINT_TAG);
    auto pidCue = ResourceStrings::Load(m_instance, IDS_HINT_PID);
    SendMessageW(m_hKeywordEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(keywordCue.c_str()));
    SendMessageW(m_hTagEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(tagCue.c_str()));
    SendMessageW(m_hPidEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(pidCue.c_str()));

    auto levelAll = ResourceStrings::Load(m_instance, IDS_LEVEL_ALL);
    auto levelVerbose = ResourceStrings::Load(m_instance, IDS_LEVEL_VERBOSE);
    auto levelDebug = ResourceStrings::Load(m_instance, IDS_LEVEL_DEBUG);
    auto levelInfo = ResourceStrings::Load(m_instance, IDS_LEVEL_INFO);
    auto levelWarn = ResourceStrings::Load(m_instance, IDS_LEVEL_WARN);
    auto levelError = ResourceStrings::Load(m_instance, IDS_LEVEL_ERROR);
    auto levelFatal = ResourceStrings::Load(m_instance, IDS_LEVEL_FATAL);

    ComboBox_AddString(m_hLevelCombo, levelAll.c_str());
    ComboBox_AddString(m_hLevelCombo, levelVerbose.c_str());
    ComboBox_AddString(m_hLevelCombo, levelDebug.c_str());
    ComboBox_AddString(m_hLevelCombo, levelInfo.c_str());
    ComboBox_AddString(m_hLevelCombo, levelWarn.c_str());
    ComboBox_AddString(m_hLevelCombo, levelError.c_str());
    ComboBox_AddString(m_hLevelCombo, levelFatal.c_str());
    ComboBox_SetCurSel(m_hLevelCombo, 0);

    InitializeListView();
    ApplyThemeToChildren();
}

void MainWindow::InitializeListView() {
    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ListView_SetBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextBkColor(m_hListView, DarkMode::kSurface);
    ListView_SetTextColor(m_hListView, DarkMode::kText);

    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    auto timeLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_TIME);
    column.pszText = timeLabel.data();
    column.cx = 150;
    ListView_InsertColumn(m_hListView, 0, &column);

    auto levelLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_LEVEL);
    column.pszText = levelLabel.data();
    column.cx = 70;
    column.iSubItem = 1;
    ListView_InsertColumn(m_hListView, 1, &column);

    auto pidLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_PID);
    column.pszText = pidLabel.data();
    column.cx = 80;
    column.iSubItem = 2;
    ListView_InsertColumn(m_hListView, 2, &column);

    auto tagLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_TAG);
    column.pszText = tagLabel.data();
    column.cx = 180;
    column.iSubItem = 3;
    ListView_InsertColumn(m_hListView, 3, &column);

    auto msgLabel = ResourceStrings::Load(m_instance, IDS_COLUMN_MESSAGE);
    column.pszText = msgLabel.data();
    column.cx = 900;
    column.iSubItem = 4;
    ListView_InsertColumn(m_hListView, 4, &column);
}

void MainWindow::ApplyThemeToChildren() {
    DarkMode::ApplyWindowDarkMode(m_hWnd);
    DarkMode::ApplyControlTheme(m_hKeywordEdit);
    DarkMode::ApplyControlTheme(m_hTagEdit);
    DarkMode::ApplyControlTheme(m_hPidEdit);
    DarkMode::ApplyControlTheme(m_hLevelCombo);
    DarkMode::ApplyControlTheme(m_hStartButton);
    DarkMode::ApplyControlTheme(m_hStopButton);
    DarkMode::ApplyControlTheme(m_hClearButton);
    DarkMode::ApplyControlTheme(m_hListView);
}

void MainWindow::LayoutControls(int width, int height) {
    const int rowY = kMargin + 8;
    const int statusHeight = 24;
    const int buttonWidth = 88;
    const int levelWidth = 110;
    const int pidWidth = 100;
    const int tagWidth = 180;
    const int actionX = width - kMargin - buttonWidth * 3 - kMargin * 2;
    const int levelX = actionX - kMargin - levelWidth;
    const int pidX = levelX - kMargin - pidWidth;
    const int tagX = pidX - kMargin - tagWidth;
    const int keywordX = kMargin;
    const int keywordWidth = std::max(180, tagX - kMargin - keywordX);

    MoveWindow(m_hToolbarPanel, 0, 0, width, kToolbarHeight, TRUE);
    MoveWindow(m_hKeywordEdit, keywordX, rowY, keywordWidth, kControlHeight, TRUE);
    MoveWindow(m_hTagEdit, tagX, rowY, tagWidth, kControlHeight, TRUE);
    MoveWindow(m_hPidEdit, pidX, rowY, pidWidth, kControlHeight, TRUE);
    MoveWindow(m_hLevelCombo, levelX, rowY, levelWidth, kControlHeight + 120, TRUE);
    MoveWindow(m_hStartButton, actionX, rowY, buttonWidth, kControlHeight, TRUE);
    MoveWindow(m_hStopButton, actionX + buttonWidth + kMargin, rowY, buttonWidth, kControlHeight, TRUE);
    MoveWindow(m_hClearButton, actionX + (buttonWidth + kMargin) * 2, rowY, buttonWidth, kControlHeight, TRUE);
    MoveWindow(m_hListView, kMargin, kToolbarHeight, width - kMargin * 2, height - kToolbarHeight - statusHeight - kMargin, TRUE);
    MoveWindow(m_hStatusLabel, kMargin, height - statusHeight - 4, width - kMargin * 2, statusHeight, TRUE);
}

void MainWindow::OnCreate() {
    InitThemeResources();
    CreateControls();
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
    if (controlId == IDC_CLEAR_BUTTON && notifyCode == BN_CLICKED) {
        OnClear();
        return;
    }

    if ((controlId == IDC_KEYWORD_EDIT || controlId == IDC_TAG_EDIT || controlId == IDC_PID_EDIT) && notifyCode == EN_CHANGE) {
        OnFilterChanged();
        return;
    }
    if (controlId == IDC_LEVEL_COMBO && notifyCode == CBN_SELCHANGE) {
        OnFilterChanged();
        return;
    }
}

void MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == kFlushTimerId) {
        FlushPendingLogs();
    }
}

void MainWindow::OnDestroy() {
    KillTimer(m_hWnd, kFlushTimerId);
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

    SetStatus(ResourceStrings::Load(m_instance, IDS_STATUS_STARTING));
    m_adbClient.Start(
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

void MainWindow::FlushPendingLogs() {
    std::vector<LogEntry> batch;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_pendingLogs.empty()) {
            return;
        }
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

    if (followBottom) {
        ScrollToBottom();
    }
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
    SetWindowTextW(m_hStatusLabel, FormatStatusLine(m_statusText, m_logBuffer.Size(), m_visibleIndexes.size()).c_str());
}

void MainWindow::SetStatus(const std::wstring& text) {
    m_statusText = text;
    UpdateStatusText();
}

FilterOptions MainWindow::ReadFilterOptions() const {
    FilterOptions options;

    wchar_t buffer[256] = {};
    GetWindowTextW(m_hKeywordEdit, buffer, static_cast<int>(std::size(buffer)));
    options.keyword = buffer;

    GetWindowTextW(m_hTagEdit, buffer, static_cast<int>(std::size(buffer)));
    options.tag = buffer;

    GetWindowTextW(m_hPidEdit, buffer, static_cast<int>(std::size(buffer)));
    options.pidText = buffer;

    switch (ComboBox_GetCurSel(m_hLevelCombo)) {
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
    case 0:
    case 1:
    default:
        options.minimumLevel = LogLevel::Verbose;
        break;
    }

    return options;
}

const LogEntry* MainWindow::GetVisibleEntry(int itemIndex) const {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_visibleIndexes.size())) {
        return nullptr;
    }
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

LRESULT MainWindow::HandleNotify(LPARAM lParam) {
    auto* hdr = reinterpret_cast<NMHDR*>(lParam);
    if (hdr->hwndFrom == m_hListView) {
        if (hdr->code == LVN_GETDISPINFOW) {
            auto* dispInfo = reinterpret_cast<NMLVDISPINFOW*>(lParam);
            const LogEntry* entry = GetVisibleEntry(dispInfo->item.iItem);
            if (entry == nullptr) {
                return 0;
            }

            static thread_local std::wstring textBuffer;
            if ((dispInfo->item.mask & LVIF_TEXT) != 0) {
                switch (dispInfo->item.iSubItem) {
                case 0:
                    textBuffer = entry->timestamp;
                    break;
                case 1:
                    textBuffer = LogParser::LevelToText(entry->level);
                    break;
                case 2:
                    textBuffer = std::to_wstring(entry->pid);
                    break;
                case 3:
                    textBuffer = entry->tag;
                    break;
                case 4:
                default:
                    textBuffer = entry->message;
                    break;
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
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
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
    if (message) {
        SetStatus(*message);
    }
    return 0;
}
