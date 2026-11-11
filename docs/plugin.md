# Legacy Randomizer as a Horizon plugin

`dist\legacy_randomizer_plugin.dll` is a Horizon Host plugin (id `legacy-randomizer`). Install:
`<game>\horizon\plugins\legacy-randomizer\legacy_randomizer_plugin.dll`. All paths it uses come from the host
(data folder), nothing is hard-coded.

## Pins

| What | Version |
|---|---|
| Plugin ABI | `src/plugin/horizon/horizon_plugin.h` + `horizon_imgui.h`, copied verbatim from `horizon-host` commit **926c17d** (ABI 1.0). Update = copy both files again, then fix `src/plugin/host_horizon.cpp` only. |
| Dear ImGui | **1.91.5** (`IMGUI_VERSION_NUM 19150`), `third_party/imgui` (MIT, `LICENSE.txt`), identical to horizon-host's and TM Vibrant Shaders' copy. Default `imconfig.h`. Host and plugin must match exactly; `horizon_imgui.h` refuses to compile otherwise and `HorizonPlugin_Init` returns 2 if the host reports another version. `ImTextureID` is `ImU64` in this version. |

## Code layout

- `src/core/` – TMX client, RMC/RMS rules (`Session`), GBX header reader (now also the embedded JPEG thumbnail, chunk `0x...3007`; ESWC maps have none, TMNF/TMUF maps do).
- `src/plugin/host.h` – the small C++ interface the plugin needs from a host. Everything else codes against it.
- `src/plugin/host_horizon.cpp` – **the only file that includes the Horizon ABI**: implements `lr::Host` with `HorizonHostApi` and exports `HorizonPlugin_Info/Init/Shutdown/DrawMenu/DrawOverlay/OnEvent/SetEnabled`.
- `run.cpp` – run state machine on top of `Session`; `fetcher.cpp` – worker thread that keeps the next map ready; `history.cpp` – run JSON files; `ui.cpp` – menu page and HUD; `tmtext.cpp` – `$`-formatted names; `image.cpp` – WIC JPEG/PNG decode.
- `test/ui_preview.cpp` – fake host + D3D9 window, see below.

## How a run works

