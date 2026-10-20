<#
.SYNOPSIS
    Prints the structure of the Windows desktop windows.

.DESCRIPTION
    Lists every top-level window (in z-order), the WorkerW windows, the
    Progman window, the window that hosts the desktop icons and the children
    of that host. It is used to work out how the desktop is composed, so that
    the wallpaper can be placed in the right layer.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File dump-windows.ps1 > windows.txt
#>
$ErrorActionPreference = 'Stop'

if (-not ('LivePaper.Win32Dump' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace LivePaper
{
    public class Win32Dump
    {
        public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left, Top, Right, Bottom; }

        [DllImport("user32.dll")]
        public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
        [DllImport("user32.dll")]
        public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr lParam);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetClassName(IntPtr hWnd, StringBuilder s, int max);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int max);
        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll", EntryPoint = "GetWindowLong")]
        public static extern int GetWindowLong(IntPtr hWnd, int index);
        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
        [DllImport("user32.dll")]
        public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
        [DllImport("user32.dll")]
        public static extern IntPtr GetParent(IntPtr hWnd);
        [DllImport("user32.dll")]
        public static extern int GetSystemMetrics(int index);
    }
}
'@
}

$script:StyleNames = [ordered]@{
    0x80000000 = 'WS_POPUP'
    0x40000000 = 'WS_CHILD'
    0x20000000 = 'WS_MINIMIZE'
    0x10000000 = 'WS_VISIBLE'
    0x08000000 = 'WS_DISABLED'
    0x04000000 = 'WS_CLIPSIBLINGS'
    0x02000000 = 'WS_CLIPCHILDREN'
    0x01000000 = 'WS_MAXIMIZE'
    0x00C00000 = 'WS_CAPTION'
    0x00200000 = 'WS_VSCROLL'
    0x00100000 = 'WS_HSCROLL'
    0x00080000 = 'WS_SYSMENU'
    0x00040000 = 'WS_THICKFRAME'
    0x00020000 = 'WS_MINIMIZEBOX'
    0x00010000 = 'WS_MAXIMIZEBOX'
}

function Get-StyleText([int]$style) {
    # The style is a signed 32 bit value (WS_POPUP is 0x80000000), so it is
    # converted through a 64 bit value to avoid an overflow.
    $u = [uint32](([int64]$style) -band 0xFFFFFFFFL)
    $parts = @()
    foreach ($key in $script:StyleNames.Keys) {
        if (($u -band [uint32]$key) -ne 0) { $parts += $script:StyleNames[$key] }
    }
    if ($parts.Count -eq 0) { return ('0x{0:X8}' -f $u) }
    return ($parts -join '|')
}

function Get-WindowInfo([IntPtr]$h) {
    $cls = New-Object System.Text.StringBuilder 256
    [void][LivePaper.Win32Dump]::GetClassName($h, $cls, $cls.Capacity)
    $txt = New-Object System.Text.StringBuilder 256
    [void][LivePaper.Win32Dump]::GetWindowText($h, $txt, $txt.Capacity)
    $r = New-Object LivePaper.Win32Dump+RECT
    [void][LivePaper.Win32Dump]::GetWindowRect($h, [ref]$r)
    $c = New-Object LivePaper.Win32Dump+RECT
    [void][LivePaper.Win32Dump]::GetClientRect($h, [ref]$c)
    $style = [LivePaper.Win32Dump]::GetWindowLong($h, -16)
    $ex    = [LivePaper.Win32Dump]::GetWindowLong($h, -20)
    [pscustomobject]@{
        Handle  = $h
        Class   = $cls.ToString()
        Title   = $txt.ToString()
        Visible = [LivePaper.Win32Dump]::IsWindowVisible($h)
        Style   = (Get-StyleText $style)
        ExStyle = (Get-StyleText $ex)
        StyleRaw   = ('0x{0:X8}' -f [uint32](([int64]$style) -band 0xFFFFFFFFL))
        ExStyleRaw = ('0x{0:X8}' -f [uint32](([int64]$ex) -band 0xFFFFFFFFL))
        Rect    = "$($r.Left),$($r.Top) - $($r.Right),$($r.Bottom)"
        Client  = "$($c.Right - $c.Left)x$($c.Bottom - $c.Top)"
        Parent  = [LivePaper.Win32Dump]::GetParent($h)
    }
}

