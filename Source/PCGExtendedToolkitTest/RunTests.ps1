<#
.SYNOPSIS
    PCGExtendedToolkit Automated Test Runner (PowerShell)

.DESCRIPTION
    Runs PCGEx automation tests via Unreal Editor command line.
    Provides better error handling and CI/CD integration than the batch script.

.PARAMETER TestFilter
    Test filter pattern. Defaults to "PCGEx" (all tests).
    Common values: Unit, Integration, Functional, Math, OBB, Threading

.PARAMETER EnginePath
    Path to Unreal Engine installation. If not specified, uses
    UNREAL_ENGINE_PATH environment variable or searches common locations.

.PARAMETER ProjectPath
    Path to .uproject file. If not specified, searches parent directories.

.PARAMETER OutputPath
    Directory for test results. Defaults to TestResults subdirectory.

.PARAMETER Timeout
    Timeout in seconds for test execution. Default: 600 (10 minutes)

.PARAMETER CI
    CI mode - exits with appropriate error code and minimal output

.EXAMPLE
    .\RunTests.ps1
    # Run all PCGEx tests

.EXAMPLE
    .\RunTests.ps1 -TestFilter "PCGEx.Unit"
    # Run only unit tests

.EXAMPLE
    .\RunTests.ps1 -TestFilter "PCGEx.Unit.Math" -CI
    # Run math tests in CI mode

.EXAMPLE
    .\RunTests.ps1 -EnginePath "C:\UE5.7" -ProjectPath "C:\MyProject\MyProject.uproject"
    # Specify paths explicitly
#>

param(
    [string]$TestFilter = "PCGEx",
    [string]$EnginePath = $env:UNREAL_ENGINE_PATH,
    [string]$ProjectPath = $env:PROJECT_PATH,
    [string]$OutputPath = "",
    [int]$Timeout = 600,
    [switch]$CI
)

# Strict mode
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Helper functions
function Write-Header($message) {
    if (-not $CI) {
        Write-Host ""
        Write-Host "=" * 50 -ForegroundColor Cyan
        Write-Host $message -ForegroundColor Cyan
        Write-Host "=" * 50 -ForegroundColor Cyan
    }
}

function Write-Step($message) {
    if (-not $CI) {
        Write-Host "[*] $message" -ForegroundColor Yellow
    }
}

function Write-Success($message) {
    Write-Host "[+] $message" -ForegroundColor Green
}

function Write-Error($message) {
    Write-Host "[-] $message" -ForegroundColor Red
}

# Find Unreal Engine
function Find-UnrealEngine {
    param([string]$Hint)

    $searchPaths = @(
        $Hint,
        "C:\Program Files\Epic Games\UE_5.7",
        "D:\UE5.7",
        "C:\UE_5.7",
        "D:\Program Files\Epic Games\UE_5.7"
    )

    foreach ($path in $searchPaths) {
        if ([string]::IsNullOrEmpty($path)) { continue }

        $editorCmd = Join-Path $path "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
        if (Test-Path $editorCmd) {
            return $path
        }
    }

    return $null
}

# Find project file
function Find-ProjectFile {
    param([string]$StartPath)

    $currentPath = $StartPath
    for ($i = 0; $i -lt 10; $i++) {
        $uprojectFiles = Get-ChildItem -Path $currentPath -Filter "*.uproject" -ErrorAction SilentlyContinue
        if ($uprojectFiles) {
            return $uprojectFiles[0].FullName
        }
        $parentPath = Split-Path $currentPath -Parent
        if ([string]::IsNullOrEmpty($parentPath) -or $parentPath -eq $currentPath) {
            break
        }
        $currentPath = $parentPath
    }
    return $null
}

# Map friendly names to test filters
function Get-TestFilter {
    param([string]$Input)

    $filterMap = @{
        "Unit"        = "PCGEx.Unit"
        "Integration" = "PCGEx.Integration"
        "Functional"  = "PCGEx.Functional"
        "Math"        = "PCGEx.Unit.Math"
        "OBB"         = "PCGEx.Unit.OBB"
        "Containers"  = "PCGEx.Unit.Containers"
        "Filters"     = "PCGEx.Integration.Filters"
        "Elements"    = "PCGEx.Integration.Elements"
        "Threading"   = "PCGEx.Functional.Threading"
        "Graph"       = "PCGEx.Functional.Graph"
    }

    if ($filterMap.ContainsKey($Input)) {
        return $filterMap[$Input]
    }
    return $Input
}

