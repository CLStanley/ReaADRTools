# ReaADR Tools

ReaADR Tools is an ADR workflow extension for REAPER 7+. It combines a thin
native REAPER extension with Lua/ReaScript modules for script import, cue
generation, video overlays, character track organization, cue filtering,
reporting, and session maintenance.
It can also create editable cue regions from detected dialogue in selected
audio/video media for script-building workflows.

## Current Architecture

```text
Native REAPER extension
  Registers a small ReaADR Tools menu
  Provides native media-analysis and XLSX import helpers
  Launches Lua application entry points

Lua application framework
  ReaADR Tools Manager
  Import, cue, session, report, video overlay, preference, and help modules
  Shared ReaADR_Core session/model helpers
```

The native extension is intentionally small. Most feature development happens
in Lua so ADR workflows can evolve quickly without requiring compiled-plugin
changes for every feature.
Native code is reserved for stable REAPER integration and helpers that are
better handled outside Lua, currently selected-media dialogue detection and
first-worksheet `.xlsx` ingestion.

## Public Actions

- `Open Manager`
- `Quick Action 1`
- `Quick Action 2`
- `Quick Action 3`
- `Quick Action 4`

Older standalone scripts remain packaged as internal manager modules, but the
extension unregisters them as public actions to keep REAPER's Action List clean.
Quick actions are configured from `Open Manager > Preferences` with inline
dropdowns. Overlay settings are available directly in `Open Manager > Video
Overlays`.

The manager launcher supports up to three concurrent ReaADR manager windows.

The manager includes hover hints for its controls and a Help tab with searchable
workflow guidance.

Cue Manager now prefers a ReaImGui interface when ReaImGui is installed and
falls back to the legacy `gfx` manager otherwise. Cue editing, cue removal with
renumbering, inline status/type editing, Refresh Session, and overlay refresh
behavior remain available through the shared Lua session layer.

## Documentation

- [User Guide](docs/USER_GUIDE.md)
- [Code Architecture](docs/CODE_ARCHITECTURE.md)
- [Addendum Implementation Backlog](docs/ADDENDUM_IMPLEMENTATION_BACKLOG.md)
- [Extension Build Notes](docs/EXTENSION_BUILD.md)
- [Installer Packaging](docs/PACKAGING.md)
- [Import Test Documents](<docs/test docs/README.md>)

## Build

Linux x86_64 builds use the Makefile in `extension/`:

```sh
cd extension
make dist
```

The distributable is created under `dist/UserPlugins`.

## Installer Packages

Installer launchers live in `packaging/`:

- Windows: `install-windows.bat`
- macOS: `install-macos.command`
- Linux: `install-linux.sh`

Create zip packages with:

```sh
packaging/create-release-packages.sh
```

The installer copies the bundled `UserPlugins` payload into the user's REAPER
resource folder and asks them to restart REAPER. The Lua scripts are
cross-platform, but the native menu extension must be built separately for each
platform:

- Windows: `reaper_reaadr*.dll`
- macOS: `reaper_reaadr*.dylib`
- Linux: `reaper_reaadr*.so`

The current local Makefile builds the Linux `.so`. Windows packages must
use the MSVC-built `reaper_reaadr.dll`. macOS packages still need their native
`.dylib` added before public release.

## Development Notes

The current UI stack is mixed:

- ReaImGui-first Cue Manager
- `gfx` utility windows and legacy Cue Manager fallback

Most workflow logic should continue to live in `scripts/ReaADR_Core.lua` so UI
migration does not fork behavior between interfaces.
