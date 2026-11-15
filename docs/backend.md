# Process matching and scheduling

The monitor runs one low-priority worker. It takes a Windows process snapshot at
the configured interval (three seconds by default), sleeps between snapshots,
and caches process handles, full image paths, creation times, and match results.
Known processes are rematched only after settings or installed-game metadata
changes. Settings are not written during scans. There is no polling network
client, telemetry, console output, toast, or runtime error dialog from the engine.

## Recognition precedence

1. An excluded full executable path wins over every other rule. Core Windows
   service-host and logon/security processes are never targets.
2. An explicitly selected full executable path is matched exactly, independent
   of its filename elsewhere. This can intentionally re-add a disabled game.
3. Reviewed catalog executable aliases and path hints identify default games or
   the optional exact game-service allowlist. Disabling a catalog ID also blocks
   that game's installed-root recognition.
4. Installed Steam roots use appmanifest AppIDs. Catalog games are recognized
   directly. Additional installed titles must be classified as `game` by the
   local Steam appinfo cache. A Steam library may contain software and tools, so
   mere installation is not treated as evidence of a game. Explicit known
   non-game AppIDs for Wallpaper Engine, Crosshair X, OBS, and Soundpad are excluded.
5. Epic manifests need a catalog match or an explicit game category. Ambiguous
   application manifests are skipped. Installer/redist, crash reporter, browser,
   launcher/web-helper, and core system executables are excluded from automatic
   root matching.

Steam/Epic discovery is cached for five minutes and reads local fixed/removable
volumes only; UNC paths and network-mapped libraries are skipped. Parsers enforce input-size,
record-count, nesting, and path-boundary limits. A corrupt or unsupported cache
produces no speculative matches. The curated catalog and explicitly selected
programs continue to work. Autodetection is therefore based on available local
metadata, not a claim to identify every possible standalone or newly released
game from an arbitrary executable name.

## Realtime and affinity

For an identified target, the worker opens only the documented process rights,
requests `REALTIME_PRIORITY_CLASS`, and reads the actual class back. It enables
the existing increase-base-priority privilege in its own token when that
privilege is available. It does not enable debug privilege, change credentials,
inject code, install a driver, bypass a protected process, or modify an anticheat.
Rejected operations are quiet and retried at a bounded rate. There is no fallback
to High and no successful count based only on an attempted API call.

On systems with a single processor group, the app uses the allowed system mask
reported by `GetProcessAffinityMask` when widening process affinity. It also
clears a nonempty process-default CPU-set selection, permitting every
scheduler-allowed CPU set. Current values are queried first, so unchanged
Realtime priority or already unrestricted affinity is not written again every
scan.

On systems with multiple processor groups, it does **not** apply a legacy
single-group process mask. Windows 11 and Windows Server 2022 normally let
processes span all groups; a legacy mask can accidentally restrict them to one
group. Clearing process-default CPU-set selection preserves that default. An
existing explicit per-thread hard affinity/group restriction, a job-object
constraint, protected process, or Windows scheduling policy cannot universally
be removed by a process-level mask. Those constraints are respected rather than
claiming unsupported control over every logical processor.

The tray count includes only processes with an opened management handle whose
current priority actually reads as Realtime. Protected/denied processes do not
produce warnings and are not counted as successful changes. The UI can still
use readable process paths for its program picker.

## Restoration and process identity

Original priority, affinity, and CPU-set selection are retained for each managed
process. Removing/disabling a target or quitting restores a changed property
only if the process is still the same instance and that property still equals
the app's last applied value. If another tool leaves a different value, it is
not overwritten during restoration. PID reuse is checked with process handles
and creation time. No other process with the same recycled PID is altered.

Quitting signals the worker, cancels synchronous I/O where Windows supports it,
and waits up to five seconds for the worker. Records are stored separately from
discovery, so an unresponsive metadata read cannot prevent conditional restoration
of already managed processes. A late discovery result sees the stopped state and
cannot start managing another process. Uncooperative native process operations
may still prevent a particular restoration; restoration is best effort.

This state is in memory, so forced termination, a crash, or power loss cannot
run normal restoration. The app does not install a second watchdog process or
persist privileged scheduling rules. A game process ending naturally removes
its scheduling state with it.

## Verification

Portable tests cover exact-path matching, path boundaries, disabled defaults,
exclusions, game services, known and metadata-classified game roots, Steam/Epic
manifest parsing, and malformed inputs. A separate Windows test launches a
unique disposable helper that waits on an event. Only this helper is selected;
all catalog targets and extra game detection are disabled for that test. The
test reads actual Realtime priority, all allowed process affinity, CPU-set
clearing, tray-count data, and restoration on exclusion/quit. It also verifies
that a different priority left by another tool is preserved.

CI's processor-group count is recorded in test output. Tests on a single-group
runner cannot establish actual behavior on a machine with more than 64 logical
processors; the multi-group code deliberately avoids a narrowing legacy mask.

## Primary Windows references

- [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass)
- [Process security and access rights](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights)
- [GetProcessAffinityMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessaffinitymask)
- [SetProcessAffinityMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setprocessaffinitymask)
- [Processor groups](https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups)
- [GetProcessDefaultCpuSets](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessdefaultcpusets)
- [SetProcessDefaultCpuSets](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessdefaultcpusets)
