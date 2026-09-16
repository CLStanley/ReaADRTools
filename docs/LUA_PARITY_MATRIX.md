# Lua → C++ Parity Matrix

This document is the migration acceptance checklist for the Lua-to-native-C++ transition. It is updated alongside migration work so the repository describes the current native implementation rather than an earlier transition state.

## Retention rule

**Keep substantive Lua reference implementations in `scripts/` until native C++ has reached full behavioral parity for the workflow they specify.**

Thin compatibility launchers that contain no workflow/domain behavior may remain temporarily to preserve historical REAPER action paths. Obsolete support modules with no remaining runtime/reference consumers may be retired once that lack of ownership is verified.

A substantive Lua implementation is not eligible for retirement merely because equivalent domain logic exists. Retirement requires all of the following:

1. Its user-visible behavior has been enumerated from the Lua implementation.
2. The native path implements those behaviors against the canonical ADR Session Model.
3. Project mutations use the native transaction/Undo and ownership rules.
4. The native path is reachable through the intended REAPER/Manager entry point.
5. Focused automated tests cover deterministic/domain behavior where practical.
6. The workflow has been smoke-tested inside REAPER against the Lua reference.
7. Cross-platform behavior has been verified for the platforms supported by the release.

Until all seven gates pass, substantive Lua implementations remain the specification and compatibility reference.

## Status vocabulary

- **Native routed** — the public/compatibility entry point invokes C++ today, but parity verification remains.
- **Native backend** — the main behavior exists in C++, but an entry point/UI or parity gate remains.
- **Native foundation** — substantial shared domain/application behavior exists, but Lua still specifies meaningful behavior.
- **Lua reference** — no claim of complete native parity yet.
- **Support module** — not a user workflow; its contracts are being absorbed by native core/adapters.
- **Compatibility launcher** — thin Lua action preserving a historical REAPER script path while delegating all workflow ownership to a named native command.
- **Retired** — obsolete transitional/support implementation removed after its ownership/references were audited.

## Current matrix

