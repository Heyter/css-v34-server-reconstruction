#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-}"
case "$PROFILE" in
  DEBUG|debug)
    PROFILE=DEBUG
    BUILD_TYPE=RelWithDebInfo
    BUILD="$ROOT/build/reconstruction-debug"
    ;;
  RELEASE|release)
    PROFILE=RELEASE
    # Final RC uses the same RelWithDebInfo code-generation family that passed
    # the complete real-engine Protocol7 gate. V34_BUILD_PROFILE=RELEASE still
    # compiles EngineProbe out, so diagnostics stay DEBUG-only. Avoid switching
    # the validated runtime to CMake Release/-O3 at the final parity step.
    BUILD_TYPE=RelWithDebInfo
    BUILD="$ROOT/build/reconstruction-release"
    ;;
  *)
    echo "usage: $0 DEBUG|RELEASE" >&2
    exit 2
    ;;
esac

"$ROOT/scripts/check-toolchain.sh"

cmake \
  -S "$ROOT" \
  -B "$BUILD" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_C_FLAGS=-m32 \
  -DCMAKE_CXX_FLAGS=-m32 \
  -DCMAKE_SHARED_LINKER_FLAGS=-m32 \
  -DV34_BUILD_PROFILE="$PROFILE" \
  -DV34_FORCE_32BIT=ON \
  -DV34_BUILD_RECONSTRUCTION_CORE=ON

cmake --build "$BUILD" --target server-reconstructed-i486

LIB="$BUILD/server_i486.so"
[[ -f "$LIB" ]] || { echo "BUILD_PROFILE=FAIL profile=$PROFILE missing=$LIB" >&2; exit 1; }

# Avoid grep -q pipelines under `set -o pipefail`: a successful early grep
# match can SIGPIPE the producer and turn a valid check into rc=141.
elf_header="$(LC_ALL=C readelf -h "$LIB")"
elf_class="$(awk -F: '/^[[:space:]]*Class:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
elf_machine="$(awk -F: '/^[[:space:]]*Machine:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
if [[ "$elf_class" != "ELF32" || "$elf_machine" != "Intel 80386" ]]; then
  file_desc="$(LC_ALL=C file -b "$LIB" 2>/dev/null || true)"
  echo "BUILD_PROFILE=FAIL profile=$PROFILE reason=not_elf32_i386 class=${elf_class:-unknown} machine=${elf_machine:-unknown} file=${file_desc:-unknown}" >&2
  exit 1
fi

exports="$({ readelf -Ws "$LIB" || true; } | awk '$5 == "GLOBAL" && $6 == "DEFAULT" && $7 != "UND" { print $8 }' | sed '/^$/d' | sort -u)"
[[ "$exports" == "CreateInterface" ]] || {
  echo "BUILD_PROFILE=FAIL profile=$PROFILE reason=export_surface" >&2
  printf '%s\n' "$exports" >&2
  exit 1
}

dynamic_tags="$(LC_ALL=C readelf -d "$LIB" 2>/dev/null || true)"
if [[ "$dynamic_tags" == *TEXTREL* ]]; then
  echo "BUILD_PROFILE=FAIL profile=$PROFILE reason=textrel" >&2
  exit 1
fi

probe_symbol="absent"
nm_symbols="$(LC_ALL=C nm -C "$LIB" 2>/dev/null || true)"
if [[ "$nm_symbols" == *"cssv34::reconstructed::abi::EngineProbe"* ]]; then
  probe_symbol="present"
fi
if [[ "$PROFILE" == RELEASE && "$probe_symbol" != absent ]]; then
  echo "BUILD_PROFILE=FAIL profile=RELEASE reason=engine_probe_not_compiled_out" >&2
  exit 1
fi
if [[ "$PROFILE" == DEBUG && "$probe_symbol" != present ]]; then
  echo "BUILD_PROFILE=FAIL profile=DEBUG reason=engine_probe_missing" >&2
  exit 1
fi

sha="$(sha256sum "$LIB" | awk '{print $1}')"
echo "BUILD_PROFILE=PASS profile=$PROFILE build_type=$BUILD_TYPE probe=$probe_symbol elf32=1 exports=CreateInterface textrel=none sha256=$sha"
echo "OUTPUT=$LIB"
