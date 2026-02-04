@echo off
REM PCGExtendedToolkit Automated Test Runner
REM =========================================
REM
REM This script runs PCGEx automation tests via Unreal Editor command line.
REM
REM Usage:
REM   RunTests.bat                    - Run all PCGEx tests
REM   RunTests.bat Unit               - Run only unit tests
REM   RunTests.bat Integration        - Run only integration tests
REM   RunTests.bat Functional         - Run only functional tests
REM   RunTests.bat Math               - Run specific test category
REM
REM Prerequisites:
REM   - Set UNREAL_ENGINE_PATH to your Unreal Engine installation
REM   - Set PROJECT_PATH to your project .uproject file
REM
REM Environment Variables (set before running):
REM   UNREAL_ENGINE_PATH - Path to UnrealEngine (e.g., C:\UE5.7)
REM   PROJECT_PATH       - Path to .uproject file

setlocal enabledelayedexpansion

REM =========================================
REM Configuration
REM =========================================

REM Default paths - override via environment variables
if not defined UNREAL_ENGINE_PATH (
    REM Try common locations
    if exist "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" (
        set UNREAL_ENGINE_PATH=C:\Program Files\Epic Games\UE_5.7
    ) else if exist "D:\UE5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" (
        set UNREAL_ENGINE_PATH=D:\UE5.7
    ) else (
        echo ERROR: UNREAL_ENGINE_PATH not set and could not find Unreal Engine
        echo Please set UNREAL_ENGINE_PATH environment variable
        exit /b 1
    )
)

set EDITOR_CMD=%UNREAL_ENGINE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe

if not exist "%EDITOR_CMD%" (
    echo ERROR: UnrealEditor-Cmd.exe not found at:
    echo %EDITOR_CMD%
    exit /b 1
)

REM Find project file
if not defined PROJECT_PATH (
    REM Look for .uproject in parent directories
    pushd "%~dp0"
    cd ..\..\..\..\..\..
    for %%f in (*.uproject) do set PROJECT_PATH=%%~dpnxf
    popd
)

if not defined PROJECT_PATH (
    echo ERROR: PROJECT_PATH not set and could not find .uproject file
    echo Please set PROJECT_PATH environment variable
    exit /b 1
)

if not exist "%PROJECT_PATH%" (
    echo ERROR: Project file not found: %PROJECT_PATH%
    exit /b 1
)

REM =========================================
REM Parse Arguments
REM =========================================

set TEST_FILTER=PCGEx
if not "%~1"=="" (
    if /i "%~1"=="Unit" set TEST_FILTER=PCGEx.Unit
    if /i "%~1"=="Integration" set TEST_FILTER=PCGEx.Integration
    if /i "%~1"=="Functional" set TEST_FILTER=PCGEx.Functional
    if /i "%~1"=="Math" set TEST_FILTER=PCGEx.Unit.Math
    if /i "%~1"=="OBB" set TEST_FILTER=PCGEx.Unit.OBB
    if /i "%~1"=="Containers" set TEST_FILTER=PCGEx.Unit.Containers
    if /i "%~1"=="Filters" set TEST_FILTER=PCGEx.Integration.Filters
    if /i "%~1"=="Elements" set TEST_FILTER=PCGEx.Integration.Elements
    if /i "%~1"=="Threading" set TEST_FILTER=PCGEx.Functional.Threading
    if /i "%~1"=="Graph" set TEST_FILTER=PCGEx.Functional.Graph

    REM Allow custom filter
    if "%TEST_FILTER%"=="PCGEx" set TEST_FILTER=%~1
)

REM =========================================
REM Setup Output Directory
REM =========================================

set TIMESTAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%
set OUTPUT_DIR=%~dp0TestResults\%TIMESTAMP%

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM =========================================
REM Run Tests
REM =========================================

echo ==========================================
echo PCGExtendedToolkit Test Runner
echo ==========================================
echo.
echo Project:     %PROJECT_PATH%
echo Engine:      %EDITOR_CMD%
echo Test Filter: %TEST_FILTER%
echo Output:      %OUTPUT_DIR%
echo.
echo Starting tests...
echo.

REM Build the command
set CMD="%EDITOR_CMD%" "%PROJECT_PATH%"
set CMD=%CMD% -ExecCmds="Automation RunTests %TEST_FILTER%;Quit"
set CMD=%CMD% -NullRHI
set CMD=%CMD% -NoSound
set CMD=%CMD% -Unattended
set CMD=%CMD% -NoSplash
set CMD=%CMD% -NoScreenMessages
set CMD=%CMD% -ReportOutputPath="%OUTPUT_DIR%"
set CMD=%CMD% -Log="%OUTPUT_DIR%\TestLog.txt"

echo Running: %CMD%
echo.

%CMD%

set EXIT_CODE=%ERRORLEVEL%

REM =========================================
REM Report Results
REM =========================================

echo.
echo ==========================================
echo Test Run Complete
echo ==========================================
echo.
echo Exit Code: %EXIT_CODE%
echo Results:   %OUTPUT_DIR%
echo.

if %EXIT_CODE% EQU 0 (
    echo Status: PASSED
) else (
    echo Status: FAILED or ERRORS
)

REM Check for result files
if exist "%OUTPUT_DIR%\index.json" (
    echo.
    echo Test report generated: %OUTPUT_DIR%\index.json
)

if exist "%OUTPUT_DIR%\TestLog.txt" (
    echo Log file: %OUTPUT_DIR%\TestLog.txt
)

exit /b %EXIT_CODE%
