# Android ADB Log Viewer

A Win10 desktop ADB log viewer built with C++17, MinGW-w64, and Win32 API.

## Current features

- Dark Win32 UI with reduced white-flash risk on startup
- Real-time `adb logcat -v threadtime` streaming
- Color rendering by Android log level
- Keyword, Tag, PID, and minimum-level filtering
- Owner-data `ListView` for large log volumes
- Batched UI updates for higher throughput

## Build

Run:

```bat
build.bat
```

Output:

```text
build\AndroidLogViewer.exe
```

## Notes

- Chinese UI strings are stored in resource files under `res\`.
- Source files and resource files should be saved as UTF-8.
- `adb.exe` must be available in `PATH`.
