@echo off
setlocal

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

del /Q "%USERPLUGINS_DIR%\reaper_reaadr*.dll" 2>nul
if exist "%SCRIPTS_DIR%\ReaADRTools" rmdir /S /Q "%SCRIPTS_DIR%\ReaADRTools"

echo Removed ReaADR Tools program files.
echo REAPER projects, recordings, and project-local ReaADR session data were not modified.
pause
