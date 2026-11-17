# PIP — People Improvement Project

![PIP preview](art/preview.png)

**Experimental local singleplayer mod for Zombies Aren't Real, Steam build 26692741.** PIP now hooks the simulation and civilian movement, reads actual safe compartments, and validates gate crossings with the game's A* pathfinder. A disposable live battle recorded 298 civilians reaching an intact inner zone. This is the first module in a broader civilian quality-of-life project.

## First feature: layered evacuation

The policy teaches residents a contingency when they enter an intact safe region. Once their region is breached, they try to reach a directly adjacent, unbreached region through a real, passable connection. Arrival teaches that region's next contingency, so a layered base can retreat repeatedly toward its remaining safe interior.

An intact overlap region remains safe even if an outer region is breached. Closed gates, one-way restrictions, walls, buildings and water require a valid path from the host. The policy never teleports civilians. If all adjacent destinations are breached or unreachable, it releases steering to the game's normal panic behaviour and retries later. Death, infection and non-civilian units also release the override.

## Current status

| Part | Status |
| --- | --- |
| Native x64 policy engine (`PIP.Core.dll`) | Implemented |
| Layered retreat, arrival knowledge, blocked routes and replanning | Tested through the DLL's C ABI |
| Native loader, simulation and collision-aware movement hooks | Implemented and exercised in the running game |
| Real safe masks, gate topology, native A* and shared route fields | Implemented |
| In-game evacuation | Observed in a disposable two-zone battle; broader acceptance testing remains |
| Repeated retreat and arrival learning | Native navigation tests pass; full live multi-stage acceptance remains |
| Save persistence, co-op and maximum crowd scale | Unsupported / unverified |
| Editable Photoshop design and preview | In `Photoshop/` and `art/` |
| Steam Workshop metadata, description and release checks | Prepared as a draft; upload blocked |

The core harness supplies synthetic policy inputs; native navigation tests exercise physical walking and repeated retreat. Separate live-game observations are recorded in [VALIDATION.md](docs/VALIDATION.md). Workshop subscription loading is still unverified.

## Play locally

Start the supported game normally in Steam, then run **`Enable-PIP.cmd`**. Repeat after every game restart. **`Disable-PIP.cmd`** releases steering immediately; the same build can be re-enabled. Rebuilds require restarting the game. The launcher requires the .NET 9 x64 runtime. It verifies the exact executable fingerprint before attaching and leaves the executable file and saves unchanged.

For a source checkout, build both DLLs and the launcher first:

```powershell
.\scripts\Build.ps1
.\scripts\Build-Loader.ps1
```

See [native adapter instructions](docs/NATIVE-LOADER.md) for status commands, implementation details and current limits. This is a local singleplayer experiment; use a disposable plan while testing.

## Build and test

On Windows x64 with .NET SDK 9 and LLVM Clang plus `ld.lld.exe`:

```powershell
.\scripts\Build.ps1
.\scripts\Build-Loader.ps1
```

The script can use the Clang/LLD binaries bundled with Unity's Android NDK, if that module is installed. Unity Editor is not used to build the game or the mod. Otherwise pass `-Clang` and `-Linker` with their full executable paths. No packages are installed and no game files or saves are changed.

Outputs include `build/PIP.Core.dll`, `build/loader/PIP.Runtime.dll` and `build/loader/PIP.Loader.exe`. The core has a plain C ABI and no runtime imports. `tests/` loads the actual native core DLL; the launcher self-test checks native navigation and the movement bridge's register/flags handling.

For optional artwork regeneration with Node.js, run `npm install` followed by `npm run preview`. These packages are authoring tools only. The committed PSD is already layered; Photoshop may ask to refresh text layers on first opening.

```powershell
.\scripts\Prepare-Workshop.ps1 -Draft
.\scripts\Prepare-Local.ps1
```

These create a Workshop development draft and a runnable local package under `dist/`; neither publishes anything. A normal Workshop package attempt remains blocked until subscriber code loading and broader gameplay acceptance are verified.

## Integration and release

See [the integration contract](docs/INTEGRATION.md), [installed-build findings](docs/COMPATIBILITY.md), and [Workshop release checklist](workshop/RELEASE.md). The [native adapter](docs/NATIVE-LOADER.md) uses explicit process attachment and two guarded memory hooks. No BepInEx, Harmony or executable-file patch is included. Future people-behaviour modules can be added under the same PIP name.

The name follows Georodin's [ACI](https://github.com/Georodin/ACI-AbilityCooldownIndicator) and [DDI](https://github.com/Georodin/DDI-DeepDrill-Indicator) acronym style. The new cover follows the DDI PSD's three-line typography and author credit, using a textured gradient and a generated white 3D-style civilian cutout; it does not reuse the separately licensed mining artwork or RimWorld expansion marks.

