#!/usr/bin/env bash
#
# Signs Windows executables and verifies the signatures. Run it from an MSYS2 shell:
#
#   sign.sh <file>...
#
# The certificate comes from the environment; the first of these that is set wins:
#
#   STAPIK_SIGN_COMMAND         command that signs the file given as its last argument (Azure Trusted Signing,
#                               a hardware token tool, ...); no verification is done afterwards
#   STAPIK_SIGN_PFX             .pfx file; its password goes in STAPIK_SIGN_PFX_PASSWORD
#   STAPIK_SIGN_THUMBPRINT      SHA-1 thumbprint of a certificate in the Windows certificate store
#
# STAPIK_SIGN_TIMESTAMP_URL (default http://timestamp.digicert.com) is the RFC 3161 timestamp server and
# STAPIK_SIGNTOOL overrides the location of signtool.exe (otherwise PATH and the Windows SDK are searched).

set -euo pipefail

DEFAULT_TIMESTAMP_URL="http://timestamp.digicert.com"

fail()
{
    echo "sign.sh: $*" >&2
    exit 1
}

to_windows_path()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s' "$1"
    fi
}

find_signtool()
{
    if [[ -n "${STAPIK_SIGNTOOL:-}" ]]; then
        printf '%s' "$STAPIK_SIGNTOOL"
        return
    fi
    if command -v signtool >/dev/null 2>&1; then
        command -v signtool
        return
    fi
    local kits="/c/Program Files (x86)/Windows Kits/10/bin"
    local candidate
    candidate="$(ls -d "$kits"/*/x64/signtool.exe 2>/dev/null | sort -V | tail -n 1 || true)"
    [[ -n "$candidate" ]] || fail "signtool.exe not found; install the Windows SDK or set STAPIK_SIGNTOOL"
    printf '%s' "$candidate"
}

[[ $# -gt 0 ]] || fail "usage: sign.sh <file>..."

for file in "$@"; do
    [[ -f "$file" ]] || fail "file not found: $file"
done

if [[ -n "${STAPIK_SIGN_COMMAND:-}" ]]; then
    for file in "$@"; do
        echo "== Signing $file"
        bash -c "$STAPIK_SIGN_COMMAND"' "$1"' sign "$file"
    done
    exit 0
fi

certificate_arguments=()
if [[ -n "${STAPIK_SIGN_PFX:-}" ]]; then
    [[ -f "$STAPIK_SIGN_PFX" ]] || fail "certificate file not found: $STAPIK_SIGN_PFX"
    certificate_arguments=(/f "$(to_windows_path "$STAPIK_SIGN_PFX")")
    if [[ -n "${STAPIK_SIGN_PFX_PASSWORD:-}" ]]; then
        certificate_arguments+=(/p "$STAPIK_SIGN_PFX_PASSWORD")
    fi
elif [[ -n "${STAPIK_SIGN_THUMBPRINT:-}" ]]; then
    certificate_arguments=(/sha1 "$STAPIK_SIGN_THUMBPRINT")
else
    fail "no certificate: set STAPIK_SIGN_COMMAND, STAPIK_SIGN_PFX or STAPIK_SIGN_THUMBPRINT"
fi

signtool="$(find_signtool)"
timestamp_url="${STAPIK_SIGN_TIMESTAMP_URL:-$DEFAULT_TIMESTAMP_URL}"

# Without this MSYS2 would turn the /f, /fd, /tr options into paths.
export MSYS2_ARG_CONV_EXCL="*"

for file in "$@"; do
    echo "== Signing $file"
    windows_file="$(to_windows_path "$file")"
    "$signtool" sign /fd SHA256 /tr "$timestamp_url" /td SHA256 "${certificate_arguments[@]}" "$windows_file"
    "$signtool" verify /pa "$windows_file"
done
