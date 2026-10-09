[CmdletBinding()]
param(
    [ValidateSet('startup-loading')]
    [string] $Scenario = 'startup-loading',
    [string] $OutputRoot = 'D:\ChatGPT\Projects\Tweakopedia\artifacts\fluent-ui\startup-loading'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
. (Join-Path $PSScriptRoot 'enter-build-env.ps1')

Push-Location $projectRoot
try {
    & cmake --preset mingw-debug
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & cmake --build --preset mingw-debug --target TweakopediaStartupPreview
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectRoot 'tests\visual\StartupWindowCapture.ps1') `
        -PreviewPath (Join-Path $projectRoot 'build\mingw-debug\tests\Tweakopedia.StartupPreview.exe') `
        -OutputRoot $OutputRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
