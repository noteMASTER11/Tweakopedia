[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$toolRoot = 'D:\ChatGPT\Tools\Tweakopedia\Qt'
$required = [ordered]@{
    'Qt 6.8.3'    = Join-Path $toolRoot '6.8.3\mingw_64\bin\qmake.exe'
    'MinGW 13.1'  = Join-Path $toolRoot 'Tools\mingw1310_64\bin\g++.exe'
    'CMake 3.30.5' = Join-Path $toolRoot 'Tools\CMake_64\bin\cmake.exe'
    'Ninja 1.12.1' = Join-Path $toolRoot 'Tools\Ninja\ninja.exe'
}

$missing = @(
    foreach ($entry in $required.GetEnumerator()) {
        if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
            [pscustomobject]@{
                Name = $entry.Key
                Path = $entry.Value
            }
        }
    }
)

if ($missing.Count -gt 0) {
    foreach ($item in $missing) {
        Write-Error "FAIL: отсутствует $($item.Name): $($item.Path)" -ErrorAction Continue
    }
    exit 1
}

$versionChecks = @(
    [pscustomobject]@{
        Name = 'Qt'
        Expected = '6.8.3'
        Actual = (& $required['Qt 6.8.3'] -query QT_VERSION).Trim()
    }
    [pscustomobject]@{
        Name = 'MinGW'
        Expected = '13.1.0'
        Actual = (& $required['MinGW 13.1'] -dumpfullversion).Trim()
    }
    [pscustomobject]@{
        Name = 'CMake'
        Expected = '3.30.5'
        Actual = ((& $required['CMake 3.30.5'] --version)[0] -replace '^cmake version\s+', '').Trim()
    }
    [pscustomobject]@{
        Name = 'Ninja'
        Expected = '1.12.1'
        Actual = (& $required['Ninja 1.12.1'] --version).Trim()
    }
)

$mismatched = @($versionChecks | Where-Object { $_.Actual -ne $_.Expected })
if ($mismatched.Count -gt 0) {
    foreach ($item in $mismatched) {
        Write-Error "FAIL: версия $($item.Name): ожидалась $($item.Expected), найдена $($item.Actual)" -ErrorAction Continue
    }
    exit 1
}

Write-Output 'PASS: Qt 6.8.3, MinGW 13.1.0, CMake 3.30.5 и Ninja 1.12.1 найдены.'
