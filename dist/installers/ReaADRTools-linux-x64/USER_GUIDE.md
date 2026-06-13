# ReaADR Tools User Guide

ReaADR Tools helps build and run ADR sessions inside REAPER. It is intended for
voice actors, indie creators, ADR engineers, and studios that need timed cue
organization without leaving REAPER.

## Installation

1. Copy the contents of `dist/UserPlugins` into REAPER's `UserPlugins` folder.
2. Restart REAPER.
3. Use the top-level `ReaADR Tools` menu.

The menu contains `Open Manager` plus four configurable quick-action slots.
Open Manager is the main workspace. Quick actions can be changed from
`Open Manager > Preferences > Configure Quick Actions`. Restart REAPER after
changing quick actions if you want the native top-menu labels to refresh.

## Recommended REAPER Setup

For recording pre-roll, use REAPER's own pre-roll/metronome settings. A good
ADR default is 3 measures. ReaADR uses its own internal 3-second cue-aid timing
for beeps, streamers, and overlap splitting, but REAPER's native transport
pre-roll controls actual recording/playback pre-roll.

## Importing A Script

Use `ReaADR Tools > Open Manager > Import > Import Cue Sheet`. By default,
`Quick Action 1` also runs Import Cue Sheet.

Supported text formats:

- CSV
- TSV

Columns can appear in any order. ReaADR detects common headers such as:

- Cue ID: `cue_id`, `cue_number`, `cue`, `id`, `number`
- Character: `character`, `actor`, `speaker`, `role`
- Start: `start`, `start_time`, `timecode`, `tc`, `in_time`, `in`
- End: `end`, `end_time`, `out_time`, `out`
- Dialogue: `line`, `dialogue`, `text`, `script`

If ReaADR cannot identify required columns, it asks you to map source columns
to ADR fields. The last successful custom mapping is saved and reused as a
default.

Before modifying the project, ReaADR shows an import preview with cue counts,
characters, blank dialogue, metadata fields, and overlap splits.

## Generated Session Layout

Import creates:

- Cue regions
- Character dialogue tracks
- Character cue tracks
- Extra cue/dialogue tracks for overlapping pre-roll windows
- Ruler lanes per character
- Video overlay processor on the ADR Source Video track

If two cues for the same character are too close, ReaADR assigns the later cue
to a lane such as `ALEX #2`, with tracks like `Cue - ALEX 2` and `ALEX 2`.

## Cue Management

Open `ReaADR Tools > Open Manager`, then choose Cue Management.

Useful tools:

- Open Cue Manager
- Character Filter

The Cue Manager shows cue rows with cue ID, character, SMPTE start/end, status,
and dialogue. Select a row to jump to the start of its region. Use the Cue
Manager buttons to move previous/next, update cue status, edit cue details,
refresh overlays, or open the information panel for the selected cue.

Editable cue details include cue number, character, dialogue, direction, cue
type, and notes. Edits update the cached session, generated region, and video
overlay.

## Character Filter

Use Character Filter to focus a recording pass. It can mute inactive generated
cue/dialogue tracks. Overlap lanes are selectable separately, such as:

- `ALEX`
- `ALEX #2`

When `Hide inactive ruler regions` is enabled, inactive generated regions are
hidden and the video overlay is refreshed to omit inactive cues.

## Video Overlay

Open `Open Manager > Preferences > Overlay Settings`.

Overlay options include:

- Cue ID
- Character
- Cue SMPTE
- Timeline SMPTE
- Dialogue
- Direction
- Cue type
- Status
- Streamer/visual cue/flash
- Studio metadata

Profile buttons provide quick setups:

- Actor
- Engineer
- Studio
- Minimal

Studio metadata fields are edited as individual fields and shown only when the
imported cue has matching metadata. Common fields include `PGID`, `MID`,
`Media Time`, `Watermark Timestamp`, `Asset Date Code`, and `Project Name`.

## Reports

Use `Open Manager > Reports > Export Cue Sheet CSV` for a cue sheet export, or
assign `Export Reports` to a quick-action slot for the multi-report exporter.

Available reports:

- Cue sheet CSV
- Recording report CSV
- Timing report CSV
- Session metadata CSV

Recording reports include cue status and take counts. Timing reports include
SMPTE start/end and cue length. Metadata reports preserve extra imported
studio columns.

## Session Maintenance

Open Manager > Session Tools.

Available tools:

- Validate Session
- Refresh Video Overlay
- Rebuild Session From Cache
- Clean Generated Cue Items

These tools help recover from manual REAPER edits without reimporting the
original script.

## Help And Hints

Manager buttons show a short hint when hovered. Use `Open Manager > Help` to
search built-in guidance by action, workflow, or keyword. Example searches:
`import`, `overlay`, `SMPTE`, `filter`, and `reports`.
