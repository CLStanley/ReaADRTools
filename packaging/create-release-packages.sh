#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dist_dir="$root_dir/dist"
payload_dir="$dist_dir/UserPlugins"
release_dir="$dist_dir/installers"

if [ ! -d "$payload_dir" ]; then
  echo "No dist/UserPlugins payload found. Run this first:"
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
  cp -R "$payload_dir" "$package_dir/UserPlugins"
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
