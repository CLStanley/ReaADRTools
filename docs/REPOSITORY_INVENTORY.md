# ReaADR Tools Repository Inventory

This inventory records why each major repository area exists and whether it is
part of the installed runtime. Items with uncertain compatibility value should
be investigated before removal.

| Path | Classification | Release payload | Purpose |
| --- | --- | --- | --- |
| `scripts/` | Source/runtime | Yes | Lua application, UI entry points, workflows, and shared core modules. |
| `assets/` | Runtime assets | Yes | Logo resources used by ReaADR interfaces. |
| `extension/` | Native source/build tooling | Binary only | C++ extension, pinned dependencies, and platform build scripts. |
| `packaging/` | Packaging tooling | Install/uninstall launchers only | Builds and validates archives; installs/removes ReaADR-owned files. |
| `tests/` | Test | No | Deterministic Lua tests and their runner. |
| `docs/` | Documentation | User Guide only | Maintainer docs, beta checklist, and user documentation. |
| `docs/test docs/` | Test fixtures | No | Fictional import fixtures for CSV, TSV, TXT, and XLSX behavior. |
| `.github/workflows/` | CI | No | Automated syntax, tests, shell, native build, and package validation. |
| `vendor/` | External dependency checkout | No | Locally fetched pinned REAPER SDK and WDL; ignored by Git. |
| `build/` | Generated output | No | Compiler output; ignored by Git. |
| `dist/` | Generated output | No | Runtime staging and release archives; ignored by Git. |

## Public actions

The native extension intentionally exposes only:

- Open Manager
- Quick Action 1
- Quick Action 2
- Quick Action 3
- Quick Action 4

Other feature scripts remain runtime-required because the manager and quick
actions launch them internally. Packaging all Lua files does not mean every file
should be registered as an Action List entry.

## Compatibility-only action records

`extension/reaper_reaadr.cpp` retains legacy action definitions so older public
ReaScript registrations can be unregistered. Some definitions reference scripts
that are no longer shipped. They are not current runtime entry points and should
not be removed until upgrade behavior from older installations is verified.

## Shared core boundaries

These modules are runtime-required and preserve safety responsibilities:

- `ReaADR_Core_Persistence.lua`: Session Model persistence and mutation
- `ReaADR_Core_Transactions.lua`: Undo and UI-refresh transaction ownership
- `ReaADR_Core_Ownership.lua`: generated-object ownership predicates
- `ReaADR_Core_Characters.lua`: character filtering state
- `ReaADR_Record_Arm.lua`: record-arm capture, isolation, and restoration

The GFX Cue Manager remains a compatibility fallback for systems without a
working ReaImGui setup. Shared behavior belongs in the core rather than being
implemented independently in both UIs.

## Pre-beta decisions still required

- Select and add the project open-source license.
- Confirm ownership and redistribution rights for the logo assets.
- Decide whether example cue sheets are offered as a separate optional download.
- Validate whether legacy action-unregistration records can be reduced after an
  upgrade test from earlier public builds.
