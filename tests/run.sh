#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec lua "$repo_root/tests/run.lua" "$repo_root"
