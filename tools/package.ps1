[CmdletBinding()]
param(
    [ValidateSet('mingw-release')]
    [string] $Preset = 'mingw-release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
. (Join-Path $PSScriptRoot 'enter-build-env.ps1')

Push-Location $projectRoot
try {
    $commonGitDirectory = (& git rev-parse --path-format=absolute --git-common-dir).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $commonGitDirectory) {
        throw 'Не удалось определить основной каталог Git.'
    }
    $repositoryRoot = [IO.Path]::GetFullPath((Split-Path $commonGitDirectory -Parent))
    $distRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist'))
    $packageRoot = [IO.Path]::GetFullPath((Join-Path $distRoot 'Tweakopedia'))
    $expectedPackageRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist\Tweakopedia'))
    if (-not $packageRoot.Equals($expectedPackageRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Недопустимый каталог portable-комплекта: $packageRoot"
    }

    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset -Clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

    & cmake --install (Join-Path $projectRoot "build\$Preset") --prefix $packageRoot --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $deployScript = Join-Path $projectRoot 'cmake\DeployQt.cmake'
    $windeployqt = Join-Path $env:CMAKE_PREFIX_PATH 'bin\windeployqt.exe'
    $qmlSource = Join-Path $projectRoot 'apps\tweakopedia\qml'
    & cmake "-DWINDEPLOYQT=$windeployqt" "-DPACKAGE_ROOT=$packageRoot" "-DQML_SOURCE=$qmlSource" -P $deployScript
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & pwsh -NoProfile -File (Join-Path $projectRoot 'tests\package\PackageLayoutTest.ps1') -PackagePath $packageRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output "PASS: portable package created at $packageRoot"
}
finally {
    Pop-Location
}