| Lua reference | Current native state | Remaining parity gate(s) |
| --- | --- | --- |
| `ReaADR_App.lua` | Native foundation | Native Manager/application services own substantial session, overlay, recording, import and quick-action behavior. Continue the remaining ownership/reference audit before retiring the application reference. |
| `ReaADR_Character_Filter.lua` | Native routed/backend | REAPER smoke-test filtering, lane and visibility behavior. |
| `ReaADR_Clean_Generated_Cues.lua` | Native routed/backend | Smoke-test preservation/ownership edge cases. |
| `ReaADR_Core.lua` | Support module | Continue contract-by-contract migration/reference audit; retain while substantive callers/reference behavior remain. |
| `ReaADR_Core_Characters.lua` | Support module / substantial native parity | Continue parity audit for character token/lane helpers. |
| `ReaADR_Core_Ownership.lua` | Support module / native foundation | Verify every owned artifact rule before retirement. |
| `ReaADR_Core_Persistence.lua` | Support module / native foundation | Complete persistence/event/snapshot parity audit. |
| `ReaADR_Core_Transactions.lua` | Support module / native foundation | Native project/model transactions exist; retain until remaining callers/reference contracts are cleared. |
| `ReaADR_Cue_Info_Panel.lua` | Native routed/backend | Native live view, take counts, inline editing, character choices, filtered navigation, Space transport shortcut, floating geometry persistence, one-shot launch behavior and REAPER/SWELL docking are implemented. Remaining: cross-platform REAPER smoke testing and final visual/interaction parity. |
| `ReaADR_Cue_Manager.lua` | Native routed/backend | Current modeless `CueManagerController`/`CueManagerSession` architecture owns the Manager. Remaining: Windows responsive child-control layout, report-tool presentation parity, and full cross-platform REAPER smoke testing. |
| `ReaADR_Cue_Manager_Gfx.lua` | Lua visual/interaction reference | Native Manager replacement is active; retain for final visual/interaction parity audit. |
| `ReaADR_Cue_Manager_ImGui.lua` | Lua visual/interaction reference | Native Manager replacement is active; retain for final visual/interaction parity audit. |
| `ReaADR_Detect_Dialogue.lua` | Compatibility launcher / native routed | Delegates to `_ReaADRDetectDialogueNative`; Lua audio-scan fallback has been removed. Audit final result/UI parity (including video-window behavior) and smoke-test in REAPER. |
| `ReaADR_Export_Cue_Sheet.lua` | Native backend/routed | Export-format parity audit and smoke test. |
| `ReaADR_Export_Reports.lua` | Native backend/routed | Report-output parity audit and smoke test; Windows Manager report presentation still needs Metadata export exposure. |
| `ReaADR_Generate_Cues.lua` | Native routed/backend | Native generation includes markers and regions by default. Remaining: marker/region REAPER parity smoke test. |
| `ReaADR_Import_Cue_Sheet.lua` | Native routed/backend | Mapping/preview/XLSX/error-path parity audit and REAPER smoke test. |
| `ReaADR_Import_Script.lua` | Compatibility launcher / native routed | Historical import-script entry delegates to the native cue-sheet import command. Remaining: cue-sheet import parity/smoke gate. |
| `ReaADR_Jump_To_Cue.lua` | Compatibility launcher / native routed | Native action owns navigation; REAPER smoke test remains. |
| `ReaADR_Jump_To_Selected_Cue.lua` | Compatibility launcher / native routed | Historical selected-cue action delegates to `_ReaADRJumpToCueNative`; smoke test remains. |
| `ReaADR_Menu.lua` | Native routed/backend | Public Manager/menu routing has been cut over to native registrations. Remaining: REAPER menu/quick-action smoke test and legacy-registration cleanup. |
| `ReaADR_Next_Cue.lua` | Compatibility launcher / native routed | Native action owns navigation; wrap/filter smoke test remains. |
| `ReaADR_Open_Manager.lua` | Compatibility launcher / native routed | Native Manager owns the UI; keep wrapper until Manager parity is approved. |
| `ReaADR_Open_Manager_1.lua` | Compatibility launcher / native routed | Native Manager startup-tab bridge owns behavior; smoke test remains. |
| `ReaADR_Open_Manager_2.lua` | Compatibility launcher / native routed | Native Manager startup-tab bridge owns behavior; smoke test remains. |
| `ReaADR_Open_Manager_3.lua` | Compatibility launcher / native routed | Native Manager startup-tab bridge owns behavior; smoke test remains. |
| `ReaADR_Overlay.lua` | Native foundation/backend | Audit remaining wrapper behavior and entry routing. |
| `ReaADR_Overlay_Settings.lua` | Compatibility launcher / native routed | Native Manager/settings path owns behavior; settings UI smoke test remains. |
| `ReaADR_Preferences.lua` | Compatibility launcher / native routed | Native preferences command owns behavior; smoke test remains. |
| `ReaADR_Previous_Cue.lua` | Compatibility launcher / native routed | Native action owns navigation; wrap/filter smoke test remains. |
| `ReaADR_Quick_Action_1.lua` | Compatibility launcher / native routed | Public host action is promoted to `_ReaADRQuickAction1Native`; REAPER smoke test remains. |
| `ReaADR_Quick_Action_2.lua` | Compatibility launcher / native routed | Public host action is promoted to `_ReaADRQuickAction2Native`; REAPER smoke test remains. |
| `ReaADR_Quick_Action_3.lua` | Compatibility launcher / native routed | Public host action is promoted to `_ReaADRQuickAction3Native`; REAPER smoke test remains. |
| `ReaADR_Quick_Action_4.lua` | Compatibility launcher / native routed | Public host action is promoted to `_ReaADRQuickAction4Native`; REAPER smoke test remains. |
| `ReaADR_Record_Arm.lua` | **Retired** | Removed after reference search found no remaining consumers; native recording workflow owns arm capture/isolation/restore. Validate that behavior in the full Record Cue smoke gate. |
| `ReaADR_Record_Cue.lua` | Native routed/backend | Native workflow/window and `_ReaADRRecordCueNative` own recording. Remaining: final docker/visual/lifecycle parity and cross-platform REAPER smoke testing. |
| `ReaADR_Start_Recording_Workflow.lua` | Compatibility launcher / native routed | Historical workflow action delegates to `_ReaADRRecordCueNative`; no Lua recording workflow remains in this launcher. |
| `ReaADR_Set_Cue_Status.lua` | Compatibility launcher / native routed | Six-choice native presentation and registered native action own status updates; REAPER smoke testing remains. |

