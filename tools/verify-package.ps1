[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PackagePath
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$packageRoot = [IO.Path]::GetFullPath($PackagePath)
$tempRoot = 'D:\ChatGPT\Temp\Tweakopedia\package-self-check'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

& pwsh -NoProfile -File (Join-Path $projectRoot 'tests\package\PackageLayoutTest.ps1') -PackagePath $packageRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$savedPath = $env:PATH
$savedTemp = $env:TEMP
$savedTmp = $env:TMP
$savedQtPluginPath = $env:QT_PLUGIN_PATH
$savedQmlImportPath = $env:QML2_IMPORT_PATH
try {
    $env:PATH = (($savedPath -split ';') | Where-Object {
        $_ -and $_ -notmatch '(?i)Tweakopedia\\Qt|mingw1310_64|build\\mingw-'
    }) -join ';'
    $env:TEMP = $tempRoot
    $env:TMP = $tempRoot
    $env:QT_PLUGIN_PATH = ''
    $env:QML2_IMPORT_PATH = ''

    $executable = Join-Path $packageRoot 'Tweakopedia.exe'
    $process = Start-Process -FilePath $executable -ArgumentList @('--self-check', '--no-elevation') `
        -WorkingDirectory $packageRoot -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Self-check portable-комплекта превысил 30 секунд.'
    }
    if ($process.ExitCode -ne 0) {
        throw "Self-check завершился с кодом $($process.ExitCode)."
    }
}
finally {
    $env:PATH = $savedPath
    $env:TEMP = $savedTemp
    $env:TMP = $savedTmp
    $env:QT_PLUGIN_PATH = $savedQtPluginPath
    $env:QML2_IMPORT_PATH = $savedQmlImportPath
}

Write-Output "PASS: portable package self-check succeeded at $packageRoot"
