@echo off
echo ====================================================
echo PSX Engine - Editor Build Script
echo ====================================================
echo.

REM Check if CMake is installed
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in your PATH.
    echo Please install CMake from https://cmake.org/download/ and try again.
    echo.
    pause
    exit /b 1
)

echo [1/3] Configuring CMake for the Editor...
cmake -S . -B build/editor -DPSX_BUILD_TARGET=EDITOR
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)
echo.

echo [2/3] Building the PSX Editor executable...
cmake --build build/editor
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)
echo.

echo [3/3] Launching PSX Editor...
cd build\editor\psx-editor
if exist Debug\psx_editor.exe (
    start Debug\psx_editor.exe
) else if exist psx_editor.exe (
    start psx_editor.exe
) else (
    echo [ERROR] psx_editor.exe was not found.
    pause
)
