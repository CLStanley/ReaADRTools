#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

payload_root="$script_dir"
if [ ! -d "$payload_root/UserPlugins" ] && [ -d "$script_dir/../dist/UserPlugins" ]; then
  payload_root="$script_dir/../dist"
fi

resource_dir="${REAPER_RESOURCE_DIR:-$HOME/.config/REAPER}"
userplugins_dir="${REAPER_USERPLUGINS_DIR:-$resource_dir/UserPlugins}"
scripts_dir="${REAPER_SCRIPTS_DIR:-$resource_dir/Scripts}"

if [ ! -d "$payload_root/UserPlugins" ] || [ ! -d "$payload_root/Scripts" ]; then
  echo "ReaADR Tools installer error: install payload was not found."
  echo "Expected: $script_dir/UserPlugins"
  echo "Expected: $script_dir/Scripts"
  exit 1
fi

if ! ls "$payload_root/UserPlugins"/reaper_reaadr*.so >/dev/null 2>&1; then
  echo "ReaADR Tools installer warning: no Linux extension binary was found in the payload."
  echo "The Lua files can be copied, but the top-level REAPER menu will not appear without reaper_reaadr*.so."
fi

mkdir -p "$userplugins_dir" "$scripts_dir"
cp -R "$payload_root/UserPlugins"/. "$userplugins_dir"/
cp -R "$payload_root/Scripts"/. "$scripts_dir"/
rm -rf "$userplugins_dir/ReaADRTools"

echo "ReaADR Tools installed to:"
echo "$userplugins_dir"
echo "$scripts_dir/ReaADRTools"
echo ""
echo "Restart REAPER, then open the ReaADR Tools menu."
