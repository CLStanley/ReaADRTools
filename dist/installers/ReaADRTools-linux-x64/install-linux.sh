#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

payload_dir="$script_dir/UserPlugins"
if [ ! -d "$payload_dir" ] && [ -d "$script_dir/../dist/UserPlugins" ]; then
  payload_dir="$script_dir/../dist/UserPlugins"
fi

target_dir="${REAPER_USERPLUGINS_DIR:-$HOME/.config/REAPER/UserPlugins}"

if [ ! -d "$payload_dir" ]; then
  echo "ReaADR Tools installer error: UserPlugins payload was not found."
  echo "Expected: $script_dir/UserPlugins"
  exit 1
fi

if ! ls "$payload_dir"/reaper_reaadr*.so >/dev/null 2>&1; then
  echo "ReaADR Tools installer warning: no Linux extension binary was found in the payload."
  echo "The Lua files can be copied, but the top-level REAPER menu will not appear without reaper_reaadr*.so."
fi

mkdir -p "$target_dir"
cp -R "$payload_dir"/. "$target_dir"/

echo "ReaADR Tools installed to:"
echo "$target_dir"
echo ""
echo "Restart REAPER, then open the ReaADR Tools menu."
