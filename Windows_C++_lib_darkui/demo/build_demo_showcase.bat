@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "DEMO_SRC_DIR=%SCRIPT_DIR%src"
set "LIB_SRC_DIR=%ROOT_DIR%\src"
set "INCLUDE_DIR=%ROOT_DIR%\include"
set "BUILD_DIR=%SCRIPT_DIR%build"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CXX=g++"
set "CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN -I"%INCLUDE_DIR%""
set "LDFLAGS=-municode -mwindows -lcomctl32 -ldwmapi -luxtheme -lgdi32"

echo Building lib_darkui showcase demo...
%CXX% "%DEMO_SRC_DIR%\demo_showcase.cpp" "%LIB_SRC_DIR%\tab.cpp" "%LIB_SRC_DIR%\scrollbar.cpp" "%LIB_SRC_DIR%\progress.cpp" "%LIB_SRC_DIR%\button.cpp" "%LIB_SRC_DIR%\table.cpp" "%LIB_SRC_DIR%\slider.cpp" "%LIB_SRC_DIR%\combobox.cpp" %CXXFLAGS% -o "%BUILD_DIR%\darkui_showcase_demo.exe" %LDFLAGS%
if errorlevel 1 goto error

echo Build succeeded: "%BUILD_DIR%\darkui_showcase_demo.exe"
goto end

:error
echo Build failed
exit /b 1

:end
endlocal
