#!/usr/bin/env bash
#
# Assembles a self-contained folder of a Stapik GTK application on Windows. Run it from an MSYS2 MinGW shell
# (UCRT64 or MINGW64) after the application was built there:
#
#   bundle.sh --exe <app.exe> --resources <build dir>/resources --output <folder>
#
# The folder holds the executable, every DLL it needs, the data GTK reads at run time and the application
# resources, laid out flat so that AppPaths finds <exe dir>/resources and BundledRuntime finds the rest:
#
#   <app>.exe, *.dll
#   resources/                              application resources and resources/stapik-common
#   lib/gdk-pixbuf-2.0/<ver>/loaders/       image loaders (those that are installed) and loaders.cache
#   share/glib-2.0/schemas/                 compiled GSettings schemas
#   share/icons/{Adwaita,hicolor}/          icon themes
#   etc/ssl/certs/ca-bundle.crt             CA certificates (fallback for the TLS of the cloud sync)
#   etc/fonts/                              fontconfig configuration (when the build uses it)
#   licenses/                               licence texts of the bundled MSYS2 packages
#
# The folder runs on a Windows PC without MSYS2: the script verifies that no DLL is resolved from outside it.

set -euo pipefail

# Image loaders worth shipping; each one pulls its own libraries into the bundle.
PIXBUF_LOADERS=(png svg jpeg gif bmp ico)

usage()
{
    cat <<'EOF'
Usage: bundle.sh --exe <file.exe> --resources <directory> --output <directory>

  --exe        the built executable
  --resources  directory copied to <output>/resources (the build tree's "resources", which already
               contains the stapik-common resources)
  --output     folder to create (an existing one is replaced)

Must run inside an MSYS2 MinGW shell (MSYSTEM_PREFIX has to be set).
EOF
}

fail()
{
    echo "bundle.sh: $*" >&2
    exit 1
}

to_unix_path()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
    else
        printf '%s' "$1"
    fi
}

executable=""
resources=""
output=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe) executable="${2:-}"; shift 2 ;;
        --resources) resources="${2:-}"; shift 2 ;;
        --output) output="${2:-}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; fail "unknown argument: $1" ;;
    esac
done

[[ -n "$executable" && -n "$resources" && -n "$output" ]] || { usage >&2; exit 2; }
[[ -n "${MSYSTEM_PREFIX:-}" ]] || fail "MSYSTEM_PREFIX is not set; run this inside an MSYS2 MinGW shell"

prefix="$(to_unix_path "$MSYSTEM_PREFIX")"
executable="$(to_unix_path "$executable")"
resources="$(to_unix_path "$resources")"
output="$(to_unix_path "$output")"

[[ -f "$executable" ]] || fail "executable not found: $executable"
[[ -d "$resources" ]] || fail "resources directory not found: $resources"
[[ -d "$prefix" ]] || fail "MinGW prefix not found: $prefix"

executable_name="$(basename "$executable")"

echo "== Bundling $executable_name into $output (prefix $prefix)"

rm -rf "$output"
mkdir -p "$output"

cp "$executable" "$output/"
cp -r "$resources" "$output/resources"

# ---- gdk-pixbuf image loaders ---------------------------------------------------------------------------------

