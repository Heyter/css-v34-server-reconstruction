#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CFG="$ROOT/config/project.env"
[[ -f "$CFG" ]] || { echo "RC1_PACKAGE=FAIL reason=missing_config path=$CFG" >&2; exit 1; }
# shellcheck disable=SC1090
source "$CFG"

INPUT="$ROOT/dist/release/server_i486.so"
OUT="$ROOT/dist/rc1"
PKG_NAME="css-v34-${V34_RECONSTRUCTION_REVISION}-${V34_RC_LABEL,,}"
STAGE="$OUT/$PKG_NAME"
ARCHIVE="$OUT/$PKG_NAME.tar.gz"
ARCHIVE_SHA="$ARCHIVE.sha256"

fail() {
  echo "RC1_PACKAGE=FAIL $*" >&2
  exit 1
}

[[ -f "$INPUT" ]] || fail "reason=missing_release_binary path=$INPUT"
actual_sha="$(sha256sum "$INPUT" | awk '{print $1}')"
[[ "$actual_sha" == "$V34_RC_EXPECTED_SHA256" ]] || \
  fail "reason=sha256_mismatch expected=$V34_RC_EXPECTED_SHA256 actual=$actual_sha"

elf_header="$(LC_ALL=C readelf -h "$INPUT")"
elf_class="$(awk -F: '/^[[:space:]]*Class:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
elf_machine="$(awk -F: '/^[[:space:]]*Machine:/{v=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", v); print v}' <<<"$elf_header")"
[[ "$elf_class" == ELF32 && "$elf_machine" == "Intel 80386" ]] || \
  fail "reason=wrong_elf class=${elf_class:-unknown} machine=${elf_machine:-unknown}"

exports="$({ readelf -Ws "$INPUT" || true; } | awk '$5 == "GLOBAL" && $6 == "DEFAULT" && $7 != "UND" { print $8 }' | sed '/^$/d' | sort -u)"
[[ "$exports" == "CreateInterface" ]] || fail "reason=export_surface exports=$(tr '\n' ',' <<<"$exports")"

dynamic_tags="$(LC_ALL=C readelf -d "$INPUT" 2>/dev/null || true)"
[[ "$dynamic_tags" != *TEXTREL* ]] || fail "reason=textrel"

nm_symbols="$(LC_ALL=C nm -C "$INPUT" 2>/dev/null || true)"
[[ "$nm_symbols" != *"cssv34::reconstructed::abi::EngineProbe"* ]] || fail "reason=debug_probe_present"

rm -rf "$STAGE"
mkdir -p "$STAGE/cstrike/bin"
cp -f "$INPUT" "$STAGE/cstrike/bin/server_i486.so"

cat > "$STAGE/RC1.txt" <<META
product=Counter-Strike: Source v34 server rebuild
revision=$V34_RECONSTRUCTION_REVISION
candidate=$V34_RC_LABEL
target_srcds_build=$CSS_BUILD
protocol=$CSS_PROTOCOL
arch=$CSS_ARCH
profile=$V34_RC_BUILD_PROFILE
cmake_build_type=$V34_RC_BUILD_TYPE
engine_probe=compiled_out
export_surface=CreateInterface_only
textrel=none
sha256=$actual_sha
original_vs_rebuilt_parity=$V34_RC_PARITY_RESULT
parity_map=$V34_RC_PARITY_MAP
builder=$V34_RC_TOOLCHAIN
long_soak=deferred_debug_profile_preserved
broad_regression=deferred_debug_profile_preserved
META

cat > "$STAGE/INSTALL.txt" <<'TXT'
RC1 contains the reconstructed game DLL only.

Install target:
  cstrike/bin/server_i486.so

Use it with the existing Counter-Strike: Source v34 / SRCDS build 4100 runtime.
Keep a backup of the previous server_i486.so before replacing it.
TXT

(
  cd "$STAGE"
  sha256sum cstrike/bin/server_i486.so RC1.txt INSTALL.txt > SHA256SUMS
)

rm -f "$ARCHIVE" "$ARCHIVE_SHA"
tar -C "$OUT" -czf "$ARCHIVE" "$PKG_NAME"
sha256sum "$ARCHIVE" > "$ARCHIVE_SHA"

echo "RC1_BINARY=PASS revision=$V34_RECONSTRUCTION_REVISION candidate=$V34_RC_LABEL sha256=$actual_sha"
echo "RC1_PACKAGE=PASS archive=$ARCHIVE archive_sha256=$(awk '{print $1}' "$ARCHIVE_SHA")"
echo "RC1_INSTALL_PATH=cstrike/bin/server_i486.so"
