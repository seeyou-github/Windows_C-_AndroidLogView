@echo off
setlocal

echo ===== Build AndroidLogViewer =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Isrc -Ires
set LDFLAGS=-municode -mwindows -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\MainWindow.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error

%CXX% -c src\AdbClient.cpp %CXXFLAGS% -o build\AdbClient.o
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

%RC% res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\MainWindow.o build\AdbClient.o build\DarkMode.o build\LogParser.o build\LogBuffer.o build\FilterEngine.o build\ResourceStrings.o build\resource.o -o build\AndroidLogViewer.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\AndroidLogViewer.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
