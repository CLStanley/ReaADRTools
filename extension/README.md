# ReaADR Native Extension Wrapper

This wrapper keeps the ADR implementation in Lua and uses a small native REAPER extension for low-friction installation:

1. The extension registers a small set of application entry-point actions.
2. It adds a top-level `ReaADR Tools` menu for the unified manager, import, reports, and preferences.
3. Feature scripts remain bundled as internal Lua modules used by the manager.

## Application Framework

ReaADR is transitioning from many standalone actions into a manager-first Lua
application framework:

```text
ReaADR Tools Manager
  Import Module
  Cue Management Module
  Session Tools Module
  Reports Module
  Preferences Module
  ReaADR Core / REAPER API helpers
```

The current production implementation remains Lua/ReaScript. The native
extension is intentionally thin: it installs the top-level menu, registers the
limited public actions, and launches the Lua application.

Additional documentation:

- `docs/USER_GUIDE.md`
- `docs/CODE_ARCHITECTURE.md`

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

The extension registers a limited set of normal REAPER main-section actions.
Users can assign or change shortcuts from `Actions > Show action list` by
searching for `ReaADR`.

Public actions:

- `Open Manager`
- `Quick Action 1`
- `Quick Action 2`
- `Quick Action 3`
- `Quick Action 4`

Older standalone utility actions are unregistered by the extension and remain
available internally through `Open Manager`.

Quick actions are configurable from `Open Manager > Preferences > Configure
Quick Actions`. The native menu labels stay stable, but each slot can run
commonly used tools such as Import Cue Sheet, Cue Manager, Export Reports,
Overlay Settings, Character Filter, Refresh Video Overlay, or cue navigation.

The manager also includes hover hints and a Help tab with searchable workflow
guidance.

## Character Filter

`Character Filter` toggles which character cue/dialogue tracks are active for
focused recording passes. Inactive ReaADR cue/dialogue tracks are muted. The
filter does not affect cue navigation or export. The optional `Hide inactive
ruler regions` checkbox hides generated cue regions for inactive characters
using REAPER's per-region hidden flag and refreshes the video overlay to omit
those hidden inactive cues. REAPER does not expose a scriptable
whole-ruler-lane disable switch.

## Import Script

`Import Script` accepts comma-delimited CSV and tab-delimited TSV files. The
importer reads column headers, so columns can appear in any order.

Recognized default aliases include:

- Cue ID: `cue_id`, `cue_number`, `cue`, `id`, `number`
- Character: `character`, `actor`, `speaker`, `role`
- Start: `start`, `start_time`, `timecode`, `tc`, `in_time`, `in`
- End: `end`, `end_time`, `out_time`, `out`
- Dialogue: `line`, `dialogue`, `text`, `script`

If required columns cannot be detected automatically, ReaADR prompts for a
column mapping. The last successful custom mapping is saved and offered as the
default for future imports. Extra unmapped columns are preserved in the cached
cue metadata where possible, which supports studio fields such as `PGID`,
`MID`, `Watermark Timestamp`, and asset/date codes.

## Cue Information And Recording

The manager includes a `Cue Information Panel` under Cue Management. It shows
the current/next cue, character, SMPTE start/end, cue length, current timeline
position, countdown to cue start, line text, status, and current take count
based on recorded items on matching ReaADR character tracks.

For recording pre-roll, use REAPER's native project/metronome pre-roll
settings. A practical ADR default is 3 measures. ReaADR keeps its own internal
3-second cue-aid timing for streamers, beeps, and overlap splitting, but it no
longer exposes a separate pre-roll preference because that setting does not
control REAPER's transport pre-roll.

## Studio Metadata Overlay

Overlay settings include an optional `Studio metadata` display. The metadata
fields are edited as individual fields and stored internally as a
comma-separated list, for example:

```text
PGID,MID,Media Time,Watermark Timestamp,Asset Date Code,Project Name
```

Only imported metadata fields with values are shown.

## Reports

`Export Reports` can write:

- Cue sheet CSV
- Recording report CSV
- Timing report CSV
- Session metadata CSV

Recording reports include cue status and a take count. Timing reports include
SMPTE start/end and cue length. Session metadata reports preserve extra columns
imported from studio cue sheets.

## Export Cue Sheet

The `Export Cue Sheet` action can export either saved ReaADR cue data or
ordinary project markers/regions. It writes:

```text
cue_id,character,start,end,line,direction,cue_type,status,notes
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
