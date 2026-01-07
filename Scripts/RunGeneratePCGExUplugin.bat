@echo off
cd /d "%~dp0"

:: Check if scripts are in current directory (running from Scripts folder)
if exist "generate-uplugin.js" (
    set "SCRIPT_DIR=%~dp0"
    goto :find_runtime
)
if exist "generate-uplugin.py" (
    set "SCRIPT_DIR=%~dp0"
    goto :find_runtime
)

:: Check if scripts are in plugin subfolder (running from project root)
set "SCRIPT_DIR=%~dp0Plugins\PCGExtendedToolkit\Scripts\"
if exist "%SCRIPT_DIR%generate-uplugin.js" goto :find_runtime
if exist "%SCRIPT_DIR%generate-uplugin.py" goto :find_runtime

echo ERROR: Could not find generate-uplugin scripts.
echo Run this from either:
echo   - Project root (containing Plugins/PCGExtendedToolkit/)
echo   - Plugins/PCGExtendedToolkit/Scripts/
goto :end

:find_runtime
:: Try Node.js first
where node >nul 2>&1
if %errorlevel% equ 0 goto :run_node

:: Try Python
where python >nul 2>&1
if %errorlevel% equ 0 goto :run_python

:: Try Python3
where python3 >nul 2>&1
if %errorlevel% equ 0 goto :run_python3

echo.
echo ERROR: Neither Node.js nor Python found.
echo Please install one of:
echo   - Node.js: https://nodejs.org
echo   - Python:  https://python.org
goto :end

:run_node
node "%SCRIPT_DIR%generate-uplugin.js"
goto :finish

:run_python
python "%SCRIPT_DIR%generate-uplugin.py"
goto :finish

:run_python3
python3 "%SCRIPT_DIR%generate-uplugin.py"
goto :finish

:finish
if %errorlevel% neq 0 (
    echo.
    echo Generation failed with error code %errorlevel%
) else (
    echo.
    echo Done!
)

:end
echo.
pause