# Native loader prototype

**The diagnostic loader successfully loaded PIP.Core.dll into the running Zombies Aren't Real process on 2026-11-17. Evacuation is still not active: no simulation hooks are installed yet.**

This proves that lack of an official plugin API is not an absolute blocker. A custom native bridge is technically viable. It requires further reverse engineering of the game's simulation functions and data ownership.

## What was demonstrated

- Windows x64 launcher verifies the installed executable's full SHA-256 before it opens the target for attachment.
- It finds only the specifically named game at its expected Steam path. There is no arbitrary-PID or arbitrary-DLL option.
- An explicit `--attach` loads the project-built native bootstrap into that running process, then calls its exported initializer on a temporary thread.
- The bootstrap has no DLL entry point. Initialization occurs through an explicit call, outside the Windows loader lock.
- It verifies PE timestamp/image size plus the actual in-memory Odin runtime type layouts of `Gate`, `Safe_Cells`, and `Home_Field`.
- It loads `PIP.Core.dll` from the same private session directory and verifies the engine's exported C ABI version.
- It writes a diagnostic log and returns. It does not modify executable instructions, access live civilian arrays, call game simulation procedures, or install update callbacks.
- The real game remained responsive after the test. Closing the game unloads the modules. No launch setting, proxy DLL, service or persistent auto-loader is installed.

The launcher writes its context and paths into newly allocated memory in the game process and uses Windows `CreateRemoteThread`/`LoadLibraryW`. This is explicit native process attachment; it is not a supported game feature. The native bootstrap then performs read-only checks on the game's static type records. Development DLLs are copied to unique session directories before loading so normal build outputs remain replaceable while the game is running.

## Supported build

Steam build **26692741**, executable SHA-256:

```text
E297BC7DB1174EB67116B2B8387B8640554E1BD46598D8D3178091D9DF19B9A9
```

PE timestamp `0x6AFA6A16`, image size `8785920`, preferred base `0x140000000`. RVAs are relative to the actual loaded module, so ASLR is accounted for. The launcher refuses an updated executable rather than guessing new offsets.

The image has no COFF symbols, exported game API, CLR header or debug directory. It does contain a `.odinti` section. Its type records match the **Odin dev-2026-10** inline `Type_Info` representation, not the newer layout in current Odin master.

| Verified named type | Type record RVA | Struct size | Fields |
| --- | --- | --- | --- |
| `sim.Gate` | `0x7D2830` | 32 | 4 |
| `sim.Safe_Cells` | `0x7C96F0` | 112 | 6 |
| `sim.Home_Field` | `0x7C7690` | 560 | 12 |
| `sim.Evac_Field` | `0x7C7FC0` | 88 | 8 |
| `sim.Barricade` | `0x7D5150` | 192 | 13 |

`scripts/Inspect-Odin.cjs` reads the executable on disk and recovers 314 named type records. An optional JSON destination contains the full locally recovered metadata. It never opens or changes the game process. Types not present in RTTI still require disassembly. Strings such as `World`, `Civilian` and `SafeZone` can also be enum labels: the presence of their names alone does not establish a recovered struct layout.

## Build and use

Build the policy engine first, then the loader:

```powershell
.\scripts\Build.ps1
.\scripts\Build-Loader.ps1
```

`Build-Loader.ps1` compiles the native bootstrap and .NET 9 x64 launcher, then runs a local self-test. LLVM Clang, LLD and llvm-dlltool are required; the script discovers Unity's Android NDK tools or accepts `-Clang` and `-Linker` paths.

```powershell
$repo = 'C:\Users\georo\Documents\GitHub\PIP-PeopleImprovementProject'
& "$repo\build\loader\PIP.Loader.exe" --self-test $repo
& "$repo\build\loader\PIP.Loader.exe" --probe $repo
& "$repo\build\loader\PIP.Loader.exe" --attach $repo
```

`--probe` only verifies the running game. `--attach` is the explicit loading step. Start the game normally in Steam first. A second attach reports an already loaded bootstrap. Restarting the game returns to an unloaded session.

Logs are `build/PIP.Loader.selftest.log` and `build/PIP.Loader.log`. Unknown builds, invalid arguments and layout mismatches fail without installing any gameplay hook. If a thread takes longer than ten seconds, the launcher reports the timeout and retains its argument buffer rather than freeing memory that a running thread may still use.

```powershell
node .\scripts\Inspect-Odin.cjs 'C:\Program Files (x86)\Steam\steamapps\common\Zombies-Arent-Real\zombies-arent-real.exe' .\build\odin-layouts.local.json
```

## Remaining work

Locate the real simulation update, world storage, safe-region membership/breach state, civilian structure-of-arrays storage, and gate/path queries. Confirm calling conventions, thread ownership, and lifecycle by disassembly and controlled observations. Then implement hooks at a verified simulation-thread point and adapt those real objects to `src/pip.h`. Do not call an arbitrary Odin procedure from the loader thread: Odin context and simulation synchronization are not established by a successful DLL load.

No decompiled game source or executable is shipped in this repository. Workshop code loading, gameplay, saves, co-op and large-crowd performance remain unverified. A functional local loader does not by itself make a Workshop subscription load native code.

References: [Odin dev-2026-10 runtime type definitions](https://github.com/odin-lang/Odin/blob/dev-2026-10/base/runtime/core.odin), [Windows DLL initialization guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices), [CreateRemoteThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createremotethread).
