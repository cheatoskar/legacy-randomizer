# Legacy Randomizer {VERSION}

I made a Random Map Challenge for old TrackMania games that runs inside the game. Press Start, get a random map from TM Exchange, reach the goal medal, next map. Challenge, Survival and Free play modes, filters, a HUD you can move around, run history.

Works with TrackMania Nations ESWC (unofficial 1.80 exe), TrackMania Sunrise (Win10 repack) and, less tested, TrackMania Original (Online Patch v2 exe), solo only. It is a plugin for the [Horizon Client](https://github.com/cheatoskar/horizon-client/releases).

## Downloads

- `LegacyRandomizer-{VERSION}-ESWC-with-Horizon-Client.zip` - Horizon Client {HOSTVERSION} and the plugin for ESWC
- `LegacyRandomizer-{VERSION}-Sunrise-with-Horizon-Client.zip` - Horizon Client {HOSTVERSION} and the plugin for Sunrise
- `LegacyRandomizer-{VERSION}-plugin.zip` - only the plugin, if you already have the Horizon Client
- `SHA256SUMS.txt`

Unzip into the game folder. The bundles need a double-click on Horizon-Install.cmd afterwards (an existing dinput8.dll is kept as dinput8.chain.dll). Menu key is Insert. More in the README inside the zip.

## Known limitations

- Sunrise: finish detection needs Horizon Client 0.3.1 or newer (the bundles have it).
- Original: no bundle; install the Horizon Client's Original zip, then the plugin zip. Less tested than ESWC.
- Not in this version: Platform / Stunts / Puzzle modes (they need a score from the host), Discord Rich Presence, replay file naming and per-run replay folders (need the host's save_replay).
- Saving the replay of every goal medal is off for now (the host does not support it yet). On ESWC the Autosave plugin still saves replays.
- Edit layout, "Never a map you finished before" and "Avoid maps you skipped" are off by default and not as well tested.
- Forever (TMNF/TMUF) is not supported, the plugin is untested there.

Maps and screenshots come from TMX. The mod asks for one map at a time, please keep it that way. RMC idea by Flink, rules modelled after BigBang1112's RandomizerTMF (no code from it). MIT licensed.

## How to rebuild

Everything goes into `release\{VERSION}\`. If the public host version differs from the default files, pass them in:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package.ps1 -EswcHostZip <path\Horizon-Client-X.Y.Z-ESWC.zip> -SunriseHostZip <path\Horizon-Client-X.Y.Z-Sunrise.zip>
```

Optional: `-Version` (default: from `src\plugin\plugin.json`), `-HostVersion` (default: from the host zip file name), `-HostReleaseDir`. Without zip parameters the newest `Horizon-Client-*-ESWC.zip` and `-Sunrise.zip` (older releases: `Horizon-Host-*`) from the host release folder are used: `-HostReleaseDir`, else the environment variable `HORIZON_RELEASE_DIR`, else `..\horizon-host\dist\release` next to this repo.
