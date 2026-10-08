[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PackagePath
)

$ErrorActionPreference = 'Stop'
$packageRoot = [IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $packageRoot -PathType Container)) {
    throw "Каталог выпуска отсутствует: $packageRoot"
}

$entries = @(Get-ChildItem -LiteralPath $packageRoot -Force)
if ($entries.Count -ne 1 -or $entries[0].PSIsContainer `
    -or $entries[0].Name -cne 'Tweakopedia.exe') {
    throw "В каталоге выпуска должен находиться ровно один файл Tweakopedia.exe."
}

Write-Output "PASS: single-file layout verified at $packageRoot"
