Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Invoke-AppCommand {
    param([string]$Executable, [string[]]$Arguments, [int]$TimeoutSeconds = 30)
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        throw "Command timed out: $Executable $($Arguments -join ' ')"
    }
    $process.Refresh()
    Assert-True ($process.ExitCode -eq 0) "Command failed ($($process.ExitCode)): $Executable $($Arguments -join ' ')"
}

function Stop-Monitor {
    param([string]$Executable, [System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return }
    $Process.Refresh()
    if ($Process.HasExited) { return }
    Invoke-AppCommand -Executable $Executable -Arguments @('--quit')
    if (-not $Process.WaitForExit(15000)) {
        $Process.Kill()
        throw 'MaxPrioGames did not exit after --quit.'
    }
}

function Get-MonitorTask {
    param([string]$Executable)
    @(Get-ScheduledTask -TaskName 'MaxPrioGames-*' -ErrorAction SilentlyContinue |
        Where-Object { @($_.Actions | Where-Object { $_.Execute -eq $Executable }).Count -gt 0 })
}

if (-not ('MaxPrioGamesTests.Windows' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
namespace MaxPrioGamesTests {
    public static class Windows {
        private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr param);
        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr param);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
        [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string className, string windowName);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder value, int count);
        public static bool HasTrayWindow() { return FindWindow("Shell_TrayWnd", null) != IntPtr.Zero; }
        public static int VisibleMainWindows(int pid) {
            int count = 0;
            EnumWindows((hwnd, param) => {
                uint owner; GetWindowThreadProcessId(hwnd, out owner);
                if (owner == pid && IsWindowVisible(hwnd)) {
                    var name = new StringBuilder(256); GetClassName(hwnd, name, name.Capacity);
                    if (name.ToString() == "MaxPrioGames.Main") ++count;
                }
                return true;
            }, IntPtr.Zero);
            return count;
        }
    }
}
'@
}

function Ensure-TestDesktop {
    if ([MaxPrioGamesTests.Windows]::HasTrayWindow()) { return }
    # CI must exercise the real Shell_NotifyIcon implementation, not a test stub.
    Start-Process -FilePath (Join-Path $env:WINDIR 'explorer.exe') | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ([MaxPrioGamesTests.Windows]::HasTrayWindow()) { return }
        Start-Sleep -Milliseconds 250
    }
    throw 'No Windows notification-area shell is available in this session; cannot verify real tray operation.'
}
