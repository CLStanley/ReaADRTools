# ReaADR Tools Code Architecture

This document is for maintainers working on ReaADR Tools.

## Design Direction

ReaADR is moving from its current Lua application framework to one native C++
REAPER extension. The migration is staged around the ADR Session Model so
existing projects and still-unmigrated features remain compatible. See
`CPP_MIGRATION.md` for the target architecture, sequencing, and cutover rules.

```text
Native REAPER extension
  Native manager and commands
  Application services
  REAPER adapters
  C++ ADR Session Model core
```

## Native Extension

File: `extension/reaper_reaadr.cpp`

Current responsibilities:

- Register the small public action set.
- Add the top-level `ReaADR Tools` menu.
- Unregister older standalone public actions.
- Launch Lua entry-point scripts.
- Provide stable native helper APIs where Lua/ReaScript is a poor fit:
  - `ReaADR_DetectDialogueSegments` reads selected media through REAPER audio
    accessors and returns timeline segment pairs.
  - `ReaADR_ReadXlsxAsTsv` extracts the first worksheet from `.xlsx` files and
    returns tab-delimited text for the Lua import pipeline.

New workflow logic should follow the target layering in `CPP_MIGRATION.md`.
During migration, keep REAPER API calls out of the standalone C++ domain core.

The first native core module is `extension/reaadr_core/session_model.*`. It
parses and serializes `adr_session_model_v1` without depending on REAPER and is
compiled into the extension.

The native foundation also includes `model_repository.*` for canonical
project-model access and `domain_utils.*` for status, stable-ID, and timecode
rules. REAPER-specific extstate and transaction adapters live under
`extension/reaadr_reaper/` so the domain modules remain host-independent.

Native import work continues in `cue_import.*`, which parses delimited text and
maps it into cue records, and `session_builder.*`, which derives every
cue-backed model collection. `session_mutation.*` preserves the model envelope
while replacing those derived collections, and `session_commit.*` provides the
model-only snapshot/save/revision/rollback sequence. `lane_assignment.*` keeps
model construction and visible rendering on one deterministic overlap rule.
`render_plan.*` compares canonical intent with an inspected project without
calling REAPER, while `track_region_adapter.*` owns the actual track/region host
calls. `render_artifact_adapter.*` handles ruler lanes and explicitly owned cue
audio, then coordinates all native render adapters under one transaction in
dependency order. `cue_wav.*` generates the project-local countdown asset, and
`session_render_service.*` keeps its model commit and complete visible render
inside one rollback-aware application workflow. `event_log.*` publishes that
workflow's operation-specific commit event (`SessionSaved` or
`CueTimingUpdated`) and `SyncFull` result to the same bounded project history
read by Lua. Publication warnings remain separate from commit failures so
callers do not retry an operation that already changed the model and project.
`character_filter.*` and `character_filter_adapter.*` load the existing
Lua-compatible selection, derive lane-aware mutations from the canonical model,
and reapply owned track mute and generated-region visibility state before the
outer render transaction completes. `region_timing_sync.*` is the explicit
reverse-sync exception to the model-first render direction: it accepts timing
only from uniquely matched generated region names, then
`session_render_service.*` rebuilds canonical derived records and visible cue
audio in the existing rollback-aware transaction. `cue_navigation.*` builds a
validated, Lua-compatible timeline catalog directly from the canonical model
and owns the paired manager/overlay selection keys. Its
`cue_navigation_service.*` application boundary resolves play/edit position and
moves the edit cursor without a model revision or Undo point. `record_arm.*`
defines complete single-target isolation intent, while
`record_arm_adapter.*` owns the transient REAPER track handles, revalidation,
compensating failure recovery, and idempotent restore/retry behavior needed by
the future native recording coordinator. `recording_setup.*` shares canonical
lane assignment with rendering, resolves cue/preroll timing and exact owned
recording-track intent, and `recording_setup_adapter.*` revalidates the chosen
track immediately before handing its non-owning handle to record-arm/transport
code. These services are not invoked directly by the UI yet; Lua remains the
public workflow layer until the relevant native command/UI wiring and in-REAPER
smoke tests are ready for coordinated cutover.

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
- Session Model serialization and focused cue mutation in project extstate.
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

Safety-critical cohesive helpers are split into small modules while preserving
the public `ReaADR` table:

- `ReaADR_Core_Persistence.lua` serializes and mutates the Session Model.
- `ReaADR_Core_Transactions.lua` owns nested-safe Undo and UI-refresh scopes.
- `ReaADR_Core_Ownership.lua` contains deletion-boundary predicates.
- `ReaADR_Record_Arm.lua` captures, isolates, and restores record-arm state for
  the transitional Lua recording UI; `record_arm.*` and
  `record_arm_adapter.*` provide its test-covered native replacement boundary.

