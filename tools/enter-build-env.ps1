$toolRoot = 'D:\ChatGPT\Tools\Tweakopedia\Qt'
$cacheRoot = 'D:\ChatGPT\Cache\Tweakopedia'
$tempRoot = 'D:\ChatGPT\Temp\Tweakopedia'

$qtBin = Join-Path $toolRoot '6.8.3\mingw_64\bin'
$mingwBin = Join-Path $toolRoot 'Tools\mingw1310_64\bin'
$cmakeBin = Join-Path $toolRoot 'Tools\CMake_64\bin'
$ninjaBin = Join-Path $toolRoot 'Tools\Ninja'

$env:PATH = (@($qtBin, $mingwBin, $cmakeBin, $ninjaBin) + ($env:PATH -split ';')) -join ';'
$env:CMAKE_PREFIX_PATH = Join-Path $toolRoot '6.8.3\mingw_64'
$env:CMAKE_GENERATOR = 'Ninja'
$env:FETCHCONTENT_BASE_DIR = Join-Path $cacheRoot 'fetchcontent'
$env:TEMP = $tempRoot
$env:TMP = $tempRoot

New-Item -ItemType Directory -Force -Path $env:FETCHCONTENT_BASE_DIR, $tempRoot | Out-Null
