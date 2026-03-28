#include "AdbClient.h"

#include "LogParser.h"

#include <windows.h>

#include <string>
#include <vector>

namespace {
constexpr wchar_t kAdbCommand[] = L"adb logcat -v threadtime";

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return L"";
    }

    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}
}  // namespace

AdbClient::AdbClient() : m_running(false), m_processHandle(nullptr), m_threadHandle(nullptr), m_stdoutReadHandle(nullptr) {
}

AdbClient::~AdbClient() {
    Stop();
}

bool AdbClient::Start(const LogCallback& logCallback, const StatusCallback& statusCallback) {
    if (m_running.load()) {
        return true;
    }

    m_logCallback = logCallback;
    m_statusCallback = statusCallback;
    m_running = true;
    m_worker = std::thread(&AdbClient::WorkerLoop, this);
    return true;
}

void AdbClient::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    if (m_stdoutReadHandle != nullptr) {
        CancelIoEx(static_cast<HANDLE>(m_stdoutReadHandle), nullptr);
    }
    if (m_processHandle != nullptr) {
        TerminateProcess(static_cast<HANDLE>(m_processHandle), 0);
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

bool AdbClient::IsRunning() const {
    return m_running.load();
}

void AdbClient::EmitStatus(const std::wstring& text) const {
    if (m_statusCallback) {
        m_statusCallback(text);
    }
}

void AdbClient::WorkerLoop() {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        EmitStatus(L"Failed to create pipe.");
        m_running = false;
        return;
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::wstring commandLine = kAdbCommand;
    const BOOL started = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(writePipe);
    writePipe = nullptr;

    if (!started) {
        CloseHandle(readPipe);
        EmitStatus(L"Failed to launch adb. Ensure adb is in PATH.");
        m_running = false;
        return;
    }

    m_processHandle = pi.hProcess;
    m_threadHandle = pi.hThread;
    m_stdoutReadHandle = readPipe;

    EmitStatus(L"ADB logcat started.");

    std::string pending;
    std::vector<char> buffer(64 * 1024);
    std::vector<LogEntry> batch;
    batch.reserve(512);
    std::uint64_t seq = 1;

    while (m_running.load()) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            break;
        }

        pending.append(buffer.data(), buffer.data() + bytesRead);

        std::size_t lineStart = 0;
        while (true) {
            const std::size_t newlinePos = pending.find('\n', lineStart);
            if (newlinePos == std::string::npos) {
                pending.erase(0, lineStart);
                break;
            }

            std::string line = pending.substr(lineStart, newlinePos - lineStart);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const std::wstring wideLine = Utf8ToWide(line);
            if (!wideLine.empty()) {
                batch.push_back(LogParser::ParseThreadTimeLine(wideLine, seq++));
            }

            if (batch.size() >= 256 && m_logCallback) {
                m_logCallback(batch);
                batch.clear();
            }

            lineStart = newlinePos + 1;
        }
    }

    if (!pending.empty()) {
        const std::wstring wideLine = Utf8ToWide(pending);
        if (!wideLine.empty()) {
            batch.push_back(LogParser::ParseThreadTimeLine(wideLine, seq++));
        }
    }

    if (!batch.empty() && m_logCallback) {
        m_logCallback(batch);
    }

    if (m_threadHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_threadHandle));
        m_threadHandle = nullptr;
    }
    if (m_processHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_processHandle));
        m_processHandle = nullptr;
    }
    if (m_stdoutReadHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_stdoutReadHandle));
        m_stdoutReadHandle = nullptr;
    }

    EmitStatus(m_running.load() ? L"ADB logcat stopped." : L"ADB logcat terminated.");
    m_running = false;
}
