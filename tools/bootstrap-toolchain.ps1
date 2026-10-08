[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path $PSScriptRoot -Parent
$lockPath = Join-Path $PSScriptRoot 'toolchain-lock.json'
$lock = Get-Content -Raw -LiteralPath $lockPath | ConvertFrom-Json

$toolRoot = 'D:\ChatGPT\Tools\Tweakopedia'
$qtRoot = Join-Path $toolRoot 'Qt'
$bootstrapRoot = Join-Path $toolRoot 'bootstrap'
$venvPython = Join-Path $bootstrapRoot 'Scripts\python.exe'
$cacheRoot = 'D:\ChatGPT\Cache\Tweakopedia'
$archiveRoot = Join-Path $cacheRoot 'aqt-archives'
$tempRoot = 'D:\ChatGPT\Temp\Tweakopedia'

foreach ($directory in @($toolRoot, $qtRoot, $cacheRoot, $archiveRoot, $tempRoot)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$env:TEMP = $tempRoot
$env:TMP = $tempRoot
$env:PIP_CACHE_DIR = Join-Path $cacheRoot 'pip'
$env:PYTHONPYCACHEPREFIX = Join-Path $cacheRoot 'pycache'

if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    $systemPython = (Get-Command python -ErrorAction Stop).Source
    & $systemPython -m venv $bootstrapRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Не удалось создать Python venv для aqtinstall.'
    }
}

& $venvPython -m pip install --disable-pip-version-check "aqtinstall==$($lock.aqtinstall)"
if ($LASTEXITCODE -ne 0) {
    throw 'Не удалось установить aqtinstall.'
}

function Invoke-AqtInstall {
    param(
        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $ExpectedFile
    )

    if (Test-Path -LiteralPath $ExpectedFile -PathType Leaf) {
        return
    }

    & $venvPython -m aqt @Arguments -O $qtRoot -k -d $archiveRoot
    if ($LASTEXITCODE -ne 0) {
        throw "aqt завершился с кодом ${LASTEXITCODE}: $($Arguments -join ' ')"
    }
}

Invoke-AqtInstall -Arguments @(
    'install-qt', 'windows', 'desktop', $lock.qt.version, $lock.qt.architecture
) -ExpectedFile (Join-Path $qtRoot '6.8.3\mingw_64\bin\qmake.exe')

foreach ($tool in $lock.tools) {
    $expectedFile = switch ($tool.name) {
        'MinGW' { Join-Path $qtRoot 'Tools\mingw1310_64\bin\g++.exe' }
        'CMake' { Join-Path $qtRoot 'Tools\CMake_64\bin\cmake.exe' }
        'Ninja' { Join-Path $qtRoot 'Tools\Ninja\ninja.exe' }
        default { throw "Неизвестный инструмент в lock-файле: $($tool.name)" }
    }

    Invoke-AqtInstall -Arguments @(
        'install-tool', 'windows', 'desktop', $tool.tool, $tool.variant
    ) -ExpectedFile $expectedFile
}

foreach ($tool in $lock.tools) {
    $archive = Get-ChildItem -LiteralPath $archiveRoot -Filter $tool.archive -File -Recurse |
        Select-Object -First 1
    if ($null -eq $archive) {
        throw "Архив не сохранён в кэше: $($tool.archive)"
    }

    $actualHash = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA1).Hash.ToLowerInvariant()
    if ($actualHash -ne $tool.sha1) {
        throw "Хеш архива $($tool.archive) не совпадает с lock-файлом."
    }
}

& (Join-Path $projectRoot 'tools\verify-toolchain.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Установленный toolchain не прошёл проверку.'
}
