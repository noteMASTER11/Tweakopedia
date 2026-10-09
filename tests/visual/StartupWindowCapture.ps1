[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $PreviewPath,
    [Parameter(Mandatory)]
    [string] $OutputRoot
)

$ErrorActionPreference = 'Stop'
$preview = [IO.Path]::GetFullPath($PreviewPath)
$output = [IO.Path]::GetFullPath($OutputRoot)
if (-not (Test-Path -LiteralPath $preview -PathType Leaf)) {
    throw "Startup preview executable not found: $preview"
}
New-Item -ItemType Directory -Force -Path $output | Out-Null

Add-Type -AssemblyName System.Drawing
if (-not ('Tweakopedia.StartupCapture.NativeMethods' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace Tweakopedia.StartupCapture {
    public static class NativeMethods {
        [StructLayout(LayoutKind.Sequential)]
        public struct Rect { public int Left, Top, Right, Bottom; }

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr window, out Rect bounds);

        [DllImport("user32.dll")]
        public static extern uint GetDpiForWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr window);

        [DllImport("user32.dll")]
        public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    }
}
'@
}

[void] [Tweakopedia.StartupCapture.NativeMethods]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
$process = Start-Process -FilePath $preview -ArgumentList '--stage', '8', '--hold-ms', '10000' -PassThru
try {
    $handle = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        $handle = $process.MainWindowHandle
    } while ($handle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline -and -not $process.HasExited)
    if ($handle -eq [IntPtr]::Zero) { throw 'Startup preview window did not appear.' }

    $bounds = [Tweakopedia.StartupCapture.NativeMethods+Rect]::new()
    if (-not [Tweakopedia.StartupCapture.NativeMethods]::GetWindowRect($handle, [ref] $bounds)) {
        throw 'Cannot read startup preview geometry.'
    }
    $width = $bounds.Right - $bounds.Left
    $height = $bounds.Bottom - $bounds.Top
    $dpi = [Tweakopedia.StartupCapture.NativeMethods]::GetDpiForWindow($handle)
    $scale = [int][Math]::Round(($dpi / 96.0) * 100)
    $scaleRoot = Join-Path $output $scale
    New-Item -ItemType Directory -Force -Path $scaleRoot | Out-Null
    $capturePath = Join-Path $scaleRoot 'startup-loading.png'

    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        [void] [Tweakopedia.StartupCapture.NativeMethods]::SetForegroundWindow($handle)
        Start-Sleep -Milliseconds 1500
        $graphics.CopyFromScreen(
            $bounds.Left, $bounds.Top, 0, 0,
            [Drawing.Size]::new($width, $height),
            [Drawing.CopyPixelOperation]::SourceCopy)
        $bitmap.Save($capturePath, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }

    Write-Output "CAPTURE: $capturePath ($dpi DPI, ${scale}%)"
    foreach ($missing in @(100, 125, 150, 200) | Where-Object { $_ -ne $scale }) {
        Write-Warning "Scale ${missing}% is unavailable on the current display session."
    }
}
finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    $process.Dispose()
}
