# C++ Migration Plan

ReaADR Tools is migrating from a Lua application hosted by a thin extension to
one native C++ REAPER extension. The migration is incremental so existing
projects remain usable throughout the work.

## Non-negotiable invariants

- `adr_session_model_v1` remains the only workflow source of truth during the
  migration. Native and Lua features must not introduce parallel cue stores.
- Existing project blobs remain readable. A model-format change requires an
  explicit version and migration path; it must not silently reinterpret v1.
- A feature has one writer. Until a feature is cut over, Lua owns its writes;
  after cutover, C++ owns them. Both layers may read v1 during the transition.
- Batch REAPER mutations remain one undoable operation and keep UI refresh
  suppression balanced on success and failure.
- Generated-object ownership checks remain at least as strict as the current
  Lua checks. Migration must not widen deletion boundaries.

## Target architecture

```text
Native REAPER extension
  Command host and menu
  Native manager UI
  Application services
    Import, cue editing, recording, reports, overlays
  REAPER adapters
    Project transactions, tracks, regions, items, FX, extstate
  C++ domain core
    ADR Session Model, validation, timecode, import mapping
```

The domain core must not include REAPER SDK headers. This keeps model and
workflow rules deterministic and unit-testable. REAPER pointers and API calls
belong in adapters at the extension boundary.

## Migration stages

### Stage 0: model compatibility foundation (complete)

- Parse and serialize `adr_session_model_v1` in a standalone C++ module.
- Preserve unknown record types for forward compatibility.
- Add native tests using Lua-compatible golden blobs.
- Compile the model module into the shipping extension.
- Expose `ReaADR_ValidateSessionModel` as a temporary compatibility API.

Exit condition: C++ can safely inspect the canonical model without REAPER and
without changing its representation.

### Stage 1: native host and project repository (implemented)

- Wrap `GetProjExtState`/`SetProjExtState` behind a project repository.
- Register native REAPER command IDs and a command hook instead of registering
  new ReaScripts.
- Add native transaction and UI-refresh RAII scopes.
- Migrate a low-risk read-only action (session validation) end to end.

Exit condition: at least one public action executes entirely in C++ against the
same project model.

Implementation status: the repository, nested-safe RAII scopes, native command
hook, and `Validate Session Model (Native Preview)` action are implemented and
covered by host-independent tests. An in-REAPER smoke test is still required
before removing or redirecting any Lua validation entry point.

### Stage 2: domain and import services (started)

- Port status normalization, timecode, CSV/TSV parsing, column mapping, stable
  IDs, model construction, cue replacement, and snapshots.
- Move XLSX import behind the same C++ import service.
- Add fixture parity tests against current Lua behavior before switching each
  writer.

Exit condition: import and model mutation no longer require Lua.

Implementation status: status/timecode/stable-ID utilities, CSV/TSV text
inspection, column aliases and mapping, row validation, metadata retention, and
complete cue-derived session construction are implemented in the native domain
core. Cue replacement now preserves the existing session envelope while
rebuilding cue-derived collections, and the model-only commit service performs
snapshot/save/revision/rollback sequencing. These paths are test-covered but
are not invoked by the UI yet. Native `SessionSaved` and `SyncFull` publication
now writes the same bounded project event history used by Lua. XLSX unification,
file-selection UI, the rendering cutover, and the import cutover remain pending.

### Stage 3: REAPER rendering and recording workflows (started)

- Port track/region/item/overlay rendering behind typed REAPER adapters.
- Port character filtering, navigation, record-arm capture/restore, recording,
  generated-cue cleanup, and explicit region-to-model synchronization.
- Preserve the model-first render direction and transaction boundaries.

Exit condition: project synchronization and recording workflows run natively.

Implementation status: deterministic character-lane assignment and native
track, region, ruler-lane, and cue-audio render planning are implemented
without REAPER dependencies. Typed adapters can inspect those artifacts and
apply one complete dependency-ordered plan inside the native transaction and
UI-refresh scopes. Track matching preserves the existing
`ReaADR.role`/`ReaADR.key` contract. Stale region and cue-audio removal requires
exact ownership evidence, media-source changes are revalidated immediately
before mutation, and recording tracks/items are never automatically removed.
Native cue-WAV generation now creates the same mono 48 kHz/16-bit three-beep
asset as Lua and publishes it through an atomic file replacement. A native
session-render application service coordinates WAV validation, model commit,
render planning, artifact application, project Undo, and post-Undo model
snapshot restoration. Successful commits publish Lua-compatible persistent
events without treating an observational logging failure as a failed render.
Native character-filter state, lane-aware planning, and typed mute/region-
visibility adapters now preserve active recording-pass filters after a full
render. Filter mutations revalidate track metadata and exact model-derived
region identity before writing, and participate in the same outer rollback.
Explicit region-to-model timing synchronization is also native: the domain
operation accepts only exact model-derived region names, rejects ambiguous
ownership, preserves missing cues, and feeds changed timing back through the
complete model/render transaction. This rebuilds derived region records and
cue audio together while unchanged projects remain revision- and Undo-free.
Native cue navigation now derives one validated timeline catalog from the
canonical model, preserving Lua-compatible ordering, lookup, epsilon, and
wraparound rules. Its application service chooses play or edit-cursor position,
atomically synchronizes the manager/overlay selection keys, and moves the
cursor without changing the session revision or creating an Undo point.
Native record-arm planning and its stateful REAPER manager now preserve the
complete pre-recording arm snapshot across repeated loop-target isolation.
Every track handle is revalidated before use; deleted tracks are skipped during
restore, isolation failures compensate earlier writes, and transient restore
failures retain only the entries that still need retrying. Each batch joins the
native transaction and UI-refresh scopes.
Native recording setup now resolves a selected canonical cue through the shared
overlap-lane algorithm, computes its bounded preroll window, and selects only an
owned `character` track. Exact lane ownership is preferred, the established
lane fallback remains compatibility-only, duplicate matches fail closed, and
the REAPER adapter revalidates role/key metadata immediately before returning
the target handle to the future transport coordinator.
The adapters and coordinator are test-covered but are not yet public writers;
the region-sync, character-filter, and navigation command/UI cutovers,
the recording transport/status coordinator, generated-cue cleanup, overlays,
and in-REAPER smoke tests remain.

