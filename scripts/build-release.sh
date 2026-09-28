#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$ROOT/scripts/build-compatible-profile.sh" RELEASE

BUILD="$ROOT/build/reconstruction-release"
DIST="$ROOT/dist/release"
mkdir -p "$DIST"
cp -f "$BUILD/server_i486.so" "$DIST/server_i486.so"

# Keep dist/release byte-identical to the validated RELEASE-profile build.
# parity4 showed the previous finalization path could exit before server startup
# after changing two variables at once (CMake Release/-O3 + strip-unneeded).
# For RC closure we prefer runtime parity over a smaller file: no destructive
# stripping here. The dynamic export surface is still enforced as CreateInterface
# only, and EngineProbe is still compiled out by V34_BUILD_PROFILE=RELEASE.
elf_header="$(LC_ALL=C readelf -h "$DIST/server_i486.so")"
elf_class="$(awk -F: '/^[[:space:]]*Class:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
elf_machine="$(awk -F: '/^[[:space:]]*Machine:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
if [[ "$elf_class" != "ELF32" || "$elf_machine" != "Intel 80386" ]]; then
  file_desc="$(LC_ALL=C file -b "$DIST/server_i486.so" 2>/dev/null || true)"
  echo "RELEASE_ARTIFACT=FAIL reason=release_artifact_elf_class class=${elf_class:-unknown} machine=${elf_machine:-unknown} file=${file_desc:-unknown}" >&2
  exit 1
fi
exports="$({ readelf -Ws "$DIST/server_i486.so" || true; } | awk '$5 == "GLOBAL" && $6 == "DEFAULT" && $7 != "UND" { print $8 }' | sed '/^$/d' | sort -u)"
[[ "$exports" == "CreateInterface" ]] || {
  echo "RELEASE_ARTIFACT=FAIL reason=export_surface" >&2
  printf '%s\n' "$exports" >&2
  exit 1
}
dynamic_tags="$(LC_ALL=C readelf -d "$DIST/server_i486.so" 2>/dev/null || true)"
if [[ "$dynamic_tags" == *TEXTREL* ]]; then
  echo "RELEASE_ARTIFACT=FAIL reason=textrel" >&2
  exit 1
fi
sha256sum "$DIST/server_i486.so" > "$DIST/server_i486.so.sha256"
cat > "$DIST/BUILD.txt" <<META
revision=$(tr -d '\r\n' < "$ROOT/scripts/reconstruction-revision.txt")
profile=RELEASE
cmake_build_type=RelWithDebInfo
engine_probe=compiled_out
export_surface=CreateInterface_only
textrel=none
stripping=none_runtime_parity_first
builder=debian12_compat_container
META
echo "RELEASE_ARTIFACT=PASS path=$DIST/server_i486.so sha256=$(awk '{print $1}' "$DIST/server_i486.so.sha256")"
