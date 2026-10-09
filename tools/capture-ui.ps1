[CmdletBinding()]
param(
    [string]$InputQml = '',
    [string]$OutputName = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$tempRoot = 'D:\ChatGPT\Temp\Tweakopedia\visual'
$outputRoot = 'D:\ChatGPT\Projects\Tweakopedia\artifacts\fluent-ui'
$captureHiddenStrip = $false

. (Join-Path $PSScriptRoot 'enter-build-env.ps1')
& (Join-Path $PSScriptRoot 'build.ps1') -Preset mingw-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$runner = Join-Path $projectRoot 'build\mingw-debug\tests\TweakopediaVisualCapture.exe'
if ([string]::IsNullOrWhiteSpace($InputQml)) {
    $inputQml = Join-Path $projectRoot 'tests\visual\TweaksCatalogPreview.qml'
    $captureHiddenStrip = $true
    if ([string]::IsNullOrWhiteSpace($OutputName)) {
        $OutputName = 'tweaks-catalog'
    }
}
elseif ([System.IO.Path]::IsPathRooted($InputQml)) {
    $inputQml = [System.IO.Path]::GetFullPath($InputQml)
}
else {
    $inputQml = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $InputQml))
}
if (-not (Test-Path -LiteralPath $inputQml -PathType Leaf)) {
    throw "QML preview not found: $inputQml"
}
if (-not [string]::IsNullOrWhiteSpace($OutputName)) {
    if ($OutputName -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$') {
        throw 'OutputName must be a simple file-system name.'
    }
    $outputRoot = Join-Path $outputRoot $OutputName
}
New-Item -ItemType Directory -Force $tempRoot, $outputRoot | Out-Null

$previous = @{
    TEMP = $env:TEMP
    TMP = $env:TMP
    QT_SCALE_FACTOR = $env:QT_SCALE_FACTOR
    QT_QPA_PLATFORM = $env:QT_QPA_PLATFORM
    QT_QUICK_CONTROLS_STYLE = $env:QT_QUICK_CONTROLS_STYLE
}

try {
    $env:TEMP = $tempRoot
    $env:TMP = $tempRoot
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_CONTROLS_STYLE = 'Basic'

    foreach ($capture in @(
        @{ Name = '100'; Scale = '1' },
        @{ Name = '125'; Scale = '1.25' },
        @{ Name = '150'; Scale = '1.5' },
        @{ Name = '200'; Scale = '2' }
    )) {
        $env:QT_SCALE_FACTOR = $capture.Scale
        $directory = Join-Path $outputRoot $capture.Name
        New-Item -ItemType Directory -Force $directory | Out-Null
        foreach ($size in @(
            @{ Name = 'wide'; Width = 1484; Height = 999 },
            @{ Name = 'narrow'; Width = 960; Height = 700 }
        )) {
            $output = Join-Path $directory ($size.Name + '.png')
            & $runner --input $inputQml --output $output --width $size.Width --height $size.Height
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }

    if ($captureHiddenStrip) {
        $env:QT_SCALE_FACTOR = '1'
        $hiddenInput = Join-Path $projectRoot 'tests\visual\TweaksCatalogHiddenPreview.qml'
        $hiddenOutput = Join-Path (Join-Path $outputRoot '100') 'hidden-strip.png'
        & $runner --input $hiddenInput --output $hiddenOutput --width 1484 --height 999
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally {
    $env:TEMP = $previous.TEMP
    $env:TMP = $previous.TMP
    $env:QT_SCALE_FACTOR = $previous.QT_SCALE_FACTOR
    $env:QT_QPA_PLATFORM = $previous.QT_QPA_PLATFORM
    $env:QT_QUICK_CONTROLS_STYLE = $previous.QT_QUICK_CONTROLS_STYLE
}

Write-Host "Fluent UI captures: $outputRoot"
