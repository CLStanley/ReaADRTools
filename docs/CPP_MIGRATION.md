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
the target handle to the transport executor.
The native recording transport state machine now owns the deterministic
preroll, punch-in, cue-end stop, loop restart, user-stop/abort cleanup, and loop
preference transitions. It emits ordered host intents rather than calling
REAPER, so the executor can apply actions only after revalidating the setup and
can retain the last committed state if an action fails.
The native recording transport executor now applies the immediate stop,
loop-range, cursor, record-arm, and play/record intents according to their
declared ordering. It preserves the user's original loop range across passes
and compensates loop/arm changes when a take cannot start. Model-first
cue/status updates and preference writes are returned as explicit pending work
rather than being performed at the asynchronous REAPER boundary.
The native recording application coordinator now consumes that pending work.
It persists the Lua-compatible preroll preference, synchronizes the paired cue
selection, commits `Recorded` status through a focused snapshot/revision
operation, and publishes `CueUpdated`. Overlay failure restores exact selection
state or the canonical model snapshot and retains the failed action for an
idempotent retry without duplicate revisions or events.
Native overlay refresh planning and its REAPER adapter now locate only the
exact owned `source_video` track and recognize the established renamed-FX or
generated-code marker. They refuse duplicate ownership, preserve unrelated
user FX, update owned effects in place, remove only exact generated effects,
revalidate plans immediately before mutation, and compensate failed create or
update attempts inside the native transaction boundary.
Native overlay settings now read and write every Lua-compatible project key,
with typed defaults and compensation for partial writes. Native EEL generation
uses canonical model cues, shared overlap-lane filtering, deterministic
region/item/active selection precedence, and the existing generated-code
ownership marker while preserving the current text, timing, metadata, and
visual-cue behavior.
The native overlay application coordinator now composes those inputs and
forwards one immutable refresh request to the transactional FX adapter,
including explicit disabled and retryable failure outcomes.
The native Manager Preferences state contract now models the established
Manager's overlay presets, quick-action slots, layout/tooltips/navigation
toggles, and Cue Manager docking preference. Its validated updates are kept
in the domain core so the eventual graphical Manager can bind to one
deterministic state model while Lua compatibility remains available.
That state now has a project-extstate repository using the exact `ui.*` keys
for layout, hover preview, tooltips, navigation wrapping, and Cue Manager
auto-docking. Saves stage UI flags before overlay values and roll back partial
writes, preserving the single-project-settings transaction boundary.
The repository also accepts a separate global-state adapter for the four
`quick_action_*` slots, preserving Lua's global (rather than project-local)
quick-action behavior.
The REAPER boundary now supplies that adapter from `GetExtState` and
`SetExtState`, while project settings continue through `GetProjExtState` and
`SetProjExtState`.
The native `ManagerViewModel` now composes this preference state with the
canonical session name, revision, and filtered Cue Manager rows in one render
payload, giving a future graphical view a consistent snapshot per frame.
Native Manager navigation now exposes the established module order—Import,
Cue Management, Session Tools, Reports, Video Overlays, Preferences, and
Help—with
safe launch-tab normalization for invalid or stale requests.
The same navigation contract publishes all 16 tab actions and their
user-facing hints, allowing native controls to route through existing C++
services while the compatibility scripts remain available.
The Preferences contract also publishes the 30 established overlay and UI
controls with stable keys, labels, tab ownership, and control types for the
native renderer.
Overlay settings also expose deterministic preset detection so the native
Preferences view can select the matching Actor, Engineer, Studio, or Minimal
profile and distinguish custom combinations.
The update API now edits every catalogued overlay field directly, normalizing
colors and metadata lists and rejecting invalid or negative preroll values so
native controls can commit changes without Lua-side validation.
Quick-action updates now use the canonical eight-choice list from the Manager
and reject unsupported values before they reach global extstate.
The Cue Manager UI contract now likewise publishes its eight table columns
and nine action-bar commands, including editability and the Lua help/tool-tip
copy used by the visual workflow.
The extension now exposes a native refresh action that binds current REAPER
selection, frame rate, project state, and FX APIs to that coordinator. The
legacy Lua action remains registered for compatibility while broader command
and UI cutover proceeds.
Native Next Cue, Previous Cue, and prompt-backed Jump To Cue actions now bind
the existing navigation service to REAPER transport/cursor APIs and paired
selection persistence.
Native generated-cue cleanup now has a proof-of-ownership domain plan and
transactional REAPER adapter. It removes only exact selected-character cue
regions, cue-audio items, and `cue_character` tracks; stale plans fail closed
and user/dialogue media remains untouched. The public cleanup command and
model-persistence coordinator now coordinates inspection, project mutation,
snapshot recovery, and canonical cue persistence in one native transaction;
public command wiring remains part of the native UI cutover.
Native character-filter application now also coordinates canonical filter
state, lane-aware planning, transactional REAPER mutation, and persistence;
the existing Lua filter window remains a compatibility UI.
These domain operations and adapters are test-covered but are not yet all
public writers; the region-sync, character-filter, and navigation command/UI
cutovers, deferred recording frame/UI wiring, other overlay UI, and in-REAPER
smoke tests remain. Lua source and packaging payloads have now been removed;
the native extension is the only installed runtime.

