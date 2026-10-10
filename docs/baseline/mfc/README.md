# MFC visual baseline (QA-05)

Reference images of the original MFC application's rendering of all 9 built-in presets. They are the visual oracle
for the cross-platform port: later phases compare their own output against these files. They were captured from the
unmodified MFC sources before any MFC code was removed.

## Provenance

| Item | Value |
|------|-------|
| Capture date | 2026-10-10 |
| Toolchain | Visual Studio 18 Insiders, MSVC 14.51.36231, PlatformToolset v145 (committed in `ElectricField/ElectricField.vcxproj`) |
| Configuration | Release, x64 |
| Windows | 10.0.26100.9278 (Windows 11) |
| System DPI | 96 (100% display scaling); the script warns and the baseline must not be captured at another scale |
| View client size | 1024x768 pixels (the main frame is resized until the view client area is exactly this) |
| Capture method | `PrintWindow` with `PW_CLIENTONLY \| PW_RENDERFULLCONTENT` on the view window, driven by Win32 messages (`LB_SETCURSEL`, `BM_CLICK`); no mouse or keyboard input is simulated |
| Completion detection | the view only paints lines when all worker threads are done; the script waits for more than 1500 dark pixels and 2 s of unchanged image (timeout 180 s, then it throws and writes no PNG) |

The MFC sources were not edited; the build writes only into a folder under the user temp directory.

### Build command

```powershell
$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -products * -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
& $msb ElectricField.sln -p:Configuration=Release -p:Platform=x64 "-p:OutDir=$env:TEMP\ef-mfc-baseline\out\" "-p:IntDir=$env:TEMP\ef-mfc-baseline\obj\" -m -nologo -v:m
```

### Capture command

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\baseline\capture-mfc-baseline.ps1 `
    -Exe "$env:TEMP\ef-mfc-baseline\out\ElectricField.exe" -OutDir docs\baseline\mfc -PresetList "0,1,2,3,4,5,6,7,8" -ResetOptions
```

`-ResetOptions` exports the app's options registry key (`HKCU\Software\Free App\Electric Field\options`) to
`%TEMP%\ElectricField-options-backup.reg` (outside the repository), deletes it so the built-in defaults apply, and
restores it when the script finishes, also when a preset fails. The `Settings` and `Workspace` keys are never touched.
The script only stops the `ElectricField.exe` process it started itself.

## Options used (defaults, `ElectricField/Options.cpp:8-23`)

- 16 field lines per unit charge, equipotentials on, potential interval 0.1
- 4 worker threads, Euler integration
- positive charge blue RGB(0,0,255), negative charge red RGB(255,0,0)
- field lines black RGB(0,0,0), equipotentials grey RGB(192,192,192), both 1 px thick
- 100 px per distance unit, charge radius 25 px

## Presets (MFC New-chooser order)

| # | File | Chooser entry (display name) | Charges (x position in distance units: charge) |
|---|------|------------------------------|-----------------------------------------------|
| 1 | `dipole.png` | Dipole | -2: +1, +2: -1 (y = 0) |
| 2 | `two-equal-charges.png` | Two Equal Charges | -2: +1, +2: +1 (y = 0) |
| 3 | `two-unequal-charges-same-sign.png` | Two Unequal Charges Same Sign | -2: +1, +2: +2 (y = 0) |
| 4 | `two-unequal-charges-different-signs.png` | Two Unequal Charges Different Signs | -2: +2, +2: -1 (y = 0) |
| 5 | `quadrupole.png` | Quadrupole | (-1.5,-1.5): +1, (1.5,-1.5): -1, (-1.5,1.5): -1, (1.5,1.5): +1 |
| 6 | `four-charges-same-sign.png` | Four Charges Same Sign | -3, -1, +1, +3: +1 each (y = 0) |
| 7 | `two-dipoles-same-direction.png` | Two Dipoles Same Direction | -3: +1, -1: -1, +1: +1, +3: -1 (y = 0) |
| 8 | `two-dipoles-opposed-direction.png` | Two Dipoles Oposed Direction | -3: +1, -1: -1, +1: -1, +3: +1 (y = 0) |
| 9 | `parallel-dipoles.png` | Parallel Dipoles | (-1.5,-1.5): +1, (1.5,-1.5): -1, (-1.5,1.5): +1, (1.5,1.5): -1 |

The File-New chooser string of preset 8 contains a typo in the `.rc` resource: it reads `Two Dipoles Oposed Direction`
(the document name is "Two Dipoles Opposed Direction"). The capture script verifies each chooser entry text
(including this typo) before selecting it, so the file names always match the preset that was captured.

## Reproducibility

Three presets were captured a second time with the same script (`-PresetList "4,5,6"`) and compared with the committed
images using `magick compare -metric AE -fuzz 10%` (ImageMagick 7.1.2). All three pass with exit code 0. "AE raw" is
the number of pixels that differ in any channel without a fuzz tolerance (a few pixels may differ
because thread scheduling affects the equipotential de-duplication; with the fuzz it is 0).

- quadrupole: AE fuzz 10% = 0; AE raw = 0
- four-charges-same-sign: AE fuzz 10% = 0; AE raw = 0
- two-dipoles-same-direction: AE fuzz 10% = 0; AE raw = 3

Later phases must therefore compare against these images with a fuzz tolerance, never byte equality.
