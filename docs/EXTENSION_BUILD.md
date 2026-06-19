# ReaADR Extension Build Notes

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
  Video Overlays Module
  Preferences Module
  ReaADR Core / REAPER API helpers
```

The current production implementation remains Lua/ReaScript. The native
extension is intentionally thin: it installs the top-level menu, registers the
limited public actions, and launches the Lua application.

Additional documentation:

- `USER_GUIDE.md`
- `CODE_ARCHITECTURE.md`

## Build

Development builds require the REAPER SDK and WDL source tree:

```sh
mkdir -p vendor
git clone --depth 1 https://github.com/justinfrankel/reaper-sdk vendor/reaper-sdk
git clone --depth 1 https://github.com/justinfrankel/WDL vendor/WDL
```

### Linux (x86_64)

Linux builds use the Makefile in `extension/`.

```sh
cd extension
make dist
```

The distributable Linux x86_64 layout is created at:

```text
dist/UserPlugins/
  reaper_reaadr-x86_64.so
  ReaADRTools/
    assets/
    scripts/*.lua
```

### Windows (x64)

Windows builds require MSVC.

1. Open **x64 Native Tools Command Prompt for VS 2022** (or later)
2. Navigate to the `extension` directory
3. Run the build script:

```bat
build-windows-msvc.bat
```

This produces `dist\UserPlugins\reaper_reaadr.dll`.

The distributable Windows layout:

```text
dist/UserPlugins/
  reaper_reaadr.dll
  ReaADRTools/
    assets/
    scripts/*.lua
```

After building, copy the updated Lua scripts from the repository's `scripts/` folder into `dist/UserPlugins/ReaADRTools/scripts/` (or directly into your REAPER `UserPlugins/ReaADRTools/scripts/` after install) to ensure the latest script versions are used.

## Install

Copy the contents of `dist/UserPlugins` into the user's REAPER resource `UserPlugins` folder, then restart REAPER.

**Windows**: After running `build-windows-msvc.bat` in the x64 Native Tools Command Prompt for VS 2022, copy the generated `reaper_reaadr.dll` and the `ReaADRTools/` folder (with updated scripts from the repo's `scripts/` directory) into your REAPER `UserPlugins` folder.

No manual ReaScript action import or menu customization is required.

> **Note on script location**: Currently scripts are bundled inside `UserPlugins/ReaADRTools/scripts/` for self-contained distribution. Future versions may follow REAPER's standard structure where scripts reside in the REAPER resource `Scripts/` directory (outside `UserPlugins`), with the extension only providing the native DLL and assets. This would allow users to manage/update scripts via ReaPack independently of the native extension.

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

Quick actions are configurable from `Open Manager > Preferences` with inline
dropdowns. Each slot can run commonly used tools such as Import Cue Sheet,
Cue Manager, Export Reports, Overlay Settings, Character Filter, Refresh Video
Overlay, or validation.

Video overlay settings are also available directly inside `Open Manager > Video
Overlays`, which is the preferred UI path.

The manager also includes hover hints and a Help tab with searchable workflow
guidance, and the launcher supports up to three concurrent manager windows.

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

The project must already contain a video item. ReaADR uses the existing video
track as the overlay target instead of creating an empty video track.

`Detect Dialogue From Selected Media` can also create a cue session from one
selected audio/video item. It uses threshold-based audio detection to create
editable cue regions and then runs the same overlap, cue audio, and overlay
setup path as script import.

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

Cue audio is generated per project as `reaadr_cue.wav` using the project frame
rate. The generated cue WAV contains three one-frame beeps spaced 1 second
apart, with 1 second of silence after the third beep before the cue point.
Existing correctly timed project cue WAV files are reused so repeated imports
do not duplicate the project-local cue audio.

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
- Full session JSON
- EDL (CMX 3600)

Recording reports include cue status and a take count. Timing reports include
SMPTE start/end and cue length. Session metadata reports preserve extra columns
imported from studio cue sheets.

## Export Cue Sheet

The `Export Cue Sheet` action can export either saved ReaADR cue data or
ordinary project markers/regions. It writes:

```text
cue_id,character,start_smpte,end_smpte,start_time,end_time,line,direction,cue_type,status,notes,metadata
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

- **Linux x86_64**: `reaper_reaadr-x86_64.so` builds with `extension/Makefile`.
- **Windows x64**: `reaper_reaadr.dll` builds only via `build-windows-msvc.bat` in the x64 Native Tools Command Prompt for VS 2022.
- **macOS**: requires a `.dylib` build for the target architecture (x86_64 and/or arm64) — not yet verified.

The wrapper source is structured for cross-platform path and menu handling. Linux and Windows builds are documented; macOS is untested.
