# ReaADR Tools — Claude Project Context

## Project Overview

**ReaADR Tools** is a professional-grade ADR (Automated Dialogue Replacement) toolkit for the REAPER DAW. The goal is to provide a feature set comparable to professional ADR suites (Nuendo, Pro Tools ADR) while remaining accessible to students, hobbyists, voice actors, and indie creators — as well as scalable for studio use.

**Target users:** Voice actors, ADR engineers, directors, localization/dubbing studios, indie animation creators, self-training actors learning lip-sync and ADR timing.

**Platform:** REAPER 7+  
**Primary languages:** C++ (native extension), Lua/ReaScript (feature scripts)  
**Distribution:** ReaPack repository (planned)  
**Cross-platform:** Windows primary, macOS and Linux supported

---

## Architecture: Hybrid C++ Extension + Lua Scripts

This project uses a deliberate hybrid architecture. Do not suggest rewriting Lua features into C++ unless there is a specific performance or API reason.

### C++ Native Extension responsibilities
- Permanent "ReaADR Tools" top-level menu in REAPER
- REAPER startup integration
- In-memory Session Model (live state host)
- Event Bus (pub/sub system)
- Dockable UI panels (Cue Manager, Director Mode, Actor Mode)
- IPC surface for Lua scripts to read/write session state
- Project-level settings access

### Lua/ReaScript responsibilities
- Cue sheet import (CSV, TSV, future XLSX)
- Cue sheet export (CSV, JSON, future EDL/AAF)
- Marker and region creation
- Cue generation (beeps, streamers, labels)
- Video overlay system
- Recording loop controller
- Transcription-assisted cue generation
- All feature scripts read/write via the Core API only — never directly to REAPER state

### Key architectural constraint
**Lua scripts are stateless between invocations.** The C++ extension holds the live Session Model in memory and exposes it to Lua via IPC. Lua scripts must not attempt to maintain long-lived in-memory state themselves.

---

## Core Architectural Principles

These are non-negotiable design rules that must be respected in all code:

1. **Session Model is the single source of truth.** REAPER project state is a *rendered output* of the Session Model, never the source of truth. If Session Model and REAPER disagree, Session Model is correct.

2. **All changes flow through the Event System.** No component may silently mutate state. The required flow is:
   ```
   UI Action → Event System → Session Model Update → Sync Engine → REAPER State Update → UI Refresh
   ```
   Never: `UI → REAPER direct modification`

3. **Sync must be idempotent and deterministic.** Running the same sync twice must produce identical results. No duplication, no stacking.

4. **Non-destructive by default.** No operation shall permanently destroy session data without explicit user confirmation. All destructive actions must be explicit, traceable, and reversible where possible.

5. **Partial failure protection.** If an operation fails mid-process, no partial state shall be committed. Session Model remains unchanged; REAPER is left in its last valid state.

---

## Session Model Structure

```
ADR_Session
├── session_id
├── session_name
├── project_metadata
├── scripts[]
│   ├── script_id
│   ├── script_name
│   ├── source_file
│   ├── import_timestamp
│   ├── revision_id (optional)
│   ├── characters[]
│   ├── cue_count
│   └── metadata
├── characters[]
│   ├── character_id
│   ├── character_name
│   ├── script_id
│   ├── cue_count
│   ├── status (active/archived)
│   └── import_state
├── cues[]
│   ├── cue_id
│   ├── script_id
│   ├── character_id
│   ├── start_time (seconds, float)
│   ├── end_time (seconds, float)
│   ├── start_smpte
│   ├── end_smpte
│   ├── dialogue
│   ├── cue_type
│   ├── status
│   ├── notes
│   ├── region_id
│   └── track_id
├── tracks[]
├── regions[]
├── timecode_settings
├── import_registry
└── session_state
    ├── active_script_id
    ├── active_character_filter[]
    ├── refresh_version
    ├── last_operation
    └── dirty_flags
```

**Cue identity:** Cues are uniquely identified by `cue_id + script_id + character_id`. This prevents duplication during refresh or re-import.

**Timecode storage rule:** Always store SMPTE as a frame-accurate integer (frame number from zero) internally. Convert to display format (HH:MM:SS:FF) only at the UI layer. Never store timecode as a float or display string internally — this prevents floating point drift across import/export cycles.

