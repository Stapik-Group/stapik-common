#!/usr/bin/env bash
#
# Builds an AppImage of a Stapik GTK application:
#
#   build-appimage.sh --source <application source directory> --output <directory> [--build <directory>]
#                     [--cmake-arg <argument>]...
#
# The application is configured with the prefix /usr, installed into an AppDir and packed with linuxdeploy and its
# GTK plugin, which have to be on PATH (linuxdeploy, linuxdeploy-plugin-gtk.sh, linuxdeploy-plugin-appimage).
# Build on the oldest distribution the AppImage should run on: the libraries of the build machine are bundled,
# but glibc is not.

set -euo pipefail

usage()
{
    cat <<'EOT'
Usage: build-appimage.sh --source <directory> --output <directory> [--build <directory>] [--cmake-arg <argument>]...

  --source     the application's source directory (the one with the CMakeLists.txt calling stapik_add_application)
  --output     directory the .AppImage is written to
  --build      build directory (default: <output>/build)
  --cmake-arg  extra argument for the CMake configuration; may be repeated
EOT
}

fail()
{
    echo "build-appimage.sh: $*" >&2
    exit 1
}

source_directory=""
output_directory=""
build_directory=""
cmake_arguments=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) source_directory="${2:-}"; shift 2 ;;
        --output) output_directory="${2:-}"; shift 2 ;;
        --build) build_directory="${2:-}"; shift 2 ;;
        --cmake-arg) cmake_arguments+=("${2:-}"); shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; fail "unknown argument: $1" ;;
    esac
done

[[ -n "$source_directory" && -n "$output_directory" ]] || { usage >&2; exit 2; }
[[ -f "$source_directory/CMakeLists.txt" ]] || fail "no CMakeLists.txt in $source_directory"
for tool in cmake linuxdeploy linuxdeploy-plugin-gtk.sh; do
    command -v "$tool" >/dev/null 2>&1 || fail "$tool not found on PATH"
done

mkdir -p "$output_directory"
output_directory="$(cd "$output_directory" && pwd)"
build_directory="${build_directory:-$output_directory/build}"
app_directory="$output_directory/AppDir"

echo "== Building (prefix /usr)"
cmake -S "$source_directory" -B "$build_directory" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DSTAPIK_APP_SYSTEM_PREFIX=/usr ${cmake_arguments[@]+"${cmake_arguments[@]}"}
cmake --build "$build_directory"

echo "== Installing into $app_directory"
rm -rf "$app_directory"
DESTDIR="$app_directory" cmake --install "$build_directory"

desktop_files=("$app_directory"/usr/share/applications/*.desktop)
[[ ${#desktop_files[@]} -eq 1 && -f "${desktop_files[0]}" ]] || fail "expected exactly one desktop file in $app_directory/usr/share/applications"
desktop_file="${desktop_files[0]}"

icon_name="$(sed -n 's/^Icon=//p' "$desktop_file")"
icon_file="$(find "$app_directory/usr/share/icons" -name "$icon_name.*" 2>/dev/null | head -n 1)"
[[ -n "$icon_file" ]] || fail "no icon named '$icon_name' installed; add <name>.svg or <name>.png to the packaging directory"

version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_directory/CMakeCache.txt")"

echo "== Packing the AppImage"
cd "$output_directory"
DEPLOY_GTK_VERSION=4 VERSION="${version:-unknown}" linuxdeploy \
    --appdir "$app_directory" \
    --plugin gtk \
    --desktop-file "$desktop_file" \
    --icon-file "$icon_file" \
    --output appimage

echo "== Done: $(ls -1 "$output_directory"/*.AppImage)"
