#pragma once

#include "AdbClient.h"
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
    void FlushPendingLogs();
    void RebuildVisibleIndexes(bool keepScrollPosition);
    void UpdateStatusText();
    void SetStatus(const std::wstring& text);
    FilterOptions ReadFilterOptions() const;
    const LogEntry* GetVisibleEntry(int itemIndex) const;
    bool IsNearBottom() const;
    void ScrollToBottom();

    LRESULT HandleNotify(LPARAM lParam);
    LRESULT HandleCustomDraw(NMLVCUSTOMDRAW* drawInfo);
    INT_PTR HandleControlColor(HDC hdc, HWND control);
    LRESULT HandleAppStatusMessage(LPARAM lParam);

    HINSTANCE m_instance;
    HWND m_hWnd;
    HWND m_hToolbarPanel;
    HWND m_hKeywordEdit;
    HWND m_hTagEdit;
    HWND m_hPidEdit;
    HWND m_hLevelCombo;
    HWND m_hStartButton;
    HWND m_hStopButton;
    HWND m_hClearButton;
    HWND m_hListView;
    HWND m_hStatusLabel;

    HFONT m_uiFont;
    HFONT m_monoFont;
    HBRUSH m_bgBrush;
    HBRUSH m_surfaceBrush;
    HBRUSH m_surfaceAltBrush;

    AdbClient m_adbClient;
    mutable std::mutex m_pendingMutex;
    std::vector<LogEntry> m_pendingLogs;
    std::wstring m_statusText;
    LogBuffer m_logBuffer;
    FilterOptions m_filters;
    std::vector<std::size_t> m_visibleIndexes;
};
