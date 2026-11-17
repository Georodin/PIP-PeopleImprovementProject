# Native evacuation adapter

PIP uses an experimental native x64 adapter for **Steam build 26692741**. It hooks simulation preparation and civilian movement, reads the real safe-zone masks and gates, and calls the game's A* pathfinder for gate crossings. The original executable on disk is unchanged. The game is written in Odin; Unity and BepInEx are not dependencies.

## Enable and disable

Build once with .NET SDK 9 and LLVM Clang/LLD/llvm-dlltool:

```powershell
.\scripts\Build.ps1
.\scripts\Build-Loader.ps1
```

Start the game normally in Steam, then double-click `Enable-PIP.cmd`. Repeat after restarting the game. `Disable-PIP.cmd` immediately releases steering. The same build can be enabled again; a changed DLL requires restarting the game.

```powershell
$repo = 'C:\Users\georo\Documents\GitHub\PIP-PeopleImprovementProject'
& "$repo\build\loader\PIP.Loader.exe" --enable $repo
& "$repo\build\loader\PIP.Loader.exe" --status $repo
& "$repo\build\loader\PIP.Loader.exe" --disable $repo
```

`--probe` verifies the running executable without attaching. The older `--attach` command loads only the diagnostic bootstrap; use `--enable` for evacuation. Logs are in `build/PIP.Runtime.log`. Arrival entries record the newly learned region and next fallback (zero means no intact adjacent fallback remains).

The launcher verifies the full executable SHA-256 before every attachment:

```text
E297BC7DB1174EB67116B2B8387B8640554E1BD46598D8D3178091D9DF19B9A9
```

It targets only the installed Steam executable at the documented local path. A developer-only `--isolated-test` switch selects the exact disposable copy under the game's `moddev/pip-native-investigation/playtest/` directory, with the same fingerprint. There are no arbitrary-PID or arbitrary-DLL options. An updated game is refused until its layout and hooks have been checked.

## How movement works

1. Before the native movement workers start, copy walkability within the safe-mask bounds. Read `Safe_Cells.inside` and `Safe_Cells.calm`; a safe interior whose calm bit is clear is breached.
2. Flood-fill physically connected compartments while treating gates as boundaries. Buildings, water and non-gate walls are excluded. Disconnected streets remain separate so an unreachable population is not promised a route.
3. Find each gate's two sides using its tangent. Interior gates can have a zero native `inward` vector. A native A* query validates the crossing; reject paths that leave the two compartments or use another connection.
4. Build a shared cardinal flow field from reachable gates leading to intact neighbors. This avoids running A* for every person. The local policy calls `pip_step` using real position and stable native ID plus one (zero is reserved in PIP's ABI).
5. The movement hook supplies a proposed step and direction for healthy evacuating civilians only. It preserves their native speed and timestep and sends every modified proposal through the game's final wall/gate traversal check. Positions are integrated normally, with no teleport.
6. Physical entry into an intact compartment ends steering and teaches its next fallback. Destination breaches, gate removal, no-route cases, infected people and other unit kinds release the override. Knowledge is reconstructed after a new battle or retry; it is not written into game saves.

All navigation allocation, native A* calls and policy updates occur before worker dispatch. Workers read the completed snapshot and their own stable-ID state. No heap allocation, game path query or logging happens in a movement hook. Shared geometry changes rebuild routes; unchanged component labels retain people’s knowledge.

## Build-specific integration points

| Item | RVA / layout |
| --- | --- |
| Movement dispatcher | `0x1D5370`, first 19 instruction bytes verified |
| Final movement branch | `0x228FFF`, first 18 instruction bytes verified |
| Native collision continuation | `0x2290BB` |
| Native A* | `0x115770`; called with the dispatcher's valid Odin context |
| Units storage | `0x6B1C80`, size 21248, alignment 128 |
| Buildings storage | `0x6B8200`, size 6912, alignment 128 |
| Native civilian panic | Units `contact` array at `0x140`; `timers` is not panic |
| Safe mask / gates | Buildings `0x1878` / `0x1908` / `0x1930` |

The runtime checks PE identity and full hook signatures. Installation prepares executable trampolines, suspends other game threads, relocates instruction pointers inside a replaced range, changes protection, writes the two detours, flushes the instruction cache, restores protection and resumes threads. The movement bridge preserves registers and flags; its normal and collision-continuation paths are exercised by native self-tests. DLL initialization is explicit, outside the loader lock.

The launcher copies DLLs into private session directories so builds remain replaceable. Hooks stay dormant after disabling and disappear when the game exits. No startup service, Steam launch-option change or auto-loader is installed.

`scripts/Inspect-Odin.cjs` is a read-only local RTTI inspector for Odin dev-2026-10. It recovers 321 named records, including types aligned to 128 bytes. Game binaries, recovered disassembly and decompiled game source are not included in this repository or release packages.

## Limits

This is a local singleplayer experiment. Full-scale crowd performance, co-op, arbitrary map geometry and every game mechanic have not been certified. Navigation is capped at 8,388,608 cropped cells, 65,535 components, 4,096 connections and 4,194,304 stable IDs; invalid or oversized snapshots retain native movement.

Steam Workshop native-code loading remains unverified. Subscribing to an INI mutator does not install this launcher. Keep the Workshop item a draft until clean-install distribution and loading are proven.

References: [Odin runtime definitions](https://github.com/odin-lang/Odin/blob/dev-2026-10/base/runtime/core.odin), [Windows DLL initialization](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices), [thread contexts](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext).
