@echo off
setlocal

set "ROOT=%~dp0.."
set "REAPER_SDK=%ROOT%\vendor\reaper-sdk"
set "WDL=%ROOT%\vendor\WDL\WDL"
set "BUILD_DIR=%ROOT%\build\extension\windows-msvc-x64"
set "DIST_DIR=%ROOT%\dist"
set "DIST_USERPLUGINS_DIR=%DIST_DIR%\UserPlugins"
set "DIST_REAADR_DIR=%DIST_DIR%\Scripts\ReaADRTools"
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
if not exist "%DIST_USERPLUGINS_DIR%" mkdir "%DIST_USERPLUGINS_DIR%"
if not exist "%DIST_REAADR_DIR%\scripts" mkdir "%DIST_REAADR_DIR%\scripts"
if not exist "%DIST_REAADR_DIR%\assets" mkdir "%DIST_REAADR_DIR%\assets"

pushd "%~dp0"
cl /nologo /EHsc /O2 /LD /std:c++17 ^
  /I"%REAPER_SDK%\sdk" ^
  /I"%WDL%" ^
  reaper_reaadr.cpp ^
  reaadr_core\session_model.cpp ^
  reaadr_core\model_repository.cpp ^
  reaadr_core\domain_utils.cpp ^
  reaadr_core\cue_import.cpp ^
  reaadr_core\session_builder.cpp ^
  reaadr_reaper\project_state.cpp ^
  reaadr_reaper\project_transaction.cpp ^
  /Fe"%BUILD_DIR%\%TARGET%" ^
  /link user32.lib /DEF:reaper_reaadr.def

if errorlevel 1 (
  popd
  exit /b 1
)

copy /Y "%BUILD_DIR%\%TARGET%" "%DIST_USERPLUGINS_DIR%\%TARGET%" >nul
xcopy "%ROOT%\scripts\*.lua" "%DIST_REAADR_DIR%\scripts\" /Y >nul
xcopy "%ROOT%\assets\*" "%DIST_REAADR_DIR%\assets\" /Y >nul
popd

echo Built "%DIST_USERPLUGINS_DIR%\%TARGET%"
echo Copy dist\UserPlugins into %%APPDATA%%\REAPER\UserPlugins and dist\Scripts into %%APPDATA%%\REAPER\Scripts, then restart REAPER.
