# Installed-build findings

Inspected on 2026-11-17:

- Steam app: **5185890**, Zombies Aren't Real.
- Installed Steam build: **26692741**.
- Executable SHA-256: `E297BC7DB1174EB67116B2B8387B8640554E1BD46598D8D3178091D9DF19B9A9`.
- Native x64 executable with Odin source-path/type strings (`src/mods.odin`, `src/sim/shelter.odin`, etc.). No Unity managed assembly layout is installed.
- Local user mutators live in `steam_cloud/mods/`, not the shipped `mutators/` directory. They are INI files. The game's own `config.ini` header and `lang/en.lang` entries `mods.template.header`, `mods.name.hint`, and `mods.why.unknown` describe this system.
- The shipped configuration describes safe-region breach state, civilians panicking after a breach, and gate attraction restricted to civilians **outside every zone**. Exposed keys adjust attraction distances/strengths and panic, but do not add region contingency memory, breach-triggered destination selection, or new steering callbacks.
- The game's MUTATORS interface includes PUBLISH, and its text describes a separate Free Play save with no leaderboard scores or achievements while mutators are active. Campaigns/daily challenges use their tuned settings. These details come from the installed English localization; recheck the current interface before release.

The [official Steam store page](https://store.steampowered.com/app/5185890/Zombies_Arent_Real/) describes Workshop support for custom maps. The [Workshop overview](https://steamcommunity.com/workshop/about/?appid=5185890) documents subscription delivery. Neither establishes a native DLL plugin ABI.

Consequently no DLL has been linked into a pretend mod folder, and no INI with invented settings has been installed. A DLL or Workshop item alone cannot activate PIP. A developer-supported source integration or code-plugin interface is required, followed by real gameplay validation.

This is a finding about the installed build and exposed configuration, not a claim that native reverse engineering is technically impossible. No executable patch, process injection, game asset redistribution, or game-source redistribution is part of this repository.