function Show-Window([IntPtr]$h, [int]$index) {
    $i = Get-WindowInfo $h
    '{0,3}  hwnd=0x{1:X8} class={2,-20} title={3,-28} visible={4} style={5,-44} ex={6,-38} rect={7,-24} client={8}' -f `
        $index, $i.Handle.ToInt64(), ("'" + $i.Class + "'"), ("'" + $i.Title + "'"), `
        ([int]$i.Visible), $i.Style, $i.ExStyle, $i.Rect, $i.Client
}

function Get-TopLevelWindows {
    $list = New-Object System.Collections.ArrayList
    $cb = [LivePaper.Win32Dump+EnumProc] {
        param($h, $l)
        [void]$list.Add($h)
        return $true
    }
    [void][LivePaper.Win32Dump]::EnumWindows($cb, [IntPtr]::Zero)
    return $list
}

function Get-ChildWindows([IntPtr]$parent) {
    $list = New-Object System.Collections.ArrayList
    $cb = [LivePaper.Win32Dump+EnumProc] {
        param($h, $l)
        [void]$list.Add($h)
        return $true
    }

Write-Output "=== screen ==="
Write-Output ("  primary : {0}x{1}" -f [LivePaper.Win32Dump]::GetSystemMetrics(0), [LivePaper.Win32Dump]::GetSystemMetrics(1))
Write-Output ("  virtual : {0}x{1} at ({2},{3})" -f `
    [LivePaper.Win32Dump]::GetSystemMetrics(78), [LivePaper.Win32Dump]::GetSystemMetrics(79), `
    [LivePaper.Win32Dump]::GetSystemMetrics(76), [LivePaper.Win32Dump]::GetSystemMetrics(77))

$top = Get-TopLevelWindows

Write-Output ""
Write-Output "=== all top-level windows (z-order, 0 = topmost) ==="
$i = 0
foreach ($h in $top) { Show-Window $h $i; $i++ }

Write-Output ""
Write-Output "=== WorkerW windows ==="
$i = 0
foreach ($h in $top) {
    $info = Get-WindowInfo $h
    if ($info.Class -eq 'WorkerW') {
        Write-Output ("  --- z-order {0,3} ---" -f $i)
        Show-Window $h $i
    }
    $i++
}

Write-Output ""
Write-Output "=== Progman ==="
foreach ($h in $top) {
    $info = Get-WindowInfo $h
    if ($info.Class -eq 'Progman') { Show-Window $h -1 }
}

Write-Output ""
Write-Output "=== the desktop icons (SHELLDLL_DefView) ==="
$iconsHost = [IntPtr]::Zero
foreach ($h in $top) {
    $dv = [LivePaper.Win32Dump]::FindWindowEx($h, [IntPtr]::Zero, 'SHELLDLL_DefView', $null)
    if ($dv -ne [IntPtr]::Zero) { $iconsHost = $h; break }
}
if ($iconsHost -eq [IntPtr]::Zero) {
    Write-Output "  not found"
} else {
    Write-Output "  host:"
    Show-Window $iconsHost -1
    $dv = [LivePaper.Win32Dump]::FindWindowEx($iconsHost, [IntPtr]::Zero, 'SHELLDLL_DefView', $null)
    Write-Output "  SHELLDLL_DefView:"
    Show-Window $dv -1
    Write-Output "  children of the host (z-order, 0 = topmost):"
    $i = 0
    foreach ($c in (Get-ChildWindows $iconsHost)) { Show-Window $c $i; $i++ }
}

    [void][LivePaper.Win32Dump]::EnumChildWindows($parent, $cb, [IntPtr]::Zero)
    return $list
}
