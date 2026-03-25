@echo off
setlocal EnableDelayedExpansion

REM Get the directory where the script is located
set "SCRIPT_DIR=%~dp0"

REM Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

REM Switch to the script's directory (even after elevation)
cd /d "%SCRIPT_DIR%"

REM Check if Python is installed
where py >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo ========================================
    echo   Python is not installed. Please install Python first. Python 3.11 is recommended.
    echo ========================================
    pause
    exit /b
)

REM Update pip and install required packages
echo Installing required packages...
py -m pip install --upgrade pip
py -m pip install -r "%SCRIPT_DIR%requirements.txt"

REM Add firewall rule
echo Adding firewall rule to allow incoming UDP connections on port 8000...
netsh advfirewall firewall add rule name="Allow UDP 8000" dir=in action=allow protocol=UDP localport=8000

echo.
echo ========================================
echo     Installation and configuration complete.
echo ========================================
pause
