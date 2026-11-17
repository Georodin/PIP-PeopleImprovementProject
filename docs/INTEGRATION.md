# Policy and native bridge contract

`src/pip.h` defines **PIP's proposed ABI**. It is not an API present in Zombies Aren't Real. `PIP.Core.dll` exports `pip_abi_version`, `pip_reset`, and `pip_step`.

## What the game integration must provide

1. On a stable simulation thread, a snapshot of actual safe **regions**, not just the overlapping lasso polygons. Each region needs a stable nonzero ID, active flag, and its own breached flag.
2. Actual adjacent region connections as portals, with points inside each side and their allowed walking directions. Shared corners and touching drawings alone are not walking routes. No portal is invented across a wall.
3. A complete walking-path query from the civilian's current position through the chosen portal to an interior destination point. It must respect buildings, water, closed gates, one-way rules, and the current crowd/path system. A breached-origin departure may require a game change to the existing one-way gate rules. Do not simply return a straight-line distance.
4. Per-civilian stable IDs, physical region membership, position, alive/civilian/infected flags, and `PipState`. Outside any region is ID zero. Do not store state by a mutable array index.
5. An incrementing world revision after every breach, portal change, region edit, or repair. `sim_time` advances only with the simulation. Recommended retry interval: 0.5 simulation seconds.
6. A way to follow a returned route using the game's existing collision-aware movement. The decision's interior waypoint is a destination, not permission to directly set the person's position.

## Simulation loop

```text
world = snapshot actual regions and passable adjacent portals
for each relevant civilian:
    state = state for that stable civilian ID, or zero-initialized state
    result = pip_step(world, civilian, state)
    if API result is not PIP_OK:
        release PIP movement override
    else if decision is PIP_EVACUATE:
        follow the validated host path through decision.portal_id
    else:
        release PIP movement override (normal shelter/panic behaviour remains)
```

`PIP_SHELTERED` updates knowledge immediately after actual arrival. `PIP_NO_ROUTE` requests no movement override; the host must retain panic and collision behaviour and try again later. An already-travelling civilian outside all regions retains its breached origin, and can reroute to another adjacent safe destination. Arrival in any intact region ends evacuation and teaches that region's fallback. No safe neighbour means no fabricated destination.

The healthy-region contingency is a topology-based hint, not a promised walking path. At breach time every candidate is revalidated from the person's actual position. Candidate score is host walking length plus nonnegative host threat penalty, with portal ID as a deterministic tiebreaker. A decision is revalidated immediately on revision changes, otherwise at the retry interval. A cached path can become obstructed between checks: collision avoidance must remain active, and the host should invalidate the revision for structural changes.

## ABI and lifetime

Use normal 8-byte structure packing on x64. `PipWorld` is 72 bytes, `PipState` 64, `PipPortal` 48, `PipPerson` and `PipDecision` 32. Arrays, callback and context must remain valid throughout `pip_step`. IDs must be unique and nonzero; connections must refer to the current snapshot. The bridge owns all memory. Calls are allocation-free. The native library does not retain pointers or access the game process.

Never re-enter or mutate the snapshot from a path callback. Return exactly 1 only for a complete reachable path and write its finite nonnegative walking length. Return 0 otherwise. A missing callback fails closed. Reset all states on load, new battle, retry, ID reuse and simulation-time reset. Forget dead/removed civilians. Session knowledge is reconstructed from physical regions after a load; there is no on-disk state format yet.

Keep civilian iteration, portal IDs, path-query results and revisions deterministic for co-op. Multiplayer remains unsupported until both peers and authoritative-host behaviour are tested. For large crowds, schedule only residents of affected regions on breach, and spread active retries across frames. Benchmark a real 500,000-person map before making performance claims.

## Remaining acceptance tests in the game

Use a disposable Free Play copy of a layered plan. Breach only the outer region, watch civilians cross an actual working gate into the adjacent blue region, breach that second region, and verify another retreat. Repeat with overlapping polygons, blocked gates, no surviving neighbour, gate destruction mid-route, arrival at an unexpected safe region, save/reload, pause/speed changes, and a large crowd. Verify no position jumps, wall clipping, commandeered police/militia, or panic suppression when no route exists.



The implemented adapter in `runtime/` supplies this contract from the verified native build: safe-mask compartments, actual gates validated with native A*, shared walking fields, stable civilian IDs, and a movement proposal that passes through native wall/gate collision checks. State is prepared before workers run and reset on battle restart. See [NATIVE-LOADER.md](NATIVE-LOADER.md) for exact integration points and [VALIDATION.md](VALIDATION.md) for observed coverage. Live multi-stage retreat, arbitrary geometry, saves and crowd-scale acceptance remain to be completed.
