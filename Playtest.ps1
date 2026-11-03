# Windows entry point. Reuse a configured CMake Python when Python is not on PATH.
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$PlaytestArguments)
$ErrorActionPreference = 'Stop'
$playtestPython = $null
foreach ($playtestCache in @('build/windows-release/CMakeCache.txt', 'build/playtest-core/CMakeCache.txt')) {
    $playtestCachePath = Join-Path $PSScriptRoot $playtestCache
    if (Test-Path -LiteralPath $playtestCachePath) {
        foreach ($playtestLine in Get-Content -LiteralPath $playtestCachePath) {
            if ($playtestLine -match '^_?Python3_EXECUTABLE:[^=]+=(.+)$' -and (Test-Path -LiteralPath $Matches[1])) {
                $playtestPython = $Matches[1]
                break
            }
        }
    }
    if ($playtestPython) { break }
}
if (!$playtestPython) {
    $playtestCommand = Get-Command py -ErrorAction SilentlyContinue
    if ($playtestCommand) {
        & $playtestCommand.Source -3 (Join-Path $PSScriptRoot 'Tools/playtest.py') @PlaytestArguments
        exit $LASTEXITCODE
    }
    $playtestCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($playtestCommand -and $playtestCommand.Source -notlike '*WindowsApps*') {
        $playtestPython = $playtestCommand.Source
    }
}
if (!$playtestPython) { throw 'Install Python 3 and add it to PATH, or configure the CMake preset with Python3_EXECUTABLE.' }
& $playtestPython (Join-Path $PSScriptRoot 'Tools/playtest.py') @PlaytestArguments
exit $LASTEXITCODE
