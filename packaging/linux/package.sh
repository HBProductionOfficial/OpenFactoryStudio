#!/usr/bin/env bash
# Builds the Linux AppImage: package.sh [--skip-build] [preset]. Needs linuxdeploy and its Qt
# plugin (on PATH or in LINUXDEPLOY_DIR) and the qmake of the Qt the build uses (QMAKE).
set -euo pipefail
skip_build=0
if [ "${1:-}" = "--skip-build" ]; then skip_build=1; shift; fi
preset="${1:-linux-release}"
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

if [ "$skip_build" = 0 ]; then
    cmake --preset "$preset"
    cmake --build --preset "$preset"
fi
build="build/$preset"
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")"

appdir="$root/build/AppDir"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
install -Dm644 packaging/linux/openfactorystudio.desktop "$appdir/usr/share/applications/openfactorystudio.desktop"
install -Dm644 packaging/linux/openfactorystudio.svg "$appdir/usr/share/icons/hicolor/scalable/apps/openfactorystudio.svg"

tools="${LINUXDEPLOY_DIR:-}"
linuxdeploy="${tools:+$tools/}linuxdeploy-x86_64.AppImage"
mkdir -p build/dist
# The engine plugins load at run time, so deploy their dependencies too.
plugins=()
while IFS= read -r -d '' dir; do plugins+=(--deploy-deps-only "$dir"); done < <(find "$appdir/usr/lib" -type d -path '*/engines/*' -print0)
QMAKE="${QMAKE:-qmake6}" OUTPUT="build/dist/OpenFactoryStudio-$version-linux-x86_64.AppImage" \
    "$linuxdeploy" --appdir "$appdir" --executable "$appdir/usr/bin/ofs" "${plugins[@]}" \
    --desktop-file "$appdir/usr/share/applications/openfactorystudio.desktop" \
    --plugin qt --output appimage
echo "AppImage: build/dist/OpenFactoryStudio-$version-linux-x86_64.AppImage"
