[CmdletBinding()]
param(
    [ValidateSet('mingw-debug', 'mingw-release')]
    [string] $Preset = 'mingw-debug',

    [string] $Regex
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

. (Join-Path $PSScriptRoot 'enter-build-env.ps1')
& (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$arguments = @('--preset', $Preset, '--output-on-failure')
if ($Regex) {
    $arguments += @('-R', $Regex)
}

Push-Location $projectRoot
try {
    & ctest @arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
