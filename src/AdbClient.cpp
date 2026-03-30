#include "AdbClient.h"

#include "LogParser.h"

#include <windows.h>

#include <sstream>
#include <string>
#include <vector>

namespace {
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

std::wstring Trim(const std::wstring& text) {
    std::size_t start = 0;
    while (start < text.size() && iswspace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && iswspace(text[end - 1])) {
        --end;
    }
    return text.substr(start, end - start);
}

std::wstring ToLower(const std::wstring& text) {
    std::wstring lowered = text;
    for (auto& ch : lowered) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return lowered;
}

std::wstring BuildLogcatCommand(const AdbLaunchOptions& options) {
    std::wstring command = L"adb ";
    if (!options.deviceSerial.empty()) {
        command += L"-s \"" + options.deviceSerial + L"\" ";
    }
    command += L"logcat -b " + (options.logBuffer.empty() ? std::wstring(L"main") : options.logBuffer) + L" -v threadtime";
    if (!options.adbPriorityFilter.empty()) {
        command += L" " + options.adbPriorityFilter;
    }
    return command;
}
}  // namespace

AdbClient::AdbClient() : m_running(false), m_processHandle(nullptr), m_threadHandle(nullptr), m_stdoutReadHandle(nullptr) {
}

AdbClient::~AdbClient() {
    Stop();
}

bool AdbClient::Start(const AdbLaunchOptions& options, const LogCallback& logCallback, const StatusCallback& statusCallback) {
    if (m_running.load()) {
        return true;
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_launchOptions = options;
    m_logCallback = logCallback;
    m_statusCallback = statusCallback;
    m_running = true;
    m_worker = std::thread(&AdbClient::WorkerLoop, this);
    return true;
}

void AdbClient::Stop() {
    const bool wasRunning = m_running.exchange(false);
    if (!wasRunning && !m_worker.joinable()) {
        return;
    }

    if (wasRunning && m_stdoutReadHandle != nullptr) {
        CancelIoEx(static_cast<HANDLE>(m_stdoutReadHandle), nullptr);
    }
    if (wasRunning && m_processHandle != nullptr) {
        TerminateProcess(static_cast<HANDLE>(m_processHandle), 0);
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

bool AdbClient::IsRunning() const {
    return m_running.load();
}

std::vector<AdbClient::DeviceInfo> AdbClient::ListDevices() {
    std::string output;
    std::vector<DeviceInfo> devices;
    if (!RunCommandCapture(L"adb devices", output, nullptr)) {
        return devices;
    }

    std::wstringstream stream(Utf8ToWide(output));
    std::wstring line;
    bool firstLine = true;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        if (firstLine) {
            firstLine = false;
            continue;
        }

        const std::size_t tabPos = line.find(L'\t');
        if (tabPos == std::wstring::npos) {
            continue;
        }

        DeviceInfo info;
        info.serial = Trim(line.substr(0, tabPos));
        info.state = Trim(line.substr(tabPos + 1));
        if (!info.serial.empty()) {
            devices.push_back(info);
        }
    }
    return devices;
}

bool AdbClient::ConnectNetworkDevice(const std::wstring& address, std::wstring& statusText, bool* timedOut) {
    std::string output;
    DWORD exitCode = 0;
    bool didTimeOut = false;
    const bool ok = RunCommandCapture(L"adb connect " + address, output, &exitCode, 5000, &didTimeOut);
    statusText = Trim(Utf8ToWide(output));
    if (timedOut != nullptr) {
        *timedOut = didTimeOut;
    }
    if (didTimeOut) {
        statusText = L"连接超时：5 秒内未收到 adb connect 响应。";
        return false;
    }
    if (statusText.empty()) {
        statusText = ok ? L"adb connect succeeded." : L"adb connect failed.";
    }

    const std::wstring lowered = ToLower(statusText);
    const bool outputLooksSuccessful =
        lowered.find(L"connected to") != std::wstring::npos || lowered.find(L"already connected to") != std::wstring::npos;
    const bool outputLooksFailed = lowered.find(L"cannot") != std::wstring::npos || lowered.find(L"unable") != std::wstring::npos ||
                                   lowered.find(L"failed") != std::wstring::npos || lowered.find(L"error") != std::wstring::npos ||
                                   lowered.find(L"unknown host") != std::wstring::npos ||
                                   lowered.find(L"no such host") != std::wstring::npos;

    return ok && exitCode == 0 && outputLooksSuccessful && !outputLooksFailed;
}

bool AdbClient::RunAdbCommand(const std::wstring& arguments, std::wstring& statusText, DWORD timeoutMs) {
    std::string output;
    DWORD exitCode = 0;
    bool timedOut = false;
    const bool ok = RunCommandCapture(L"adb " + arguments, output, &exitCode, timeoutMs, &timedOut);
    statusText = Trim(Utf8ToWide(output));
    if (timedOut) {
        statusText = L"执行超时：" + arguments;
        return false;
    }
    if (statusText.empty()) {
        statusText = ok && exitCode == 0 ? (L"执行成功：adb " + arguments) : (L"执行失败：adb " + arguments);
    }
    return ok && exitCode == 0;
}

bool AdbClient::RunCommandCapture(const std::wstring& commandLine, std::string& output, DWORD* exitCode, DWORD timeoutMs, bool* timedOut) {
    if (timedOut != nullptr) {
        *timedOut = false;
    }

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        return false;
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
    std::wstring mutableCommand = commandLine;
    const BOOL started = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!started) {
        CloseHandle(readPipe);
        return false;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        if (timedOut != nullptr) {
            *timedOut = true;
        }
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
    }

    std::vector<char> buffer(4096);
    while (true) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            break;
        }
        output.append(buffer.data(), buffer.data() + bytesRead);
    }

    DWORD processExitCode = 0;
    GetExitCodeProcess(pi.hProcess, &processExitCode);
    if (exitCode != nullptr) {
        *exitCode = processExitCode;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);
    return processExitCode == 0;
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
    std::wstring commandLine = BuildLogcatCommand(m_launchOptions);
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

            if (batch.size() >= 64 && m_logCallback) {
                m_logCallback(batch);
                batch.clear();
            }

            lineStart = newlinePos + 1;
        }

        if (!batch.empty() && m_logCallback) {
            m_logCallback(batch);
            batch.clear();
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
