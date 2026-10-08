[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$tempRoot = 'D:\ChatGPT\Temp\Tweakopedia\visual'
$outputRoot = 'D:\ChatGPT\Projects\Tweakopedia\artifacts\fluent-ui'

. (Join-Path $PSScriptRoot 'enter-build-env.ps1')
& (Join-Path $PSScriptRoot 'build.ps1') -Preset mingw-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$runner = Join-Path $projectRoot 'build\mingw-debug\tests\TweakopediaVisualCapture.exe'
$inputQml = Join-Path $projectRoot 'tests\visual\FluentCatalogPreview.qml'
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
        @{ Name = '150'; Scale = '1.5' }
    )) {
        $env:QT_SCALE_FACTOR = $capture.Scale
        $directory = Join-Path $outputRoot $capture.Name
        New-Item -ItemType Directory -Force $directory | Out-Null
        foreach ($size in @(
            @{ Name = 'wide'; Width = 1280; Height = 800 },
            @{ Name = 'narrow'; Width = 960; Height = 700 }
        )) {
            $output = Join-Path $directory ($size.Name + '.png')
            & $runner --input $inputQml --output $output --width $size.Width --height $size.Height
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
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
