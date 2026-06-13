@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PAYLOAD_DIR=%SCRIPT_DIR%UserPlugins"
if not exist "%PAYLOAD_DIR%\" (
  if exist "%SCRIPT_DIR%..\dist\UserPlugins\" set "PAYLOAD_DIR=%SCRIPT_DIR%..\dist\UserPlugins"
)

if "%REAPER_USERPLUGINS_DIR%"=="" (
  set "TARGET_DIR=%APPDATA%\REAPER\UserPlugins"
) else (
  set "TARGET_DIR=%REAPER_USERPLUGINS_DIR%"
)

if not exist "%PAYLOAD_DIR%\" (
  echo ReaADR Tools installer error: UserPlugins payload was not found.
  echo Expected: %SCRIPT_DIR%UserPlugins
  exit /b 1
)

dir /b "%PAYLOAD_DIR%\reaper_reaadr*.dll" >nul 2>nul
if errorlevel 1 (
  echo ReaADR Tools installer warning: no Windows extension binary was found in the payload.
  echo The Lua files can be copied, but the top-level REAPER menu will not appear without reaper_reaadr*.dll.
)

if not exist "%TARGET_DIR%\" mkdir "%TARGET_DIR%"
xcopy "%PAYLOAD_DIR%\*" "%TARGET_DIR%\" /E /I /Y >nul

echo ReaADR Tools installed to:
echo %TARGET_DIR%
echo.
echo Restart REAPER, then open the ReaADR Tools menu.
pause
