@echo off
setlocal

echo ===== Build AndroidLogViewer =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Isrc -Ires -IWindows_C++_lib_darkui\include
set LDFLAGS=-municode -mwindows -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\MainWindowImpl.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error

%CXX% -c src\AdbClient.cpp %CXXFLAGS% -o build\AdbClient.o
if errorlevel 1 goto error

%CXX% -c src\AddDeviceDialog.cpp %CXXFLAGS% -o build\AddDeviceDialog.o
if errorlevel 1 goto error

%CXX% -c src\SettingsDialog.cpp %CXXFLAGS% -o build\SettingsDialog.o
if errorlevel 1 goto error

%CXX% -c src\FilterRulesDialog.cpp %CXXFLAGS% -o build\FilterRulesDialog.o
if errorlevel 1 goto error

%CXX% -c src\DarkMode.cpp %CXXFLAGS% -o build\DarkMode.o
if errorlevel 1 goto error

%CXX% -c src\LogParser.cpp %CXXFLAGS% -o build\LogParser.o
if errorlevel 1 goto error

%CXX% -c src\LogBuffer.cpp %CXXFLAGS% -o build\LogBuffer.o
if errorlevel 1 goto error

%CXX% -c src\FilterEngine.cpp %CXXFLAGS% -o build\FilterEngine.o
if errorlevel 1 goto error

%CXX% -c src\ResourceStrings.cpp %CXXFLAGS% -o build\ResourceStrings.o
if errorlevel 1 goto error

%CXX% -c src\Config.cpp %CXXFLAGS% -o build\Config.o
if errorlevel 1 goto error

%CXX% -c Windows_C++_lib_darkui\src\button.cpp %CXXFLAGS% -o build\darkui_button.o
if errorlevel 1 goto error

%CXX% -c Windows_C++_lib_darkui\src\combobox.cpp %CXXFLAGS% -o build\darkui_combobox.o
if errorlevel 1 goto error

%CXX% -c Windows_C++_lib_darkui\src\edit.cpp %CXXFLAGS% -o build\darkui_edit.o
if errorlevel 1 goto error

%CXX% -c Windows_C++_lib_darkui\src\scrollbar.cpp %CXXFLAGS% -o build\darkui_scrollbar.o
if errorlevel 1 goto error

%CXX% -c Windows_C++_lib_darkui\src\toolbar.cpp %CXXFLAGS% -o build\darkui_toolbar.o
if errorlevel 1 goto error

%RC% res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\MainWindow.o build\AdbClient.o build\AddDeviceDialog.o build\SettingsDialog.o build\FilterRulesDialog.o build\DarkMode.o build\LogParser.o build\LogBuffer.o build\FilterEngine.o build\ResourceStrings.o build\Config.o build\darkui_button.o build\darkui_combobox.o build\darkui_edit.o build\darkui_scrollbar.o build\darkui_toolbar.o build\resource.o -o build\AndroidLogViewer.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\AndroidLogViewer.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