## ADR Session Model

The canonical project data model is stored in project extstate as
`adr_session_model_v1`. It is a structured line-based model with these sections:

- `session`: session ID and name.
- `project_metadata`: project/studio metadata with escaped values.
- `timecode`: project timecode settings.
- `script`: imported script/source records.
- `character`: character records independent of cue rows.
- `cue`: production cue records.
- `track`: intended REAPER track mappings.
- `region`: intended region mappings.
- `import`: import registry records used for duplicate/revision safety.
- `state` and `dirty`: runtime state and dirty flags.

The old cue-cache and script-registry extstate records are no longer written.
Current test projects can be recreated, so the project now favors a clean
single-model architecture over preserving experimental data layouts.

Core rule: REAPER project state is rendered output. The ADR Session Model is the
source of truth. General refresh renders from the session model and does not
pull region positions from REAPER into the model. Explicit sync tools may still
read REAPER region edits back into the model, but that is a separate user action.

A valid model with zero cues is an empty model-backed session. A missing or
invalid model is distinct and is never populated implicitly from project
regions. `adopt_legacy_project_regions()` is the explicit compatibility bridge.

`save_session_cues()` is a compatibility wrapper around
`replace_session_cues()`. It loads the existing model, deep-copies caller cues,
and replaces only cue-derived collections. Session identity, project metadata,
script metadata, character state, import identity, timecode settings, runtime
state, dirty flags, and unknown record types remain intact where applicable.

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

## Session Data Fields

Session data is stored in project extstate using `adr_session_model_v1`.

Cue fields:

- `id`
- `character`
- `character_id`
- `script_id`
- `session_cue_id`
- `start_time`
- `end_time`
- `line`
- `notes`
- `direction`
- `cue_type`
- `source_line`
- `status`
- `region_id`
- `track_id`
- `metadata`

`metadata` preserves unknown/studio-specific imported columns.

Scripts, characters, track mappings, region mappings, import records, and
runtime dirty flags are stored beside cues in the same session model. Avoid
adding separate project extstate stores for ADR data unless the data is truly
UI-only or temporary.

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
7. The ADR Session Model is saved.
8. Tracks, regions, cue audio, ruler lanes, and overlay are rendered from the
   model.

Import and marker/region cue generation require an existing video item in the
project. `setup_project()` marks that track as `source_video` and installs the
Video Processor overlay there.

## Dialogue Detection Flow

`ReaADR_Detect_Dialogue.lua` analyzes the active take of the first selected
media item using REAPER audio accessor APIs. It detects threshold-based
speech-like regions, creates sequential editable cues, saves them to the ADR
Session Model, and calls the same project setup path used by imported cue
sheets. The generated cues are intentionally reviewable rather than final
transcription data.

Transcription-assisted cue generation is not implemented yet. Future
transcription work should add transcript text, confidence metadata, and speaker
placeholders to the Session Model before sync renders the cues into REAPER.

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
- Refresh Session, which rebuilds cue audio, generated regions, lanes, filters,
  and overlay state from the Session Model
- overlay refresh and character lane rebuilds

This keeps the migration to a richer UI layer from duplicating core workflow
logic.

## Performance Guidance

Prefer cached cue data over project-wide scans. Scan REAPER tracks/items only
when needed, such as take counting or cleanup.

Batch project edits use `ReaADR.with_project_transaction()`. Nested helpers join
the current operation; only the outer user-facing action calls:

- `reaper.Undo_BeginBlock`
- `reaper.Undo_EndBlock`
- `reaper.PreventUIRefresh(1)`
- `reaper.PreventUIRefresh(-1)`

`ReaADR.commit_session_cues()` owns model save plus full rendering for import,
generation, region-timing updates, and Cue Manager add/remove operations.
`sync_full()`, `rebuild_session_from_model()`, `setup_project()`, and character
filtering join an existing transaction and must not start nested undo blocks.
Failure closes the owned block before conditionally invoking REAPER Undo; the
undo description is checked first so an empty failed block cannot undo an
unrelated prior user operation.

Avoid adding new standalone public actions unless there is a strong workflow
reason. Prefer adding manager controls.

## Sync Direction

The current sync bridge is being consolidated behind explicit Sync Engine-style
core APIs:

- `ReaADR.sync_full(options)`
- `ReaADR.sync_incremental(change_set, options)`
- `ReaADR.sync_validate(change_set, options)`
- `ReaADR.detect_session_drift(options)`

