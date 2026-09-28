@echo off
setlocal
title Lab Operations Engine - Launcher

:: Ensure working directory is the script's root folder
cd /d "%~dp0"

echo ====================================================================
echo   Lab Operations, Emergency Fault Triage ^& Calibration Engine
echo   Academic DSA Project - VIT Pune (Semester 3, Group 8)
echo ====================================================================

:: Check if executable exists; build it if missing
if not exist "lab_engine.exe" (
    echo [INFO] lab_engine.exe not found. Building project with mingw32-make...
    mingw32-make
    if errorlevel 1 (
        echo [ERROR] Build failed! Please verify MinGW toolchain is installed.
        pause
        exit /b 1
    )
)

:: Check if an instance is already active
tasklist /FI "IMAGENAME eq lab_engine.exe" 2>NUL | find /I "lab_engine.exe" >NUL
if "%ERRORLEVEL%"=="0" (
    echo [INFO] Lab Engine server is already active!
) else (
    echo [INFO] Starting Lab Engine server on http://localhost:8080 ...
    start "Lab Engine Server (Port 8080)" lab_engine.exe --server 8080
    ping 127.0.0.1 -n 3 >nul
)

echo [INFO] Opening Web UI in your default browser...
start http://localhost:8080/

echo.
echo ====================================================================
echo   Server is running on port 8080.
echo   To stop the server, simply run stop.bat
echo ====================================================================
