# ReaADR Tools Code Architecture

This document is for maintainers working on ReaADR Tools.

## Design Direction

ReaADR is moving from a collection of independent scripts to a Lua application
framework hosted by a thin native REAPER extension.

```text
ReaADR Tools Manager
  Import module
  Cue management module
  Session tools module
  Reports module
  Preferences module
  Help module
  ReaADR_Core
  REAPER API
```

## Native Extension

File: `extension/reaper_reaadr.cpp`

Responsibilities:

- Register the small public action set.
- Add the top-level `ReaADR Tools` menu.
- Unregister older standalone public actions.
- Launch Lua entry-point scripts.
- Provide stable native helper APIs where Lua/ReaScript is a poor fit:
  - `ReaADR_DetectDialogueSegments` reads selected media through REAPER audio
    accessors and returns timeline segment pairs.
  - `ReaADR_ReadXlsxAsTsv` extracts the first worksheet from `.xlsx` files and
    returns tab-delimited text for the Lua import pipeline.

The extension should stay thin. Avoid moving workflow logic into C++ unless a
measured performance or integration limitation requires it.

## Lua Application Layer

File: `scripts/ReaADR_App.lua`

Responsibilities:

- Define manager modules.
- Dispatch up to three concurrent manager instances through dedicated launcher scripts.
- Route manager buttons to scripts or application actions.
- Store and execute configurable quick-action menu slots.
- Provide hover hint text and searchable built-in help topics.
- Provide report export selection.
- Provide session maintenance actions.

Current module groups:

- Import
- Cue Management
- Session Tools
- Reports
- Video Overlays
- Preferences
- Help

## Core Session/REAPER Layer

File: `scripts/ReaADR_Core.lua`

Responsibilities:

- CSV/TSV parsing and column mapping. `.xlsx` imports use the native
  `ReaADR_ReadXlsxAsTsv` helper, then use the same Lua parsing/mapping path as
  other delimited files.
- Cue cache serialization in project extstate.
- Timecode parsing/formatting.
- Project-scoped UI state such as window geometry/layout preferences.
- Track and region creation.
- Project-local cue WAV generation and cue audio item placement.
- Overlay video processor generation.
- Character filtering state.
- Cue status handling.
- Cached cue add/update/remove helpers.
- Validation/report export helpers.

Most shared behavior belongs here instead of inside feature scripts.

## Feature Scripts

Feature scripts should remain thin and call into `ReaADR_Core` or
`ReaADR_App`.

Examples:

- `ReaADR_Import_Cue_Sheet.lua`
- `ReaADR_Detect_Dialogue.lua`
- `ReaADR_Cue_Manager.lua`
- `ReaADR_Cue_Manager_ImGui.lua`
- `ReaADR_Cue_Manager_Gfx.lua`
- `ReaADR_Character_Filter.lua`
- `ReaADR_Overlay_Settings.lua`
- `ReaADR_Export_Cue_Sheet.lua`
- `ReaADR_Quick_Action_1.lua` through `ReaADR_Quick_Action_4.lua`

Quick-action wrappers should stay tiny. They delegate to
`App.run_quick_action(slot)` so users can customize what each top-menu slot
runs without adding more public REAPER actions.

## Session Data Model

Cached cues are stored in project extstate using `last_import_cues_v1`.

Cue fields:

- `id`
- `character`
- `start_time`
- `end_time`
- `line`
- `notes`
- `direction`
- `cue_type`
- `source_line`
- `status`
- `metadata`

`metadata` preserves unknown/studio-specific imported columns.

## Generated Object Ownership

Generated REAPER objects should be identifiable by name and/or extstate. The
cleanup tools must avoid deleting user recordings.

Track roles include:

- `source_video`
- `cue_character`
- `character`

Generated cue audio items use media item extstate:

- `ReaADR.role = cue_audio`
- `ReaADR.cue_key`

## Import Flow

1. User selects CSV, TSV, or another plain text comma/tab-delimited table.
2. Parser detects delimiter and headers.
3. Auto column mapping is attempted.
4. User maps fields manually if required.
5. Cues are validated.
6. User confirms import preview.
7. Tracks, regions, cue audio, ruler lanes, cache, and overlay are built.

