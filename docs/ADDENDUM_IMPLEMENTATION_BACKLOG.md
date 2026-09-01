# Addendum Implementation Backlog

This document tracks the remaining engineering work from SRS addendums E through
J. It is intentionally implementation-oriented: each item should either become a
code change, a user-visible workflow, or a documented constraint.

## Current Status Summary

| Addendum | Status | Notes |
| --- | --- | --- |
| E - Recording loop and transcription | Partial | Pre-roll repeat behavior is mostly implemented. Transcription remains future work; selected-media detection is currently threshold-based speech detection. |
| F - Reliability and integrity | Mostly implemented | Model-first import/update, ownership-scoped deletion, model snapshots, centralized Undo rollback, and QA logging exist. Durable logs need expansion. |
| G - Session state model | Mostly implemented | `adr_session_model_v1` is the source of truth for imported/generated ADR data. Some compatibility paths still read REAPER state directly. |
| H - Sync engine | Partial | Initial sync APIs exist and are used by refresh, cue edits, import, detection, and cue generation. Exact-ownership region timing sync is implemented in the C++ domain/application layers but not cut over in the UI. Remaining work is drift resolution UI, merge handling, and broader incremental sync coverage. |
| I - Event system | Partial | Lua provides the synchronous queue/subscriptions, and both Lua and C++ now publish to one bounded project-local event log. UI windows still use `session_revision` polling. |
| J - Recovery and snapshots | Partial | Last-operation model snapshots and Undo-backed render rollback exist. Full snapshot history, restore UI, diffing, autosave, and crash recovery are not implemented. |

## Phase 1 - Safety Fixes And Documentation

- Keep `Refresh Session` routed through `ReaADR.refresh_session()` so snapshot,
  logging, undo, and manager return data stay consistent.
- Keep destructive character clearing protected by a session snapshot before
  model mutation and generated-artifact removal.
- Preserve and isolate complete record-arm state for Record Current Cue. (Done
  in Lua and implemented behind the test-covered native record-arm boundary;
  recording UI/transport cutover remains.)
- Resolve selected cue timing, overlap lane, preroll window, and owned recording
  track in the native domain/adapter boundary. (Implemented and test-covered;
  REAPER action execution and recording UI cutover remain.)
- Keep recording transport sequencing in a host-independent native transition
  engine. (Implemented and test-covered for preroll, punch-in, loop restart,
  loop preferences, stop/abort, arm restoration, and take finalization; the
  ordered REAPER executor and failed-start compensation are also implemented;
  canonical Recorded-status commit, CueUpdated publication, preference writes,
  overlay rollback, and retry coordination are implemented. Native overlay-FX
  ownership/mutation adaptation is implemented with failed-update compensation;
  complete overlay settings persistence, EEL generation, and native input
  composition and the native refresh-action REAPER callback binding are also
  implemented. Deferred recording UI wiring remains.)
- Port generated-cue cleanup ownership predicates to the native boundary.
  (The exact selected-character cleanup plan and transactional REAPER adapter
  are implemented and test-covered; public command wiring and canonical model
  persistence remain.)
- Keep model save and generated-project rendering in one Undo-owned operation.
  (Done for import, generation, region timing, Cue Manager add/remove, refresh,
  setup, filtering, and character clearing. The native full-render coordinator
  now reapplies persisted filter state inside this same operation.)
- Document current selected-media behavior as speech-region detection, not
  transcription.
- Document the current recovery model as last-operation rollback, not snapshot
  history.

## Phase 2 - Sync Engine Boundary

Create a sync module/API inside the Lua layer before adding more sync features.
Initial public functions now live in `scripts/ReaADR_Core.lua`:

- `ReaADR.sync_full(options)`
- `ReaADR.sync_incremental(change_set, options)`
- `ReaADR.sync_validate(change_set, options)`
- `ReaADR.detect_session_drift(options)`

Drift resolution now has an initial API:

- `ReaADR.resolve_session_drift(choice, options)`

The first supported user-facing choice is refresh/repair from saved session data.
Merge/preserve is intentionally rejected until recovery and event tracking are
stronger.

Initial migration targets:

- Move full rebuild logic out of direct UI paths and behind `sync_full`. (Done:
  `refresh_session()` and user-facing rebuild paths call the sync boundary.)
- Move cue status/field update rendering behind `sync_incremental`. (Started:
  cue status and cached cue edits now use `sync_incremental()`.)
- Move import preview validation behind `sync_validate`. (Started: cue sheet
  import and dialogue detection preview now use `sync_validate()`.)
- Move import/generation build paths behind `commit_session_cues()` and
  `sync_full()`. (Done for cue sheet import, dialogue detection, marker/region
  cue generation, region timing updates, and Cue Manager add/remove.)
- Port explicit region timing adoption to the native domain and render
  coordinator. (Implemented and test-covered; native UI cutover and in-REAPER
  smoke testing remain.)
- Port canonical cue navigation and paired manager/overlay selection state to
  native services. (Implemented and test-covered; native Next/Previous/Jump
  actions are now bound, while in-REAPER smoke testing remains.)