# Main execution
try {
    Write-Header "PCGExtendedToolkit Test Runner"

    # Resolve test filter
    $resolvedFilter = Get-TestFilter -Input $TestFilter
    Write-Step "Test Filter: $resolvedFilter"

    # Find Unreal Engine
    $EnginePath = Find-UnrealEngine -Hint $EnginePath
    if ([string]::IsNullOrEmpty($EnginePath)) {
        throw "Could not find Unreal Engine. Set UNREAL_ENGINE_PATH environment variable."
    }

    $editorCmd = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    if (-not (Test-Path $editorCmd)) {
        throw "UnrealEditor-Cmd.exe not found at: $editorCmd"
    }
    Write-Step "Engine: $editorCmd"

    # Find project file
    if ([string]::IsNullOrEmpty($ProjectPath)) {
        $scriptDir = $PSScriptRoot
        $ProjectPath = Find-ProjectFile -StartPath $scriptDir
    }

    if ([string]::IsNullOrEmpty($ProjectPath) -or -not (Test-Path $ProjectPath)) {
        throw "Could not find .uproject file. Set PROJECT_PATH environment variable."
    }
    Write-Step "Project: $ProjectPath"

    # Setup output directory
    if ([string]::IsNullOrEmpty($OutputPath)) {
        $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $OutputPath = Join-Path $PSScriptRoot "TestResults\$timestamp"
    }

    if (-not (Test-Path $OutputPath)) {
        New-Item -Path $OutputPath -ItemType Directory -Force | Out-Null
    }
    Write-Step "Output: $OutputPath"

    # Build arguments
    $logFile = Join-Path $OutputPath "TestLog.txt"
    $arguments = @(
        "`"$ProjectPath`"",
        "-ExecCmds=`"Automation RunTests $resolvedFilter;Quit`"",
        "-NullRHI",
        "-NoSound",
        "-Unattended",
        "-NoSplash",
        "-NoScreenMessages",
        "-ReportOutputPath=`"$OutputPath`"",
        "-Log=`"$logFile`""
    )

    Write-Header "Running Tests"

    if (-not $CI) {
        Write-Host "Command: $editorCmd" -ForegroundColor Gray
        Write-Host "Args: $($arguments -join ' ')" -ForegroundColor Gray
        Write-Host ""
    }

    # Run tests
    $process = Start-Process -FilePath $editorCmd -ArgumentList $arguments -NoNewWindow -PassThru -Wait:$false

    # Wait with timeout
    $completed = $process.WaitForExit($Timeout * 1000)

    if (-not $completed) {
        $process.Kill()
        throw "Test execution timed out after $Timeout seconds"
    }

    $exitCode = $process.ExitCode

    # Report results
    Write-Header "Results"

    $reportFile = Join-Path $OutputPath "index.json"
    $testsPassed = 0
    $testsFailed = 0
    $testsTotal = 0

    if (Test-Path $reportFile) {
        try {
            $report = Get-Content $reportFile -Raw | ConvertFrom-Json
            if ($report.tests) {
                $testsTotal = $report.tests.Count
                foreach ($test in $report.tests) {
                    if ($test.state -eq "Success") {
                        $testsPassed++
                    } else {
                        $testsFailed++
                    }
                }
            }
        } catch {
            Write-Warning "Could not parse test report"
        }
    }

    Write-Host ""
    Write-Host "Total Tests: $testsTotal"
    Write-Host "Passed:      $testsPassed" -ForegroundColor Green
    Write-Host "Failed:      $testsFailed" -ForegroundColor $(if ($testsFailed -gt 0) { "Red" } else { "Green" })
    Write-Host ""
    Write-Host "Log:    $logFile"
    Write-Host "Report: $reportFile"
    Write-Host ""

    if ($exitCode -eq 0 -and $testsFailed -eq 0) {
        Write-Success "All tests PASSED"
        exit 0
    } else {
        Write-Error "Tests FAILED (Exit code: $exitCode, Failed: $testsFailed)"
        exit 1
    }

} catch {
    Write-Error $_.Exception.Message
    exit 1
}
