# Lua → C++ Parity Matrix

This document is the migration acceptance checklist for the Lua-to-native-C++ transition.

## Retention rule

**Keep every Lua script in `scripts/` until native C++ has reached full behavioral parity.**

A Lua script is not eligible for retirement merely because equivalent domain logic exists. Retirement requires all of the following:

1. Its user-visible behavior has been enumerated from the Lua implementation.
2. The native path implements those behaviors against the canonical ADR Session Model.
3. Project mutations use the native transaction/Undo and ownership rules.
4. The native path is reachable through the intended REAPER/Manager entry point.
5. Focused automated tests cover deterministic/domain behavior where practical.
6. The workflow has been smoke-tested inside REAPER against the Lua reference.
7. Cross-platform behavior has been verified for the platforms supported by the release.

Until all seven gates pass, the Lua implementation remains the specification and compatibility reference.

## Status vocabulary

- **Native routed** — the primary native UI/Manager path invokes C++ today, but Lua remains for parity/reference.
- **Native backend** — the main behavior exists in C++, but an entry point/UI or parity work remains.
- **Native foundation** — substantial shared domain/application behavior exists, but the Lua workflow still owns meaningful behavior.
- **Lua reference** — no claim of complete native parity yet.
- **Support module** — not a user workflow; its contracts are being absorbed by native core/adapters.

## Current matrix

| Lua reference | Current native state | Remaining parity gate(s) |
| --- | --- | --- |
| `ReaADR_App.lua` | Native foundation | Finish Manager/app parity audit and all remaining action routing. |
| `ReaADR_Character_Filter.lua` | Native routed/backend | REAPER smoke-test against Lua filtering, lane and visibility behavior. |
| `ReaADR_Clean_Generated_Cues.lua` | Native routed/backend | Smoke-test preservation/ownership edge cases. |
| `ReaADR_Core.lua` | Support module | Continue contract-by-contract migration; retain through the entire transition. |
| `ReaADR_Core_Characters.lua` | Support module / substantial native parity | Continue parity audit for character token/lane helpers. |
| `ReaADR_Core_Ownership.lua` | Support module / native foundation | Verify every owned artifact rule before retirement. |
| `ReaADR_Core_Persistence.lua` | Support module / native foundation | Complete persistence/event/snapshot parity audit. |
| `ReaADR_Core_Transactions.lua` | Support module / native foundation | Native project/model transactions exist; keep Lua until all callers migrate. |
| `ReaADR_Cue_Info_Panel.lua` | Native foundation/backend | Native live view, take counts, inline editing, character choices, filtered navigation and Space transport shortcut exist. Remaining: docking/window-state persistence, Windows presentation, launch-option parity, host/UI exposure and REAPER smoke testing. |
| `ReaADR_Cue_Manager.lua` | Native routed/backend | Full native Manager parity smoke test. |
| `ReaADR_Cue_Manager_Gfx.lua` | Lua reference / native Manager replacement in progress | Visual/interaction parity audit; keep as reference. |
| `ReaADR_Cue_Manager_ImGui.lua` | Lua reference / native Manager replacement in progress | Visual/interaction parity audit; keep as reference. |
| `ReaADR_Detect_Dialogue.lua` | Native routed/backend | REAPER scan/result parity and native-entry smoke test. |
| `ReaADR_Export_Cue_Sheet.lua` | Native backend/routed | Export-format parity audit and smoke test. |
| `ReaADR_Export_Reports.lua` | Native backend/routed | Report-output parity audit and smoke test. |
| `ReaADR_Generate_Cues.lua` | Native routed/backend | Marker/region parity smoke test; retain Lua reference. |
| `ReaADR_Import_Cue_Sheet.lua` | Native routed/backend | Mapping/preview/XLSX/error-path parity audit and REAPER smoke test. |
| `ReaADR_Import_Script.lua` | Lua wrapper/reference | Confirm equivalent native import routing before retirement. |
| `ReaADR_Jump_To_Cue.lua` | Native backend/routed | Action-entry parity and smoke test. |
| `ReaADR_Menu.lua` | Lua reference | Native command/menu routing audit. |
| `ReaADR_Next_Cue.lua` | Native backend/routed | Action-entry parity and wrap/filter smoke test. |
| `ReaADR_Open_Manager.lua` | Native routed | Keep wrapper until native Manager is parity-approved. |
| `ReaADR_Open_Manager_1.lua` | Lua wrapper/reference | Quick/alternate Manager-entry parity. |
| `ReaADR_Open_Manager_2.lua` | Lua wrapper/reference | Quick/alternate Manager-entry parity. |
| `ReaADR_Open_Manager_3.lua` | Lua wrapper/reference | Quick/alternate Manager-entry parity. |
| `ReaADR_Overlay.lua` | Native foundation/backend | Audit wrapper behavior and entry routing. |
| `ReaADR_Overlay_Settings.lua` | Native backend/routed | Settings UI parity and smoke test. |
| `ReaADR_Preferences.lua` | Native backend/routed | Preferences UI parity and smoke test. |
| `ReaADR_Previous_Cue.lua` | Native backend/routed | Action-entry parity and wrap/filter smoke test. |
| `ReaADR_Quick_Action_1.lua` | Lua wrapper/reference | Native configurable quick-action entry parity. |
| `ReaADR_Quick_Action_2.lua` | Lua wrapper/reference | Native configurable quick-action entry parity. |
| `ReaADR_Quick_Action_3.lua` | Lua wrapper/reference | Native configurable quick-action entry parity. |
| `ReaADR_Quick_Action_4.lua` | Lua wrapper/reference | Native configurable quick-action entry parity. |
| `ReaADR_Record_Arm.lua` | Native backend | Record-arm capture/isolation/restore exists; verify through full Record Cue smoke test. |
| `ReaADR_Record_Cue.lua` | Native foundation/backend | Native workflow/window, Space transport shortcut, SMPTE timing display and floating geometry persistence exist. Remaining: true docker-state parity, Windows presentation, host/UI exposure and REAPER smoke tests. |
| `ReaADR_Set_Cue_Status.lua` | Native backend | Add native choice presentation/host routing and smoke-test positional targeting. |

## Host registration audit

The host action table currently contains a stale compatibility entry for `Scripts/ReaADRTools/scripts/ReaADR_Monitor_Markers.lua`, but that file is not present in the current `scripts/` tree. Treat this as host-registration cleanup, not as a migration target. Remove the stale registration when the host action table is next edited safely.

The native host service also now consumes `GetSet_LoopTimeRange2` and `CountTakes`. Because the extension uses `REAPERAPI_MINIMAL`, their `REAPERAPI_WANT_*` declarations must be added in the same translation unit as `REAPERAPI_IMPLEMENT` before runtime parity is approved. Do not treat a successful shared-object link alone as proof that those APIs will be loaded by REAPER.

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

When native behavior intentionally differs from Lua, document the difference here and in the relevant migration/design document before treating it as parity. Do not silently redefine parity by deleting the reference implementation.
