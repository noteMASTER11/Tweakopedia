[CmdletBinding()]
param(
    [ValidateSet('mingw-debug', 'mingw-release')]
    [string] $Preset = 'mingw-debug',

    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

. (Join-Path $PSScriptRoot 'enter-build-env.ps1')
& (Join-Path $PSScriptRoot 'verify-toolchain.ps1')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$buildPath = [IO.Path]::GetFullPath((Join-Path $projectRoot "build\$Preset"))
$expectedPrefix = [IO.Path]::GetFullPath((Join-Path $projectRoot 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $buildPath.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Недопустимый каталог сборки: $buildPath"
}

if ($Clean -and (Test-Path -LiteralPath $buildPath)) {
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

Push-Location $projectRoot
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & cmake --build --preset $Preset
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
