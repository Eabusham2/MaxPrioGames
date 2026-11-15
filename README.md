# MaxPrioGames

[![Windows build, verify, and release](https://github.com/Eabusham2/MaxPrioGames/actions/workflows/release.yml/badge.svg)](https://github.com/Eabusham2/MaxPrioGames/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/Eabusham2/MaxPrioGames)](https://github.com/Eabusham2/MaxPrioGames/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A small, silent Windows tray app that automatically applies **Realtime process priority** and the widest supported **CPU affinity** to recognized games, known game services, and programs you select.

Written in **C++17 using native Win32**. One executable, one sleeping monitor thread, no web UI, no bundled runtime, no driver, no background service, no account, and no network traffic.

## Download

**[Download the latest verified release](https://github.com/Eabusham2/MaxPrioGames/releases/latest)**

| Download | Behavior |
| --- | --- |
| `MaxPrioGames-<version>-windows-x64-setup.exe` | Installs the app, registers startup, creates a searchable Start menu entry, and launches it in the tray. No desktop shortcut. |
| `MaxPrioGames-<version>-windows-x64-portable.exe` | Put the EXE where you want. The first manual launch opens the small settings window. Use **Add to startup** to register that exact location, or **Remove from startup** to undo it. |

Requires Windows 10 version 1809 or later, or Windows 11, x64. The executable requests administrator rights so Windows can permit changes to eligible game processes. The downloads are unsigned; Windows may show its normal publisher/SmartScreen prompt.

## What it does

- Scans running processes every **3 seconds** by default; adjust the interval from **1–60 seconds**.
- Automatically requests Realtime priority for matched processes. Windows-denied or unsupported changes are skipped silently.
- Expands affinity using supported Windows APIs, respecting processor groups and OS restrictions.
- Shows the number of **managed processes actually at Realtime** in the tray icon. Failed attempts do not increase the count.
- Includes a reviewed catalog of **110 games and 10 game-service families**, plus installed-game discovery.
- Lets you add an EXE, select an open window, disable a default entry, or remove a custom/discovered program.
- Saves changes locally and restores the previous priority/affinity on removal or a normal exit where the process still has the values MaxPrioGames applied.

There are **no automatic popups, warnings, sounds, permission-denial notifications, or confirmation dialogs**. Realtime is the default behavior. The main window shows the program list and successful Realtime counts only when you open it.

## Tray and window controls

| Action | Result |
| --- | --- |
| Left-click the tray icon | Open the main window. |
| Right-click the tray icon | **Open** at the top, visible open windows in the middle, **Close** at the bottom. |
| Select a window in that menu | Add its actual executable path, or re-enable an existing default rule. |
| Middle-click the tray icon | Quit. |
| Click a program's checkbox | Enable or disable its rule. |
| **Add EXE** | Select another executable to manage whenever it runs. |
| **Remove** | Disable a built-in entry, or remove and exclude an added/detected executable. |
| Minimize | Minimize normally to the taskbar. |
| Window **X**, Escape, or **To tray** | Hide the window and keep monitoring. |
| **Quit** | Stop monitoring, restore changes where possible, and remove the tray icon. |

The window has no maximize button. Its searchable list includes default games, game services, added programs, and currently running automatically detected games. Multiple windows from the same program remain selectable in the tray menu; rules are stored once per executable path.

The gray tray icon means **0** managed Realtime processes; a green icon shows a nonzero count. The icon is rebuilt only when that number changes.

## Startup and single instance

The installer registers a per-user Task Scheduler logon task with the exact installed EXE path and `--startup`. It launches the app immediately in the tray. The portable version exposes the startup button in its main window; moving that EXE requires registering its new location.

After the portable EXE's first manual launch, ordinary launches also go straight to the tray. Every automatic startup launch goes to the tray. A second launch exits without creating another monitor; there is one instance per interactive Windows session. Use the tray icon to open the running instance, or the optional `--show` command.

A Start menu entry is necessary for normal Windows app search. It is the only shortcut the installer creates. Settings are stored under `%LOCALAPPDATA%\MaxPrioGames`; uninstall removes the app, startup task, and Start menu entry while keeping your preferences.

Startup registration belongs to the account running the app. For an account that supplies a different administrator's credentials at UAC, Windows runs the program as that administrator; register startup from the intended administrator account.

## Game coverage — November 2026

The catalog was reviewed on **15 November 2026** against [Valve's live top-100 Most Played chart](https://store.steampowered.com/charts/mostplayed), and the linked Steam pages were checked for app identities. Four non-game utilities were excluded: Wallpaper Engine, Crosshair X, OBS Studio, and Soundpad.

The remaining **96 Steam games** are joined by **14 additional major PC games/editions**, including Fortnite, VALORANT, League of Legends, Minecraft Java and Bedrock, Roblox, World of Warcraft, Diablo IV, Hearthstone, StarCraft II, Genshin Impact, Honkai: Star Rail, Zenless Zone Zero, and Wuthering Waves.

This is a coverage catalog based on a dated Steam chart plus selected major games elsewhere. It is not presented as an invented global ranking across incompatible storefront metrics.

**[Browse all games, sources, executable aliases, and service families](docs/game-catalog.md).**

Automatic recognition uses:

1. Exact catalog executable names, with game-specific path checks for generic names.
2. Installed Steam app IDs and their manifest directories.
3. Local Steam metadata identifying additional installed apps as games.
4. Epic installation metadata explicitly identifying a game, or matching a known catalog entry.
5. Executable paths you explicitly add.

The monitor does not classify every arbitrary EXE as a game. Unknown games without usable local metadata can be added from their open window or with **Add EXE**. Installers, crash reporters, redistributables, browser helpers, general launchers, shared system-service hosts, and the excluded utilities are kept out of automatic matching. Turning off **Include game-related services** excludes that category.

No game is downloaded, launched, patched, or contacted during detection. The bundled catalog is compiled into the EXE; no extra JSON file is needed beside the portable download.

## Priority, affinity, and compatibility

The app uses [SetPriorityClass](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass), checks the resulting class, and counts only confirmed Realtime processes. It does not substitute a lower priority and label it Realtime. Protected games or anti-cheat services can refuse access; that is a silent skip.

On ordinary systems with a single processor group, affinity is expanded to the available system mask and process CPU-set restrictions are cleared when permitted. On systems with multiple processor groups, the monitor preserves Windows' all-group scheduling instead of calling the legacy mask API in a way that could confine a game to the first 64 logical CPUs. It does not rewrite per-thread hard group assignments or bypass job/OS restrictions. See [Microsoft's processor-group documentation](https://learn.microsoft.com/windows/win32/procthread/processor-groups).

Realtime changes scheduling order; it is not a guarantee of higher FPS or lower latency. [Microsoft documents that sustained Realtime work can delay input and disk-flush threads](https://learn.microsoft.com/windows/win32/procthread/scheduling-priorities). MaxPrioGames keeps this behavior silent at runtime as intended.

No kernel driver, injection, SeDebugPrivilege, service tampering, anti-cheat bypass, persistent process-priority registry override, or power-plan modification is used. Normal shutdown restores settings only when they still match this app's last applied values. A forced process kill or power loss cannot run that cleanup.

## Footprint

Each release includes **FOOTPRINT.md** and **footprint.json** measured on the Windows CI runner using the actual hidden tray process and the real monitor worker:

- A 50-second run with a five-second warmup.
- A 40-second external CPU/private-memory/working-set sample.
- Process scan count, actual Realtime count, handles, threads, binary size, installer size, and installed file size.
- Native UI, startup, and installer evidence alongside SHA-256 checksums.

**[Open the latest measured footprint](https://github.com/Eabusham2/MaxPrioGames/releases/latest/download/FOOTPRINT.md).**

There is no polling web view, continuous animation, busy-wait, or always-running high-frequency GUI timer. Native controls are created when the settings window is first opened. The monitor sleeps between scans, caches process identities, and refreshes installation metadata infrequently. Idle CI results do not prove identical game performance on every PC; active-process counts, the game library, and OS behavior affect the result.

## Build and verification

Use Visual Studio 2022 with Desktop development with C++, CMake, and a Windows SDK:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./scripts/Test-Windows.ps1 -Executable ./build/Release/MaxPrioGames.exe
./scripts/Measure-Footprint.ps1 -Executable ./build/Release/MaxPrioGames.exe
```

The Windows-only integration test changes priority and affinity on a disposable sleeping helper process, verifies the values, then checks restoration and exclusions. It does not promote unrelated CI processes. Portable policy/parser/catalog tests also build on Linux.

There is one development branch: **main**. On each main push, CI builds a size-optimized native binary with the static C++ runtime, validates the catalog, runs unit and real Windows integration tests, measures footprint, packages the portable EXE and Inno Setup installer, and verifies install/uninstall behavior. A separate job publishes an immutable **v1.0.<run number>** release only after all those gates succeed. Pull requests validate without publishing releases.

Optional commands:

```text
MaxPrioGames.exe --show
MaxPrioGames.exe --quit
MaxPrioGames.exe --enable-startup
MaxPrioGames.exe --disable-startup
MaxPrioGames.exe --smoke-test report.json
MaxPrioGames.exe --benchmark 50 report.json
```

The last two are explicit test/measurement modes, not normal runtime logging.

## Related app

[DriveTempMon](https://github.com/Eabusham2/DriveTempMon) is the companion native Windows tray monitor for HDD/SSD temperatures.

## License

[MIT](LICENSE). Game names belong to their respective owners. MaxPrioGames is an independent utility.
