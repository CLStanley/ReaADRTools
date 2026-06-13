@echo off
setlocal

set "ROOT=%~dp0.."
set "REAPER_SDK=%ROOT%\vendor\reaper-sdk"
set "WDL=%ROOT%\vendor\WDL\WDL"
set "BUILD_DIR=%ROOT%\build\extension\windows-msvc-x64"
set "DIST_DIR=%ROOT%\dist\UserPlugins"
set "TARGET=reaper_reaadr.dll"

if not exist "%REAPER_SDK%\sdk\reaper_plugin.h" (
  echo Missing REAPER SDK at "%REAPER_SDK%\sdk".
  exit /b 1
)

if not exist "%WDL%\swell" (
  echo Missing WDL at "%WDL%".
  exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
  echo MSVC cl.exe was not found.
  echo Open "x64 Native Tools Command Prompt for VS" and run this script again.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

pushd "%~dp0"
cl /nologo /EHsc /O2 /LD ^
  /I"%REAPER_SDK%\sdk" ^
  /I"%WDL%" ^
  reaper_reaadr.cpp ^
  /Fe"%BUILD_DIR%\%TARGET%" ^
  /link user32.lib /DEF:reaper_reaadr.def

if errorlevel 1 (
  popd
  exit /b 1
)

copy /Y "%BUILD_DIR%\%TARGET%" "%DIST_DIR%\%TARGET%" >nul
popd

echo Built "%DIST_DIR%\%TARGET%"
echo Copy it to %%APPDATA%%\REAPER\UserPlugins and restart REAPER.