## Native Manager architecture

The active Manager is the persistent/modeless C++ implementation built around `CueManagerController` and `CueManagerSession`, with platform presentation in the current SWELL/Win32 Manager sources. The earlier `reaadr_ui/legacy/native_manager_window.*` interim shell has been removed; it is not a second supported Manager implementation.

The Manager startup-tab bridge persists `ui.manager.launch_tab` as a one-shot launch request so historical Manager slot actions can route into the native Manager without recreating workflow behavior in Lua.

On Windows, top-level size/layout persistence and modeless lifecycle are implemented, but child controls are still positioned with fixed coordinates. Responsive `WM_SIZE` layout remains an explicit parity item. On Linux/macOS, the Manager continues to use the host-provided SWELL/modstub architecture; do not replace it with a directly linked SWELL copy.

## Host registration audit

Native workflow/action registration now owns Record Cue, Cue Info, Set Cue Status, Dialogue Detection and Quick Actions. Public Quick Action slots are promoted to `_ReaADRQuickAction1Native` through `_ReaADRQuickAction4Native`; historical Lua files are compatibility launchers/reference rather than the workflow owners.

The host action table still contains a stale compatibility entry for `Scripts/ReaADRTools/scripts/ReaADR_Monitor_Markers.lua`. Repository history confirms that script was intentionally inert (a placeholder for a future live-monitoring feature), so continuous marker monitoring is **not** a migration parity requirement. Remove the stale registration during host-registration cleanup; treat any future live monitor as a post-migration feature.

`CountTakes` and `GetSet_LoopTimeRange2` are required by the native take-count and recording-loop services. Because the extension uses `REAPERAPI_MINIMAL`, native Make/MSVC build paths request those APIs before the host translation unit is preprocessed. Runtime smoke testing in REAPER is still required before parity approval.

## Cross-platform CI state

Native CI covers:

- Linux native tests, serial/parallel native builds and Linux release-payload validation;
- Windows MSVC native extension build and native payload validation;
- macOS native tests plus a Cocoa-linked native extension build emitted as `reaper_reaadr.dylib` and verified as an installable macOS binary name.

These gates establish compile/link/test coverage; they do **not** replace the required in-REAPER behavioral smoke tests in the retirement rule.

## Record Cue parity checklist

The Lua `ReaADR_Record_Cue.lua` remains authoritative until all of these are verified natively:

- active cue resolution honors Manager selection, active character/lane filter, play/edit position, and next-cue fallback;
- exact owned character/lane recording track resolution with compatibility fallback;
- record-arm state capture, isolation, validation and restoration;
- preroll start and punch-in at cue start;
- immediate recording when preroll is disabled/not applicable;
- automatic stop at cue end;
- loop wait/restart behavior;
- loop-range capture/configuration/restoration;
- loop-recording overwrite warning;
- `include_preroll_each_loop` persistence;
- take-count progression;
- completed takes mark the canonical cue `Recorded` and refresh overlay;
- external transport stop behavior;
- Stop, Escape, close and error paths converge on safe cleanup;
- transport keyboard shortcut parity;
- SMPTE timecode/status/display parity;
- floating window size/position persistence parity;
- true REAPER docker-state persistence parity;
- Linux/macOS/Windows presentation support expected by the release.

## Migration discipline

Migration completeness and native parity come before cleanup/new feature work. Preserve the canonical `adr_session_model_v1`, ownership/deletion boundaries, transaction/Undo behavior, and the host-provided SWELL/modstub architecture while moving workflow ownership to C++.

When native behavior intentionally differs from Lua, document the difference here and in the relevant migration/design document before treating it as parity. Do not silently redefine parity by deleting a substantive reference implementation.

Update this matrix in the same migration slice whenever practical. The final documentation pass should verify and consolidate current facts, not reconstruct the C++ migration from commit history.
