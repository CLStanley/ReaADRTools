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
are not invoked by the UI yet. XLSX unification, event publication,
file-selection UI, REAPER rendering, and the import cutover remain pending.

### Stage 3: REAPER rendering and recording workflows

- Port track/region/item/overlay rendering behind typed REAPER adapters.
- Port character filtering, navigation, record-arm capture/restore, recording,
  generated-cue cleanup, and explicit region-to-model synchronization.
- Preserve the model-first render direction and transaction boundaries.

Exit condition: project synchronization and recording workflows run natively.

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
- `extension/reaadr_reaper/project_state.*`: dynamically sized REAPER project
  extstate access.
- `extension/reaadr_reaper/project_transaction.*`: nested-safe undo and UI
  refresh RAII scopes with conservative failure rollback.

`make -C extension test` runs these modules without the REAPER SDK; normal
extension builds compile the same sources into the plugin.
