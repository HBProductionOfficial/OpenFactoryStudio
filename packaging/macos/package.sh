#!/usr/bin/env bash
# Builds the macOS DMG: package.sh [--skip-build] [preset]. Needs macdeployqt (or MACDEPLOYQT).
set -euo pipefail
skip_build=0
if [ "${1:-}" = "--skip-build" ]; then skip_build=1; shift; fi
preset="${1:-macos-release}"
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"

if [ "$skip_build" = 0 ]; then
    cmake --preset "$preset"
    cmake --build --preset "$preset"
fi
build="build/$preset"
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")"

stage="$root/build/macstage"
rm -rf "$stage"
cmake --install "$build" --prefix "$stage"
app="$stage/ofs.app"
[ -d "$app" ] || { echo "no app bundle installed" >&2; exit 1; }

deploy="${MACDEPLOYQT:-macdeployqt}"
# The engine plugins in Contents/PlugIns/engines load at run time, so pass them in too.
args=()
while IFS= read -r -d '' lib; do args+=("-executable=$lib"); done < <(find "$app/Contents/PlugIns" \( -name '*.so' -o -name '*.dylib' \) -print0)
"$deploy" "$app" ${args[@]+"${args[@]}"} -always-overwrite

mkdir -p build/dist
dmg="build/dist/OpenFactoryStudio-$version-macos.dmg"
rm -f "$dmg"
hdiutil create -volname "Open Factory Studio" -srcfolder "$app" -ov -format UDZO "$dmg"
echo "DMG: $dmg"
