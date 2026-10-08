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

$version = $entries[0].VersionInfo.ProductVersion
if ($version -ne '0.9.0.0') {
    throw "Версия Tweakopedia.exe должна быть 0.9.0.0, фактически: $version"
}

Write-Output "PASS: single-file layout and version 0.9 verified at $packageRoot"
