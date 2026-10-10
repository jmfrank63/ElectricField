<#
  QA-05: capture the MFC build's view of all 9 presets (default options) as PNG.
  Windows PowerShell 5.1 (powershell.exe). Local-only tool; touches only the process it starts and the app's own
  registry key HKCU\Software\Free App\Electric Field\options (backed up first, restored afterwards).

  Usage:
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\baseline\capture-mfc-baseline.ps1 `
        -Exe <path-to-ElectricField.exe> -OutDir docs\baseline\mfc [-PresetList "0,1,2,3,4,5,6,7,8"] [-ResetOptions] [-BackupDir <dir>]

  -ResetOptions  Exports the options key to <BackupDir>\ElectricField-options-backup.reg, deletes it so the
                 built-in defaults apply, and restores it (also on failure) when the script finishes.
  -BackupDir     Where the registry backup is written. Defaults to the user temp folder. Never use a path inside
                 the repository: the backup must not be committed.
  -PresetList    Quote it when calling through -Command (an unquoted 4,5,6 would bind as an array).
#>
param(
  [Parameter(Mandatory = $true)][string]$Exe,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [string]$PresetList = '0,1,2,3,4,5,6,7,8',
  [int]$ClientW = 1024, [int]$ClientH = 768, [int]$TimeoutSec = 180,
  [switch]$ResetOptions,
  [string]$BackupDir = $env:TEMP
)
$ErrorActionPreference = 'Stop'
# D-18: validate -Exe before anything else (no Add-Type, no registry backup/delete happens for a bad path).
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Exe not found: $Exe" }
Add-Type -AssemblyName System.Drawing
$Presets = $PresetList.Split(',') | ForEach-Object { [int]$_ }

Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Text; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public class W {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr dlg, int id);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  // near-black OPAQUE pixels (BGRA, alpha 255): field lines are black by default, charges blue/red, equipotentials grey(192).
  // Transparent black (a failed or empty grab) must not count as ink (D-18).
  public static int Dark(Bitmap b) {
    var d = b.LockBits(new Rectangle(0,0,b.Width,b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    byte[] buf = new byte[d.Stride * b.Height]; Marshal.Copy(d.Scan0, buf, 0, buf.Length); b.UnlockBits(d);
    int n = 0; for (int i = 0; i + 3 < buf.Length; i += 4) if (buf[i] < 80 && buf[i+1] < 80 && buf[i+2] < 80 && buf[i+3] == 255) n++;
    return n;
  }
}
"@
[void][W]::SetProcessDPIAware()      # physical pixels: the recorded client size is then a real pixel count
$dpi = [W]::GetDpiForSystem()
if ($dpi -ne 96) { Write-Warning "System DPI is $dpi (display scaling != 100%). The MFC app is not DPI aware, so DWM would bitmap-scale it: run the baseline at 100% scaling." }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$expected = @(
  @{ slug = 'dipole';                              name = 'Dipole' },
  @{ slug = 'two-equal-charges';                   name = 'Two Equal Charges' },
  @{ slug = 'two-unequal-charges-same-sign';       name = 'Two Unequal Charges Same Sign' },
  @{ slug = 'two-unequal-charges-different-signs'; name = 'Two Unequal Charges Different Signs' },
  @{ slug = 'quadrupole';                          name = 'Quadrupole' },
  @{ slug = 'four-charges-same-sign';              name = 'Four Charges Same Sign' },
  @{ slug = 'two-dipoles-same-direction';          name = 'Two Dipoles Same Direction' },
  @{ slug = 'two-dipoles-opposed-direction';       name = 'Two Dipoles Oposed Direction' },   # sic: the .rc "File New" string has the typo
  @{ slug = 'parallel-dipoles';                    name = 'Parallel Dipoles' })

function Get-Cls($h) { $c = New-Object System.Text.StringBuilder 256; [void][W]::GetClassName($h, $c, 256); $c.ToString() }
function Get-Txt($h) { $c = New-Object System.Text.StringBuilder 256; [void][W]::GetWindowText($h, $c, 256); $c.ToString() }
function Find-Top($procId) {
  $script:acc = New-Object System.Collections.ArrayList
  $cb = [W+EnumProc]{ param($h, $l); $p = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$p); if ($p -eq $procId -and [W]::IsWindowVisible($h)) { [void]$script:acc.Add($h) }; $true }
  [void][W]::EnumWindows($cb, [IntPtr]::Zero); $script:acc.ToArray()
}
function Find-Desc($parent) {
  $script:acc = New-Object System.Collections.ArrayList
  $cb = [W+EnumProc]{ param($h, $l); [void]$script:acc.Add($h); $true }
  [void][W]::EnumChildWindows($parent, $cb, [IntPtr]::Zero); $script:acc.ToArray()
}
function Wait-For($block, $sec, $what) {
  $sw = [Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt $sec) { $r = & $block; if ($r) { return $r }; Start-Sleep -Milliseconds 200 }
  throw "timeout waiting for $what"
}
function Grab($h) {   # PW_CLIENTONLY (1) | PW_RENDERFULLCONTENT (2): works for the Direct2D HWND render target
  $r = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$r)
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
  $ok = [W]::PrintWindow($h, $hdc, 3)
  $g.ReleaseHdc($hdc); $g.Dispose()
  if (-not $ok) { $bmp.Dispose(); return $null }   # D-18: a failed PrintWindow is "no frame", never an (empty) image
  $bmp
}
function Hash($bmp) { $ms = New-Object System.IO.MemoryStream; $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png); [BitConverter]::ToString([Security.Cryptography.SHA1]::Create().ComputeHash($ms.ToArray())) }

# reg.exe wrapper: a missing key writes to stderr, which would abort the script under Windows PowerShell 5.1
# with $ErrorActionPreference = 'Stop'. Returns the exit code only.
function Invoke-Reg {
  $ErrorActionPreference = 'Continue'
  & reg.exe @args 2>&1 | Out-Null
  return $LASTEXITCODE
}

$regKey = 'HKCU\Software\Free App\Electric Field\options'
$keyExisted = $false
$bak = $null
if ($ResetOptions) {
  New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null
  $bak = Join-Path $BackupDir 'ElectricField-options-backup.reg'
  if (Test-Path -LiteralPath $bak) { Remove-Item -LiteralPath $bak -Force }
  $keyExisted = ((Invoke-Reg query $regKey) -eq 0)
  if ($keyExisted) {
    $rc = Invoke-Reg export $regKey $bak /y
    if ($rc -ne 0 -or -not (Test-Path -LiteralPath $bak)) { throw "could not back up $regKey to $bak (reg export exit $rc)" }
    [void](Invoke-Reg delete $regKey /f)      # -> Options::Options() defaults apply on next start
    "options reset (backup: $bak)"
  } else {
    "options key not present: defaults already apply"
  }
}

try {
  foreach ($idx in $Presets) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $Exe -PassThru
    try {
      # 1. MFC shows the "New" document chooser at start-up (9 doc templates). Classic list box id 100, OK id 1.
      $dlg = Wait-For { Find-Top $p.Id | Where-Object { (Get-Cls $_) -eq '#32770' -and (Get-Txt $_) -eq 'New' } | Select-Object -First 1 } 30 'New dialog'
      $lb = [W]::GetDlgItem($dlg, 100)
      $count = [int][W]::SendMessage($lb, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero)               # LB_GETCOUNT
      if ($count -ne 9) { throw "expected 9 templates, got $count" }
      $sb = New-Object System.Text.StringBuilder 256; [void][W]::SendMessage($lb, 0x0189, [IntPtr]$idx, $sb)   # LB_GETTEXT
      if ($sb.ToString() -ne $expected[$idx].name) { throw "list item $idx is '$($sb.ToString())', expected '$($expected[$idx].name)'" }
      [void][W]::SendMessage($lb, 0x0186, [IntPtr]$idx, [IntPtr]::Zero)                         # LB_SETCURSEL
      [void][W]::SendMessage([W]::GetDlgItem($dlg, 1), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) # BM_CLICK(OK)

      # 2. main frame, leave the maximized state, size it until the VIEW's client area is ClientW x ClientH
      $main = Wait-For { Find-Top $p.Id | Where-Object { (Get-Txt $_) -like '*Electric Field*' -and (Get-Cls $_) -like 'Afx:*' } | Select-Object -First 1 } 30 'main frame'
      [void][W]::ShowWindow($main, 9)                                                           # SW_RESTORE
      [void][W]::SetWindowPos($main, [IntPtr]::Zero, 20, 20, 1400, 1000, 0x0004)                # SWP_NOZORDER
      $view = Wait-For { Find-Desc $main | Where-Object { (Get-Cls $_) -eq 'AfxFrameOrView140u' } | Select-Object -First 1 } 15 'view'   # MFC unicode CView class
      for ($i = 0; $i -lt 5; $i++) {
        Start-Sleep -Milliseconds 400
        $vr = New-Object W+RECT; [void][W]::GetClientRect($view, [ref]$vr)
        $dw = $ClientW - ($vr.R - $vr.L); $dh = $ClientH - ($vr.B - $vr.T)
        if ($dw -eq 0 -and $dh -eq 0) { break }
        $mr = New-Object W+RECT; [void][W]::GetWindowRect($main, [ref]$mr)
        [void][W]::SetWindowPos($main, [IntPtr]::Zero, 20, 20, ($mr.R - $mr.L + $dw), ($mr.B - $mr.T + $dh), 0x0004)
      }
      $vr = New-Object W+RECT; [void][W]::GetClientRect($view, [ref]$vr)
      if (($vr.R - $vr.L) -ne $ClientW -or ($vr.B - $vr.T) -ne $ClientH) { throw "could not size the view to ${ClientW}x${ClientH} (got $($vr.R - $vr.L)x$($vr.B - $vr.T))" }

      # 3. the view paints lines only once ALL worker threads are done (OnTimer -> GetData): wait for lines + 2 s of stable pixels
      $last = ''; $stable = [Diagnostics.Stopwatch]::StartNew(); $wait = [Diagnostics.Stopwatch]::StartNew(); $done = $false; $dark = 0
      while ($wait.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $b = Grab $view
        if ($null -eq $b) { Start-Sleep -Milliseconds 500; continue }   # no frame this round: retry until the timeout
        $dark = [W]::Dark($b); $h = Hash $b; $b.Dispose()
        if ($h -ne $last) { $last = $h; $stable.Restart() } elseif ($dark -gt 1500 -and $stable.Elapsed.TotalSeconds -ge 2.0) { $done = $true; break }
        Start-Sleep -Milliseconds 500
      }
      if (-not $done) { throw "not finished within $TimeoutSec s (dark pixels: $dark) - human-assisted fallback needed" }

      # 4. capture
      $b = Grab $view
      if ($null -eq $b) { throw "PrintWindow failed for $($expected[$idx].slug)" }
      $file = Join-Path $OutDir ("{0}.png" -f $expected[$idx].slug)
      $b.Save($file, [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
      "{0}: {1}x{2} dark={3} total={4}s -> {5}" -f $expected[$idx].slug, $ClientW, $ClientH, $dark, [math]::Round($sw.Elapsed.TotalSeconds, 1), $file
    }
    finally { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }   # only the process this script started
  }
}
finally {
  if ($ResetOptions) {
    Start-Sleep -Milliseconds 300                       # let a just-killed app instance finish any registry write
    [void](Invoke-Reg delete $regKey /f)                # drop whatever the app recreated during the run
    if ($keyExisted) {
      $rc = Invoke-Reg import $bak
      if ($rc -ne 0) { Write-Warning "reg import failed (exit $rc): restore manually with: reg import `"$bak`"" } else { "options restored" }
    } else {
      "options key removed (did not exist before)"
    }
  }
}
