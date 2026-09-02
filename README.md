# ReaADR Tools

ReaADR Tools is an ADR workflow extension for REAPER 7+. It currently combines
a native REAPER extension with Lua/ReaScript modules for script import, cue
generation, video overlays, character track organization, cue filtering,
reporting, and session maintenance.
It can also create editable cue regions from detected dialogue in selected
audio/video media for script-building workflows.

The project is migrating incrementally to an entirely C++ implementation. The
[C++ Migration Plan](docs/CPP_MIGRATION.md) defines the compatibility rules,
target architecture, and feature cutover sequence.

## Architecture Overview
The system follows a hybrid architecture:

*   **Native Bridge:** A small C++ extension handles core REAPER integration, advanced media analysis (detection), and heavy lifiting like `.xlsx` parsing.
*   **Lua Application Framework:** The primary logic layer where most features reside, ensuring rapid development and flexibility.
*   **Session Model (Source of Truth):** All workflow data is stored in a unified ADR Session Model within the project's `extstate`. REAPER elements are rendered from this model rather than being directly manipulated as disconnected objects.

## Core Components
- **ReaADR_Core:** The shared logic layer for state management, persistence, undo/redo handling, and media integration.
- **Feature Modules:** Specific handlers for Import, Cue Management (supporting both ReaImGui and legacy `gfx` UI), Reporting, and Overlay generation.

## Public Actions

- `Open Manager`
- `Quick Action 1`
- `Quick Action 2`
- `Quick Action 3`
- `Quick Action 4`
- `Validate Session Model (Native Preview)`

Older standalone scripts remain packaged as internal manager modules, but the
extension unregisters them as public actions to keep REAPER's Action List clean.
The native-preview validation action is the first public workflow action that
runs entirely in C++; it reads and validates the same canonical project model
without modifying the session.
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
- [Branding and Asset Use](docs/BRANDING.md)
- [Repository Inventory](docs/REPOSITORY_INVENTORY.md)
- [Beta Readiness](docs/BETA_READINESS.md)
- [Import Test Documents](<docs/test docs/README.md>)

## Build

Linux x86_64 builds use the Makefile in `extension/`:

```sh
extension/fetch-dependencies.sh
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

The packaging script creates a platform archive only when the matching native
binary is present. A Linux build does not fabricate Windows or macOS packages.
Every created payload is checked by `packaging/validate-release-package.sh`.

The installer copies the native `UserPlugins` payload and assets into the
user's REAPER resource folder and asks them to restart REAPER. The native menu
extension must be built separately for each platform:

- Windows: `reaper_reaadr*.dll`
- macOS: `reaper_reaadr*.dylib`
- Linux: `reaper_reaadr*.so`

The current local Makefile builds the Linux `.so`. Windows packages must
use the MSVC-built `reaper_reaadr.dll`. macOS packages still need their native
`.dylib` added before public release.

Each package includes a platform-specific uninstaller. Uninstallers remove only
the ReaADR native extension and `Scripts/ReaADRTools`; they do not modify REAPER
projects, recordings, or project-local ReaADR Session Model data.

## Development Notes

The native extension is now the only installed workflow runtime. Native UI
cutover must preserve the established ReaADR Tools Manager and visual Cue
Manager designs; remaining parity work is tracked in `docs/CPP_MIGRATION.md`.

Run local deterministic checks without launching REAPER:

```sh
tests/run.sh
make -C extension test
shellcheck packaging/*.sh packaging/*.command extension/*.sh tests/*.sh
```

Native dependency revisions are pinned in `extension/dependencies.lock`.
GitHub Actions runs native tests, shellcheck, and serial/parallel Linux native
build validation on pushes and pull requests.

## License

Except where otherwise noted, the source code in this repository is licensed
under the [MIT License](LICENSE). Third-party components and assets remain
subject to their respective licenses.

The ReaADR Tools name, logo, and other branding assets are not included in the
MIT License. See [Branding and Asset Use](docs/BRANDING.md) for permitted uses.
