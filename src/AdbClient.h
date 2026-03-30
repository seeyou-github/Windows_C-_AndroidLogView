#pragma once

#include "LogTypes.h"

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class AdbClient {
public:
    using LogCallback = std::function<void(std::vector<LogEntry>)>;
    using StatusCallback = std::function<void(const std::wstring&)>;

    struct DeviceInfo {
        std::wstring serial;
        std::wstring state;
    };

    AdbClient();
    ~AdbClient();

    bool Start(const AdbLaunchOptions& options, const LogCallback& logCallback, const StatusCallback& statusCallback);
    void Stop();
    bool IsRunning() const;
    static std::vector<DeviceInfo> ListDevices();
    static bool ConnectNetworkDevice(const std::wstring& address, std::wstring& statusText, bool* timedOut = nullptr);
    static bool RunAdbCommand(const std::wstring& arguments, std::wstring& statusText, DWORD timeoutMs = 5000);

private:
    static bool RunCommandCapture(const std::wstring& commandLine, std::string& output, DWORD* exitCode = nullptr, DWORD timeoutMs = INFINITE,
                                  bool* timedOut = nullptr);
    void WorkerLoop();
    void EmitStatus(const std::wstring& text) const;

    std::atomic<bool> m_running;
    std::thread m_worker;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;
    AdbLaunchOptions m_launchOptions;
    void* m_processHandle;
    void* m_threadHandle;
    void* m_stdoutReadHandle;
};
