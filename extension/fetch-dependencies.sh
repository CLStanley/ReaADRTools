#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# shellcheck disable=SC1091
. "$root/extension/dependencies.lock"

fetch_at_commit() {
  url=$1
  destination=$2
  commit=$3
  if [ -d "$destination/.git" ] && [ "$(git -C "$destination" rev-parse HEAD)" = "$commit" ]; then
    return
  fi
  if [ -e "$destination" ]; then
    echo "Dependency path exists at the wrong revision: $destination" >&2
    echo "Remove it explicitly before fetching pinned dependencies." >&2
    exit 1
  fi
  git init -q "$destination"
  git -C "$destination" remote add origin "$url"
  git -C "$destination" fetch -q --depth 1 origin "$commit"
  git -C "$destination" checkout -q --detach FETCH_HEAD
  actual=$(git -C "$destination" rev-parse HEAD)
  if [ "$actual" != "$commit" ]; then
    echo "Revision verification failed for $destination" >&2
    exit 1
  fi
}

mkdir -p "$root/vendor"
fetch_at_commit https://github.com/justinfrankel/reaper-sdk.git "$root/vendor/reaper-sdk" "$REAPER_SDK_COMMIT"
fetch_at_commit https://github.com/justinfrankel/WDL.git "$root/vendor/WDL" "$WDL_COMMIT"
