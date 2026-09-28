#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/runtime/original"
DST="${1:-$ROOT/runtime/work}"

if [[ -e "$DST" ]]; then
  echo "Destination already exists: $DST" >&2
  exit 1
fi

cp -a "$SRC" "$DST"
echo "Created mutable runtime copy: $DST"
echo "Original remains: $SRC"