### Stage 4: native UI and Lua removal

- Replace manager, cue editor, preferences, filter, overlay, and report windows
  with one native manager UI.
- UI parity target: the native replacement must follow the established ReaADR
  Tools Manager design from the Lua application, including its Preferences,
  Video Overlay, filter, help, quick-action, refresh, and error/retry surfaces.
  “Cue Manager” specifically means the visual cue-management window launched
  by Open Cue Manager: its cue list, selection, editing, status/type controls,
  timing/region workflows, and visible synchronization behavior—not a text
  summary or a generic status prompt.
- Reach feature parity for every existing UI control, persisted preference,
  keyboard/menu action, refresh path, and error/retry dialog before removing
  its compatibility implementation. A read-only summary or reduced input
  dialog is a preview and does not satisfy this exit condition.
- Convert the five public actions and configurable quick actions to native
  commands.
- Stop packaging `scripts/*.lua`, unregister legacy ReaScripts, and remove the
  temporary native APIs that existed only for Lua interoperability.

Compatibility rule: Lua UI remains installed and registered for any feature
whose native replacement has not passed parity tests and an in-REAPER smoke
test. Removing compatibility code before that point is a breaking change.

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
- `extension/reaadr_core/recording_transport.*`: deterministic preroll,
  punch-in, looping, stop, cleanup, and preference transitions expressed as
  ordered host intents.
- `extension/reaadr_core/cue_status.*`: focused canonical status mutation and
  idempotent snapshot/revision commit with rollback.
- `extension/reaadr_core/recording_preferences.*`: compatibility persistence
  for the per-project Lua recording-preroll setting.
- `extension/reaadr_core/overlay_settings.*`: typed persistence for the complete
  Lua-compatible per-project overlay preference set.
- `extension/reaadr_core/overlay_eel.*`: deterministic Video Processor source
  generation from canonical cues, native selection state, and shared
  character/lane filtering.
- `extension/reaadr_core/overlay_refresh.*`: exact source-track/FX ownership and
  create/update/remove planning for generated video overlays.
- `extension/reaadr_core/cue_cleanup.*`: exact selected-character ownership
  planning for generated regions, cue audio, and cue-character tracks.
- `extension/reaadr_core/cue_manager_model.*`: deterministic native UI row
  projection of canonical cues, including display fields, selection state, and
  validated canonical row edits.
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
- `extension/reaadr_reaper/recording_transport_executor.*`: ordered REAPER loop,
  cursor, record-arm, and transport execution with failed-start compensation
  and deferred model/status application actions.
- `extension/reaadr_reaper/recording_application_service.*`: retryable
  selection, preference, canonical Recorded-status, overlay-refresh, and
  CueUpdated-event coordination with model/view rollback.
- `extension/reaadr_reaper/overlay_refresh_adapter.*`: revalidated generated-FX
  inspection and transactional create/update/remove application with user-FX
  preservation and failed-mutation compensation.
- `extension/reaadr_reaper/overlay_application_service.*`: native composition
  of persisted settings, canonical model, selection/filter state, and EEL
  generation before the transactional refresh callback.
- `extension/reaadr_reaper/cue_cleanup_adapter.*`: transactional, stale-plan-
  checked deletion of generated cue artifacts while preserving user objects.
- `extension/reaadr_reaper/cue_cleanup_application_service.*`: native
  inspection, snapshot, project mutation, and canonical-model cleanup boundary.
- `extension/reaadr_reaper/character_filter_application_service.*`: native
  filter-state persistence and transactional planner/adapter coordination.

`make -C extension test` runs these modules without the REAPER SDK; normal
extension builds compile the same sources into the plugin.