1. **Start** (Run tab): the session config and TMX filter come from the settings. The TMX site follows the game (ESWC → nations, Sunrise → sunrise, Original → original, TMNF → tmnf.exchange, TMUF → tmuf.exchange) unless overridden.
2. The fetcher thread picks a random map (`/trackrandom` + `/api/tracks` check), downloads it to `<data>\maps\`, reads the header (medal times; TMX values only as fallback), and fetches the TMX screenshot (`/trackshow/<id>/image/1`), falling back to the thumbnail inside the map file. TMX errors: up to 8 picks per attempt, then back-off 2/5/10/20/30 s, message in the HUD.
3. In the **main menu** the plugin queues `play_map_file` via `run_on_game_thread`. Result codes: `WRONG_STATE`/`BUSY` → retry in 1 s; `BLOCKED_ONLINE` → wait; `NOT_SUPPORTED` → run ends with a message; `FAILED`/`INVALID_ARG` → map dropped, next one (5 in a row → run ends). If the race does not start within 15 s (menu) / 45 s, the HUD offers "Another map".
4. `RACE_START` with the expected uid starts the map in the session; the fetcher immediately prepares the next map.
5. `FINISH` (local, solo, on our map) goes to `Session::finish`. Goal medal reached → wait for `RACE_END` (max 5 s), then the advance delay (default 3 s, or "Next map" when auto-advance is off), then `return_to_menu`, then `MENU_ENTERED` / state main menu → next map (already downloaded).
6. **Skip / Gold skip** (HUD buttons, hotkeys): resolved in the session, then the map is left the same way.
7. The **timer** only ticks while the session is playing *and* the game state is solo race / end-race screen on our map (load times and menus don't count), exactly as `core::Session` defines (free skips refund, RMS bonuses/penalties).
8. Run ends on timer 0 or "End run" → results page + toast; the run is written to `<data>\runs\run-YYYYMMDD-HHMMSS.json` (game, site, mode, goal, times, counters, per map: TMX id, uid, raw name, author, result, best medal, best time, time on map, medal times, replay path).

### Rules that keep it safe

- `is_online()` or `actions_allowed() == 0` → the plugin does nothing (no calls, timer paused, HUD says so). Declared `HORIZON_FLAG_SOLO_ONLY`.
- `play_map_file` only in `HORIZON_STATE_MAIN_MENU`.
- `return_to_menu`: ESWC/Sunrise/Original in a solo race or on the end-race screen; **TMNF/TMUF only on the end-race screen** (Competition Patch 2.0). A skip during a Forever run tells the player to quit the map (Esc › Quit); the next map starts from the menu. If the host returns `NOT_SUPPORTED` for leaving, the same manual way is used.
- Nothing touches physics, timing, input or replays.

## Settings (host settings store, `horizon\settings\legacy-randomizer.json`)

| Key | Default | Meaning |
|---|---|---|
| `mode` | `rmc` | `rmc`, `rms`, `free` |
| `goal` | `author` | `author` / `gold` |
| `rmc_minutes` | 60 | RMC time limit |
| `rms_start_minutes`, `rms_author_bonus_s`, `rms_gold_bonus_s`, `rms_skip_penalty_s` | 15, 180, 60, 60 | RMS rules |
| `free_skips` | 1 | free skips (-1 unlimited) |
| `gold_skip` | 1 | gold skip allowed (goal Author only) |
| `site` | 0 | 0 auto, 1 tmnf, 2 tmuf, 3 nations, 4 sunrise, 5 original |
| `at_min_s`, `at_max_s` | 0, 0 | author time window, 0 = no bound |
| `environment` | 0 | TMX environment id (1 Snow … 7 Stadium; Sunrise 4–6, Original 1–3), 0 any |
| `difficulty` | -1 | 0–3, -1 any |
| `tag` | -1 | TMX tag id 0–18 (same list on all five sites), -1 any |
| `exclude_unlimiter` | 1 | |
| `require_author_time` | 1 | always on with goal Author |
| `exclude_played` | 1 | skip TMX ids found in saved runs of the same site |
| `auto_advance`, `advance_delay_s` | 1, 3 | after a goal medal |
| `replay_autosave` | **0** | **off: needs a host replay-save call (below); greyed out in the UI until then** |
| `hud_visible`, `hud_compact`, `hud_corner` | 1, 0, 0 | HUD; corner 0 top left, 1 top right |
| `thumb_source` | 0 | 0 TMX screenshot (map file fallback), 1 map file only, 2 off |

Hotkeys (registered with the host, rebind there): `skip` Ctrl+F9, `gold-skip` Ctrl+F10, `toggle-hud` Ctrl+F11, `next-map` (none).

## What the host must add / confirm

ABI 1.0 covers everything except these. The plugin works without them; the features are off and the UI says so.

1. **Textures for plugins** (thumbnails). Proposed, appended to `HorizonHostApi` (ABI minor 1):
   ```c
   /* RGBA8, top-down, pitch = w*4. Returns an ImTextureID for the host's ImGui renderer (IDirect3DTexture9*,
      D3DPOOL_MANAGED so it survives Reset), 0 on failure. Game thread only. */
   uint64_t (HORIZON_CALL* create_texture_rgba)(HorizonPluginHandle plugin, int32_t w, int32_t h, const void* rgba);
   void (HORIZON_CALL* release_texture)(HorizonPluginHandle plugin, uint64_t texture);
   ```
   Until then the plugin uses `host->device()` (if it returns the D3D9 device for a non-`_DEVICE` plugin) and creates a managed texture itself. **Please confirm `device()` is non-NULL for every plugin**, or add the two calls; if neither, there are simply no thumbnails.
2. **Replay save for plugins**:
   ```c
   /* Saves the replay of the run that just ended (valid from RACE_END until the map is left).
      name_utf8 is relative to the game's Replays folder ("LegacyRandomizer\\x.Replay.Gbx").
      Posts REPLAY_SAVED with the full path. */
   HorizonResult (HORIZON_CALL* save_replay)(HorizonPluginHandle plugin, const char* name_utf8);
   ```
   Until then `replay_autosave` is off; replays saved by other plugins (ESWC Autosave, `REPLAY_SAVED`) are still recorded in the run file.
3. **Queued callbacks**: `run_on_game_thread` callbacks of a plugin must run or be dropped *before* its `Shutdown` returns / the DLL is unloaded (the plugin guards against late calls, but its code must still be mapped).
4. **Events the flow relies on**: `RACE_START` after `MAP_LOADED` with `map->uid` filled (an empty uid is accepted as "ours"), `FINISH` with `play_mode` = solo, `RACE_END` before the end-race screen, `MENU_ENTERED`, and `get_game_state()` = `END_RACE` on the end-race screen (TMNF/TMUF leave only there).
5. `play_map_file(path_utf8)`: the plugin passes a full path in its data folder; gamectl copies it into the game's Tracks folder.

## Preview without the game

`build\test\ui_preview.exe` runs the real plugin code (everything except `host_horizon.cpp`) with a fake host: settings in `fake_host_settings.txt`, game control that only logs and simulates map load / quit, a "Fake game" panel (finish with AT/gold/silver/slow, restart, Esc › Quit, online on/off, +1/+10 min), F8 menu, Ctrl+F9/F10/F11 hotkeys. The ImGui style is TM Vibrant Shaders' (`src/overlay.cpp`) plus orange tab colours, standing in for the host style; the plugin itself never sets global colours (only the medal colours are its own).

`ui_preview.exe --shots docs\screenshots` plays a scripted run with real TMX maps (ESWC → nations.tm-exchange.com, about 15 requests): start page, settings, HUD (expanded/compact), live page, results, history, 1920×1080 PNGs. Options: `--game eswc|sunrise|original|tmnf|tmuf`, `--data <dir>`.

## Known limits

- Original: marked *experimental*; the host refuses map loading there today (`NOT_SUPPORTED` → the run ends with a message).
- `$w`/`$n` are drawn by scaling glyphs, `$i` by shearing, `$o` with the title font if the host gives one, else drawn twice; very dark colours are lifted for readability.
- Glyphs outside the host's font atlas show as `?`.
- Exclude-played compares TMX ids per site; maps played before this plugin existed are unknown.
