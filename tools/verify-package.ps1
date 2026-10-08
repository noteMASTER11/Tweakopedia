[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PackagePath
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$container = [IO.Path]::GetFullPath($PackagePath)
if (-not (Test-Path -LiteralPath $container -PathType Leaf) `
    -or [IO.Path]::GetFileName($container) -cne 'Tweakopedia.exe') {
    throw "EXE-контейнер отсутствует или имеет неверное имя: $container"
}
$packageRoot = Split-Path $container -Parent
& pwsh -NoProfile -File (Join-Path $projectRoot 'tests\package\PackageLayoutTest.ps1') -PackagePath $packageRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$packager = Join-Path $projectRoot 'build\mingw-release\apps\packager\Tweakopedia.Packager.exe'
if (-not (Test-Path -LiteralPath $packager -PathType Leaf)) {
    throw "Packager inspect отсутствует: $packager"
}
$inspection = (& $packager inspect --container $container --json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $inspection.schema_version -ne 1) {
    throw 'Не удалось проверить footer и manifest контейнера.'
}

$isolatedRoot = [IO.Path]::GetFullPath('D:\ChatGPT\Temp\Tweakopedia\single-exe-self-check')
if (-not $isolatedRoot.Equals('D:\ChatGPT\Temp\Tweakopedia\single-exe-self-check',
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Недопустимый каталог проверки: $isolatedRoot"
}
if (Test-Path -LiteralPath $isolatedRoot) {
    Remove-Item -LiteralPath $isolatedRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $isolatedRoot | Out-Null

$saved = @{}
$variables = @('PATH', 'TEMP', 'TMP', 'LOCALAPPDATA', 'QT_PLUGIN_PATH',
    'QML2_IMPORT_PATH', 'QML_IMPORT_PATH', 'QML_PLUGIN_PATH', 'QML_DISK_CACHE_PATH')
foreach ($name in $variables) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Start-ContainerSelfCheck {
    return Start-Process -FilePath $container `
        -ArgumentList @('--self-check', '--no-elevation') `
        -WorkingDirectory $packageRoot -PassThru -WindowStyle Hidden
}

function Wait-ContainerSelfCheck {
    param([Parameter(Mandatory)] $Process)
    $process = $Process
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Self-check EXE-контейнера превысил 30 секунд.'
    }
    if ($process.ExitCode -ne 0) {
        throw "Self-check завершился с кодом $($process.ExitCode)."
    }
}

function Invoke-ContainerSelfCheck {
    Wait-ContainerSelfCheck -Process (Start-ContainerSelfCheck)
}

function Get-RuntimeDirectories {
    $runtimeRoot = Join-Path $isolatedRoot 'Tweakopedia\Runtime'
    if (-not (Test-Path -LiteralPath $runtimeRoot -PathType Container)) { return @() }
    return @(Get-ChildItem -LiteralPath $runtimeRoot -Directory | Where-Object {
        $_.Name -cmatch '^[0-9a-f]{64}$'
    })
}

try {
    $env:PATH = (($saved.PATH -split ';') | Where-Object {
        $_ -and $_ -notmatch '(?i)Tweakopedia\\Qt|mingw1310_64|build\\mingw-'
    }) -join ';'
    $env:TEMP = $isolatedRoot
    $env:TMP = $isolatedRoot
    $env:LOCALAPPDATA = $isolatedRoot
    foreach ($name in @('QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH',
            'QML_PLUGIN_PATH', 'QML_DISK_CACHE_PATH')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }

    $firstProcess = Start-ContainerSelfCheck
    $concurrentProcess = Start-ContainerSelfCheck
    Wait-ContainerSelfCheck -Process $firstProcess
    Wait-ContainerSelfCheck -Process $concurrentProcess
    $runtimes = @(Get-RuntimeDirectories)
    if ($runtimes.Count -ne 1) {
        throw "После первого запуска ожидался один runtime, найдено: $($runtimes.Count)."
    }
    $currentRuntime = $runtimes[0].FullName
    $marker = Join-Path $currentRuntime '.ready.json'
    $manifest = Join-Path $currentRuntime 'payload-manifest.json'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf) `
        -or -not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw 'Runtime не содержит marker или manifest.'
    }
    $markerTime = (Get-Item -LiteralPath $marker).LastWriteTimeUtc.Ticks
    Invoke-ContainerSelfCheck
    if ((Get-Item -LiteralPath $marker).LastWriteTimeUtc.Ticks -ne $markerTime) {
        throw 'Повторный запуск необоснованно пересоздал готовый runtime.'
    }
    $runtimeRoot = Split-Path $currentRuntime -Parent
    $staging = @(Get-ChildItem -LiteralPath $runtimeRoot -Directory | Where-Object {
        $_.Name -like '.staging-*' -or $_.Name -like '.invalid-*'
    })
    if ($staging.Count -ne 0) { throw 'После запуска остались временные runtime-каталоги.' }

    $repairTarget = Join-Path $currentRuntime 'content\categories.yaml'
    $expectedHash = (Get-FileHash -LiteralPath $repairTarget -Algorithm SHA256).Hash
    [IO.File]::AppendAllText($repairTarget, "`ncorrupted")
    Invoke-ContainerSelfCheck
    if ((Get-FileHash -LiteralPath $repairTarget -Algorithm SHA256).Hash -ne $expectedHash) {
        throw 'Повреждённый runtime-файл не был восстановлен.'
    }

    $freeOld = Join-Path $runtimeRoot ('b' * 64)
    New-Item -ItemType Directory -Force -Path $freeOld | Out-Null
    [IO.File]::WriteAllText((Join-Path $freeOld '.runtime.lock'), '')
    Invoke-ContainerSelfCheck
    if (Test-Path -LiteralPath $freeOld) { throw 'Свободная устаревшая версия runtime не удалена.' }

    $leasedOld = Join-Path $runtimeRoot ('c' * 64)
    New-Item -ItemType Directory -Force -Path $leasedOld | Out-Null
    $leasedLock = Join-Path $leasedOld '.runtime.lock'
    [IO.File]::WriteAllText($leasedLock, '')
    $lease = [IO.FileStream]::new($leasedLock, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        Invoke-ContainerSelfCheck
        if (-not (Test-Path -LiteralPath $leasedOld -PathType Container)) {
            throw 'Занятая устаревшая версия runtime была удалена.'
        }
    }
    finally {
        $lease.Dispose()
    }
    Invoke-ContainerSelfCheck
    if (Test-Path -LiteralPath $leasedOld) {
        throw 'Освобождённая устаревшая версия runtime не удалена при следующем запуске.'
    }
    if (@(Get-RuntimeDirectories).Count -ne 1) {
        throw 'После итоговой очистки количество runtime-каталогов отличается от одного.'
    }
}
finally {
    foreach ($name in $variables) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}

$containerHash = (Get-FileHash -LiteralPath $container -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "PASS: isolated single-EXE verification completed"
Write-Output "Container SHA-256: $containerHash"
Write-Output "Payload SHA-256: $($inspection.payload_sha256)"
