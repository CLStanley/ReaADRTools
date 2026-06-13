# ReaADR Tools Installer Packaging

The install payload is the REAPER `UserPlugins` folder:

```text
UserPlugins/
  reaper_reaadr-... platform extension
  ReaADRTools/
    scripts/
    assets/
```

Install targets:

- Windows: `%APPDATA%\REAPER\UserPlugins`
- macOS: `~/Library/Application Support/REAPER/UserPlugins`
- Linux: `~/.config/REAPER/UserPlugins`

The installer scripts copy the bundled `UserPlugins` payload into the correct
REAPER resource folder and then instruct the user to restart REAPER.

## Build Packages

From a Linux development machine:

```sh
cd extension
make dist
cd ..
packaging/create-release-packages.sh
```

This creates:

```text
dist/installers/ReaADRTools-linux-x64.zip
dist/installers/ReaADRTools-macos.zip
dist/installers/ReaADRTools-windows.zip
```

## Native Binary Requirement

The Lua scripts are cross-platform, but the top-level REAPER menu comes from a
native extension binary. Each release package needs the correct binary:

- Linux: `reaper_reaadr*.so`
- macOS: `reaper_reaadr*.dylib`
- Windows: `reaper_reaadr*.dll`

The current Linux build only creates the Linux `.so`. Windows and macOS
packages can be assembled with these installer scripts, but they will not show
the top-level ReaADR Tools menu until platform-native binaries are built and
included.
