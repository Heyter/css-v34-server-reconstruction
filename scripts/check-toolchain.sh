#!/usr/bin/env bash
set -euo pipefail

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/t.c" <<'SRC'
int main(void) { return 0; }
SRC

if ! cc -m32 "$TMP/t.c" -o "$TMP/t" >/dev/null 2>&1; then
  cat >&2 <<'MSG'
32-bit i386 compiler/linker runtime is not available on this host.
Use the provided dev container:

  docker compose -f docker/compose.dev.yml run --rm dev

or install gcc-multilib/g++-multilib + i386 libc development packages.
MSG
  exit 1
fi

file "$TMP/t" | grep -q 'ELF 32-bit'
echo "[ok] working ELF32 i386 compiler/linker toolchain"
