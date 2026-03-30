#pragma once

#include "AdbClient.h"
#include "Config.h"
#include "FilterEngine.h"
#include "darkui/combobox.h"
#include "darkui/edit.h"
#include "darkui/toolbar.h"

#include <windows.h>
#include <commctrl.h>

#include <mutex>
#include <unordered_map>
#include <vector>

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();

    bool Create(int x, int y, int width, int height, const wchar_t* title);
    HWND Window() const;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    void InitThemeResources();
    void CreateControls();
    void LayoutControls(int width, int height);
    void InitializeListView();
    void UpdateColumnWidths(int listWidth);
    void ApplyThemeToChildren();

    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnTimer(UINT_PTR timerId);
    void OnDestroy();
    void OnFilterChanged();
    void OnStart();
    void OnStop();
    void OnClear();
    void OnPauseResume();
    void OnExport();
    void RefreshDevices(bool keepSelection);
    void BeginConnectDevice(const std::wstring& address);
    void SetDeviceConnectInProgress(bool inProgress);
    void FlushPendingLogs();
    void RebuildVisibleIndexes(bool keepScrollPosition);
    void UpdateStatusText();
    void UpdateActionToolbarState();
    void SetStatus(const std::wstring& text);
    FilterOptions ReadFilterOptions() const;
    AdbLaunchOptions ReadAdbLaunchOptions() const;
    const LogEntry* GetVisibleEntry(int itemIndex) const;
    bool IsNearBottom() const;
    void ScrollToBottom();
    std::wstring GetWindowTextString(HWND hWnd) const;
    void LoadConfig();
    void SaveConfigIfNeeded();
    void PaintCustomChrome(HDC hdc);
    void PaintHeader(HWND hWnd, HDC hdc);
    void DrawButton(LPDRAWITEMSTRUCT drawInfo);
    void DrawControlBorder(HDC hdc, HWND control, bool focused);
    void DrawEditCueBanner(HWND control, HDC hdc);
    void DrawDropdownButton(LPDRAWITEMSTRUCT drawInfo);
    void DrawPopupListItem(LPDRAWITEMSTRUCT drawInfo);
    void CopySelectedLogs();
    void SaveKnownDevice(const std::wstring& address);
    void RemoveKnownDevice(const std::wstring& address);
    void LoadKnownDevices();
    void ShowPickerPopup(HWND picker);
    void HidePickerPopup();
    void ApplyPopupSelection();
    std::wstring GetSelectedDeviceText() const;
    std::wstring GetSelectedLevelText() const;

    LRESULT HandleNotify(LPARAM lParam);
    LRESULT HandleCustomDraw(NMLVCUSTOMDRAW* drawInfo);
    INT_PTR HandleControlColor(HDC hdc, HWND control);
    LRESULT HandleAppStatusMessage(LPARAM lParam);
    LRESULT HandleDeviceConnectResultMessage(LPARAM lParam);
    LRESULT HandleDrawItem(WPARAM wParam, LPARAM lParam);
    LRESULT HandleMeasureItem(LPARAM lParam);
    LRESULT HandleContextMenu(WPARAM wParam, LPARAM lParam);
    LRESULT HandleListKeyDown(MSG* msg);
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData);
    static LRESULT CALLBACK ListSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData);
    static LRESULT CALLBACK HeaderSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData);
    static LRESULT CALLBACK PopupListSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData);

    HINSTANCE m_instance;
    HWND m_hWnd;
    HWND m_hToolbarPanel;
    HWND m_hDeviceCombo;
    HWND m_hAdbFilterEdit;
    HWND m_hKeywordEdit;
    HWND m_hTagEdit;
    HWND m_hPidEdit;
    HWND m_hLevelCombo;
    HWND m_hListView;
    HWND m_hListHeader;
    HWND m_hStatusLabel;
    HWND m_hPopupHost;
    HWND m_hPopupList;
    HWND m_hActivePicker;

    HFONT m_uiFont;
    HFONT m_monoFont;
    HBRUSH m_bgBrush;
    HBRUSH m_surfaceBrush;
    HBRUSH m_surfaceAltBrush;
    HPEN m_borderPen;
    HPEN m_focusPen;
    darkui::Theme m_darkTheme;
    darkui::ComboBox m_deviceComboControl;
    darkui::ComboBox m_levelComboControl;
    darkui::Edit m_adbFilterEditControl;
    darkui::Edit m_keywordEditControl;
    darkui::Edit m_tagEditControl;
    darkui::Edit m_pidEditControl;
    darkui::Toolbar m_actionToolbar;

    AdbClient m_adbClient;
    Config m_config;
    mutable std::mutex m_pendingMutex;
    std::vector<LogEntry> m_pendingLogs;
    std::wstring m_statusText;
    LogBuffer m_logBuffer;
    FilterOptions m_filters;
    std::vector<std::size_t> m_visibleIndexes;
    std::vector<std::wstring> m_knownDevices;
    std::unordered_map<HWND, std::wstring> m_editCueTexts;
    std::vector<darkui::ComboItem> m_deviceComboItems;
    std::vector<darkui::ComboItem> m_levelComboItems;
    std::vector<std::wstring> m_deviceItems;
    std::vector<std::wstring> m_levelItems;
    int m_pauseToolbarIndex;
    int m_selectedDeviceIndex;
    int m_selectedLevelIndex;
    bool m_deviceConnectInProgress;
    bool m_paused;
};
