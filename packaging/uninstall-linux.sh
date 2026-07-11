#!/usr/bin/env sh
set -eu

resource_dir="${REAPER_RESOURCE_DIR:-$HOME/.config/REAPER}"
userplugins_dir="${REAPER_USERPLUGINS_DIR:-$resource_dir/UserPlugins}"
scripts_dir="${REAPER_SCRIPTS_DIR:-$resource_dir/Scripts}"

rm -f "$userplugins_dir"/reaper_reaadr*.so
rm -rf "$scripts_dir/ReaADRTools"

echo "Removed ReaADR Tools program files."
echo "REAPER projects, recordings, and project-local ReaADR session data were not modified."
