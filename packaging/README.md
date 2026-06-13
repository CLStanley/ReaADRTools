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

## Building The Windows DLL

Recommended build environment: MSYS2 UCRT64 on Windows.

1. Install MSYS2 from <https://www.msys2.org/>.
2. Open the `MSYS2 UCRT64` terminal, not the plain MSYS terminal.
3. Install the build tools:

```sh
pacman -Syu
pacman -S --needed git make mingw-w64-ucrt-x86_64-gcc zip
```

4. Clone or copy this repo onto the Windows machine.
5. Make sure the vendor dependencies exist:

```sh
git clone https://github.com/justinfrankel/reaper-sdk vendor/reaper-sdk
git clone https://github.com/justinfrankel/WDL vendor/WDL
```

If the repo already includes `vendor/reaper-sdk` and `vendor/WDL`, skip this
step.

6. Build the DLL:

```sh
cd extension
make -f Makefile.windows dist
```

Expected output:

```text
dist/UserPlugins/reaper_reaadr-x64.dll
```

Verify the DLL exports REAPER's real entrypoint name:

```sh
objdump -p ../dist/UserPlugins/reaper_reaadr-x64.dll | grep ReaperPluginEntry
```

`REAPER_PLUGIN_ENTRYPOINT` is a C/C++ macro name. The symbol REAPER needs to
see in the compiled DLL is `ReaperPluginEntry`.

7. Rebuild the release packages:

```sh
cd ..
packaging/create-release-packages.sh
```

The Windows zip should then contain:

```text
ReaADRTools-windows/UserPlugins/reaper_reaadr-x64.dll
```

That DLL is what makes the top-level `ReaADR Tools` menu appear in Windows
REAPER after install and restart.

If the Action List does not show `ReaADR`, check:

```text
%APPDATA%\REAPER\UserPlugins\ReaADRTools\reaper_reaadr.log
```

If actions appear but the top-level menu does not, the DLL loaded but REAPER did
not provide the customizable-menu hook expected by this extension build.
