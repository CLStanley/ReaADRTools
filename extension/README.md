# ReaADR Native Extension Wrapper

This wrapper keeps the ADR implementation in Lua and uses a small native REAPER extension for low-friction installation:

1. The extension registers the bundled Lua scripts as main-section REAPER actions.
2. It adds a top-level `ReaADR Tools` menu for import, export, cue navigation, cue generation, cleanup, and overlay settings.

## Build

Development builds require the REAPER SDK and WDL source tree:

```sh
mkdir -p vendor
git clone --depth 1 https://github.com/justinfrankel/reaper-sdk vendor/reaper-sdk
git clone --depth 1 https://github.com/justinfrankel/WDL vendor/WDL
```

```sh
cd extension
make dist
```

The distributable Linux x86_64 layout is created at:

```text
dist/UserPlugins/
  reaper_reaadr-x86_64.so
  ReaADRTools/
    assets/cue.wav
    scripts/*.lua
```

## Install

Copy the contents of `dist/UserPlugins` into the user's REAPER resource `UserPlugins` folder, then restart REAPER.

No manual ReaScript action import or menu customization is required.

## Keyboard Shortcuts

The extension registers each tool as a normal REAPER main-section action. Users
can assign or change shortcuts from `Actions > Show action list` by searching
for `ReaADR`.

Cue navigation actions:

- `Next Cue`
- `Previous Cue`
- `Jump To Cue`

## Export Cue Sheet

The `Export Cue Sheet` action can export either saved ReaADR cue data or
ordinary project markers/regions. It writes:

```text
cue_id,character,start,end,line,direction,cue_type,notes
```

Blank CSV columns are valid. For marker/region exports, names are interpreted
as:

- `AOI` -> character only
- `AOI: Fight them all off?` -> character plus dialogue
- `AOI - Fight them all off?` -> character plus dialogue
- empty name -> blank character and dialogue

This supports spotting cues in REAPER first, exporting a partial cue sheet,
finishing the script externally, and re-importing later.

## Platform Support

The Lua scripts and bundled cue asset are platform-neutral. The native REAPER extension must be built separately for each REAPER platform:

- Linux x86_64: `reaper_reaadr-x86_64.so` currently builds with this Makefile.
- macOS: requires a `.dylib` build for the target architecture, usually x86_64 and/or arm64.
- Windows: requires a `.dll` build, usually x64.

The wrapper source is structured for cross-platform path and menu handling, but only the Linux x86_64 build is currently verified in this repo.
