[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PackagePath
)

$ErrorActionPreference = 'Stop'
$packageRoot = [IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $packageRoot -PathType Container)) {
    throw "Portable-каталог отсутствует: $packageRoot"
}

$required = @(
    'Tweakopedia.exe',
    'Tweakopedia.Executor.exe',
    'Qt6Core.dll',
    'Qt6Gui.dll',
    'Qt6Network.dll',
    'Qt6Qml.dll',
    'Qt6Quick.dll',
    'Qt6QuickControls2.dll',
    'Qt6Sql.dll',
    'platforms\qwindows.dll',
    'sqldrivers\qsqlite.dll',
    'qml\QtQuick\qmldir',
    'qml\QtQuick\Controls\qmldir',
    'content\tweaks\filesystem\win32-long-paths.yaml'
)

$missing = @($required | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $packageRoot $_) -PathType Leaf)
})
if ($missing.Count -gt 0) {
    throw "В portable-комплекте отсутствуют: $($missing -join ', ')"
}

$forbiddenDirectories = @('src', 'tests', 'apps', 'cmake', '.git', '.worktrees')
$presentForbidden = @($forbiddenDirectories | Where-Object {
    Test-Path -LiteralPath (Join-Path $packageRoot $_)
})
if ($presentForbidden.Count -gt 0) {
    throw "В portable-комплект попали каталоги исходников: $($presentForbidden -join ', ')"
}

$forbiddenFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {
    $_.Extension -in @('.cpp', '.cxx', '.cc', '.h', '.hpp', '.cmake') -or $_.Name -eq 'CMakeLists.txt'
})
if ($forbiddenFiles.Count -gt 0) {
    throw "В portable-комплект попали исходники: $($forbiddenFiles.FullName -join ', ')"
}

$textFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {
    $_.Extension -in @('.txt', '.json', '.ini', '.conf', '.qml', '.yaml', '.qmldir')
})
$leaks = @($textFiles | Select-String -SimpleMatch -Pattern @(
    'D:\ChatGPT\Projects\Tweakopedia\',
    'D:/ChatGPT/Projects/Tweakopedia/',
    '\build\mingw-',
    '/build/mingw-'
) -ErrorAction SilentlyContinue)
if ($leaks.Count -gt 0) {
    throw "В portable-комплекте найдены абсолютные build-пути: $($leaks.Path -join ', ')"
}

$binaryLeaks = @()
$binaryFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Where-Object {
    $_.Extension -in @('.exe', '.dll')
})
foreach ($file in $binaryFiles) {
    $bytes = [IO.File]::ReadAllBytes($file.FullName)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    if ($ascii.Contains('D:/ChatGPT/Projects/Tweakopedia/') `
        -or $ascii.Contains('D:\ChatGPT\Projects\Tweakopedia\') `
        -or $unicode.Contains('D:/ChatGPT/Projects/Tweakopedia/') `
        -or $unicode.Contains('D:\ChatGPT\Projects\Tweakopedia\')) {
        $binaryLeaks += $file.FullName
    }
}
if ($binaryLeaks.Count -gt 0) {
    throw "В бинарниках portable-комплекта найдены абсолютные пути проекта: $($binaryLeaks -join ', ')"
}

Write-Output "PASS: portable layout verified at $packageRoot"
