# Legacy Randomizer

**A Random Map Challenge (RMC) inside the old TrackMania games: TrackMania Nations ESWC, TrackMania Sunrise and TrackMania Original.**

Press Start, the mod loads a random map from TM Exchange, you try to reach the goal medal, then the next map comes. It runs as a module (plugin) of the [Horizon Client](https://github.com/cheatoskar/horizon-client), solo only.

Version 0.3.1. MIT licence. Built with [Claude Code](https://claude.com/claude-code).

**[Download the latest release](https://github.com/cheatoskar/legacy-randomizer/releases/latest)** · [Install](#install) · [How to play](#how-to-play) · [Features](#features) · [Controls](#controls) · [Known issues](#known-issues) · [Building](#building) · [TM Horizon family](#tm-horizon-family)

![A run in TrackMania Nations ESWC](docs/images/eswc-run.jpg)

*A run in TrackMania Nations ESWC: timer, map, medals, skips and the maps of this run. Map: "hang 'em high" by LookACar (TMX 1274100); its thumbnail is blanked here.*

![The Rules page](docs/images/rules-1280x960.png)

*The Rules page of the menu (preview render).*

---

## TM Horizon family

Six projects by the same author, made to work together. TM Horizon installs and updates the others where it can; each one also works without it.

| Project | What it is | Games | How to get it | Repo |
|---|---|---|---|---|
| **TM Horizon** | Launcher for every TrackMania game on Windows: library, mods, maps, servers, statistics. Installs and updates Horizon Client, Legacy Essentials and TM Vibrant Shaders. | All, from TrackMania (2003) to Trackmania (2020) | Installer from the releases (0.5.1, Launcher Preview) | [cheatoskar/tm-horizon](https://github.com/cheatoskar/tm-horizon) |
| **Horizon Client** | In-game menu and mod loader with modules: Legacy Essentials, TMX Browser, TMX Upload, WR Hunter, Input Display, Horizon Performance, Horizon Scripts. | ESWC, Sunrise, Original; TMNF and TMUF through TMLoader (experimental) | TM Horizon (game page), or a zip per game (0.3.1) | [cheatoskar/horizon-client](https://github.com/cheatoskar/horizon-client) |
| **Legacy Essentials** | Autosave, controller fix, login fix and game settings. A standalone `dinput8.dll` mod with its own menu (`Ctrl+F2`). The same features are the "Legacy Essentials" group in Horizon Client. | ESWC, Sunrise, Original | A zip per game (1.3.1); TM Horizon installs it on ESWC | [cheatoskar/eswc-autosave](https://github.com/cheatoskar/eswc-autosave) |
| **Legacy Randomizer** (this repo) | Random Map Challenge with maps from TMX, inside the game. A Horizon Client module. | ESWC, Sunrise, Original (solo) | A zip with Horizon Client, or the plugin alone (0.3.1) | [cheatoskar/legacy-randomizer](https://github.com/cheatoskar/legacy-randomizer) |
| **TM Vibrant Shaders** | Lighting, weather, skies, photos, clips and video export. Standalone, or as four Horizon Client modules. | TMNF, TMUF, TM2; ESWC, Sunrise, Original experimental | Setup or ModLoader server (1.6.0), Horizon modules zip (2.0.0); TM Horizon installs it on TMNF, TMUF and ESWC | [cheatoskar/TM-Vibrant-Shaders](https://github.com/cheatoskar/TM-Vibrant-Shaders) |
| ***For developers*** | | | | |
| **trackmania-internals** | Notes for mod developers: addresses, structures, file formats and physics of the games. | TMNF, TMUF, ESWC, Sunrise, Original, ManiaPlanet | Read it on GitHub | [cheatoskar/trackmania-internals](https://github.com/cheatoskar/trackmania-internals) |

TMNF/TMUF = TrackMania Nations/United Forever, ESWC = TrackMania Nations ESWC, TM2 = TrackMania 2 (ManiaPlanet).

- **Legacy Essentials or Horizon Client:** one is enough. Horizon Client has the same features in its "Legacy Essentials" group. With both in one game, the standalone keeps the autosave (the client's Autosave module pauses itself and logs `standalone ESWC Autosave found (eswc_autosave.dll): module paused`), and the login fix, controller fix, game settings and menu are the client's.
- **TM Vibrant Shaders standalone or as Horizon modules:** one of the two. With both installed, the one the game loads first runs and the other stays off.

---

## Supported games

| Game | Exe | Status |
|---|---|---|
| TrackMania Nations ESWC | unofficial 1.80 patch, `TmNationsESWC.exe` | ✓ |
| TrackMania Sunrise | Win10 repack, `TmSunrise.exe` | ✓ (finish detection needs Horizon Client 0.3.1 or newer) |
| TrackMania Original | Online Patch v2, `TMORIGINAL.EXE` | experimental (less tested) |
| TrackMania Nations / United Forever | | – (not supported; the plugin is untested there) |
| TrackMania 2, Trackmania (2020) | | – (Horizon Client does not run there) |

The exact exes and their SHA-256 are in the [Horizon Client README](https://github.com/cheatoskar/horizon-client#supported-exes). Solo only: nothing runs while you are online. No StarForce versions.

---

## Install

**From the release zips** ([latest release](https://github.com/cheatoskar/legacy-randomizer/releases/latest)):

| File | What it holds |
|---|---|
| `LegacyRandomizer-<version>-ESWC-with-Horizon-Client.zip` | Horizon Client 0.3.1 and the plugin, for ESWC |
| `LegacyRandomizer-<version>-Sunrise-with-Horizon-Client.zip` | Horizon Client 0.3.1 and the plugin, for Sunrise |
| `LegacyRandomizer-<version>-plugin.zip` | only the plugin, if you already have the Horizon Client |
| `SHA256SUMS.txt` | checksums of the zips |

- **With Horizon Client in the zip (ESWC, Sunrise):** unzip into the game folder (next to `TmNationsESWC.exe` or `TmSunrise.exe`), double-click `Horizon-Install.cmd`, start the game and press **Insert**. An existing `dinput8.dll` of another mod is kept as `dinput8.chain.dll`.
- **Plugin only:** with the Horizon Client already installed, unzip into the game folder so the files land in `horizon\plugins\legacy-randomizer\`.
- **Original:** no bundle. Install the [Horizon Client's](https://github.com/cheatoskar/horizon-client/releases/latest) Original zip first, then the plugin zip.

**With TM Horizon:** the [TM Horizon](https://github.com/cheatoskar/tm-horizon) launcher installs the Horizon Client (game page), but 0.5.1 does not download the Randomizer yet. Add the plugin zip by hand; the Horizon Client card on the game page then lists it with its switch.

**Uninstall:** delete `horizon\plugins\legacy-randomizer`. To remove the whole Horizon Client: run `Horizon-Uninstall.cmd`, then delete the `horizon` folder.

---

## How to play

1. Press **Insert**, open the Legacy Randomizer page, set filters and rules if you want.
2. Press **Start run**. The mod loads the first map; the next one is fetched in the background.
3. Reach the goal medal (Author by default) to get the next map. Skip a map you cannot do.
4. When the timer runs out (or you press Stop), the run ends with its results, and the run goes into the history.

---

## Features

| | |
|---|---|
| **Modes** | **Challenge** (RMC): collect as many goal medals as you can before the timer runs out. **Survival** (RMS): medals add time, skips cost time, the run ends at 0. **Free play**: no timer. |
| **Goal and skips** | Goal is Author, Gold, Silver, Bronze or Finished; a better medal counts too. Free skips (the first one by default; it gives back the time spent on that map). **Gold skip** (goal Author only): move on once you have at least gold; it counts as a gold. Stop ends the run like the timer (results and history); it asks first when the run has progress. |
| **Filters** | TMX site (Auto: the site of your game), author time, environments, difficulty, mood, routes, style, "Skip Unlimiter maps", "Need author time". Under *Advanced TMX search*: name, author, uploaded after / before, leaderboard, vehicle, race maps only. |
| **Rules** | Mode, time limit, goal, free skips, gold skip, "Even environments" (environment first, then a map of it), "Skip played maps", "Avoid skipped maps", "Skip finished maps", "Auto next map" and its delay. Under *Advanced rules*: Survival numbers (author bonus, gold bonus, skip penalty) and banned maps (TMX ids that are never picked). |
| **HUD & layout** | The HUD is split into Timer, Map, Medals, Controls and Session panels, or one Compact card. *Edit layout* moves and resizes them while the menu is open; otherwise they let clicks through to the game. Panels can also be popped out as windows. |
| **Hide Clock and Map Info** | During a run the Horizon Client's Clock and Map Info widgets are hidden and come back when the run ends (on by default, needs Horizon Client 0.3.1). |
| **History** | Every run is saved as a JSON file with its maps and medals, and listed on the History page. Maps of earlier runs are not picked again (can be switched off). |
| **Narrow windows** | The page lays itself out by its width: in a small game window the navigation becomes icons and cards stack. |

## Controls

| Key | Action |
|---|---|
| `Insert` | Open the Horizon menu (Horizon Client's key) |
| `Ctrl+F9` | Skip the map |
| `Ctrl+F10` | Gold skip |
| `Ctrl+F11` | Show or hide the HUD |

The keys can be changed in the Horizon menu (*Hotkeys*).

---

## Compatibility

| Together with | |
|---|---|
| [Horizon Client](https://github.com/cheatoskar/horizon-client) | Needed. 0.3.1 or newer for Sunrise finish detection and for hiding Clock and Map Info. |
| Autosave ([Legacy Essentials](https://github.com/cheatoskar/eswc-autosave) group in the Horizon Client) | On ESWC it still saves the replays of your runs. |
| Online play | The plugin is `solo-only`: it stops while the game is online. |

---

## Known issues

- **Sunrise:** finish detection needs Horizon Client 0.3.1 or newer (the bundles have it). TrackMania Sunrise has no online servers any more.
- **Original:** no bundle; less tested than ESWC.
- **Not in this version:** Platform, Stunts and Puzzle modes (they need a score from the host), Discord Rich Presence, replay file naming and per-run replay folders.
- **Save goal replays** is off for now (the Horizon Client does not support it yet). On ESWC the Autosave still saves replays.
- *Edit layout*, "Skip finished maps" and "Avoid skipped maps" are off by default and not as well tested.
- **Forever (TMNF/TMUF)** is not supported; the plugin is untested there.

---

## Please be kind to TMX

The mod asks the TMX API for one map at a time. Please do not start many runs in a loop and do not use it to download maps in bulk.

---

## Building

MSVC Build Tools (x86) with `vcvarsamd64_x86.bat` at the path set in `build.cmd`:

```
build.cmd
```

This builds `dist\legacy_randomizer_plugin.dll`, `dist\plugin.json`, the test `build\test\core_test.exe` and the preview tool `build\test\ui_preview.exe`. Tests: run `build\test\core_test.exe` (add `--live` for a few live TMX requests). The Rules screenshot above: `build\test\ui_preview.exe --shots <folder> --display 1280x960`. Release zips: `scripts\package.ps1`.

Plugin details (ABI pins, code layout, panels): [docs/plugin.md](docs/plugin.md). The plugin ABI it is built on: [Horizon Client, docs/plugin-abi.md](https://github.com/cheatoskar/horizon-client/blob/main/docs/plugin-abi.md). Game internals: [trackmania-internals](https://github.com/cheatoskar/trackmania-internals).

## Credits

- Flink: the idea of the Random Map Challenge
- BigBang1112: [RandomizerTMF](https://github.com/BigBang1112/randomizer-tmf) is the model for the rules and filters; no code was taken from it
- TM Exchange: the maps, screenshots and the API
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut (MIT)
- TrackMania is a trademark of Ubisoft/Nadeo. This project is not affiliated with them.

## License

[MIT](LICENSE)
