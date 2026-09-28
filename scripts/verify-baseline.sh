#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
R="$ROOT/runtime/original"
check_hash() {
  local expected="$1" path="$2" actual
  actual="$(sha256sum "$R/$path" | awk '{print $1}')"
  [[ "$actual" == "$expected" ]] || { echo "[FAIL] $path expected=$expected actual=$actual" >&2; exit 1; }
  echo "[ok] $path $actual"
}
check_hash fa5df895aaa0b852b28fc86e62e97dc69d6594bc0100c32353b6a48c1a246f47 srcds_i486
check_hash 3925345a18f308c4486de9ea1f550fdadd79ae93cd9e1e8a0e5f0e0b8bd7519c bin/engine_i486.so
check_hash 1fde488de2e4227f446534327c35de3c180515bab2f7c03b4ab676f8ee0664a9 cstrike/bin/server_i486.so
check_hash 1f901ca24b9cabea0afbcf8f7ef93294d855d986394e769aa9cb04e5c861ccef bin/vphysics_i486.so
grep -q '^PatchVersion=1.0.0.34' "$R/cstrike/steam.inf"
grep -q '^appID=240' "$R/cstrike/steam.inf"
for f in srcds_i486 bin/engine_i486.so cstrike/bin/server_i486.so bin/vphysics_i486.so; do
  file "$R/$f" | grep -q 'ELF 32-bit'
done
echo "Baseline build4100 core verified."
