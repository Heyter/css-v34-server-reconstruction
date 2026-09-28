#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
MODE="dry-run"
if [[ "${1:-}" == "--apply" ]]; then
  MODE="apply"
  shift
elif [[ "${1:-}" == "--dry-run" || -z "${1:-}" ]]; then
  [[ "${1:-}" == "--dry-run" ]] && shift || true
else
  echo "usage: $0 [--dry-run|--apply]" >&2
  exit 2
fi
[[ $# -eq 0 ]] || { echo "usage: $0 [--dry-run|--apply]" >&2; exit 2; }

[[ "$ROOT" != "/" && "$ROOT" != "$HOME" ]] || {
  echo "CLEANUP_PROJECT=FAIL reason=unsafe_root root=$ROOT" >&2
  exit 1
}
[[ -f "$ROOT/CMakeLists.txt" && -f "$ROOT/scripts/reconstruction-revision.txt" && -d "$ROOT/src/reconstructed" ]] || {
  echo "CLEANUP_PROJECT=FAIL reason=project_markers_missing root=$ROOT" >&2
  exit 1
}

revision="$(tr -d '\r\n' < "$ROOT/scripts/reconstruction-revision.txt")"
[[ "$revision" == "v1.8.35g" ]] || {
  echo "CLEANUP_PROJECT=FAIL reason=unexpected_revision revision=$revision expected=v1.8.35g" >&2
  exit 1
}

# These are obsolete reconstruction/research generations. Current RELEASE and
# DEBUG build/test paths do not depend on them.
historical_paths=(
  "SPEC.md"
  "CLEANUP-INSTRUCTIONS.txt"
  "SNAPSHOT-MANIFEST.sha256"
  "SNAPSHOT_MANIFEST.txt"
  "config/reconstruction.env"
  "config/release-rc1.env"
  "config/target.env"
  "docs/ABI_MAP_BUILD4100.md"
  "docs/ACCEPTANCE_HISTORY.md"
  "docs/AI_HANDOFF.md"
  "docs/ARCHITECTURE.md"
  "docs/PROBE_HISTORY.md"
  "docs/PROJECT_STATUS_CURRENT.md"
  "docs/RECONSTRUCTION.md"
  "docs/REFERENCE_SOURCES.md"
  "docs/RELEASE_CLOSURE.md"
  "docs/RESEARCH_STATE_0.0.3.199.md"
  "docs/RESEARCH_STATE_0.0.3.310.md"
  "docs/RESEARCH_STATE_0.0.3.331.md"
  "docs/RESEARCH_STATE_CURRENT.md"
  "docs/RESEARCH_STATE_V51.10.1.md"
  "docs/RESEARCH_STATE_V51.31.1.md"
  "docs/ROADMAP.md"
  "docs/TESTING.md"
  "docs/analys"
  "patches"
  "scripts/apply-autoprobe-v49-next.sh"
  "scripts/audit-baseline.sh"
  "scripts/auto-dllinit-progress.sh"
  "scripts/bootstrap-sources.sh"
  "scripts/build-dllinit-progress.sh"
  "scripts/build-tools.sh"
  "scripts/lock-sources.sh"
  "scripts/run-dllinit-progress.sh"
  "scripts/test-autoprobe-v50.sh"
  "scripts/test-handoff-sync-v0.1.3.sh"
  "scripts/test-hybrid-loader.sh"
  "scripts/test-reconstructed-abi-v0.2.0.sh"
  "scripts/test-reconstructed-abi-v0.2.1.sh"
  "scripts/test-reconstructed-abi-v0.2.2.sh"
  "scripts/test-reconstructed-abi-v0.2.3.sh"
  "scripts/test-reconstructed-source.sh"
  "scripts/update-snapshot-manifest.sh"
  "scripts/verify-main-source.sh"
  "scripts/verify-snapshot-manifest.sh"
  "tools/auto-boundary"
  "tools/autoprobe-v43"
  "tools/autoprobe-v44"
  "tools/autoprobe-v45"
  "tools/autoprobe-v46"
  "tools/autoprobe-v47"
  "tools/autoprobe-v48"
  "tools/autoprobe-v49"
  "tools/dllinit-progress"
  "tools/dllinit-trace"
  "tools/hybrid-loader"
  "tools/interface_probe"
  "tools/runtime-file-index"
  "tools/vtable-probe"
  "src/compat/protocol7/README.md"
  "src/engine/README.md"
  "src/game/cstrike/README.md"
  "src/reconstructed/README.md"
  "src/vphysics/README.md"
  "build"
  "out"
  "reports"
  "runtime/work"
)

entries=()
add_if_exists() {
  local rel="$1"
  if [[ -e "$ROOT/$rel" || -L "$ROOT/$rel" ]]; then
    entries+=("$rel")
  fi
  return 0
}

for rel in "${historical_paths[@]}"; do
  add_if_exists "$rel"
done

# Versioned root test notes were useful during reconstruction but are replaced
# by docs/DEBUG_TESTING.md and docs/TECHNICAL_REFERENCE.md.
while IFS= read -r -d '' path; do
  rel="${path#"$ROOT/"}"
  entries+=("$rel")
done < <(find "$ROOT" -maxdepth 1 -type f \( -name 'TEST-v*.md' -o -name 'TEST-v*.txt' \) -print0)

# Generated logs/caches. Never traverse immutable original runtime, external
# source clones, Git metadata or dist artifacts.
while IFS= read -r -d '' path; do
  rel="${path#"$ROOT/"}"
  entries+=("$rel")
done < <(find "$ROOT" \
  -path "$ROOT/.git" -prune -o \
  -path "$ROOT/runtime/original" -prune -o \
  -path "$ROOT/sources" -prune -o \
  -path "$ROOT/dist" -prune -o \
  \( -type d -name '__pycache__' -o -type f \( -name '*.pyc' -o -name '*.pyo' -o -name '*.log' -o -name 'core' -o -name 'core.*' -o -name '*.core' \) \) \
  -print0)

# Deduplicate and collapse children when their parent directory is already
# scheduled for deletion. This keeps dry-run output readable.
mapfile -t entries < <(printf '%s\n' "${entries[@]:-}" | sed '/^$/d' | LC_ALL=C sort -u)
compact_entries=()
for rel in "${entries[@]}"; do
  nested=0
  for parent in "${compact_entries[@]:-}"; do
    if [[ -n "$parent" && "$rel" == "$parent/"* ]]; then
      nested=1
      break
    fi
  done
  [[ $nested -eq 0 ]] && compact_entries+=("$rel")
done
entries=("${compact_entries[@]}")

echo "CLEANUP_PROJECT_BEGIN mode=$MODE revision=$revision root=$ROOT"
for rel in "${entries[@]}"; do
  printf '%s\n' "REMOVE $rel"
done

if [[ "$MODE" == "apply" ]]; then
  for rel in "${entries[@]}"; do
    rm -rf -- "$ROOT/$rel"
  done
  # Remove empty directories left by obsolete documentation/tool generations.
  find "$ROOT/docs" "$ROOT/tools" "$ROOT/scripts" -depth -type d -empty -delete 2>/dev/null || true
fi

# Safety invariants: these must always survive cleanup.
protected=(
  "README.md"
  "THIRD_PARTY.md"
  "config/project.env"
  "docs/DEBUG_TESTING.md"
  "docs/TECHNICAL_REFERENCE.md"
  "scripts/build-debug.sh"
  "scripts/build-release.sh"
  "scripts/test-current.sh"
  "scripts/test-real-engine-docker.sh"
  "scripts/protocol7-client-probe.py"
  "scripts/compare-original-release.sh"
  "scripts/package-rc1.sh"
  "scripts/audit-public.py"
  "tools/abi-inventory/abi_inventory.py"
  "tools/entity-factory-oracle.py"
  "tools/sendtable-oracle/sendtable_oracle.py"
  "src/reconstructed/server/server_game_dll.cpp"
)
if [[ "$MODE" == "apply" ]]; then
  for rel in "${protected[@]}"; do
    [[ -f "$ROOT/$rel" ]] || {
      echo "CLEANUP_PROJECT=FAIL reason=protected_file_missing file=$rel" >&2
      exit 1
    }
  done
fi

echo "CLEANUP_PROJECT=PASS mode=$MODE remove_entries=${#entries[@]} protected_original_runtime=1 protected_test_maps=1 protected_sources=1 protected_dist=1"
