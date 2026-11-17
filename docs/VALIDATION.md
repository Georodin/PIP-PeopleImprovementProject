# Validation record — 2026-11-17

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

The draft package command rebuilt and reran the tests. The normal Workshop package command was checked to fail with a clear missing-integration error.

Artwork: exported PNG visually inspected at 1276 x 716. The PSD was read back to confirm its dimensions, 20 layers, and 12 editable text layers. Photoshop's scripting API could read the original DDI template, but failed while creating new text layers, so the final PSD was authored with ag-psd and canvas. The DDI source file was not modified. First-open text-layer refresh may be requested by Photoshop.

**Not validated:** actual game loading, AI control inside the game, game pathfinding, saves, performance at full game scale, co-op, or Workshop loading. No gameplay bridge exists yet. The policy tests supply synthetic path results and region snapshots. There is no claim that this prototype changes gameplay.


Artwork revision: replaced the grid and framed screenshot with a generated mineral-texture gradient and a transparent white civilian running while looking back. Revised PSD read-back confirms 13 layers and 10 editable text layers at 1276 x 716. The figure is a 3D-style render, not a mesh. Preview visually checked after export.


Grounding/alignment revision: added a separate soft ambient/contact shadow layer under the unchanged civilian asset. The E, M and R word endings all begin at x=171. PSD read-back confirms 14 layers, 10 editable text layers and the shared x coordinate. Civilian PNG SHA-256 is unchanged: 8938D28CE2BD873BE1AF41B863DF3E2FF4C5325D66974A4791255428CA956D72.
