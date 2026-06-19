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
`Open Manager > Preferences` using the inline dropdowns. Open Manager can keep
up to three manager windows open at once.

If you enable `Open Manager > Preferences > Remember ReaADR window layout per project`,
ReaADR saves the size and placement of its utility windows in the current
project so they reopen where you left them.

If your desktop environment makes manual docking difficult, enable
`Open Manager > Preferences > Open Cue Manager docked`. This opens Cue Manager
with the dock-capable fallback window and asks REAPER to place it in a docker
programmatically.

In the overlay settings, informational text can be switched between white and
yellow. Cue status keeps its status color, while cue type and character labels
use the normal overlay text color.

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

Supported import formats:

- CSV
- TSV
- Excel `.xlsx`
- Google Sheets CSV/TSV exports
- Plain text delimited tables using comma or tab separators

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

For best `.xlsx` imports, use a simple first worksheet with one header row and
cue data below it. Formulas should be saved with calculated values by Excel,
LibreOffice, or Google Sheets before import.

Sample import files for each supported format are available in
`docs/test docs/`.

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
cue type, dialogue, and notes. ReaADR now prefers a ReaImGui-based Cue Manager
when ReaImGui is installed, and falls back to the legacy REAPER `gfx` version
if it is not.
Select a row to jump to the start of its region. Use the Cue Manager buttons to
jump by cue number, move previous/next, add cues at the current timeline
position, remove cues, open Character Filter, refresh overlays, or open the
information panel for the selected cue. If Character Filter is applied while
Cue Manager is open, Cue Manager updates its visible cue list to show only
active character targets.

Double-click a cell in Cue Manager to edit that field inline. Status and cue
type open inline dropdowns. Text and time fields become inline text editors.
Press `Enter` or click away to commit text changes back to the cached session,
generated region, and overlay. This is intended to make Cue Manager the main
script-building workspace instead of forcing every small edit through a
separate panel.

Cue Manager includes a Notes column. If dialogue or notes text is too long for
the table cell, hover the cell to preview the full text. In the legacy `gfx`
Cue Manager, the hover preview now wraps and expands near the cursor, and it
can be disabled from `Open Manager > Preferences`.

If you manually move or resize generated regions in REAPER, press `Refresh
Session` in Cue Manager. This applies the current region positions back to
ReaADR's cue cache, rebuilds cue beep items, reapplies character filters,
reapplies overlap lane logic, updates ruler lanes such as `ALEX #2`, refreshes
open ADR windows through session state, and refreshes the overlay.

If you remove a cue from Cue Manager, ReaADR deletes the cached cue, rebuilds
generated regions and cue audio, and renumbers the remaining cues in order.

Cue Information Panel is now a read-only detail view for the selected cue. Use
it when you want a larger dialogue/notes display plus timing, countdown, take
count, status, and cue type context without changing the cue there.

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

Each text field can also use an optional black background panel. Dialogue keeps
its dark backing by default, and the other overlay fields can enable their own
backgrounds individually from the Video Overlays tab. The former performance
direction overlay slot now displays cue notes.

Profile buttons provide quick setups:

- Actor
- Engineer
- Studio
- Minimal

Studio metadata fields are edited as individual fields and shown only when the
imported cue has matching metadata. Common fields include `PGID`, `MID`,
`Media Time`, `Watermark Timestamp`, `Asset Date Code`, and `Project Name`.

Long dialogue lines are automatically wrapped into multiple centered lines so
they stay readable on screen instead of running off the frame.

## Reports

Use `Open Manager > Reports > Export Cue Sheet CSV` for a cue sheet export, or
assign `Export Reports` to a quick-action slot for the multi-report exporter.

Available reports and exports:

- Cue sheet CSV
- Recording report CSV
- Timing report CSV
- Session metadata CSV
- Full session JSON
- EDL (CMX 3600)

Recording reports include cue status and take counts. Timing reports include
SMPTE start/end and cue length. Metadata reports preserve extra imported
studio columns.

Future export investigations:

- AAF

## Session Maintenance

Open Manager > Session Tools.

Available tools:

- Validate Session
- Rebuild Session From Cache
- Clear Character Cues
- Toggle QA Mode

These tools help recover from manual REAPER edits without reimporting the
original script.

ReaADR keeps a project-local snapshot of the last cue cache before risky
session-changing operations such as imports, detected-dialogue builds, and Cue
Manager add/remove flows. If one of those workflows fails during rebuild,
ReaADR restores the cached session state and reports the error instead of
silently leaving the session cache half-updated.

## Help And Hints

Manager buttons show a short hint when hovered. Use `Open Manager > Help` to
search built-in guidance by action, workflow, or keyword. Example searches:
`import`, `overlay`, `SMPTE`, `filter`, and `reports`.
