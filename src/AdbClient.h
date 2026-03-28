#pragma once

#include "LogTypes.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class AdbClient {
public:
    using LogCallback = std::function<void(std::vector<LogEntry>)>;
    using StatusCallback = std::function<void(const std::wstring&)>;

    AdbClient();
    ~AdbClient();

    bool Start(const LogCallback& logCallback, const StatusCallback& statusCallback);
    void Stop();
    bool IsRunning() const;

private:
    void WorkerLoop();
    void EmitStatus(const std::wstring& text) const;

    std::atomic<bool> m_running;
    std::thread m_worker;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;
    void* m_processHandle;
    void* m_threadHandle;
    void* m_stdoutReadHandle;
};
