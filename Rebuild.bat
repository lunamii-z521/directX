@echo off
chcp 65001 > nul

echo ========================================
echo [1/3] Removing build folder...
echo ========================================
cmake -E rm -rf build

echo ========================================
echo [2/3] Generating CMake Project...
echo ========================================
cmake -B build
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMake Generation Failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo [3/3] Building Project (Debug)...
echo ========================================
cmake --build build --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build Failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo Build Succeeded!
echo ========================================
pause