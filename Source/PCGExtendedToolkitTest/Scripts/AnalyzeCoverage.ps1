<#
.SYNOPSIS
    Analyzes PCGEx test coverage by comparing source files to test files.

.DESCRIPTION
    Scans the PCGExtendedToolkit source modules and identifies:
    - Components that have corresponding test files
    - Components that are missing tests
    - Test files that don't map to source components

.EXAMPLE
    .\AnalyzeCoverage.ps1
    .\AnalyzeCoverage.ps1 -Module "PCGExCore"
    .\AnalyzeCoverage.ps1 -Verbose
#>

param(
    [string]$Module = "",
    [switch]$Verbose
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TestRoot = Split-Path -Parent $ScriptDir
$SourceRoot = Split-Path -Parent $TestRoot

# Define module priorities based on usage/importance
# Tier 1 = Core/Essential, Tier 2 = Commonly Used, Tier 3 = Niche, Tier 4 = Rarely Used, 0 = Skip
$ModulePriorities = @{
    # Core Infrastructure - Tier 1
    "PCGExCore" = 1
    "PCGExFoundations" = 1
    "PCGExGraphs" = 1
    "PCGExFilters" = 1
    "PCGExBlending" = 1

    # Shared Systems
    "PCGExHeuristics" = 1          # Core/Essential
    "PCGExMatching" = 2            # Commonly Used
    "PCGExNoise3D" = 3             # Niche
    "PCGExPickers" = 3             # Niche
    "PCGExProperties" = 2          # Commonly Used

    # Collections
    "PCGExCollections" = 2         # Commonly Used

    # Element Modules - Core/Essential
    "PCGExElementsPaths" = 1
    "PCGExElementsPathfinding" = 1
    "PCGExElementsClustersDiagrams" = 1
    "PCGExElementsSampling" = 1
    "PCGExElementsBridges" = 1

    # Element Modules - Commonly Used
    "PCGExElementsMeta" = 2
    "PCGExElementsSpatial" = 2
    "PCGExElementsShapes" = 2
    "PCGExElementsClusters" = 2
    "PCGExElementsClustersRefine" = 2
    "PCGExElementsFloodFill" = 2
    "PCGExElementsProbing" = 2
    "PCGExElementsClipper2" = 2

    # Element Modules - Niche
    "PCGExElementsClustersRelax" = 3
    "PCGExElementsValency" = 3

    # Element Modules - Rarely Used
    "PCGExElementsTopology" = 4
    "PCGExElementsTensors" = 4
    "PCGExElementsActions" = 4

    # Skip Testing
    "PCGExElementsPathfindingNavmesh" = 0
    "PCGExtendedToolkit" = 0       # Plugin module definition
    "PCGExtendedToolkitTest" = 0   # Test module itself
    "ThirdParty" = 0               # External code

    # Editor Modules - Skip
    "PCGExCoreEditor" = 0
    "PCGExCollectionsEditor" = 0
    "PCGExFoundationsEditor" = 0
    "PCGExElementsValencyEditor" = 0
    "PCGExPropertiesEditor" = 0
    "PCGExtendedToolkitEditor" = 0
}

# Categories that are easy to unit test (pure functions, data structures)
$EasyTestCategories = @("Math", "Containers", "Helpers", "Types", "Details", "Filters")

# Get all source modules
function Get-SourceModules {
    $modules = Get-ChildItem -Path $SourceRoot -Directory |
        Where-Object {
            $priority = $ModulePriorities[$_.Name]
            # Include if priority > 0 or not in list (default to include)
            ($null -eq $priority) -or ($priority -gt 0)
        }
    return $modules
}

# Get all header files in a module
function Get-ModuleHeaders {
    param([string]$ModulePath)

    $publicPath = Join-Path $ModulePath "Public"
    if (Test-Path $publicPath) {
        return Get-ChildItem -Path $publicPath -Filter "*.h" -Recurse
    }
    return @()
}

# Get all test files
function Get-TestFiles {
    $testPath = Join-Path $TestRoot "Private\Tests"
    if (Test-Path $testPath) {
        return Get-ChildItem -Path $testPath -Filter "*.cpp" -Recurse
    }
    return @()
}

# Check if a source file has a corresponding test
function Find-TestForSource {
    param(
        [string]$SourceFileName,
        [array]$TestFiles
    )

    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($SourceFileName)

    # Look for exact match or Tests suffix
    foreach ($testFile in $TestFiles) {
        $testBaseName = [System.IO.Path]::GetFileNameWithoutExtension($testFile.Name)
        if ($testBaseName -eq "${baseName}Tests" -or
            $testBaseName -eq $baseName -or
            $testBaseName -like "*$baseName*") {
            return $testFile
        }
    }
    return $null
}

# Categorize a header file
function Get-HeaderCategory {
    param([System.IO.FileInfo]$Header)

    $relativePath = $Header.DirectoryName
    foreach ($category in $EasyTestCategories) {
        if ($relativePath -like "*\$category\*" -or $relativePath -like "*/$category/*") {
            return $category
        }
    }

    if ($relativePath -like "*\Filters\*") { return "Filters" }
    if ($relativePath -like "*\Elements\*") { return "Elements" }
    if ($relativePath -like "*\Core\*") { return "Core" }
    if ($relativePath -like "*\Data\*") { return "Data" }
    if ($relativePath -like "*\Graphs\*") { return "Graphs" }
    if ($relativePath -like "*\Clusters\*") { return "Clusters" }
    if ($relativePath -like "*\Paths\*") { return "Paths" }
    if ($relativePath -like "*\Sampling\*") { return "Sampling" }
    if ($relativePath -like "*\Search\*") { return "Search" }
    if ($relativePath -like "*\Heuristics\*") { return "Heuristics" }
    if ($relativePath -like "*\Blenders\*") { return "Blenders" }

    return "Other"
}

# Get tier name
function Get-TierName {
    param([int]$Priority)
    switch ($Priority) {
        1 { return "Core/Essential" }
        2 { return "Commonly Used" }
        3 { return "Niche" }
        4 { return "Rarely Used" }
        default { return "Unknown" }
    }
}

# Main analysis
Write-Host "PCGEx Test Coverage Analysis" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan
Write-Host ""

$testFiles = Get-TestFiles
$modules = Get-SourceModules

if ($Module) {
    $modules = $modules | Where-Object { $_.Name -eq $Module }
}

$totalHeaders = 0
$testedHeaders = 0
$results = @()

foreach ($mod in $modules) {
    $headers = Get-ModuleHeaders -ModulePath $mod.FullName
    $priority = $ModulePriorities[$mod.Name]
    if ($null -eq $priority) { $priority = 2 }  # Default to commonly used if not listed
    if ($priority -eq 0) { continue }  # Skip modules marked as 0

    $moduleResults = @{
        Name = $mod.Name
        Priority = $priority
        TierName = Get-TierName -Priority $priority
        TotalHeaders = $headers.Count
        TestedHeaders = 0
        Categories = @{}
        Untested = @()
        Tested = @()
    }

    foreach ($header in $headers) {
        $totalHeaders++
        $category = Get-HeaderCategory -Header $header

        if (-not $moduleResults.Categories.ContainsKey($category)) {
            $moduleResults.Categories[$category] = @{ Total = 0; Tested = 0 }
        }
        $moduleResults.Categories[$category].Total++

        $testFile = Find-TestForSource -SourceFileName $header.Name -TestFiles $testFiles
        if ($testFile) {
            $testedHeaders++
            $moduleResults.TestedHeaders++
            $moduleResults.Categories[$category].Tested++
            $moduleResults.Tested += @{
                Header = $header.Name
                TestFile = $testFile.Name
                Category = $category
            }
        } else {
            $moduleResults.Untested += @{
                Header = $header.Name
                Category = $category
                Priority = if ($category -in $EasyTestCategories) { "High" } else { "Normal" }
            }
        }
    }

    $results += $moduleResults
}

# Sort by priority then name
$results = $results | Sort-Object { $_.Priority }, { $_.Name }

# Output summary by tier
Write-Host "Module Summary by Priority Tier:" -ForegroundColor Yellow
Write-Host ""

$currentTier = 0
foreach ($mod in $results) {
    if ($mod.Priority -ne $currentTier) {
        $currentTier = $mod.Priority
        Write-Host ""
        Write-Host ("=== Tier {0}: {1} ===" -f $currentTier, (Get-TierName -Priority $currentTier)) -ForegroundColor Magenta
    }

    $coverage = if ($mod.TotalHeaders -gt 0) {
        [math]::Round(($mod.TestedHeaders / $mod.TotalHeaders) * 100, 1)
    } else { 0 }

    $color = switch ($coverage) {
        { $_ -ge 50 } { "Green" }
        { $_ -ge 20 } { "Yellow" }
        default { "Red" }
    }

    Write-Host ("  {0,-35} {1,3}/{2,-3} headers ({3,5}%)" -f
        $mod.Name, $mod.TestedHeaders, $mod.TotalHeaders, $coverage) -ForegroundColor $color

    if ($Verbose) {
        foreach ($cat in $mod.Categories.GetEnumerator() | Sort-Object { $_.Value.Total } -Descending) {
            $catCoverage = if ($cat.Value.Total -gt 0) {
                [math]::Round(($cat.Value.Tested / $cat.Value.Total) * 100, 1)
            } else { 0 }
            Write-Host ("       {0,-15} {1,3}/{2,-3} ({3}%)" -f
                $cat.Key, $cat.Value.Tested, $cat.Value.Total, $catCoverage) -ForegroundColor DarkGray
        }
    }
}

Write-Host ""
Write-Host ("Total: {0}/{1} headers tested ({2}%)" -f
    $testedHeaders, $totalHeaders,
    [math]::Round(($testedHeaders / $totalHeaders) * 100, 1)) -ForegroundColor Cyan

# Tier summary
Write-Host ""
Write-Host "Coverage by Tier:" -ForegroundColor Yellow
for ($t = 1; $t -le 4; $t++) {
    $tierModules = $results | Where-Object { $_.Priority -eq $t }
    $tierTotal = ($tierModules | Measure-Object -Property TotalHeaders -Sum).Sum
    $tierTested = ($tierModules | Measure-Object -Property TestedHeaders -Sum).Sum
    if ($tierTotal -gt 0) {
        $tierCoverage = [math]::Round(($tierTested / $tierTotal) * 100, 1)
        Write-Host ("  Tier {0} ({1,-15}): {2,3}/{3,-3} ({4}%)" -f
            $t, (Get-TierName -Priority $t), $tierTested, $tierTotal, $tierCoverage)
    }
}

# High priority untested items (Tier 1 only)
Write-Host ""
Write-Host "High Priority Untested (Tier 1 - Core/Essential, Easy Categories):" -ForegroundColor Yellow
Write-Host ""

$highPriority = $results |
    Where-Object { $_.Priority -eq 1 } |
    ForEach-Object {
        $modName = $_.Name
        $_.Untested | Where-Object { $_.Priority -eq "High" } | ForEach-Object {
            [PSCustomObject]@{
                Module = $modName
                Header = $_.Header
                Category = $_.Category
            }
        }
    }

$highPriority | Select-Object -First 25 | ForEach-Object {
    Write-Host ("  {0,-30} {1,-40} [{2}]" -f $_.Module, $_.Header, $_.Category)
}

if (($highPriority | Measure-Object).Count -gt 25) {
    Write-Host "  ... and $((($highPriority | Measure-Object).Count) - 25) more" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "Run with -Verbose for detailed category breakdown" -ForegroundColor DarkGray
Write-Host "Run with -Module 'PCGExCore' to analyze a specific module" -ForegroundColor DarkGray
