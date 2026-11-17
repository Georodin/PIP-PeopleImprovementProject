# Workshop release preparation

**Status: blocked on actual game integration. Do not publish this as a working mutator.**

Prepared: title, author, source URL, description in BBCode, editable PSD, preview PNG, native library, automated behaviour tests and a draft packaging command. No item has been created or published on Steam.

Before a playable release:

- Finish and validate the game-specific simulation bridge as described in `docs/INTEGRATION.md`.
- Verify how that code is distributed and loaded for Workshop subscribers. The current built-in INI-mutator uploader is not proof of DLL support.
- Run all native tests and the real-game acceptance tests on the current Steam build. Record results in `docs/VALIDATION.md`.
- Verify save/reload, performance, and multiplayer support or explicitly limit the release to singleplayer.
- Update the description and artwork's development badge to match observed behaviour, and specify actual dependencies and the installation method.
- Only then set the three verification flags in `metadata.json` to true, provide a reviewed `workshop/payload/` containing the real loadable mod, and run `scripts/Prepare-Workshop.ps1`.
- Use the game's MUTATORS > PUBLISH flow only if the finished implementation is supported there. Otherwise follow the developer's documented publishing route. A generic SteamCMD VDF would not make an unsupported DLL mutator work.
- Initially publish privately, subscribe from a clean installation, verify loading, then review the listing and change visibility when ready. Save the returned real Workshop ID; `0` is a placeholder only.

`-Draft` creates a development zip for reviewing the work. It is intentionally different from a playable Workshop payload. No script in this project uploads, accepts the Workshop agreement, or changes Steam visibility.