pixbuf_root="$prefix/lib/gdk-pixbuf-2.0"
pixbuf_version=""
for candidate in "$pixbuf_root"/*/; do
    if [[ -d "${candidate}loaders" ]]; then
        pixbuf_version="$(basename "$candidate")"
        break
    fi
done
[[ -n "$pixbuf_version" ]] || fail "no gdk-pixbuf loaders found in $pixbuf_root"

loaders_source="$pixbuf_root/$pixbuf_version/loaders"
loaders_output="$output/lib/gdk-pixbuf-2.0/$pixbuf_version/loaders"
mkdir -p "$loaders_output"

bundled_loader_files=""
for loader in "${PIXBUF_LOADERS[@]}"; do
    file="libpixbufloader-$loader.dll"
    if [[ -f "$loaders_source/$file" ]]; then
        cp "$loaders_source/$file" "$loaders_output/"
        bundled_loader_files+=" $file"
    else
        echo "   note: image loader '$loader' is not installed, skipped"
    fi
done
# Newer librsvg no longer ships the gdk-pixbuf loader, and GTK 4 draws its own symbolic icons itself, so the SVG
# loader is only worth a note. The PNG loader is the one that has to be there.
[[ -f "$loaders_output/libpixbufloader-svg.dll" ]] || echo "   note: no SVG loader for gdk-pixbuf in this MSYS2 (librsvg dropped it); GTK 4 does not need it"
[[ -n "$bundled_loader_files" ]] || echo "   note: no image loader at all was bundled"

# loaders.cache lists the loaders; keep only the bundled ones. The paths in it are rewritten at run time by
# BundledRuntime, so they do not need to be right here.
cache_source="$pixbuf_root/$pixbuf_version/loaders.cache"
cache_output="$output/lib/gdk-pixbuf-2.0/$pixbuf_version/loaders.cache"
if [[ -f "$cache_source" ]]; then
    awk -v keep="$bundled_loader_files " '
        BEGIN { RS = ""; ORS = "\n\n" }
        /^#/ { print; next }
        {
            split($0, lines, "\n")
            count = split(lines[1], parts, /[\\\/]/)
            name = parts[count]
            gsub(/"/, "", name)
            if (index(keep, " " name " ") > 0) print
        }
    ' "$cache_source" > "$cache_output"
else
    echo "   note: $cache_source does not exist, generating loaders.cache"
    GDK_PIXBUF_MODULEDIR="$loaders_output" gdk-pixbuf-query-loaders > "$cache_output"
fi

# ---- DLLs -----------------------------------------------------------------------------------------------------

# Every DLL that ldd resolves inside the MinGW prefix; ldd lists the whole dependency tree.
collect_dependencies()
{
    ldd "$@" | awk '/=>/ { print $3 }' | grep -iF "$prefix/" | sort -u || true
}

echo "== Copying DLLs"
loader_files=()
for loader_file in "$loaders_output"/*.dll; do
    loader_files+=("$loader_file")
done

while IFS= read -r dll; do
    if [[ -n "$dll" && ! -e "$output/$(basename "$dll")" ]]; then
        cp "$dll" "$output/"
    fi
done < <(collect_dependencies "$executable" "${loader_files[@]}")

# ---- Data read by GLib, GTK and curl at run time -------------------------------------------------------------------

echo "== Copying GTK data"
schemas_output="$output/share/glib-2.0/schemas"
mkdir -p "$schemas_output"
cp "$prefix"/share/glib-2.0/schemas/*.xml "$schemas_output/"
glib-compile-schemas "$schemas_output"

mkdir -p "$output/share/icons"
for theme in Adwaita hicolor; do
    if [[ -d "$prefix/share/icons/$theme" ]]; then
        cp -r "$prefix/share/icons/$theme" "$output/share/icons/"
    else
        echo "   note: icon theme $theme is not installed, skipped"
    fi
done
rm -rf "$output/share/icons/Adwaita/cursors"

if [[ -d "$prefix/etc/fonts" ]]; then
    mkdir -p "$output/etc"
    cp -r "$prefix/etc/fonts" "$output/etc/"
fi

if [[ -f "$prefix/etc/ssl/certs/ca-bundle.crt" ]]; then
    mkdir -p "$output/etc/ssl/certs"
    cp "$prefix/etc/ssl/certs/ca-bundle.crt" "$output/etc/ssl/certs/"
else
    echo "   note: no CA bundle in $prefix/etc/ssl/certs; the Windows certificate store is the only source of trust"
fi

if [[ -d "$prefix/share/licenses" ]]; then
    cp -r "$prefix/share/licenses" "$output/licenses"
fi

# ---- Verification ---------------------------------------------------------------------------------------------

echo "== Verifying"
[[ -f "$schemas_output/gschemas.compiled" ]] || fail "GSettings schemas were not compiled"
[[ -f "$output/resources/stapik-common/locales/en.json" ]] || fail "the stapik-common resources are missing from the resources directory"
dll_count="$(find "$output" -maxdepth 1 -iname '*.dll' | wc -l)"
(( dll_count > 0 )) || fail "no DLLs were copied"

# Resolve the DLLs the way a PC without MSYS2 would: only the folder itself and the Windows directories.
restricted_path="$output:$(dirname "$(command -v ldd)")"
if [[ -n "${SYSTEMROOT:-}" ]]; then
    system_root="$(to_unix_path "$SYSTEMROOT")"
    restricted_path+=":$system_root/System32:$system_root"
fi

resolved="$(PATH="$restricted_path" ldd "$output/$executable_name" "${loader_files[@]}")" || fail "ldd failed while verifying the bundle"
[[ -n "$resolved" ]] || fail "ldd printed nothing while verifying the bundle"
unresolved="$(grep -i "not found" <<<"$resolved" || true)"
if [[ -n "$unresolved" ]]; then
    echo "$unresolved" >&2
    fail "the bundle misses DLLs (listed above)"
fi

echo "== Done: $dll_count DLLs, $(du -sh "$output" | cut -f1), $(find "$output" -type f | wc -l) files in $output"
