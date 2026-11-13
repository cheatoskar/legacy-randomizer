# Contributing

Bug reports, ideas and pull requests are welcome.

## Reporting a bug

Open an [issue](../../issues) and include:

- the game and its version (and which exe, if you know it)
- the version of Legacy Randomizer and of Horizon Client
- what you did, what you expected and what happened
- the log of that start from the Horizon Client: `horizon\logs\` in the game folder. The plugin runs inside the Horizon Client and writes into its log.

A screenshot or a short clip helps. Search the existing issues first.

## Feature ideas

Open an issue and describe the problem you want solved. Check the rules below first: some things are out of scope on purpose.

## Pull requests

Pull requests are welcome. Keep them small and focused: one fix or one feature per pull request. For a larger change, open an issue first so the direction is clear before you spend time on it.

In the description, say what changed and how you tested it in the game (game, version, what you did). Rule or filter changes: say which game and TMX site you tried them on.

## Build and test

MSVC Build Tools (x86) at the path set in `build.cmd`, as in the [README](README.md#building):

```
build.cmd
build\test\core_test.exe          # add --live for a few live TMX requests
scripts\package.ps1                # release zips
```

`build.cmd` builds the plugin (`dist\legacy_randomizer_plugin.dll`), the test and the preview tool `build\test\ui_preview.exe`.

## Be kind to TMX

The plugin asks the TMX API for one map at a time. Changes must not make it hit the API faster or download maps in bulk (see the README, "Please be kind to TMX").

## Rules for in-game mods

These mods run inside the game, also in online play, so a few things are out of scope. A pull request that does any of this will not be merged:

- features that give a competitive advantage online or on leaderboards
- anything that touches physics, inputs or timing of the simulation, or edits runs and replays
- checkpoint or trigger display, ideal or brake line, disabling the camera or MediaTracker behaviour
- anything that gets around the Competition Patch or other anti-cheat and replay validation

The plugin is `solo-only` and stops while the game is online. The rules of the Horizon Client apply to it: [Horizon Client README, safety rules](https://github.com/cheatoskar/horizon-client#safety-rules). The rules come from what the TrackMania Exchange moderators accept: [TMX rules for mods](https://github.com/cheatoskar/trackmania-internals/blob/main/modding/tmx-rules-for-mods.md).

## AI-assisted contributions

Contributions written with AI tools are fine, as long as the contributor has tested them and can explain what the change does. Untested generated code will be sent back.
