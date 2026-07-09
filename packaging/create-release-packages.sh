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
  package_dir="$release_dir/ReaADRTools-$platform"

  mkdir -p "$package_dir"
  cp -R "$dist_dir/UserPlugins" "$package_dir/UserPlugins"
  cp -R "$dist_dir/Scripts" "$package_dir/Scripts"
  cp "$root_dir/packaging/$installer" "$package_dir/"
  cp "$root_dir/README.md" "$package_dir/"
  cp "$root_dir/docs/USER_GUIDE.md" "$package_dir/"
  chmod +x "$package_dir/$installer" 2>/dev/null || true

  (
    cd "$release_dir"
    zip -qr "ReaADRTools-$platform.zip" "ReaADRTools-$platform"
  )
}

make_package "linux-x64" "install-linux.sh"
make_package "macos" "install-macos.command"
make_package "windows" "install-windows.bat"

echo "Created installer packages in:"
echo "$release_dir"
