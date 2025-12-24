@echo off
cd /d "%~dp0"

where node >nul 2>&1
if %errorlevel% equ 0 (
    node generate-uplugin.js
    goto :done
)

where python >nul 2>&1
if %errorlevel% equ 0 (
    python generate-uplugin.py
    goto :done
)

where python3 >nul 2>&1
if %errorlevel% equ 0 (
    python3 generate-uplugin.py
    goto :done
)

echo.
echo ERROR: Neither Node.js nor Python found.
echo Please install one of:
echo   - Node.js: https://nodejs.org
echo   - Python:  https://python.org
echo.
pause
exit /b 1

:done
if %errorlevel% neq 0 (
    echo.
    echo Generation failed!
    pause
    exit /b %errorlevel%
)
echo.
echo Done!
pause