@echo off
setlocal enabledelayedexpansion

echo ========================================
echo CMake Build Script
echo ========================================

:: Check if CMake is installed
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake is not installed or not in PATH!
    echo.
    echo Please download and install CMake from:
    echo https://cmake.org/download/
    echo.
    echo Make sure to add CMake to your system PATH during installation.
    echo.
    pause
    exit /b 1
)

:: Display CMake version
echo CMake found:
cmake --version | findstr /r "cmake version"

:: Check if CMakeLists.txt exists
if not exist "CMakeLists.txt" (
    echo.
    echo [ERROR] CMakeLists.txt not found in current directory!
    echo Please make sure you're running this script from the project root.
    echo.
    pause
    exit /b 1
)

echo.
echo Starting CMake configuration and build...
echo.

:: Create build directory and configure
echo Generating project...
cmake -B build
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    echo Check the error messages above for details.
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Project Generated
echo ========================================
echo.
echo Project files are located in the 'build' directory.
echo.

pause