---

## Sync Engine

The Sync Engine translates the Session Model into REAPER project state.

**Sync modes:**
- **Full Sync** — Complete reconstruction of REAPER state from Session Model. Used on project open, major refresh, structural changes.
- **Incremental Sync** — Applies only changed elements. Used for cue status changes, minor edits.
- **Safe Sync (Validation Mode)** — Dry run. Reports what would change without modifying REAPER state.

**Sync triggers:** Script import, character selection changes, cue edits, refresh command, session load, undo/redo boundaries.

**Conflict resolution:** When REAPER state diverges from Session Model, the system prompts: Rebuild from Session Model / Merge / Cancel. Default: Session Model override.

---

## Event System

All state changes must emit events. No silent mutations.

**Event categories:**
- **Session Events:** ScriptImported, CharacterAdded, CueCreated, CueUpdated, CueDeleted, CharacterSelected, SessionCleared
- **Sync Events:** SyncFull, SyncIncremental, SyncValidation, RefreshRequested
- **UI Events:** WindowOpened, WindowClosed, FilterChanged, SelectionChanged, OverlayUpdated
- **System Events:** ErrorOccurred, WarningIssued, StateConflictDetected, ImportCompleted

**Event coalescing:** Bulk operations (e.g. importing 300 cues) must emit a single aggregated event, not 300 individual events. Frequent UI changes must be debounced.

**Events are immutable once created.**

---

## Snapshot & Recovery System

- **Auto-snapshots** trigger on: script import, full sync, cue generation, character import, destructive operations, session open/close.
- **Manual snapshots** available via "Create Restore Point".
- **Recovery modes:** Full Session Restore, Partial Restore (cues/characters/tracks/regions), Safe Preview Restore (diff view before applying).
- **Pre-restore safety:** System always creates a backup snapshot before applying any restore.
- **Crash detection:** On startup, system checks for incomplete shutdown and prompts user.

**Implementation sequence (do not skip ahead):**
1. Basic `ProjExtState` persistence
2. Manual snapshots only
3. Auto-snapshots on major operations
4. Diff view and partial restore

---

## Cue Types

`Dialogue` | `Reaction` | `Effort` | `Walla` | `Crowd` | `Announcement` | `Narration` | `Custom`

Cue types are color-coded in the REAPER timeline.

---

## Cue Status Values

`Not Recorded` | `Recorded` | `Approved` | `Needs Retake`

Status colors are displayed on markers and regions.

---

## Cue Timing Defaults

```
Pre-roll:        3.0 seconds
Beep count:      3
Beep interval:   1.0 second
Beep length:     0.10 seconds
Beep frequency:  1000 Hz (or imported beep.wav)
Streamer length: 2.0–3.0 seconds, ending at cue point
```

---

## Import System

- **Supported formats:** CSV, TSV, Google Sheets export, future XLSX
- **Column mapping:** Users map source columns to ADR fields. Mappings savable as presets (Netflix, Crunchyroll, Funimation, Custom Studio, etc.)
- **Script identity:** Each import assigned a `script_id` and `file_hash` for duplicate detection and revision tracking.
- **Selective import:** Users choose which characters to import from a script. Previously imported characters are disabled by default.
- **Safe re-import modes:** Skip existing / Import new characters only / Update existing (explicit) / Cancel

**Supported frame rates:** 23.976, 24, 25, 29.97, 30 fps

---

## Export System

**Current:** CSV  
**Planned:** JSON (session format), EDL  
**Future investigation:** AAF

The export layer is modular — new formats are added without changing the Session Model.

---

## Recording Loop Behavior

REAPER's native loop recording does not support pre-roll + beeps on every repeated take. **Do not use REAPER's native loop recording for ADR repeat takes.** Instead, implement a custom loop via a Lua defer loop that manually repositions and re-arms between takes, giving full control over pre-roll behavior.

Expected per-take flow:
```
Pre-roll → Cue Beeps → Record Take N → (loop) → Pre-roll → Cue Beeps → Record Take N+1
```

---

## Track & Lane Model

