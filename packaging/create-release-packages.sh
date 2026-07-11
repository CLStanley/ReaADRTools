#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dist_dir="$root_dir/dist"
release_dir="$dist_dir/installers"

if [ ! -d "$dist_dir/UserPlugins" ] || [ ! -d "$dist_dir/Scripts" ]; then
  echo "No complete dist payload found. Run this first:"
  echo "  cd extension && make dist"
  exit 1
fi

rm -rf "$release_dir"
mkdir -p "$release_dir"

make_package() {
  platform="$1"
  installer="$2"
  uninstaller="$3"
  binary_pattern="$4"
  package_dir="$release_dir/ReaADRTools-$platform"

  binaries=$(find "$dist_dir/UserPlugins" -maxdepth 1 -type f -name "$binary_pattern")
  if [ -z "$binaries" ]; then
    echo "Skipping $platform: no $binary_pattern binary is available."
    return 0
  fi

  mkdir -p "$package_dir/UserPlugins"
  find "$dist_dir/UserPlugins" -maxdepth 1 -type f -name "$binary_pattern" \
    -exec cp {} "$package_dir/UserPlugins/" \;
  cp -R "$dist_dir/Scripts" "$package_dir/Scripts"
  cp "$root_dir/packaging/$installer" "$package_dir/"
  cp "$root_dir/packaging/$uninstaller" "$package_dir/"
  cp "$root_dir/README.md" "$package_dir/"
  cp "$root_dir/docs/USER_GUIDE.md" "$package_dir/"
  cp "$root_dir/THIRD_PARTY_NOTICES.md" "$package_dir/"
  chmod +x "$package_dir/$installer" "$package_dir/$uninstaller" 2>/dev/null || true

  "$root_dir/packaging/validate-release-package.sh" "$package_dir" "$platform"

  (
    cd "$release_dir"
    zip -qr "ReaADRTools-$platform.zip" "ReaADRTools-$platform"
  )
}

make_package "linux-x64" "install-linux.sh" "uninstall-linux.sh" "reaper_reaadr*.so"
make_package "macos" "install-macos.command" "uninstall-macos.command" "reaper_reaadr*.dylib"
make_package "windows" "install-windows.bat" "uninstall-windows.bat" "reaper_reaadr*.dll"

echo "Created installer packages in:"
echo "$release_dir"
