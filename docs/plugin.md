# Legacy Randomizer as a Horizon plugin

`dist\legacy_randomizer_plugin.dll` + `dist\plugin.json` (copied from `src\plugin\plugin.json` by `build.cmd`) form the
Horizon Host plugin `legacy-randomizer`, version 0.3.0. Install both into
`<game>\horizon\plugins\legacy-randomizer\`. All paths come from the host (data folder, game folder, user folder).

## Pins

| What | Version |
|---|---|
| Plugin ABI | `src/plugin/horizon/horizon_plugin.h` + `horizon_imgui.h`, copied verbatim from `horizon-host` commit **5f24d88** (ABI 1, minor 1). Update = copy both files again, then fix `src/plugin/host_horizon.cpp` only. |
| TMX site logos | `assets/sites/<host>.png`, 64 px, taken from TMX-Downloader 2.1 (`icons/sites`), originally the site-switch logos at `account.mania.exchange/img/logos/` (TMNF, TMU, TMN, TMS, TMO). Origin per file in `assets/sites/SOURCES.md`. They belong to Nadeo/Ubisoft and ManiaExchange and only mark which exchange the plugin uses. Embedded in the DLL as byte arrays (`src/plugin/site_logos.inc`, regenerate with `python assets/make_site_logos.py`), decoded with WIC at init, textures via `create_texture_rgba`. Nothing is fetched at runtime. |
| Dear ImGui | **1.91.5** (`IMGUI_VERSION_NUM 19150`), `third_party/imgui` (MIT, `LICENSE.txt`), the same copy as horizon-host. Default `imconfig.h`. `HorizonPlugin_Init` returns 2 if the host reports another version. `ImTextureID` is `ImU64`. |

## Code layout

- `src/core/` – TMX client (now with RandomizerTMF's filters), RMC/RMS rules (`Session`, goal Author/Gold/Silver/Bronze/Finished), GBX reader (map header, embedded thumbnail, the map uid of a replay).
- `src/plugin/host.h` – the small C++ interface the plugin needs from a host; `host_horizon.cpp` is **the only file that includes the Horizon ABI** and exports `HorizonPlugin_Info/Init/Shutdown/DrawMenu/DrawOverlay/OnEvent/SetEnabled`.
- `panels.cpp` – the page/panel table and the window logic (below). Meant to move into the host as a shared API.
- `ui.cpp` – the pages and HUD panels; `widgets.cpp` – cards (TMVS look: accent mark, icon, title), switch rows, labels above controls, the author-time range slider, dropdowns without the arrow box, the *Advanced* disclosure row, chips with icons, segmented control, info tooltips, big button (host style colours only, local pushes); `site_logos.cpp` – the TMX logos; `icons.cpp` – line icons drawn with ImDrawList (no icon font: plugins can't add fonts to the host's atlas; a host icon font can replace `drawIcon()`).
- `run.cpp` – run state machine on top of `Session`; `fetcher.cpp` – worker thread that keeps the next map ready (and scans the autosaves); `history.cpp` – run JSON files; `tmtext.cpp` – `$`-formatted names; `image.cpp` – WIC decode.

## Pages and panels (TM Vibrant Shaders' detachable pages)

One table, one row per page or panel (key, title, hint, icon, HUD or not, draw). The same draw function runs as a page of the plugin's menu (left sidebar with icons) or in a window of its own.

- Menu pages: **Run** (start card with a one-line summary of the settings, Start, last run; during a run the live view; after it the results), **Filters**, **Rules**, **HUD & layout**, **History**, **About**.
- HUD panels: **Timer**, **Map** (thumbnail, name, author, TMX id), **Medals** (medal times, the reached one highlighted, personal best, AT/Gold/skip counters), **Controls** (Skip, Gold skip, Next map / Resume / Another map, notices), **Session** (maps of this run with their medal), **Compact card** (all of it in one card).
- The pop-out symbol on a sidebar row makes it a window; the row then reads *(detached)* and a click on it brings the window to the front. Window title bar: minimize arrow, cross (= back into the menu), *Window options*: *Keep open when the menu closes*, *Opacity*, *Back to the menu* (TMVS wording).
- During a run the HUD **is** the popped-out windows that are kept open. Presets (HUD & layout): **Split** (default; timer top centre, map top left, controls below it, medals top right, session below it) and **Compact card** (one card top left). *Reset layout* re-applies the preset's places.
- **Edit layout** (off by default): while the menu is open the HUD windows have title bars and an accent outline and can be moved and resized. **Locked** (edit off): no title bar, no move/resize, the height fits the content, and every panel lets clicks through to the game except Controls and the card's button row.
- Saved per row in the host settings as `win_<key>` = `detached,minimized,keepOpen,x,y,w,h,opacity`, with x/y/w/h as **fractions of the screen** (resolution changes keep the layout). `layout_version` marks that the default preset was applied once.

## How a run works

Unchanged from 0.1 except where the new rules need it:

1. **Start** (Run page) → the fetcher picks a map on the game's TMX site (`/trackrandom` + `/api/tracks` check, filters below), downloads it, reads the header (medal times), fetches the thumbnail (TMX screenshot, map file as fallback).
2. Main menu → `play_map_file` via `run_on_game_thread` (result codes as before: retry / drop map / end run).
3. `RACE_START` with the expected uid → the map starts in the session. **New fallback:** if the host sends `MAP_LOADED` but no `RACE_START` (Sunrise in host 0.1.0), the map starts after 2 s in the solo race.
4. `FINISH` → `Session::finish`. Goal reached → wait for `RACE_END` (max 5 s) and the delay → `return_to_menu` → next map (already downloaded).
5. Skip / Gold skip / timer / results / JSON per run: as in 0.1.
6. **Sunrise:** host 0.1.0 has no race events there, so finishes are not detected. The HUD says "Sunrise: finish detection not available yet, skip manually."

Safety rules (unchanged): nothing while online or gated; `play_map_file` only in the main menu; `return_to_menu` on ESWC/Sunrise/Original in a solo race or on the end screen, on **TMNF/TMUF only on the end-race screen** (Competition Patch 2.0).

## RandomizerTMF parity

Filters (Filters page; rarely used ones under *Advanced*), checked live 2026-11-11 against the old API (`/api/tracks` with `count=4`, results compared field by field, about 70 requests in total, once):

| Filter | Query | tmnf | nations | sunrise | original | tmuf |
|---|---|---|---|---|---|---|
| Name | `name=` | ✓ | ✓ | ✓ | ✓ | (same API) |
| Author (uploader, exact) | `author=` | ✓ | – | – | – | (same API) |
| Mood | `mood=0..3` (comma list) | ✓ | accepted, no night maps | ✓ | accepted, no night maps | (same API) |
| Routes | `routes=0..2` | ✓ | ✓ | ✓ | ✓ | (same API) |
| Map type | `primarytype=0` (Race, always sent) | ✓ | ✓ | ✓ (1 = Puzzle verified) | ✓ | ✓ |
| Leaderboard type | `lbtype=0..5` | ✓ (changes results) | ✓ | ✓ | ✓ | (same API) |
| Uploaded after/before | `uploadedafter=` / `uploadedbefore=` `YYYY-MM-DD` | ✓ | ✓ | ✓ | ✓ | (same API) |
| Vehicle | `vehicle=0..6` | – | – | – | – | ✓ (TMUF only, shown only there) |
| Has screenshot / Featured / Has record | `inscreenshot`, `infeatured`, `inhasrecord` `=0/1` | ✓ | ✓ | ✓ | ✓ | (same API) |
| Has replays / Author's latest / Supporter | `inreplays`, `inlatestauthor`, `insupporter` | sent as RandomizerTMF does, not verified | | | | |

"–" for Author on the legacy sites: not verified (only checked on tmnf with a known uploader); the filter is sent there too. Environments (chips), difficulty, style (tag), author time window, exclude Unlimiter: as in 0.1. Maps of another type than Race are dropped (the run can't score Stunts/Platform/Puzzle).

Rules (Rules page):

- **Goal** = RandomizerTMF's auto-skip mode: Author, Gold, Silver, Bronze, Finished. A better medal counts too. Silver/Bronze/Finished goals count in `finishedCount()` / result `finished`; RMC/RMS timing unchanged (RMS bonuses only for author and gold). Gold skip only with goal Author.
- **Equal environment distribution**: the fetcher picks the environment first (evenly among the chosen ones, or all of the site's), then a random map of it. Chosen environments are always picked evenly.
- **Exclude maps from past runs** (on), **Avoid maps you skipped** (off), **Banned maps** (Advanced: TMX ids, comma separated, any site).
- **Never a map you finished before** (off): reads the map uid of every replay in `<user folder>\Tracks\Replays\Autosaves\` (host game info; Forever/Sunrise) and `<game folder>\GameData\Tracks\Replays\Autosaves\` (ESWC), first 64 KiB each, with a cache in `<data>\autosaves_cache.txt`. Maps with such a uid are skipped before download. Checked on 4 ESWC and 200 TMF autosaves: every one gave a uid.

## Settings (host settings store)

`mode`, `goal` (author/gold/silver/bronze/finished), `rmc_minutes`, `rms_*`, `free_skips`, `gold_skip`, `equal_environments`, `exclude_played`, `avoid_skipped`, `exclude_finished` (0), `banned`, `site`, `at_min_s`, `at_max_s`, `environments` (bit mask), `difficulty`, `tag`, `moods`, `routes`, `lb_type`, `vehicle`, `name`, `author`, `uploaded_after`, `uploaded_before`, `flag_in*` (-1/0/1), `exclude_unlimiter`, `require_author_time`, `auto_advance`, `advance_delay_s`, `replay_autosave` (**0**, greyed out), `hud_visible`, `hud_preset` (1 split), `layout_edit` (0), `hud_compact`, `thumb_source`, plus `win_<panel>` and `layout_version`.

Hotkeys (host registry): `skip` Ctrl+F9, `gold-skip` Ctrl+F10, `toggle-hud` Ctrl+F11, `next-map` (none). Free in host 0.1.0 (menu Insert, Autosave F9).

## Host API use (ABI 1.1)

- Textures: `create_texture_rgba` / `release_texture` (minor ≥ 1); with a minor-0 host the plugin falls back to `device()` and a managed D3D9 texture of its own.
- `save_replay`: host 0.1.0 answers `NOT_SUPPORTED`, so *Save the replay of every goal medal* stays **off and greyed out**; replays saved by ESWC Autosave (`REPLAY_SAVED`) are still recorded in the run file.
- Still wanted from the host: race events on Sunrise (`RACE_START`, `FINISH`, `RACE_END`); a `score` in `FINISH` for Stunts/Platform; a shared version of the panel table (`panels.cpp`).

## Off by default / not solid yet

- *Edit layout*, *Never a map you finished before*, *Avoid maps you skipped*, replay autosave (greyed out).
- Sunrise: no finish detection (host); skip manually.
- Original: experimental; the host refuses map loading there today (`NOT_SUPPORTED` → the run ends with a message).
- `inreplays` / `inlatestauthor` / `insupporter` were not verified live.

## 0.3 polish (UI only, run logic unchanged)

- Labels above the controls (small, muted) instead of ImGui's labels on the right; switches as rows (label left, switch right).
- Author time: one range slider (square-root scale, 5 s steps) with the values under it (`0:30 – 1:30`, *any* at the open ends).
- Dropdowns without ImGui's arrow box (a quiet chevron), full width.
- *Advanced* (Filters, Rules) is a subtle disclosure row with a chevron instead of a full-width header bar.
- Run page: start card with the site logo, the mode and a one-line summary, the Start button; *Last run* has an empty state (icon + one line) and is as tall as the start card; below, *Recent runs* (from two runs on) or *How a run goes*.
- Header: the TMX site's logo, host and game as a chip on the right.
- Mood chips with sunrise/sun/sunset/moon icons, routes with a route icon, goal chips with medal icons in the medal colours.
- HUD panels: TMVS overlay look (radius 10, padding 12/10); a locked panel placed right under another one follows that panel's real height, so Split has no gap between Map and Controls (or Medals and Session). Timer and Controls have taller default places for edit mode.
- Kept: the sidebar, the page/panel table and the window logic (`panels.cpp`). Not done: tabs instead of the sidebar, mode tiles (both need restructuring).

## Planned (not in 0.3)

- Platform / Stunts / Puzzle modes (need a score in the `FINISH` event).
- Discord Rich Presence.
- Replay file naming and per-run replay folders (need the host's `save_replay`).

## Preview without the game

`build\test\ui_preview.exe` runs the real plugin code (everything except `host_horizon.cpp`) with a fake host: settings in a text file, game control that only logs and simulates map load / quit, a "Fake game" panel (finish with AT/gold/silver/slow, restart, Esc › Quit, online, +1/+10 min), F8 menu, Ctrl+F9/F10/F11. The ImGui style and fonts are the Horizon host's (`horizon-host` `src/host/ui/ui_style.cpp` at 895610c: launcher tokens, accent #ff6b1a, radii 10/14/20, Segoe UI 16, Bahnschrift 28 title), so the screenshots match the host; the plugin never sets global colours (its own colours are the medal colours).

`ui_preview.exe --shots docs\screenshots` renders (the 0.2 set is kept in `docs/screenshots/before-0.2/`), 1920×1080: start, filters, rules, layout editor, split HUD, compact HUD, results, history — with real nations.tm-exchange.com maps (a scripted run of three maps, about 15 requests). Options: `--game eswc|sunrise|original|tmnf|tmuf`, `--data <dir>`.
