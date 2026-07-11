# ReaADR Tools Installer Packaging

The install payload mirrors the REAPER resource folder:

```text
UserPlugins/
  reaper_reaadr-... platform extension
Scripts/
  ReaADRTools/
    assets/
```

Install targets:

- Windows: `%APPDATA%\REAPER\UserPlugins` and `%APPDATA%\REAPER\Scripts`
- macOS: `~/Library/Application Support/REAPER/UserPlugins` and `~/Library/Application Support/REAPER/Scripts`
- Linux: `~/.config/REAPER/UserPlugins` and `~/.config/REAPER/Scripts`

The installer scripts copy the native extension and assets into the REAPER
resource folder, remove the old bundled
`UserPlugins/ReaADRTools` folder, and then instruct the user to restart REAPER.
The platform uninstallers remove only the ReaADR native extension and the
`Scripts/ReaADRTools` program directory. Project files, recordings, and
project-local Session Model data are not touched.

## Build Packages

From a Linux development machine:

```sh
cd extension
make dist
cd ..
packaging/create-release-packages.sh
```

`make dist` uses `extension/Makefile`.

This creates an archive only for each matching native binary currently present
in `dist/UserPlugins`. After a normal Linux build, this creates:

```text
dist/installers/ReaADRTools-linux-x64.zip
```

Windows and macOS archives are skipped until their `.dll` or `.dylib` has been
built and placed in `dist/UserPlugins`. This prevents a Linux `.so` from being
published inside an archive labeled for another platform.

Each package is validated before its ZIP is written. Validation checks required
runtime directories and documentation, requires the correct platform binary,
rejects wrong-platform binaries, and rejects common development artifacts.

## Native Binary Requirement

The Lua scripts are cross-platform, but the top-level REAPER menu comes from a
native extension binary. Each release package needs the correct binary:

- Linux: `reaper_reaadr*.so`
- macOS: `reaper_reaadr*.dylib`
- Windows: `reaper_reaadr*.dll`

The current Makefile only creates the Linux `.so`. Windows packages need
the MSVC-built `reaper_reaadr.dll`, and macOS packages need a platform-native
`.dylib`, before they will show the top-level ReaADR Tools menu.

## Building The Windows DLL With MSVC

Supported build environment: Microsoft Visual Studio Build Tools on Windows.
Windows builds require MSVC.

1. Install Visual Studio Build Tools with the `Desktop development with C++`
   workload.
2. Open `x64 Native Tools Command Prompt for VS`.
3. From the repo root, run:

```bat
extension\build-windows-msvc.bat
```

Expected output:

```text
dist\UserPlugins\reaper_reaadr.dll
dist\Scripts\ReaADRTools\assets\*
```

Verify the DLL exports REAPER's entrypoint:

```bat
dumpbin /exports dist\UserPlugins\reaper_reaadr.dll | findstr ReaperPluginEntry
```

Install `dist\UserPlugins\reaper_reaadr.dll` directly into:

```text
%APPDATA%\REAPER\UserPlugins
```

Install `dist\Scripts\ReaADRTools` into:

```text
%APPDATA%\REAPER\Scripts\ReaADRTools
```

Then restart REAPER.

After building the MSVC DLL, rebuild the release packages:

```sh
packaging/create-release-packages.sh
```

If Linux and Windows binaries share `dist/UserPlugins`, separate Linux and
Windows archives are produced and each receives only its matching native file.

The Windows zip should then contain:

```text
ReaADRTools-windows/UserPlugins/reaper_reaadr.dll
ReaADRTools-windows/Scripts/ReaADRTools/assets/*
```

That DLL is what makes the top-level `ReaADR Tools` menu appear in Windows
REAPER after install and restart.

If the Action List does not show `ReaADR`, check:

```text
%APPDATA%\REAPER\UserPlugins\reaper_reaadr.log
```

If actions appear but the top-level menu does not, the DLL loaded but REAPER did
not provide the customizable-menu hook expected by this extension build.

## Uninstall

Each platform package contains a matching uninstaller:

- Linux: `uninstall-linux.sh`
- Windows: `uninstall-windows.bat`
- macOS: `uninstall-macos.command`

Environment overrides supported by the installers are also honored by the
uninstallers. Uninstall removes only `reaper_reaadr*` for the active platform
from `UserPlugins` and the `Scripts/ReaADRTools` program folder.

## Release Content Boundary

Release packages include runtime scripts, assets, the correct native extension,
the README, user guide, third-party notices, and platform install/uninstall
launchers. They exclude tests, test documents, SRS/roadmap documents, build
scripts, dependency source, Git metadata, and compiler intermediates.
