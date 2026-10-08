@echo off
echo ==========================================
echo Chippy Firmware Flasher (Sleep Test)
echo ==========================================
cd /d "%~dp0"

echo [1/2] Flashing Right Side (Peripheral)...
powershell -ExecutionPolicy Bypass -File "%~dp0flash_with_trigger.ps1" -Side right

echo.
echo Waiting 3 seconds...
timeout /t 3 /nobreak >nul

echo.
echo [2/2] Flashing Left Side (Central)...
powershell -ExecutionPolicy Bypass -File "%~dp0flash_with_trigger.ps1" -Side left

echo.
echo ==========================================
echo Done! Both sides updated.
echo ==========================================
pause
