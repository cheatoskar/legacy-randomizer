# Legacy Randomizer {VERSION}

I made a Random Map Challenge for old TrackMania games that runs inside the game. Press Start, get a random map from TM Exchange, reach the goal medal, next map. Challenge, Survival and Free play modes, filters, a HUD you can move around, run history.

Works with TrackMania Nations ESWC (unofficial 1.80 exe) and TrackMania Sunrise (Win10 repack), solo only. It is a plugin for the Horizon Host.

## Downloads

- `LegacyRandomizer-{VERSION}-ESWC-with-Horizon-Host.zip` - host {HOSTVERSION} and the plugin for ESWC
- `LegacyRandomizer-{VERSION}-Sunrise-with-Horizon-Host.zip` - host {HOSTVERSION} and the plugin for Sunrise
- `LegacyRandomizer-{VERSION}-plugin.zip` - only the plugin, if you already have the Horizon Host
- `SHA256SUMS.txt`

Unzip into the game folder. The bundles need a double-click on Horizon-Install.cmd afterwards (an existing dinput8.dll is kept as dinput8.chain.dll). Menu key is Insert. More in the README inside the zip.

## Known limitations

- Sunrise: finish detection is not available yet (the host has no race events there). Skip maps manually.
- Original: experimental, the host refuses map loading there for now.
- Not in this version: Platform / Stunts / Puzzle modes (they need a score from the host), Discord Rich Presence, replay file naming and per-run replay folders (need the host's save_replay).
- Saving the replay of every goal medal is off for now (the host does not support it yet). On ESWC the Autosave plugin still saves replays.
- Edit layout, "Never a map you finished before" and "Avoid maps you skipped" are off by default and not as well tested.
- The filters "has replays", "author's latest" and "supporter" were not checked against the live API.
- Forever (TMNF/TMUF) is not supported, the plugin is untested there.

Maps and screenshots come from TMX. The mod asks for one map at a time, please keep it that way. RMC idea by Flink, rules modelled after BigBang1112's RandomizerTMF (no code from it). MIT licensed.

## How to rebuild

Everything goes into `release\{VERSION}\`. If the public host version differs from the default files, pass them in:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package.ps1 -EswcHostZip <path\Horizon-Host-X.Y.Z-ESWC.zip> -SunriseHostZip <path\Horizon-Host-X.Y.Z-Sunrise.zip>
```

Optional: `-Version` (default: from `src\plugin\plugin.json`), `-HostVersion` (default: from the host zip file name). Without parameters the newest `Horizon-Host-*-ESWC.zip` and `-Sunrise.zip` from `C:\Users\osaro\Downloads\horizon-host\dist\release` are used.