`sync_full()` currently wraps `rebuild_session_from_model()` and `setup_project()`.
`sync_incremental()` handles small cue-region/lane/overlay updates for cue status
and cue field edits. `sync_validate()` is a dry-run validation surface.
`detect_session_drift()` compares the Session Model against ReaADR-owned tracks,
regions, and cue audio.

New code should route REAPER mutations through one of these core helpers instead
of writing tracks, regions, cue items, or overlay FX directly from UI scripts.
Cue sheet import, dialogue detection, marker/region cue generation, Cue Manager
add/remove, cue edits, and refresh now route through these sync APIs. The next
architecture step is to add richer drift resolution UI and broaden incremental
sync coverage. See `docs/ADDENDUM_IMPLEMENTATION_BACKLOG.md`.

Performance rule: keep drift detection on-demand. It scans project tracks,
regions, and generated cue items, so it should run from explicit checks or repair
workflows rather than every UI frame. Full refresh should avoid duplicate overlay
rebuilds; `setup_project()` already renders the overlay when overlay settings and
the source video track are available.

## Event Direction

The core now includes a small synchronous event layer:

- `ReaADR.emit_event(type, payload, options)`
- `ReaADR.process_event_queue(options)`
- `ReaADR.subscribe_event(type, handler)`
- `ReaADR.log_event(event)`

Events are immutable payload snapshots after dispatch and are logged in a
bounded project-local event log (`event_log_v1`, latest 200 entries). Bulk
workflows emit aggregate events such as `ScriptImported` or `BulkCueCreated`
instead of one event per cue. Sync validation, full sync, incremental sync,
refresh requests, and sync failures also emit typed events.

Open windows still stay consistent by polling `session_revision` and
re-querying the Session Model when it changes. This remains the compatibility
signal while UI surfaces migrate to event subscriptions.

New state-changing code should still call `ReaADR.bump_session_revision()` via
the existing save/update helpers so open windows do not operate on stale data.
New UI code can subscribe to typed events for narrower refreshes, but should
continue to tolerate `session_revision` changes until all windows have migrated.

## Reliability And Safety

The ADR Session Model is the source of truth for imported/generated ADR cues.
ReaADR-owned cue items, regions, ruler lanes, mappings, and video overlays are
rendered from that model and are safe to rebuild. Character recording tracks may
contain user recordings and are never considered disposable merely because the
track itself has a ReaADR role.

Risky session-changing operations should create a session snapshot before
writing Session Model state or deleting generated project artifacts. Current
protected paths include:

- Script import/update
- Dialogue detection session build
- Cue Manager add/remove cue flows
- Character-specific cue clearing

`ReaADR.create_session_snapshot()` stores the last model snapshot in project
extstate. `ReaADR.restore_session_model_snapshot()` restores model extstate only
and bumps the revision once so open windows re-query state. It does not restore
tracks, regions, items, or FX. `ReaADR.commit_session_cues()` combines a model
mutation with rendered project changes under one Undo-backed transaction.
`ReaADR.protected_session_operation()` remains available for callback workflows.

This is currently a last-operation model snapshot, not a snapshot history or
crash-recovery system. Full recovery work should add named restore points,
snapshot diffing, restore UI, autosave/crash detection, and full-sync-after-
restore behavior.

The native equivalents in `model_repository.*` and `session_commit.*` preserve
the same last-operation snapshot boundary. Native rollback additionally keeps
the compatibility `adr_session_id` index synchronized with the restored model.
They intentionally make no claim to restore rendered REAPER objects; that must
remain the responsibility of the outer transaction and renderer.

Destructive operations must remain scoped and confirmed. They should delete
only ReaADR-owned generated objects, identified by names or extstate such as
`ReaADR.role` and `ReaADR.cue_key`.

Import update mode builds the replacement session before removing stale
generated artifacts. Stale cleanup only targets old selected-character cues
that no longer exist in the replacement import, which avoids deleting newly
rebuilt cue items with matching cue IDs.

On a failed full sync, the outer transaction closes its undo block, invokes
REAPER Undo to restore project objects, restores the model snapshot, balances
UI refresh suppression, and logs the error. Cleanup predicates only match
ReaADR-owned roles and cue keys; recorded/user media is outside this boundary.

## Automated Checks

`tests/run.sh` runs deterministic Lua tests against fake REAPER APIs. GitHub
Actions validates Lua syntax, runs tests, shellchecks packaging/build scripts,
fetches pinned native dependencies, and validates serial and parallel builds.
The native dependency SHAs live in `extension/dependencies.lock`; the fetcher
verifies those revisions and refuses to replace a mismatched existing checkout.

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
