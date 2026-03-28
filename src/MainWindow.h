#pragma once

#include "AdbClient.h"
#include "Config.h"
#include "FilterEngine.h"

#include <windows.h>
#include <commctrl.h>

#include <mutex>
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
    void FlushPendingLogs();
    void RebuildVisibleIndexes(bool keepScrollPosition);
    void UpdateStatusText();
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
    void DrawButton(LPDRAWITEMSTRUCT drawInfo);
    void DrawControlBorder(HDC hdc, HWND control, bool focused);

    LRESULT HandleNotify(LPARAM lParam);
    LRESULT HandleCustomDraw(NMLVCUSTOMDRAW* drawInfo);
    INT_PTR HandleControlColor(HDC hdc, HWND control);
    LRESULT HandleAppStatusMessage(LPARAM lParam);
    LRESULT HandleDrawItem(WPARAM wParam, LPARAM lParam);

    HINSTANCE m_instance;
    HWND m_hWnd;
    HWND m_hToolbarPanel;
    HWND m_hDeviceCombo;
    HWND m_hAdbFilterEdit;
    HWND m_hKeywordEdit;
    HWND m_hTagEdit;
    HWND m_hPidEdit;
    HWND m_hLevelCombo;
    HWND m_hStartButton;
    HWND m_hStopButton;
    HWND m_hPauseButton;
    HWND m_hExportButton;
    HWND m_hClearButton;
    HWND m_hListView;
    HWND m_hStatusLabel;

    HFONT m_uiFont;
    HFONT m_monoFont;
    HBRUSH m_bgBrush;
    HBRUSH m_surfaceBrush;
    HBRUSH m_surfaceAltBrush;
    HPEN m_borderPen;
    HPEN m_focusPen;

    AdbClient m_adbClient;
    Config m_config;
    mutable std::mutex m_pendingMutex;
    std::vector<LogEntry> m_pendingLogs;
    std::wstring m_statusText;
    LogBuffer m_logBuffer;
    FilterOptions m_filters;
    std::vector<std::size_t> m_visibleIndexes;
    bool m_paused;
};
