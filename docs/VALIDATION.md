# Validation record — 2026-11-18

## Native gameplay adapter

The runtime installs two guarded in-memory detours in Steam build 26692741. In a disposable Boston level 1 sandbox with two nested zones and an interior gate, it recorded **298 physical arrivals** in the intact inner compartment, **618,741 delivered movement proposals**, 20 native A* queries and zero reported adapter errors. Once no intact adjacent refuge remained, the steering count returned to zero. The user's original layered base was not used for the battle.

The final build, preserving each civilian's native speed and timestep, recorded **181 arrivals and 654,896 delivered proposals** in a three-zone layout. Disable reported enabled=0 immediately; re-enabling the same loaded DLL succeeded. Retrying the battle reset simulation time and rebuilt the navigation snapshot. The retry produced the same 181 arrivals and 654,896 proposals, for cumulative totals of 362 and 1,309,792, with zero reported adapter errors. Both gate connections initially passed native A*.

The first recorded arrivals had no further intact adjacent fallback in their physical compartment. This demonstrates arrival learning and no-fallback handling; it does not establish a full two-stage retreat in the live game. That sequence is covered by the native navigation test. These counts establish gate-crossing evacuation in this layout, not general collision certification for every map.

The native runtime self-test passes **21 navigation checks**, including physical walking through a gate, arrival learning, a second breach and retreat, strict adjacency, blocked gates and non-civilian exclusion. A duplicate native-A* endpoint regression was reproduced against the earlier navigation object and passes with the fix. Assembly tests exercise both the normal replay and forced collision-continuation branches while checking register/flags preservation. The loader builds with zero warnings/errors.

Full live multi-stage retreat, complex overlapping geometry, gate destruction mid-route, persistent saves, co-op and maximum crowd scale remain unverified. Workshop native-code distribution remains unverified. Policy and navigation tests use synthetic snapshots; they complement the separate live observation above.

## Policy core

Native x64 DLL built locally using LLVM Clang and LLD bundled with Unity 6000.3.9f1's Android NDK. This is a freestanding C DLL with no CLR header, no entry point, no runtime imports, and three exported ABI functions. The .NET 9 harness loads this actual DLL with `NativeLibrary.Load`.

**18 scenarios / 71 assertions passed**:

- Contingency learning in a healthy region, without movement or path queries.
- Shortest reachable adjacent safe destination after an outer breach.
- Repeated retreat with new contingency learning after physical arrival.
- Alternative route after a blocked gate.
- Closed and wrong-way portals rejected.
- Breached and inactive destinations excluded.
- Intact overlapping region remains sheltered.
- Destination breach causes immediate replanning.
- Outside transit retains its origin until actual safe arrival.
- No-route fallback and bounded recovery retry.
- Path closures rechecked within 0.5 simulation seconds.
- Distant regions across breached intermediates never selected.
- Outside people, soldiers, dead and infected people stay under vanilla control.
- Threat-weighted scoring and deterministic ties.
- NaN, infinite and negative path lengths rejected.
- Deleted and repaired regions release overrides.
- Invalid ABI, null buffers and invalid times fail closed.
- Reset erases session knowledge.

The draft package command rebuilds and reruns the tests. Normal Workshop packaging remains guarded by gameplay acceptance and subscriber code-loading flags.

Artwork: exported PNG visually inspected at 1276 x 716. The PSD was read back to confirm its dimensions, 20 layers, and 12 editable text layers. Photoshop's scripting API could read the original DDI template, but failed while creating new text layers, so the final PSD was authored with ag-psd and canvas. The DDI source file was not modified. First-open text-layer refresh may be requested by Photoshop.

At the earlier policy-only milestone, game loading, AI control and native pathfinding were unverified. The native adapter observation above supersedes that limitation; it does not certify saves, full-scale performance, co-op or Workshop loading.


Artwork revision: replaced the grid and framed screenshot with a generated mineral-texture gradient and a transparent white civilian running while looking back. Revised PSD read-back confirms 13 layers and 10 editable text layers at 1276 x 716. The figure is a 3D-style render, not a mesh. Preview visually checked after export.


Grounding/alignment revision: added a separate soft ambient/contact shadow layer under the unchanged civilian asset. The E, M and R word endings all begin at x=171. PSD read-back confirms 14 layers, 10 editable text layers and the shared x coordinate. Civilian PNG SHA-256 is unchanged: 8938D28CE2BD873BE1AF41B863DF3E2FF4C5325D66974A4791255428CA956D72.


Earlier diagnostic loader milestone: built with zero warnings/errors; self-test passed null argument, ABI size, unsupported-host and core ABI-handshake checks. The launcher verified the installed game SHA-256, attached the diagnostic bootstrap, passed actual in-memory Gate/Safe_Cells/Home_Field checks, and loaded PIP.Core.dll with C ABI version 1. That diagnostic mode installs no simulation hooks. Evacuation now uses the separate runtime adapter described above. Logs remain local under build/.
