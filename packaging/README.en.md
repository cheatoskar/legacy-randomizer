# Legacy Randomizer {VERSION}

A Random Map Challenge (RMC) for old TrackMania games, inside the game. Press Start and the mod loads a random map from TM Exchange, you try to reach the goal medal, then the next map comes. It is a plugin for the Horizon Client.

## Supported games

- TrackMania Nations ESWC, unofficial 1.80 patch exe
- TrackMania Sunrise, Win10 repack. Sunrise: finish detection is not available yet. You have to skip maps manually.

Solo only. Nothing runs while you are online.

## Install

The "with Horizon Client" zip contains the Horizon Client and the plugin:

1. Unzip it into the game folder (next to TmNationsESWC.exe or TmSunrise.exe).
2. Double-click Horizon-Install.cmd. An existing dinput8.dll of another mod becomes dinput8.chain.dll and keeps working.
3. Start the game and press Insert.

If you already have the Horizon Client, use the "plugin" zip instead: unzip it into the game folder, so that the files land in horizon\plugins\legacy-randomizer\.

## Keys

- Insert: open the menu (start a run, filters, rules, HUD layout, history)
- Ctrl+F9: skip the map
- Ctrl+F10: gold skip (only with goal Author)
- Ctrl+F11: show/hide the HUD

## Modes and rules

- Challenge (RMC): collect as many goal medals as you can before the timer runs out.
- Survival (RMS): medals add time, skips cost time, the run ends at 0.
- Free play: no timer.
- Goal: Author, Gold, Silver, Bronze or Finished. A better medal counts too.
- Filters: environment, difficulty, length, mood, routes, name, author, upload date and more.
- Maps you played in earlier runs are not picked again (can be switched off).

## Uninstall

Delete the folder horizon\plugins\legacy-randomizer. To remove the whole Horizon Client: run Horizon-Uninstall.cmd, then delete the horizon folder.

## Please be kind to TMX

The mod asks the TMX API for one map at a time. Please do not start many runs in a loop and do not use it to download maps in bulk.

## Credits

- Maps and screenshots: TM Exchange (tm-exchange.com)
- The RMC idea: Flink
- Rules modelled after BigBang1112's RandomizerTMF (no code taken from it)
- Dear ImGui by Omar Cornut (MIT), see THIRD-PARTY.txt
- Legacy Randomizer is MIT licensed, see LICENSE.txt