### Stage 4: native UI and Lua removal

- Replace manager, cue editor, preferences, filter, overlay, and report windows
  with one native manager UI.
- Convert the five public actions and configurable quick actions to native
  commands.
- Stop packaging `scripts/*.lua`, unregister legacy ReaScripts, and remove the
  temporary native APIs that existed only for Lua interoperability.

Exit condition: the installed payload contains the native extension and assets,
with no runtime Lua dependency.

## Cutover checklist for each feature

1. Identify every model field and REAPER object the feature reads or writes.
2. Add C++ unit tests and, where practical, Lua/C++ parity fixtures.
3. Implement the domain operation before the UI command.
4. Wrap all project changes in the native transaction scope.
5. Verify model, tracks, regions, items, and overlay remain synchronized.
6. Switch the single writer to C++ and remove the superseded Lua entry point
   only after the native path passes in-REAPER smoke testing.

## Current implementation

The initial target-architecture modules now include:

- `extension/reaadr_core/session_model.*`: v1 field escaping, metadata encoding,
  parsing, canonical serialization, and preservation of unknown records.
- `extension/reaadr_core/model_repository.*`: canonical session-model loading
  and saving through a narrow project-state port.
- `extension/reaadr_core/domain_utils.*`: Lua-compatible status normalization,
  deterministic IDs, and non-drop-frame timecode parsing/formatting.
- `extension/reaadr_core/cue_import.*`: CSV/TSV inspection, column mapping,
  cue validation, and retention of unmapped studio metadata.
- `extension/reaadr_core/session_builder.*`: construction of scripts,
  characters, cues, lane tracks, regions, and import identity from cue rows.
- `extension/reaadr_core/session_mutation.*`: cue replacement that retains
  session identity, metadata, history, runtime state, and unknown records.
- `extension/reaadr_core/session_commit.*`: model-only snapshot, persistence,
  monotonic revision, and rollback orchestration.
- `extension/reaadr_core/lane_assignment.*`: deterministic overlap-lane
  allocation shared by model construction and rendering.
- `extension/reaadr_core/render_plan.*`: host-independent track and region
  mutation planning with conservative ownership boundaries.
- `extension/reaadr_core/cue_wav.*`: deterministic, Lua-compatible cue-beep
  synthesis and atomic WAV publication.
- `extension/reaadr_core/event_log.*`: monotonic event IDs, Lua-compatible
  event-line encoding, and bounded project history shared across both runtimes.
- `extension/reaadr_core/character_filter.*`: compatible project filter state,
  character/lane selection rules, and ownership-scoped mutation planning.
- `extension/reaadr_core/region_timing_sync.*`: exact-ownership import of
  deliberate generated-region timing edits into canonical cue records.
- `extension/reaadr_core/cue_navigation.*`: canonical cue ordering, lookup,
  wraparound selection, and paired manager/overlay selection persistence.
- `extension/reaadr_core/record_arm.*`: complete single-target arm-isolation
  planning over a preserved original snapshot.
- `extension/reaadr_core/recording_setup.*`: selected-cue timing, overlap-lane,
  preroll-window, and owned recording-track resolution.
- `extension/reaadr_reaper/project_state.*`: dynamically sized REAPER project
  extstate access.
- `extension/reaadr_reaper/project_transaction.*`: nested-safe undo and UI
  refresh RAII scopes with conservative failure rollback.
- `extension/reaadr_reaper/track_region_adapter.*`: testable project inspection
  and transactional application of native render plans.
- `extension/reaadr_reaper/render_artifact_adapter.*`: ruler-lane and cue-audio
  inspection/application plus the single-transaction complete-render
  coordinator.
- `extension/reaadr_reaper/session_render_service.*`: application-level
  coordination of canonical model commits and visible project rendering, with
  snapshot recovery after failed REAPER transactions and success-event
  publication, including the explicit region-timing update workflow.
- `extension/reaadr_reaper/character_filter_adapter.*`: verified track mute and
  generated-region visibility inspection/application under native Undo.
- `extension/reaadr_reaper/cue_navigation_service.*`: play/edit-position-aware
  cursor navigation backed only by the canonical session model.
- `extension/reaadr_reaper/record_arm_adapter.*`: revalidated track-arm capture,
  isolation, compensation, idempotent restoration, and targeted retry.
- `extension/reaadr_reaper/recording_setup_adapter.*`: narrow track inspection
  and final ownership revalidation before recording handoff.

`make -C extension test` runs these modules without the REAPER SDK; normal
extension builds compile the same sources into the plugin.