- Return one consistent summary shape:
  - `sync_type`
  - `session_id`
  - `affected_cues`
  - `tracks_created`
  - `tracks_reused`
  - `regions_created`
  - `regions_updated`
  - `cue_audio_created`
  - `cue_audio_updated`
  - `duration_seconds`

Drift detection should compare the Session Model against ReaADR-owned REAPER
objects:

- Missing generated tracks by role/key. (Started.)
- Missing cue regions by generated name. (Started.)
- Cue regions whose start/end differ from model timing. (Started.)
- Unexpected cue audio items with `ReaADR.role = cue_audio`. (Started.)
- Missing cue audio for expected cues. (Started.)

Default resolution should be saved-session override. Preserve/merge should be an
explicit user action and should remain unavailable until it can be made
recoverable.

## Phase 3 - Event System

Replace ad hoc direct calls and revision-only polling with a small typed event
system. The first implementation can remain project-local and synchronous.

Minimum API:

- `ReaADR.emit_event(type, payload, options)` (Started.)
- `ReaADR.process_event_queue(options)` (Started.)
- `ReaADR.subscribe_event(type, handler)` (Started.)
- `ReaADR.log_event(event)` (Started.)

Minimum event fields:

- `event_id`
- `event_type`
- `timestamp`
- `session_id`
- `source`
- `payload`
- `batch_id`

Initial event types:

- `CueCreated`
- `CueUpdated`
- `CueDeleted`
- `ScriptImported`
- `BulkCueCreated`
- `CharacterFilterChanged`
- `RefreshRequested`
- `SyncFull`
- `SyncIncremental`
- `SyncValidation`
- `ErrorOccurred`
- `StateConflictDetected`

Current event coverage:

- Native session rendering publishes Lua-compatible `SessionSaved` and
  `SyncFull` records with the shared monotonic event counter and retention cap.
- Session saves emit one aggregate event with cue count and revision.
- Cue status/edit/add/remove paths emit cue-level change events.
- Import, dialogue detection, and marker/region cue generation emit aggregate
  events and sync events rather than one event per cue.
- Sync validation, full sync, incremental sync, refresh requests, and sync errors
  emit traceable events.
- The event log is capped to the latest 200 events in project extstate to keep
  project files from growing indefinitely.

Batch requirements:

- Importing or detecting hundreds of cues should emit one aggregate session event
  plus one sync event, not one sync per cue.
- UI selection/filter changes should update project state once per committed
  change, not on every paint frame.

The existing `session_revision` remains as a compatibility signal while open
windows migrate to event subscriptions.

## Phase 4 - Recovery System

Extend the current single last-snapshot helper into a snapshot history.

Snapshot data model:

- `snapshot_id`
- `session_id`
- `label`
- `timestamp`
- `model_blob`
- `event_log_pointer`
- `sync_version`
- `reason`

Required APIs:

- `ReaADR.create_session_snapshot(label, options)`
- `ReaADR.list_session_snapshots(options)`
- `ReaADR.load_session_snapshot(snapshot_id)`
- `ReaADR.diff_session_snapshot(snapshot_id, options)`
- `ReaADR.restore_session_snapshot_by_id(snapshot_id, options)`
- `ReaADR.create_restore_point(label)`

Restore rules:

- Create a pre-restore safety snapshot before applying a restore.
- Restore the Session Model first.
- Trigger full sync after restore.
- If full sync fails, restore the pre-restore safety snapshot and surface the
  error.

User-facing workflows:

- Create Restore Point.
- View Restore Points.
- Preview Snapshot Diff.
- Restore Selected Snapshot.

Crash recovery can be added after snapshot history exists:

- Mark clean open/close state in project extstate.
- Autosave snapshots after major events and on a time interval.
- Prompt on startup if the last close was not clean.

## Phase 5 - Transcription-Assisted Cue Generation

Current selected-media detection only finds speech-like regions. Transcription
requires a separate engine integration.

Required decisions:

- Built-in native engine vs external executable/API.
- Offline-only support vs network-capable service.
- Storage location for transcript text, confidence, and engine metadata.
- Speaker labeling strategy when speaker diarization is unavailable.

Minimum cue fields from transcription:

- `line`
- `character` placeholder such as `Unknown` or `Speaker 1`
- `notes` containing engine/source metadata
- confidence score in cue metadata

Generated transcript cues must remain draft data and be reviewable in Cue
Manager before recording.

## Implementation Guardrails

- New UI actions should call the Session Model, Event System, or Sync Engine
  APIs rather than writing REAPER state directly.
- Destructive operations must be scoped and confirmed.
- Risky model changes must create a snapshot before mutation.
- Internal sync/render helpers must join the outer transaction instead of
  opening nested Undo blocks.
- Model-only restoration must not claim that REAPER project objects were
  restored; project rollback requires the owned Undo transaction.
- Sync/recovery functions should return structured summaries rather than
  formatted UI strings.
- User-facing docs must distinguish implemented behavior from planned behavior.
