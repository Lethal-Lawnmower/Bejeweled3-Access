# Posts a key press to the Bejeweled 3 window (for testing mod hotkeys
# without focusing the window). Usage: send-key.ps1 -Key 0x7B   (F12)
param([Parameter(Mandatory)][int]$Key)
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class B3Keys {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
"@
$h = [B3Keys]::FindWindow("MainWindow", $null)
if ($h -eq [IntPtr]::Zero) { Write-Error "Bejeweled 3 window not found"; exit 1 }
[void][B3Keys]::PostMessage($h, 0x100, [IntPtr]$Key, [IntPtr]1)
[void][B3Keys]::PostMessage($h, 0x101, [IntPtr]$Key, [IntPtr]0xC0000001)
"posted key $Key"
