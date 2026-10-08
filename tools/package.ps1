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
    $expectedDistRoot = [IO.Path]::GetFullPath('D:\ChatGPT\Projects\Tweakopedia\dist')
    if (-not $distRoot.Equals($expectedDistRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Недопустимый каталог выпуска: $distRoot"
    }

    $workRoot = [IO.Path]::GetFullPath('D:\ChatGPT\Temp\Tweakopedia')
    $stagingRoot = [IO.Path]::GetFullPath((Join-Path $workRoot 'package-staging'))
    $candidateRoot = [IO.Path]::GetFullPath((Join-Path $workRoot 'container-candidate'))
    if (-not $stagingRoot.Equals('D:\ChatGPT\Temp\Tweakopedia\package-staging', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Недопустимый staging-каталог: $stagingRoot"
    }

    $env:TEMP = 'D:\ChatGPT\Temp'
    $env:TMP = 'D:\ChatGPT\Temp'
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset -Clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    foreach ($path in @($stagingRoot, $candidateRoot)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }

    $buildRoot = Join-Path $projectRoot "build\$Preset"
    & cmake --install $buildRoot --prefix $stagingRoot --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $deployScript = Join-Path $projectRoot 'cmake\DeployQt.cmake'
    $windeployqt = Join-Path $env:CMAKE_PREFIX_PATH 'bin\windeployqt.exe'
    $qmlSource = Join-Path $projectRoot 'apps\tweakopedia\qml'
    & cmake "-DWINDEPLOYQT=$windeployqt" "-DPACKAGE_ROOT=$stagingRoot" "-DQML_SOURCE=$qmlSource" -P $deployScript
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $licenseRoot = Join-Path $stagingRoot 'licenses'
    New-Item -ItemType Directory -Force -Path $licenseRoot | Out-Null
    $qtRoot = Split-Path (Split-Path $env:CMAKE_PREFIX_PATH -Parent) -Parent
    $qtLgpl = Join-Path $qtRoot 'Tools\CMake_64\share\cmake-3.30\Licenses\LGPLv3.txt'
    $licenseCopies = @{
        $qtLgpl = 'Qt-LGPLv3.txt'
        (Join-Path $projectRoot 'licenses\THIRD-PARTY-NOTICES.txt') = 'THIRD-PARTY-NOTICES.txt'
        (Join-Path $buildRoot '_deps\tweakopedia_miniz-src\LICENSE') = 'miniz-MIT.txt'
        (Join-Path $buildRoot '_deps\tweakopedia_nlohmann_json-src\LICENSE.MIT') = 'nlohmann-json-MIT.txt'
        (Join-Path $buildRoot '_deps\yaml-cpp-src\LICENSE') = 'yaml-cpp-MIT.txt'
    }
    foreach ($entry in $licenseCopies.GetEnumerator()) {
        if (-not (Test-Path -LiteralPath $entry.Key -PathType Leaf)) {
            throw "Файл лицензии отсутствует: $($entry.Key)"
        }
        Copy-Item -LiteralPath $entry.Key -Destination (Join-Path $licenseRoot $entry.Value)
    }

    $requiredPayload = @(
        'Tweakopedia.App.exe',
        'Tweakopedia.Executor.exe',
        'content\categories.yaml',
        'content\tweaks\filesystem\win32-long-paths.yaml',
        'licenses\Qt-LGPLv3.txt',
        'licenses\THIRD-PARTY-NOTICES.txt'
    )
    $missing = @($requiredPayload | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $stagingRoot $_) -PathType Leaf)
    })
    if ($missing.Count -gt 0) {
        throw "В payload отсутствуют: $($missing -join ', ')"
    }

    $sourceMarkers = @(
        'D:\ChatGPT\Projects\Tweakopedia\',
        'D:/ChatGPT/Projects/Tweakopedia/'
    )
    $binaryLeaks = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $stagingRoot -Recurse -File | Where-Object Extension -in @('.exe', '.dll'))) {
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        $ascii = [Text.Encoding]::ASCII.GetString($bytes)
        $unicode = [Text.Encoding]::Unicode.GetString($bytes)
        if ($sourceMarkers | Where-Object { $ascii.Contains($_) -or $unicode.Contains($_) }) {
            $binaryLeaks += $file.FullName
        }
    }
    if ($binaryLeaks.Count -gt 0) {
        throw "В release-бинарниках найдены абсолютные пути проекта: $($binaryLeaks -join ', ')"
    }

    $bootstrap = Join-Path $buildRoot 'apps\bootstrap\Tweakopedia.exe'
    $packager = Join-Path $buildRoot 'apps\packager\Tweakopedia.Packager.exe'
    $candidate = Join-Path $candidateRoot 'Tweakopedia.exe'
    & $packager create --bootstrap $bootstrap --payload-root $stagingRoot --output $candidate
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $inspection = (& $packager inspect --container $candidate --json | ConvertFrom-Json)
    if ($LASTEXITCODE -ne 0 -or $inspection.schema_version -ne 1) {
        throw 'Проверка EXE-контейнера завершилась ошибкой.'
    }
    foreach ($required in @('Tweakopedia.App.exe', 'Tweakopedia.Executor.exe',
            'content/categories.yaml', 'licenses/Qt-LGPLv3.txt', 'licenses/THIRD-PARTY-NOTICES.txt')) {
        if ($required -notin $inspection.files) {
            throw "Manifest контейнера не содержит $required"
        }
    }
    & pwsh -NoProfile -File (Join-Path $projectRoot 'tests\package\PackageLayoutTest.ps1') -PackagePath $candidateRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if (Test-Path -LiteralPath $distRoot) {
        Remove-Item -LiteralPath $distRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
    Move-Item -LiteralPath $candidate -Destination (Join-Path $distRoot 'Tweakopedia.exe')
    & pwsh -NoProfile -File (Join-Path $projectRoot 'tests\package\PackageLayoutTest.ps1') -PackagePath $distRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output "PASS: single EXE package created at $(Join-Path $distRoot 'Tweakopedia.exe')"
}
finally {
    Pop-Location
}