```
[Character Name] Dialogue    — recording track
[Character Name] Cues        — beeps/streamers/labels

ReaADR Tools (system track)
  Lane 1: ADR marker labels
  Lane 2: Cue Set 1 (primary)
  Lane 3: Cue Set 2 (overlapping cues)
  Lane 4: Cue Set 3 (additional overlaps)
```

Project markers and regions are **not** the source of truth — the Session Model is. Markers/regions are a rendered representation.

---

## Studio Metadata Fields

Optional fields supported on import: `PGID`, `MID`, `Media Time`, `Watermark Timestamp`, `Asset Date Code`, `Project Name`, `Episode Number`, `Language`, `Revision Number`, `Client Name`

Unknown fields are preserved where possible.

---

## Feature Roadmap (Priority Order)

**Priority 1 (next)**
- Cue navigation (Next/Prev/Jump To Cue)
- Cue status tracking with color coding
- Project settings persistence via `SetProjExtState`

**Priority 2**
- Character filtering (actor-specific session views)
- Record Current Cue (jump to preroll → arm → record → stop)
- Import progress window

**Priority 3**
- Auto phrase detection from guide audio (silence threshold)
- Cue renumbering (sequential and insert: 002A, 002B)
- Region generation modes (marker-to-marker, fixed, manual)

**Priority 4**
- Take tracking per cue (take count, preferred take, notes)
- Session reports (CSV, HTML, future PDF)

**Priority 5**
- Actor Mode (simplified overlay view, hides engineering controls)
- Director Mode (cue list, status tracking, session notes, navigation)

**Long-term**
- ADR Session Builder (one-click session creation from video + cue sheet)
- Word synchronization (karaoke-style dialogue highlighting)
- Transcription-assisted cue generation (with speaker detection)
- Studio database integration
- Cloud-based script synchronization

---

## Extension Migration Phases

| Phase | Work |
|---|---|
| 1 | Continue Lua feature development |
| 2 | Native extension menu creation |
| 3 | Extension-based settings management |
| 4 | Dockable ADR panel (Cue Manager) |
| 5 | Migrate performance-critical systems to C++ if needed |

Do not accelerate phases. Lua remains the primary feature development environment until integration or performance limitations require otherwise.

---

## Lua Core Module Pattern

All Lua scripts must require `ReaADR_Core.lua`. No script should implement its own timecode parsing, marker scanning, or state I/O.

`ReaADR_Core.lua` provides:
- Timecode utilities (SMPTE parse/format, frame rate conversion)
- `GetProjExtState` / `SetProjExtState` wrappers (session persistence)
- Cue object model helpers
- Track/lane naming conventions
- Structured logging (`reaper.ShowConsoleMsg` wrappers)
- IPC surface to C++ extension (when available)

---

## Persistence

Session state is stored in REAPER project metadata via `SetProjExtState` / `GetProjExtState` under the key namespace `ReaADR`. This allows session data to survive project save/load without external files.

For studio deployments, an external JSON session file is the preferred long-term storage format.

---

## Error Handling Rules

- Errors must be explicit, actionable, logged, and non-silent.
- Failed operations must not commit partial state.
- All errors surface to the UI with context (what failed, which cue/script/character, what action was taken).
- QA mode shall support verbose logging for debugging.

---

## Key Files (when repository is set up)

```
ReaADR-Tools/
├── CLAUDE.md                        ← this file
├── extension/                       ← C++ native extension source
├── scripts/
│   ├── ReaADR_Core.lua              ← required by all scripts
│   ├── ReaADR_Import_Cue_Sheet.lua
│   ├── ReaADR_Export_Cue_Sheet.lua
│   ├── ReaADR_Generate_Cues.lua
│   ├── ReaADR_Record_Cue.lua
│   ├── ReaADR_Overlay.lua
│   └── ReaADR_Monitor_Markers.lua
├── assets/
│   └── beep.wav
├── docs/                            ← SRS documents
└── examples/
    └── cue_sheet_template.csv
```

---

## Project Background

- Developer: Charles Stanley
- Started with ChatGPT/Codex assistance; migrated to Claude
- SRS documents maintained in Google Drive under "ReaADR Tools" folder
- Current SRS version: 0.4 Draft (Addenda A–J)
- The project targets both the hobbyist market and professional studios, with the explicit goal of lowering the barrier to entry for ADR training and self-directed actor development