Import and marker/region cue generation require an existing video item in the
project. `setup_project()` marks that track as `source_video` and installs the
Video Processor overlay there.

## Dialogue Detection Flow

`ReaADR_Detect_Dialogue.lua` analyzes the active take of the first selected
media item using REAPER audio accessor APIs. It detects threshold-based
speech-like regions, creates sequential editable cues, saves them to the normal
session cache, and calls the same project setup path used by imported cue
sheets. The generated cues are intentionally reviewable rather than final
transcription data.

## Overlay Flow

The overlay is generated as source code for REAPER's Video Processor FX on the
ADR Source Video track.

Overlay code is regenerated when settings change, cue status changes, character
filtering hides regions, or the user refreshes the overlay.

Overlay settings now include per-field background toggles for the major text
elements. Dialogue is wrapped into multiple lines before code generation so
long cue text remains visible in-frame. The legacy direction overlay slot is
now used to display cue notes when notes are present.

## Cue Manager UI Flow

`ReaADR_Cue_Manager.lua` is now a lightweight entry point. It prefers the
ReaImGui manager and falls back to the legacy `gfx` manager if ReaImGui is not
available or the newer UI fails during startup.

The shared cue-management behaviors remain in `ReaADR_Core.lua`:

- session cue loading and filtering
- selected cue tracking
- cue add/update/remove operations
- Refresh Session, which syncs generated region positions back into cached cues
  and rebuilds cue audio, lanes, filters, and overlay state
- overlay refresh and character lane rebuilds

This keeps the migration to a richer UI layer from duplicating core workflow
logic.

## Performance Guidance

Prefer cached cue data over project-wide scans. Scan REAPER tracks/items only
when needed, such as take counting or cleanup.

Batch project edits inside:

- `reaper.Undo_BeginBlock`
- `reaper.PreventUIRefresh(1)`
- `reaper.PreventUIRefresh(-1)`

Avoid adding new standalone public actions unless there is a strong workflow
reason. Prefer adding manager controls.

## Reliability And Safety

The cached session model is the source of truth for imported/generated ADR
cues. REAPER tracks, cue items, regions, ruler lanes, and video overlays are
rendered from that model and are safe to rebuild.

Risky session-changing operations should create a session snapshot before
writing cache state or deleting generated project artifacts. Current protected
paths include:

- Script import/update
- Dialogue detection session build
- Cue Manager add/remove cue flows
- Character-specific cue clearing

`ReaADR.create_session_snapshot()` stores the last cache/registry snapshot in
project extstate. `ReaADR.restore_session_snapshot()` restores it and bumps the
session revision so open windows re-query state. `ReaADR.protected_session_operation()`
is available for new workflows that need snapshot/restore and structured
logging around a callback.

Destructive operations must remain scoped and confirmed. They should delete
only ReaADR-owned generated objects, identified by names or extstate such as
`ReaADR.role` and `ReaADR.cue_key`.

Import update mode builds the replacement session before removing stale
generated artifacts. Stale cleanup only targets old selected-character cues
that no longer exist in the replacement import, which avoids deleting newly
rebuilt cue items with matching cue IDs.

## Supported File Types

Current imports:

- CSV
- TSV
- Excel `.xlsx`
- Google Sheets CSV/TSV exports
- Plain text comma/tab-delimited tables, including `.txt`

Import test fixtures live in `docs/test docs/`.

Future imports:

- Additional workbook sheet selection and mapping presets by studio template.

Current exports:

- CSV
- Full session JSON
- EDL (CMX 3600)

Future export investigations:

- AAF

## UI Direction

The manager stack is now hybrid:

- ReaImGui-first for Cue Manager
- `gfx` for the legacy Cue Manager fallback and the remaining utility windows

The intended direction is to continue moving complex editing surfaces toward
ReaImGui while keeping workflow logic in `ReaADR_Core.lua` so both UI layers
can share the same session behavior during migration.
