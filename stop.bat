@echo off
setlocal
title Lab Operations Engine - Stopper

:: Ensure working directory is the script's root folder
cd /d "%~dp0"

echo ====================================================================
echo   Shutting Down Lab Operations Engine Server...
echo ====================================================================

tasklist /FI "IMAGENAME eq lab_engine.exe" 2>NUL | find /I "lab_engine.exe" >NUL
if "%ERRORLEVEL%"=="0" (
    echo [INFO] Terminating lab_engine.exe process...
    taskkill /F /IM lab_engine.exe >nul 2>&1
    echo [SUCCESS] Lab Operations Engine server has been shut down successfully.
) else (
    echo [INFO] No running instance of lab_engine.exe was found.
)

echo ====================================================================
ping 127.0.0.1 -n 2 >nul
