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

The extension should stay thin. Avoid moving workflow logic into C++ unless a
measured performance or integration limitation requires it.

## Lua Application Layer

File: `scripts/ReaADR_App.lua`

Responsibilities:

- Define manager modules.
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
- Preferences
- Help

## Core Session/REAPER Layer

File: `scripts/ReaADR_Core.lua`

Responsibilities:

- CSV/TSV parsing and column mapping.
- Cue cache serialization in project extstate.
- Timecode parsing/formatting.
- Track and region creation.
- Cue audio item generation.
- Overlay video processor generation.
- Character filtering state.
- Cue status handling.
- Validation/report export helpers.

Most shared behavior belongs here instead of inside feature scripts.

## Feature Scripts

Feature scripts should remain thin and call into `ReaADR_Core` or
`ReaADR_App`.

Examples:

- `ReaADR_Import_Cue_Sheet.lua`
- `ReaADR_Cue_Manager.lua`
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

1. User selects CSV/TSV.
2. Parser detects delimiter and headers.
3. Auto column mapping is attempted.
4. User maps fields manually if required.
5. Cues are validated.
6. User confirms import preview.
7. Tracks, regions, cue audio, ruler lanes, cache, and overlay are built.

## Overlay Flow

The overlay is generated as source code for REAPER's Video Processor FX on the
ADR Source Video track.

Overlay code is regenerated when settings change, cue status changes, character
filtering hides regions, or the user refreshes the overlay.

## Performance Guidance

Prefer cached cue data over project-wide scans. Scan REAPER tracks/items only
when needed, such as take counting or cleanup.

Batch project edits inside:

- `reaper.Undo_BeginBlock`
- `reaper.PreventUIRefresh(1)`
- `reaper.PreventUIRefresh(-1)`

Avoid adding new standalone public actions unless there is a strong workflow
reason. Prefer adding manager controls.

## Future UI Direction

Current UI uses REAPER `gfx`. ReaImGui should be evaluated for the next major
manager revision because cue tables, filters, status dropdowns, and import
mapping views will be easier to build and maintain there.
