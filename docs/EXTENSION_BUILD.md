# ReaADR Extension Build Notes

ReaADR Tools is migrating to a native C++ REAPER extension. The extension now owns the public workflow/action registrations and the active native Manager, Record Cue, Cue Info, Set Cue Status, Dialogue Detection, Quick Actions, import/export, overlay, navigation, and session-service paths. Small Lua files that remain in `scripts/` are compatibility launchers or substantive migration references; they are not the production implementation of the migrated workflows.

The canonical project model remains `adr_session_model_v1`. During migration, preserve the model, ownership/deletion boundaries, undo/rollback behavior, and the host-provided SWELL/modstub architecture.

Migration/parity status is tracked in `LUA_PARITY_MATRIX.md`. Architecture details are in `CPP_MIGRATION.md` and `CODE_ARCHITECTURE.md`.

## Dependencies

Development builds require the pinned REAPER SDK and WDL revisions recorded in `extension/dependencies.lock`:

- REAPER SDK: `ec60fb4c38e1f575e29e28bd01fcf50dbf1c0bc7`
- WDL: `599228ffee6ad8d02122a171e0e79271b24abbd3`

Fetch and verify them with:

```sh
extension/fetch-dependencies.sh
```

The fetcher refuses to silently replace an existing dependency checkout at another revision.

## Native tests

On Linux:

```sh
cd extension
make test
```

On macOS, the native tests currently need Cocoa on the final test link. CI supplies it through `CXX` because the Makefile test link does not consume `LDFLAGS`:

```sh
cd extension
make test CXX="c++ -framework Cocoa"
```

Windows compilation is validated through the MSVC extension build described below.

## Linux x86_64

```sh
cd extension
make clean
make
make test
make dist
```

The distributable layout contains the native extension and native runtime assets:

```text
dist/UserPlugins/
  reaper_reaadr-x86_64.so
dist/Scripts/
  ReaADRTools/
    assets/
```

## Windows x64

Use an **x64 Native Tools Command Prompt for Visual Studio 2022** (or later):

```bat
cd extension
build-windows-msvc.bat
```

The build produces:

```text
dist\UserPlugins\reaper_reaadr.dll
dist\Scripts\ReaADRTools\assets\
```

The Windows Cue Manager is a persistent/modeless native Win32 window. REAPER owns the host message loop. Window layout/dock state is persisted through the native Manager controller and REAPER docking adapter.

## macOS

The macOS extension must be emitted as a `.dylib`; the macOS installer explicitly expects `reaper_reaadr*.dylib` in `UserPlugins`.

The CI build is:

```sh
cd extension
make test CXX="c++ -framework Cocoa"
make clean && make TARGET=reaper_reaadr.dylib LDFLAGS="-shared -framework Cocoa"
test -f build/extension/reaper_reaadr.dylib
file build/extension/reaper_reaadr.dylib
```

The native non-Windows UI retains the host-provided SWELL/modstub design. Do not link a private SWELL implementation into the extension. Linux uses `swell-modstub-generic.cpp`; macOS uses `swell-modstub.mm` and the host-provided SWELL boundary.

## Continuous integration

GitHub Actions currently validates all three platform paths:

- Linux native tests, extension build, and package/layout checks.
- Windows MSVC extension build and payload validation.
- macOS native tests plus a Cocoa-linked `reaper_reaadr.dylib` build and binary verification.
- Shared native/shell validation jobs used by the repository migration gates.

CI proves compilation and automated behavior but does not replace in-REAPER smoke testing. Docking, modeless lifecycle, transport interaction, video-window behavior, and end-to-end recording should be exercised in REAPER before release.

## Install

Copy the platform extension binary from `dist/UserPlugins` into the user's REAPER resource `UserPlugins` folder. Copy `dist/Scripts/ReaADRTools` into the resource `Scripts/ReaADRTools` folder when that payload is present, then restart REAPER.

No manual ReaScript action import is required for migrated public workflows; the extension registers the native actions.

## Native action ownership

The extension is the authoritative owner of the migrated action surface. This includes the native Manager and the migrated workflow actions such as Record Cue, Cue Info, Set Cue Status, Dialogue Detection, Quick Actions, navigation, import/export, overlay refresh/settings, and session operations exposed through the Manager.

Historical Lua action paths may still be explicitly unregistered by the legacy-registration cleanup boundary so old installs do not leave duplicate actions. Compatibility launchers may remain temporarily for historical action paths, but they delegate to native commands and are not fallback implementations.

## Cue Manager

The active Cue Manager is the native C++ `CueManagerController` / `CueManagerSession` workflow with persistent modeless host ownership. It provides cue browsing, filtering, selection, navigation, editing, add/remove operations, recording and Cue Info entry points, import/session/report/overlay/preferences modules, project revision refresh, and window layout/dock persistence.

Windows uses the programmatic Win32 Manager implementation. Linux/macOS use the host-provided SWELL implementation. Remaining UI parity items are tracked in `LUA_PARITY_MATRIX.md` rather than being hidden in build documentation.

## Import and dialogue detection

Cue-sheet/script import is routed through the native transactional importer. Supported mapping and format parity should be verified against `LUA_PARITY_MATRIX.md` during migration testing.

`Detect Dialogue From Selected Media` is also native. It detects dialogue from selected media, previews the detected cue set, commits through the canonical repositories/render services, and refreshes generated overlay state transactionally.

## Character filter and overlays

Character filtering, generated overlay refresh, overlay settings/profiles, and their Manager entry points are native-routed. The native implementation must continue to respect canonical cue state and ReaADR ownership boundaries when muting/hiding generated material.

## Reports and exports

Native report/export services are available through the Manager. The intended report surface includes cue-sheet data, timing data, session metadata, session summary/full-session data, and other migrated report formats. Platform-specific Manager presentation parity is tracked in `LUA_PARITY_MATRIX.md`.

## Migration discipline

Migration completeness and behavioral parity come before cleanup or new features. Substantive Lua implementations remain available as behavioral references until the corresponding native workflow has passed parity review and in-REAPER smoke testing. Thin compatibility launchers can remain while historical action paths are useful. Obsolete support modules with no remaining consumers can be retired after a reference/runtime audit.

Before deleting the final Lua reference stack, verify:

1. Public workflow ownership is native.
2. Canonical model, ownership, transaction, and rollback invariants are preserved.
3. Linux, Windows, and macOS CI are green.
4. Manager, recording, import/export, dialogue detection, overlays, navigation, docking, and lifecycle have been smoke-tested in REAPER.
5. `LUA_PARITY_MATRIX.md`, this build guide, and the architecture/migration docs describe the actual native implementation.
