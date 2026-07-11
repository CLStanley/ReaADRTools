#!/usr/bin/env sh
set -eu

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 PACKAGE_DIRECTORY PLATFORM" >&2
  exit 2
fi

package_dir=$1
platform=$2

fail() {
  echo "Release validation failed for $platform: $1" >&2
  exit 1
}

[ -d "$package_dir/UserPlugins" ] || fail "UserPlugins is missing"
[ -d "$package_dir/Scripts/ReaADRTools/scripts" ] || fail "runtime scripts are missing"
[ -d "$package_dir/Scripts/ReaADRTools/assets" ] || fail "runtime assets are missing"
[ -f "$package_dir/README.md" ] || fail "README.md is missing"
[ -f "$package_dir/USER_GUIDE.md" ] || fail "USER_GUIDE.md is missing"
[ -f "$package_dir/THIRD_PARTY_NOTICES.md" ] || fail "THIRD_PARTY_NOTICES.md is missing"

case "$platform" in
  linux-x64)
    expected='*.so'
    forbidden='*.dll *.dylib'
    ;;
  windows)
    expected='*.dll'
    forbidden='*.so *.dylib'
    ;;
  macos)
    expected='*.dylib'
    forbidden='*.so *.dll'
    ;;
  *) fail "unknown platform" ;;
esac

[ -n "$(find "$package_dir/UserPlugins" -maxdepth 1 -type f -name "$expected")" ] ||
  fail "required native extension is missing"

for pattern in $forbidden; do
  [ -z "$(find "$package_dir/UserPlugins" -maxdepth 1 -type f -name "$pattern")" ] ||
    fail "wrong-platform native extension found: $pattern"
done

for forbidden_path in tests build vendor .git; do
  [ ! -e "$package_dir/$forbidden_path" ] || fail "development path included: $forbidden_path"
done

if find "$package_dir" -type f \( -name '*.obj' -o -name '*.o' -o -name '*.log' -o -name '*.reapeaks' \) | grep -q .; then
  fail "compiler, log, or peak artifacts are present"
fi

echo "Validated $platform release payload: $package_dir"
