@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PAYLOAD_ROOT=%SCRIPT_DIR%"
if not exist "%PAYLOAD_ROOT%UserPlugins\" (
  if exist "%SCRIPT_DIR%..\dist\UserPlugins\" set "PAYLOAD_ROOT=%SCRIPT_DIR%..\dist\"
)

if "%REAPER_RESOURCE_DIR%"=="" (
  set "RESOURCE_DIR=%APPDATA%\REAPER"
) else (
  set "RESOURCE_DIR=%REAPER_RESOURCE_DIR%"
)

if "%REAPER_USERPLUGINS_DIR%"=="" (
  set "USERPLUGINS_DIR=%RESOURCE_DIR%\UserPlugins"
) else (
  set "USERPLUGINS_DIR=%REAPER_USERPLUGINS_DIR%"
)

if "%REAPER_SCRIPTS_DIR%"=="" (
  set "SCRIPTS_DIR=%RESOURCE_DIR%\Scripts"
) else (
  set "SCRIPTS_DIR=%REAPER_SCRIPTS_DIR%"
)

if not exist "%PAYLOAD_ROOT%UserPlugins\" (
  echo ReaADR Tools installer error: install payload was not found.
  echo Expected: %SCRIPT_DIR%UserPlugins
  exit /b 1
)
if not exist "%PAYLOAD_ROOT%Scripts\" (
  echo ReaADR Tools installer error: install payload was not found.
  echo Expected: %SCRIPT_DIR%Scripts
  exit /b 1
)

dir /b "%PAYLOAD_ROOT%UserPlugins\reaper_reaadr*.dll" >nul 2>nul
if errorlevel 1 (
  echo ReaADR Tools installer warning: no Windows extension binary was found in the payload.
  echo The Lua files can be copied, but the top-level REAPER menu will not appear without reaper_reaadr*.dll.
)

if not exist "%USERPLUGINS_DIR%\" mkdir "%USERPLUGINS_DIR%"
if not exist "%SCRIPTS_DIR%\" mkdir "%SCRIPTS_DIR%"
xcopy "%PAYLOAD_ROOT%UserPlugins\*" "%USERPLUGINS_DIR%\" /E /I /Y >nul
xcopy "%PAYLOAD_ROOT%Scripts\*" "%SCRIPTS_DIR%\" /E /I /Y >nul
if exist "%USERPLUGINS_DIR%\ReaADRTools\" rmdir /S /Q "%USERPLUGINS_DIR%\ReaADRTools"

echo ReaADR Tools installed to:
echo %USERPLUGINS_DIR%
echo %SCRIPTS_DIR%\ReaADRTools
echo.
echo Restart REAPER, then open the ReaADR Tools menu.
pause
