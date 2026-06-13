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

Place the video in the timeline before importing scripts, generating cues, or
adding cues from Cue Manager. ReaADR attaches its video overlay to an existing
video track and will stop with an error if no video item is found.

Set the REAPER project frame rate to match the picture before building cues.
ReaADR generates a project-local `reaadr_cue.wav` using that frame rate:
three 1000 Hz beeps, each one frame long, spaced 1 second apart, with 1 second
of silence after the third beep before the cue point. If the project already
has a correctly timed `reaadr_cue.wav`, ReaADR reuses it during later imports
or rebuilds.

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

## Detecting Dialogue From Media

Use `Open Manager > Import > Detect Dialogue From Selected Media` after
selecting one audio or video item. ReaADR analyzes the selected item's audio,
detects speech-like regions, assigns cue numbers, and builds editable ADR cues.

Detection settings:

- Character: default character name for generated cues.
- Threshold dB: lower values detect quieter speech.
- Min speech sec: shortest accepted dialogue region.
- Min silence sec: silence needed to split one cue from the next.
- Pad sec: extra time added around detected speech.

Detected cues use the same overlap handling as imported scripts, so close cues
can create additional cue/dialogue lanes. Review and edit detected cues in Cue
Manager and Cue Information Panel.

## Generated Session Layout

Import creates:

- Cue regions
- Character dialogue tracks
- Character cue tracks
- Extra cue/dialogue tracks for overlapping pre-roll windows
- Ruler lanes per character
- Video overlay processor on the ADR Source Video track
- Project-local `reaadr_cue.wav` in the REAPER project folder

If two cues for the same character are too close, ReaADR assigns the later cue
to a lane such as `ALEX #2`, with tracks like `Cue - ALEX 2` and `ALEX 2`.

For recording multiple takes, use REAPER's native take system on the generated
character dialogue tracks. ReaADR does not pre-create `Take 1`, `Take 2`, etc.
tracks by default because native takes are better for comping, take selection,
and keeping recordings tied to the same cue.

## Cue Management

Open `ReaADR Tools > Open Manager`, then choose Cue Management.

Useful tools:

- Open Cue Manager

The Cue Manager shows cue rows with cue ID, character, SMPTE start/end, status,
and dialogue. Select a row to jump to the start of its region. Use the Cue
Manager buttons to jump by cue number, move previous/next, add cues at the
current timeline position, open Character Filter, refresh overlays, or open the
information panel for the selected cue. The selected cue status field is a
dropdown; changing it refreshes the cue data and overlay. If Character Filter
is applied while Cue Manager is open, Cue Manager updates its visible cue list
to show only active character targets.

If you manually move or resize generated regions in REAPER, press `Sync Regions`
in Cue Manager. This applies the current region positions back to ReaADR's cue
cache, rebuilds cue beep items, reapplies overlap lane logic, updates ruler
lanes such as `ALEX #2`, and refreshes the overlay.

Cue details are edited from the Cue Information Panel. Editable fields include
cue number, character, dialogue, direction, cue type, and notes. Edits update
the cached session, generated region, and video overlay while the window stays
open. The edit mode expands to show larger dialogue and notes fields for long
lines or monologues.

Double-click a cue row in Cue Manager to open that cue directly in edit mode.
When opened this way, the Cue Information window closes automatically after a
successful save.

## Character Filter

Use Character Filter to focus a recording pass. It can mute inactive generated
cue/dialogue tracks. Overlap lanes are selectable separately, such as:

- `ALEX`
- `ALEX #2`

When `Hide inactive ruler regions` is enabled, inactive generated regions are
hidden and the video overlay is refreshed to omit inactive cues.

## Video Overlay

Open `Open Manager > Video Overlays`.

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
- Rebuild Session From Cache
- Clean Generated Cue Items

These tools help recover from manual REAPER edits without reimporting the
original script.

## Help And Hints

Manager buttons show a short hint when hovered. Use `Open Manager > Help` to
search built-in guidance by action, workflow, or keyword. Example searches:
`import`, `overlay`, `SMPTE`, `filter`, and `reports`.
