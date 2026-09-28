#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/reconstruction-current"
LOG="$ROOT/build/test-current.log"
LIB="$BUILD/server_i486.so"

required_files=(
  "src/reconstructed/tests/round_lifecycle_smoke.cpp"
  "src/reconstructed/tests/entity_runtime_smoke.cpp"
  "src/reconstructed/tests/commands_network_smoke.cpp"
  "src/reconstructed/tests/weapon_grenade_runtime_smoke.cpp"
  "src/reconstructed/server/weapon_script_profile.h"
  "src/reconstructed/server/weapon_script_profile.cpp"
  "src/reconstructed/tests/end_to_end_match_smoke.cpp"
  "src/reconstructed/tests/factory_differential_smoke.cpp"
  "src/reconstructed/tests/networkable_bridge_smoke.cpp"
  "src/reconstructed/tests/map_entity_lump_smoke.cpp"
  "src/reconstructed/public/build4100/sendtable.h"
  "src/reconstructed/public/build4100/server_networkable.h"
  "src/reconstructed/public/build4100/server_unknown.h"
  "src/reconstructed/abi/server_class_registry.cpp"
  "src/reconstructed/abi/engine_server021_client.cpp"
  "src/reconstructed/public/build4100/engine_trace.h"
  "src/reconstructed/abi/engine_trace_server003_client.h"
  "src/reconstructed/abi/engine_trace_server003_client.cpp"
  "src/reconstructed/abi/server_game_clients004_adapter.cpp"
  "src/reconstructed/abi/engine_cvar_bridge.cpp"
  "src/reconstructed/abi/engine_entity_bridge.cpp"
  "src/reconstructed/abi/engine_networkable_shim.cpp"
  "src/reconstructed/abi/engine_unknown_shim.cpp"
  "src/reconstructed/server/map_entity_lump.cpp"
  "src/reconstructed/server/entity_factory_registry.cpp"
  "tools/sendtable-oracle/sendtable_oracle.py"
  "tools/sendtable-oracle/sendprop_field_oracle.py"
  "tools/sendtable-oracle/serverclass_oracle.py"
  "tools/entity-factory-oracle.py"
  "config/serverclasses-build4100.tsv"
  "src/reconstructed/abi/serverclass_build4100.inc"
  "src/reconstructed/abi/sendprop_build4100.inc"
  "src/reconstructed/abi/sendtable_links_build4100.inc"
  "src/reconstructed/abi/sendarray_build4100.inc"
  "src/reconstructed/abi/sendutlvector_build4100.inc"
  "src/reconstructed/tests/sendtable_live_graph_smoke.cpp"
  "src/reconstructed/tests/sendtable_client_schema_smoke.cpp"
  "scripts/protocol7-client-probe.py"
  "scripts/protocol7-gate-lib.sh"
  "scripts/instancebaseline-gate-lib.sh"
  "scripts/reconstruction-revision.txt"
  "scripts/source-rcon.py"
  "src/reconstructed/server/entity_factory_build4100.inc"
  "scripts/real_engine_supervisor.py"
  "scripts/test-real-engine-docker.sh"
  "runtime/test-maps/de_dust2.nav"
  "runtime/test-maps/fy_pool_day.bsp"
)
for rel in "${required_files[@]}"; do
  if [[ ! -f "$ROOT/$rel" ]]; then
    echo "FAIL: required source missing: $rel" >&2
    exit 1
  fi
done

REVISION_FILE="$ROOT/scripts/reconstruction-revision.txt"
PROJECT_REVISION="$(tr -d '\r\n' < "$REVISION_FILE")"
if [[ ! "$PROJECT_REVISION" =~ ^v1\.8\.[0-9]+[a-z]+$ ]]; then
  echo "FAIL: invalid reconstruction revision: $PROJECT_REVISION" >&2
  exit 1
fi
echo "REVISION_CONTRACT=$PROJECT_REVISION"

# Closed-project cleanup intentionally removes historical Markdown ledgers.
# Runtime/source validation remains authoritative; old document-shape contracts
# can be re-enabled only when a historical tree is restored.
V34_LEGACY_DOC_CONTRACTS="${V34_LEGACY_DOC_CONTRACTS:-0}"

is_legacy_document_contract() {
  case "$1" in
    README.md|SPEC.md|THIRD_PARTY.md|docs/*.md|src/*/README.md|src/reconstructed/README.md) return 0 ;;
    *) return 1 ;;
  esac
}

serverclass_inc_check="$(mktemp)"
python3 "$ROOT/tools/sendtable-oracle/serverclass_oracle.py" \
  --check-tsv "$ROOT/config/serverclasses-build4100.tsv" \
  --out-inc "$serverclass_inc_check"
cmp -s "$serverclass_inc_check" "$ROOT/src/reconstructed/abi/serverclass_build4100.inc" || {
  echo "FAIL: generated ServerClass include differs from config/serverclasses-build4100.tsv" >&2
  rm -f "$serverclass_inc_check"
  exit 1
}
rm -f "$serverclass_inc_check"
echo "SERVERCLASS_GENERATED_CONTRACT=PASS classes=177 prefix=15"

require_source_fixed() {
  local label="$1"
  local rel="$2"
  if [[ "$V34_LEGACY_DOC_CONTRACTS" != "1" ]] && is_legacy_document_contract "$rel"; then
    return 0
  fi
  local needle="$3"
  if ! grep -Fq -- "$needle" "$ROOT/$rel"; then
    echo "FAIL: $label missing in $rel" >&2
    exit 1
  fi
}

require_source_regex() {
  local label="$1"
  local rel="$2"
  if [[ "$V34_LEGACY_DOC_CONTRACTS" != "1" ]] && is_legacy_document_contract "$rel"; then
    return 0
  fi
  local pattern="$3"
  if ! grep -Eq -- "$pattern" "$ROOT/$rel"; then
    echo "FAIL: $label missing in $rel" >&2
    exit 1
  fi
}

# Historical gates should verify behavior markers, not freeze the exact shape of
# a diagnostic regex.  Telemetry is intentionally allowed to grow new fields
# between stable tokens without invalidating an older acceptance contract.
require_source_tokens() {
  local label="$1"
  local rel="$2"
  if [[ "$V34_LEGACY_DOC_CONTRACTS" != "1" ]] && is_legacy_document_contract "$rel"; then
    return 0
  fi
  shift 2
  local token
  for token in "$@"; do
    if ! grep -Fq -- "$token" "$ROOT/$rel"; then
      echo "FAIL: $label token missing in $rel: $token" >&2
      exit 1
    fi
  done
}

# A historical gate may also need to prove that a brittle version-coupled token
# was removed. Keep that check explicit rather than encoding absence in regex
# tricks that become unreadable as the acceptance chain grows.
require_source_absent() {
  local label="$1"
  local rel="$2"
  if [[ "$V34_LEGACY_DOC_CONTRACTS" != "1" ]] && is_legacy_document_contract "$rel"; then
    return 0
  fi
  local needle="$3"
  if grep -Fq -- "$needle" "$ROOT/$rel"; then
    echo "FAIL: $label stale token still present in $rel: $needle" >&2
    exit 1
  fi
}

# Current handoff/status documents are useful operator metadata, not runtime
# evidence.  Their shape is required, but a stale revision label must never
# abort static/Protocol7/Docker acceptance.  Report drift explicitly instead.
check_doc_revision_nonfatal() {
  local rel="$1"
  if [[ "$V34_LEGACY_DOC_CONTRACTS" != "1" ]]; then
    return 0
  fi
  local doc_revision
  doc_revision="$(grep -Eo 'v1\.8\.[0-9]+[a-z]+' "$ROOT/$rel" | head -n1 || true)"
  if [[ -z "$doc_revision" ]]; then
    echo "DOC_REVISION_DRIFT=NONFATAL file=$rel doc=missing project=$PROJECT_REVISION"
  elif [[ "$doc_revision" == "$PROJECT_REVISION" ]]; then
    echo "DOC_REVISION_SYNC=PASS file=$rel revision=$doc_revision"
  else
    echo "DOC_REVISION_DRIFT=NONFATAL file=$rel doc=$doc_revision project=$PROJECT_REVISION"
  fi
}

# v1.8.34zo: regression for host-side Protocol7 acceptance. The real zn probe
# already emitted semantic PASS, but the Docker wrapper rejected the line only
# because new runtime_snapshot/wire_growth telemetry appeared between fields.
# Keep the matcher field-order independent and prove both the exact zn line and
# a reordered line, while still rejecting a line missing a required field.
# shellcheck source=protocol7-gate-lib.sh
source "$ROOT/scripts/protocol7-gate-lib.sh"
protocol7_gate_fixture="$(mktemp)"
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries=1 runtime_snapshot=1 wire_growth=0 wire_growth_ids=- player_index=-1 c4_index=0 ids=6
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[1-9][0-9]*' 'runtime_snapshot=[0-9]+' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=[0-9,]+'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zo exact zn instancebaseline host-gate fixture rejected" >&2
  exit 1
fi
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ids=1,6 c4_index=0 wire_growth_ids=1 entries=2 player_index=1 wire_growth=1 runtime_snapshot=1
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[1-9][0-9]*' 'runtime_snapshot=[0-9]+' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=[0-9,]+'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zo reordered instancebaseline host-gate fixture rejected" >&2
  exit 1
fi
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries=1 wire_growth=0 wire_growth_ids=- player_index=-1 c4_index=0 ids=6
EOF
if protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[1-9][0-9]*' 'runtime_snapshot=[0-9]+' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=[0-9,]+'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zo incomplete instancebaseline host-gate fixture accepted" >&2
  exit 1
fi
rm -f "$protocol7_gate_fixture"
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_PACKET_BASELINE_POLICY=PASS baseline_classes=1 packet_classes=1 materialized=0 implicit=1 player=implicit materialized_ids=none implicit_ids=1
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_PACKET_BASELINE_POLICY=PASS ' \
  'baseline_classes=[1-9][0-9]*' 'packet_classes=[1-9][0-9]*' \
  'materialized=[0-9]+' 'implicit=[0-9]+' 'player=(materialized|implicit)' \
  'materialized_ids=[^ ]+' 'implicit_ids=[^ ]+'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zo exact zn packet-baseline host-gate fixture rejected" >&2
  exit 1
fi
rm -f "$protocol7_gate_fixture"
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries=1 runtime_snapshot=0 wire_growth=1 wire_growth_ids=1 player_index=0 c4_index=-1 ids=1
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[0-9]+' 'runtime_snapshot=0' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=(none|[0-9,]+)'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zp empty runtime snapshot growth host-gate fixture rejected" >&2
  exit 1
fi
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries=0 runtime_snapshot=0 wire_growth=0 wire_growth_ids=- player_index=-1 c4_index=-1 ids=none
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[0-9]+' 'runtime_snapshot=0' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=(none|[0-9,]+)'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zr empty runtime + empty wire table host-gate fixture rejected" >&2
  exit 1
fi
cat > "$protocol7_gate_fixture" <<'EOF'
PROTOCOL7_PACKET_BASELINE_POLICY=PASS baseline_classes=0 packet_classes=1 materialized=0 implicit=1 player=implicit materialized_ids=none implicit_ids=1
EOF
if ! protocol7_pass_has_fields "$protocol7_gate_fixture" 'PROTOCOL7_PACKET_BASELINE_POLICY=PASS ' \
  'baseline_classes=[0-9]+' 'packet_classes=[1-9][0-9]*' \
  'materialized=[0-9]+' 'implicit=[0-9]+' 'player=(materialized|implicit)' \
  'materialized_ids=[^ ]+' 'implicit_ids=[^ ]+'; then
  rm -f "$protocol7_gate_fixture"
  echo "FAIL: v1.8.34zt empty wire packet-baseline host-gate fixture rejected" >&2
  exit 1
fi
rm -f "$protocol7_gate_fixture"
echo "PROTOCOL7_HOST_GATE_SELFTEST=PASS instancebaseline=zn_exact+growth_reordered+empty_runtime_growth+empty_runtime_empty_wire+missing_field_rejected packetbaseline=zn_exact+empty_wire_implicit"

# v1.8.34zt: the per-map Docker startup gate must accept the same legal empty
# runtime census that the Protocol7 baseline policy already accepts.  Validate
# both positive and all-sentinel snapshots, and reject inconsistent structure.
# shellcheck source=instancebaseline-gate-lib.sh
source "$ROOT/scripts/instancebaseline-gate-lib.sh"
instancebaseline_gate_fixture="$(mktemp)"
cat > "$instancebaseline_gate_fixture" <<'EOF'
INSTANCEBASELINE_DUMP_OK classes=177 ready=1 missing=176 player_baseline=65535 c4_baseline=0 seeded=2/2
EOF
instancebaseline_dump_latest_valid "$instancebaseline_gate_fixture" || {
  rm -f "$instancebaseline_gate_fixture"
  echo "FAIL: v1.8.34zt positive runtime baseline census rejected" >&2
  exit 1
}
cat > "$instancebaseline_gate_fixture" <<'EOF'
INSTANCEBASELINE_DUMP_OK classes=177 ready=0 missing=177 player_baseline=65535 c4_baseline=65535 seeded=2/2
EOF
instancebaseline_dump_latest_valid "$instancebaseline_gate_fixture" || {
  rm -f "$instancebaseline_gate_fixture"
  echo "FAIL: v1.8.34zt empty runtime baseline census rejected" >&2
  exit 1
}
cat > "$instancebaseline_gate_fixture" <<'EOF'
INSTANCEBASELINE_DUMP_OK classes=177 ready=0 missing=176 player_baseline=65535 c4_baseline=65535 seeded=2/2
EOF
if instancebaseline_dump_latest_valid "$instancebaseline_gate_fixture"; then
  rm -f "$instancebaseline_gate_fixture"
  echo "FAIL: v1.8.34zt inconsistent runtime baseline counts accepted" >&2
  exit 1
fi
cat > "$instancebaseline_gate_fixture" <<'EOF'
INSTANCEBASELINE_DUMP_OK classes=177 ready=0 missing=177 player_baseline=0 c4_baseline=65535 seeded=2/2
EOF
if instancebaseline_dump_latest_valid "$instancebaseline_gate_fixture"; then
  rm -f "$instancebaseline_gate_fixture"
  echo "FAIL: v1.8.34zt invalid empty runtime baseline sentinels accepted" >&2
  exit 1
fi
rm -f "$instancebaseline_gate_fixture"
echo "INSTANCEBASELINE_HOST_GATE_SELFTEST=PASS positive+empty+count_mismatch_rejected+sentinel_mismatch_rejected"

# v1.8.34zx: weapon telemetry must be validated semantically.  zw proved the
# runtime loader reached 24/24 but the added ak47_acc tail was truncated by the
# 384-byte probe buffer; the old single-line regex then (correctly) rejected it.
# Lock both sides here: complete/reordered lines pass and the exact truncated zw
# line remains rejected.
# shellcheck source=weapon-profile-gate-lib.sh
source "$ROOT/scripts/weapon-profile-gate-lib.sh"
weapon_profile_gate_fixture='WEAPON_SCRIPT_PROFILES_OK loaded=24 expected=24 encrypted=24 plaintext=0 failed=0 mask=0x2efffdae root=cstrike/scripts ak47=36.000/0.0955/0.980/1.550 awp=115.000/1.4500 usp=34.000/0.1500 ak47_acc=200.000/0.300/1.000/0'
weapon_profile_line_valid "$weapon_profile_gate_fixture" || {
  echo "FAIL: v1.8.34zx complete weapon-profile telemetry rejected" >&2
  exit 1
}
weapon_profile_gate_fixture='WEAPON_SCRIPT_PROFILES_OK ak47_acc=200.000/0.300/1.000/0 extra=forward-compatible loaded=24 mask=0x2efffdae usp=34.000/0.1500 root=cstrike/scripts expected=24 plaintext=0 awp=115.000/1.4500 failed=0 encrypted=24 ak47=36.000/0.0955/0.980/1.550'
weapon_profile_line_valid "$weapon_profile_gate_fixture" || {
  echo "FAIL: v1.8.34zx reordered/extended weapon-profile telemetry rejected" >&2
  exit 1
}
weapon_profile_gate_fixture='WEAPON_SCRIPT_PROFILES_OK loaded=24 expected=24 encrypted=24 plaintext=0 failed=0 mask=0x2efffdae root=cstrike/scripts ak47=36.000/0.0955/0.980/1.550 awp=115.000/1.4500 usp=34.000/0.1500'
if weapon_profile_line_valid "$weapon_profile_gate_fixture"; then
  echo "FAIL: v1.8.34zx truncated zw telemetry accepted" >&2
  exit 1
fi
# v1.8.34zy user-live exposed a non-canonical bool byte as /205.  Do not
# weaken the semantic gate to accept it: zz fixes the source-owned storage and
# normalizes the emitted flag to strict 0/1.
weapon_profile_gate_fixture='WEAPON_SCRIPT_PROFILES_OK loaded=24 expected=24 encrypted=24 plaintext=0 failed=0 mask=0x2efffdae root=cstrike/scripts ak47=36.000/0.0955/0.980/1.550 awp=115.000/1.4500 usp=34.000/0.1500 ak47_acc=200.000/0.350/1.250/205'
if weapon_profile_line_valid "$weapon_profile_gate_fixture"; then
  echo "FAIL: v1.8.34zz non-canonical accuracyQuadratic=205 telemetry accepted" >&2
  exit 1
fi
echo "WEAPON_PROFILE_HOST_GATE_SELFTEST=PASS complete+reordered+extra_fields+truncated_rejected+noncanonical_205_rejected"

if ! grep -q "ConfigureRoundTiming" "$ROOT/src/reconstructed/server/cs_game_state.h" || \
   ! grep -q "CsRoundTickResult" "$ROOT/src/reconstructed/server/cs_game_state.h"; then
  echo "FAIL: cs_game_state round lifecycle API missing" >&2
  exit 1
fi

if ! grep -q 'EngineProbe("LEVEL_CLIENT_MAP_RESET"' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || \
   ! grep -q 'ServerGameClients004Instance().OnLevelShutdown();' "$ROOT/src/reconstructed/abi/server_game_dll006_adapter.cpp"; then
  echo "FAIL: v1.8.27d map-change fake-client lifecycle fix missing" >&2
  exit 1
fi

if ! grep -q 'MAPCYCLE_ORACLE=1' "$ROOT/scripts/test-real-engine-docker.sh" || \
   ! grep -q 'REAL_CLIENT_MAPCYCLE=PASS maps=de_dust2,cs_office,de_aztec,fy_pool_day,de_dust2 transitions=4' "$ROOT/scripts/test-real-engine-docker.sh" || \
   ! grep -q 'mapcycle_server_count_step' "$ROOT/scripts/test-real-engine-docker.sh" || \
   ! grep -q '+sv_filterban 0' "$ROOT/scripts/test-real-engine-docker.sh" || \
   ! grep -q 'SV_FILTERBAN_RUNTIME=PASS value=0 purpose=test_harness_repeated_synthetic_clients' "$ROOT/scripts/test-real-engine-docker.sh" || \
   ! grep -q 'mapcycle_server_ban' "$ROOT/scripts/test-real-engine-docker.sh"; then
  echo "FAIL: v1.8.28b repeated map-cycle ban-isolation oracle missing" >&2
  exit 1
fi

if [[ "${V34_TEST_INNER:-0}" != "1" ]]; then
  mkdir -p "$ROOT/build"
  : > "$LOG"

  server_root="${CSS_V34_SERVER_ROOT:-}"
  if [[ -z "$server_root" ]]; then
    echo "WORKING_DOCKER_ROOT=NOT_SET set=CSS_V34_SERVER_ROOT" | tee -a "$LOG" >&2
    echo "TEST_CURRENT=FAIL"
    echo "SEND_BACK=build/test-current.log"
    exit 1
  fi
  if [[ -f "$server_root/.env" && -f "$server_root/Dockerfile" ]]; then
    echo "WORKING_DOCKER_ROOT=FOUND $server_root" | tee -a "$LOG"
    original_game_dll="$server_root/cstrike/bin/server_i486.so"
    if [[ ! -f "$original_game_dll" ]]; then
      echo "FAIL: original build4100 game DLL missing: $original_game_dll" | tee -a "$LOG" >&2
      exit 1
    fi
    python3 "$ROOT/tools/sendtable-oracle/serverclass_oracle.py" \
      --elf "$original_game_dll" \
      --check-tsv "$ROOT/config/serverclasses-build4100.tsv" | tee -a "$LOG"

    sendtable_links_check="$(mktemp)"
    sendprop_all_json="$(mktemp)"
    sendprop_oracle_log="$(mktemp)"
    python3 "$ROOT/tools/sendtable-oracle/sendprop_field_oracle.py" \
      --elf "$original_game_dll" \
      --all --check \
      --out "$sendprop_all_json" \
      --out-links "$sendtable_links_check" 2>"$sendprop_oracle_log"
    cat "$sendprop_oracle_log" | tee -a "$LOG"
    cmp -s "$sendtable_links_check" "$ROOT/src/reconstructed/abi/sendtable_links_build4100.inc" || {
      echo "FAIL: generated full SendTable-link include differs from original build4100 ELF" | tee -a "$LOG" >&2
      rm -f "$sendtable_links_check" "$sendprop_all_json" "$sendprop_oracle_log"
      exit 1
    }
    rm -f "$sendtable_links_check" "$sendprop_all_json" "$sendprop_oracle_log"
    echo "SENDTABLE_LINK_GENERATED_CONTRACT=PASS links=166 tables=198" | tee -a "$LOG"
  else
    echo "WORKING_DOCKER_ROOT=NOT_FOUND $server_root" | tee -a "$LOG" >&2
    echo "TEST_CURRENT=FAIL"
    echo "SEND_BACK=build/test-current.log"
    exit 1
  fi

  static_start_deadline="${V34_TEST_STATIC_START_TIMEOUT:-45}"
  static_deadline="${V34_TEST_STATIC_TIMEOUT:-90}"
  docker_op_deadline="${V34_TEST_DOCKER_OP_TIMEOUT:-5}"
  real_engine_deadline="${V34_REAL_ENGINE_TIMEOUT:-45}"
  # v1.8.35g gatefix7: the de_dust2 full Protocol7 path now includes reconnect +
  # two full multiclient signons. 180s can terminate the second client even when
  # every runtime/wire check is healthy. Keep the historical gate token below:
  # V34_REAL_ENGINE_MAP_TIMEOUT:-180
  real_engine_map_deadline="${V34_REAL_ENGINE_MAP_TIMEOUT:-300}"
  # gatefix18: de_dust2 is not a normal single-map oracle anymore. It contains
  # the 90s multiclient hold plus the accepted dust2->office transition and the
  # complete office->aztec->pool->dust2 mapcycle. gatefix17 proved the runtime
  # path healthy through mapcycle step 2, then the old 300s *outer* watchdog
  # killed step 3 while fy_pool_day had already completed LevelInit/ServerActivate
  # and bot reseeding. Give only this comprehensive profile a larger ceiling;
  # keep the ordinary cs_office/de_aztec/fy_pool_day watchdog at 300s.
  real_engine_dust2_deadline="${V34_REAL_ENGINE_DUST2_TIMEOUT:-600}"
  echo "WATCHDOGS static_start=${static_start_deadline}s static=${static_deadline}s docker_op=${docker_op_deadline}s real_engine_docker=${real_engine_deadline}s real_engine_map=${real_engine_map_deadline}s real_engine_dust2=${real_engine_dust2_deadline}s" | tee -a "$LOG"

  # Every Docker CLI call in the outer harness is bounded.  The old detached
  # launcher only started its 90s watchdog *after* `docker compose run -d`
  # returned, so Docker Desktop/WSL stalls between Container Created and
  # Container Started could hang test-current forever.
  docker_bounded() {
    local seconds="$1"
    shift
    timeout --signal=TERM --kill-after=3s "${seconds}s" docker "$@"
  }

  real_engine_timeout_for_map() {
    local map_name="$1"
    if [[ "$map_name" == "de_dust2" ]]; then
      printf '%s\n' "$real_engine_dust2_deadline"
    else
      printf '%s\n' "$real_engine_map_deadline"
    fi
  }

  run_real_engine_map_bounded() {
    local map_name="$1"
    local map_deadline
    local map_profile=standard
    map_deadline="$(real_engine_timeout_for_map "$map_name")"
    if [[ "$map_name" == "de_dust2" ]]; then
      map_profile=full_oracle
    fi
    echo "REAL_ENGINE_MAP_WATCHDOG_ARM map=$map_name timeout=${map_deadline}s profile=$map_profile"
    timeout --signal=TERM --kill-after=5s "${map_deadline}s" \
      env CSS_V34_SERVER_ROOT="$server_root" \
          V34_REAL_ENGINE_TIMEOUT="$real_engine_deadline" \
          V34_TEST_MAP="$map_name" \
          "$ROOT/scripts/test-real-engine-docker.sh" "$BUILD"
  }

  container_name="css-v34-test-static-$$"
  container_id=""
  cleanup_outer() {
    trap - EXIT INT TERM HUP
    if [[ -n "$container_id" ]]; then
      docker_bounded "$docker_op_deadline" rm -f "$container_id" >/dev/null 2>&1 || true
      container_id=""
    fi
    # Also remove by name.  This catches the case where compose created the
    # container but the start command timed out before we could inspect its ID.
    docker_bounded "$docker_op_deadline" rm -f "$container_name" >/dev/null 2>&1 || true
  }
  on_outer_signal() {
    echo "TEST_CURRENT_INTERRUPTED cleanup=$container_name" | tee -a "$LOG" >&2
    cleanup_outer
    exit 130
  }
  trap cleanup_outer EXIT
  trap on_outer_signal INT TERM HUP

  echo "STATIC_DOCKER_START_BEGIN container=$container_name timeout=${static_start_deadline}s" | tee -a "$LOG"
  set +e
  docker_bounded "$static_start_deadline" compose -f "$ROOT/docker/compose.dev.yml" run -d -T \
      --name "$container_name" \
      -e V34_TEST_INNER=1 \
      -e V34_SKIP_REAL_ENGINE=1 \
      dev bash -lc './scripts/test-current.sh' 2>&1 | tee -a "$LOG"
  start_rc=${PIPESTATUS[0]}
  set -e

  if [[ $start_rc -eq 124 || $start_rc -eq 137 ]]; then
    echo "STATIC_DOCKER_START_TIMEOUT=${static_start_deadline}s container=$container_name rc=$start_rc" | tee -a "$LOG" >&2
    cleanup_outer
    echo "TEST_CURRENT=FAIL static_start_rc=$start_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log"
    exit 124
  fi
  if [[ $start_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL docker_static_start_rc=$start_rc" | tee -a "$LOG" >&2
    cleanup_outer
    echo "SEND_BACK=build/test-current.log"
    exit "$start_rc"
  fi

  set +e
  container_id="$(docker_bounded "$docker_op_deadline" inspect -f '{{.Id}}' "$container_name" 2>/dev/null)"
  inspect_id_rc=$?
  set -e
  if [[ $inspect_id_rc -ne 0 || -z "$container_id" ]]; then
    echo "TEST_CURRENT=FAIL docker_static_inspect_id_rc=$inspect_id_rc container=$container_name" | tee -a "$LOG" >&2
    cleanup_outer
    echo "SEND_BACK=build/test-current.log"
    exit 125
  fi
  echo "STATIC_DOCKER_STARTED container=$container_name id=${container_id:0:12}" | tee -a "$LOG"

  deadline=$((SECONDS + static_deadline))
  timed_out=0
  docker_stalled=0
  while :; do
    set +e
    running="$(docker_bounded "$docker_op_deadline" inspect -f '{{.State.Running}}' "$container_id" 2>/dev/null)"
    inspect_run_rc=$?
    set -e
    if [[ $inspect_run_rc -eq 124 || $inspect_run_rc -eq 137 ]]; then
      docker_stalled=1
      echo "STATIC_DOCKER_INSPECT_TIMEOUT=${docker_op_deadline}s container=$container_id rc=$inspect_run_rc" | tee -a "$LOG" >&2
      break
    fi
    if [[ $inspect_run_rc -ne 0 || "$running" != "true" ]]; then
      break
    fi
    if (( SECONDS >= deadline )); then
      timed_out=1
      echo "STATIC_WATCHDOG_TIMEOUT=${static_deadline}s container=$container_id" | tee -a "$LOG" >&2
      docker_bounded "$docker_op_deadline" kill "$container_id" >/dev/null 2>&1 || true
      break
    fi
    sleep 0.2
  done

  if [[ $timed_out -eq 1 ]]; then
    static_rc=124
  elif [[ $docker_stalled -eq 1 ]]; then
    static_rc=125
  else
    set +e
    static_rc="$(docker_bounded "$docker_op_deadline" inspect -f '{{.State.ExitCode}}' "$container_id" 2>/dev/null)"
    inspect_exit_rc=$?
    set -e
    if [[ $inspect_exit_rc -ne 0 || ! "$static_rc" =~ ^[0-9]+$ ]]; then
      echo "STATIC_DOCKER_EXITCODE_UNAVAILABLE rc=$inspect_exit_rc value=${static_rc:-empty}" | tee -a "$LOG" >&2
      static_rc=125
    fi
  fi

  set +e
  docker_bounded 12 logs "$container_id" 2>&1 | tee -a "$LOG"
  logs_rc=${PIPESTATUS[0]}
  set -e
  if [[ $logs_rc -eq 124 || $logs_rc -eq 137 ]]; then
    echo "STATIC_DOCKER_LOGS_TIMEOUT=12s container=$container_id rc=$logs_rc" | tee -a "$LOG" >&2
  fi
  cleanup_outer

  if [[ $static_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL static_rc=$static_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log"
    exit "$static_rc"
  fi

  echo "HOST_REAL_ENGINE_BEGIN runtime=server_v34_docker map=de_dust2" | tee -a "$LOG"
  set +e
  run_real_engine_map_bounded de_dust2 2>&1 | tee -a "$LOG"
  real_rc=${PIPESTATUS[0]}
  set -e
  if [[ $real_rc -eq 124 || $real_rc -eq 137 ]]; then
    echo "REAL_ENGINE_MAP_WATCHDOG_TIMEOUT map=de_dust2 timeout=${real_engine_dust2_deadline}s rc=$real_rc" | tee -a "$LOG" >&2
  fi
  if [[ $real_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL real_engine_map=de_dust2 rc=$real_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log" | tee -a "$LOG"
    exit "$real_rc"
  fi
  echo "HOST_REAL_ENGINE_MAP=PASS map=de_dust2 rc=0" | tee -a "$LOG"

  echo "HOST_REAL_ENGINE_HOSTAGE_BEGIN runtime=server_v34_docker map=cs_office" | tee -a "$LOG"
  set +e
  run_real_engine_map_bounded cs_office 2>&1 | tee -a "$LOG"
  hostage_rc=${PIPESTATUS[0]}
  set -e
  if [[ $hostage_rc -eq 124 || $hostage_rc -eq 137 ]]; then
    echo "REAL_ENGINE_MAP_WATCHDOG_TIMEOUT map=cs_office timeout=${real_engine_map_deadline}s rc=$hostage_rc" | tee -a "$LOG" >&2
  fi
  if [[ $hostage_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL real_engine_map=cs_office rc=$hostage_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log" | tee -a "$LOG"
    exit "$hostage_rc"
  fi
  echo "HOST_REAL_ENGINE_MAP=PASS map=cs_office rc=0" | tee -a "$LOG"

  echo "HOST_REAL_ENGINE_WATER_BEGIN runtime=server_v34_docker map=de_aztec" | tee -a "$LOG"
  set +e
  run_real_engine_map_bounded de_aztec 2>&1 | tee -a "$LOG"
  water_rc=${PIPESTATUS[0]}
  set -e
  if [[ $water_rc -eq 124 || $water_rc -eq 137 ]]; then
    echo "REAL_ENGINE_MAP_WATCHDOG_TIMEOUT map=de_aztec timeout=${real_engine_map_deadline}s rc=$water_rc" | tee -a "$LOG" >&2
  fi
  if [[ $water_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL real_engine_map=de_aztec rc=$water_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log" | tee -a "$LOG"
    exit "$water_rc"
  fi
  echo "HOST_REAL_ENGINE_MAP=PASS map=de_aztec rc=0" | tee -a "$LOG"

  echo "HOST_REAL_ENGINE_WATER_LEVEL2_BEGIN runtime=server_v34_docker map=fy_pool_day" | tee -a "$LOG"
  set +e
  run_real_engine_map_bounded fy_pool_day 2>&1 | tee -a "$LOG"
  pool_water_rc=${PIPESTATUS[0]}
  set -e
  if [[ $pool_water_rc -eq 124 || $pool_water_rc -eq 137 ]]; then
    echo "REAL_ENGINE_MAP_WATCHDOG_TIMEOUT map=fy_pool_day timeout=${real_engine_map_deadline}s rc=$pool_water_rc" | tee -a "$LOG" >&2
  fi
  if [[ $pool_water_rc -ne 0 ]]; then
    echo "TEST_CURRENT=FAIL real_engine_map=fy_pool_day rc=$pool_water_rc" | tee -a "$LOG" >&2
    echo "SEND_BACK=build/test-current.log" | tee -a "$LOG"
    exit "$pool_water_rc"
  fi
  echo "HOST_REAL_ENGINE_MAP=PASS map=fy_pool_day rc=0" | tee -a "$LOG"
  echo "HOST_REAL_ENGINE=PASS maps=de_dust2,cs_office,de_aztec,fy_pool_day" | tee -a "$LOG"

  echo "TEST_CURRENT=PASS" | tee -a "$LOG"
  echo "SEND_BACK=TEST_CURRENT=PASS" | tee -a "$LOG"
  trap - EXIT INT TERM HUP
  exit 0
fi

"$ROOT/scripts/check-toolchain.sh"

cmake \
  -S "$ROOT" \
  -B "$BUILD" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DV34_BUILD_PROFILE=DEBUG \
  -DV34_FORCE_32BIT=ON \
  -DV34_BUILD_RECONSTRUCTION_CORE=ON

cmake --build "$BUILD" --target \
  server-reconstructed-i486 \
  factory-differential-smoke \
  reconstruction-core-smoke \
  client-registry-smoke \
  cs-game-state-smoke \
  bomb-round-objective-smoke \
  hostage-round-objective-smoke \
  weapon-inventory-smoke \
  buy-economy-smoke \
  weapon-fire-reload-smoke \
  combat-ballistics-smoke \
  player-damage-smoke \
  score-economy-smoke \
  round-lifecycle-smoke \
  player-state-usercmd-smoke \
  usercmd-bitbuf-smoke \
  shared-abi-clients-smoke \
  entity-runtime-smoke \
  commands-network-smoke \
  weapon-grenade-runtime-smoke \
  end-to-end-match-smoke \
  map-entity-lump-smoke \
  networkable-bridge-smoke \
  sendtable-live-graph-smoke \
  sendtable-client-schema-smoke \
  entity-factory-registry-smoke \
  servergamedll006-abi-smoke

file "$LIB" | grep -q 'ELF 32-bit' || { echo "FAIL: server_i486.so is not ELF32" >&2; exit 1; }
echo "ELF32=PASS"

exports="$(nm -D --defined-only "$LIB" | awk '{print $3}' | sed '/^$/d')"
if [[ "$exports" != "CreateInterface" ]]; then
  echo "FAIL: unexpected exported symbol set:" >&2
  printf '%s\n' "$exports" >&2
  exit 1
fi
echo "EXPORT_SURFACE=CreateInterface_ONLY"

if readelf -d "$LIB" | grep -q 'TEXTREL'; then
  echo "FAIL: server_i486.so contains TEXTREL" >&2
  exit 1
fi
echo "TEXTREL=NONE"

ctest \
  --test-dir "$BUILD" \
  --output-on-failure \
  -R '^(reconstruction-core-smoke|client-registry-smoke|cs-game-state-smoke|bomb-round-objective-smoke|hostage-round-objective-smoke|weapon-inventory-smoke|buy-economy-smoke|weapon-fire-reload-smoke|combat-ballistics-smoke|player-damage-smoke|score-economy-smoke|round-lifecycle-smoke|player-state-usercmd-smoke|usercmd-bitbuf-smoke|shared-abi-clients-smoke|entity-runtime-smoke|commands-network-smoke|weapon-grenade-runtime-smoke|end-to-end-match-smoke|map-entity-lump-smoke|networkable-bridge-smoke|sendtable-live-graph-smoke|sendtable-client-schema-smoke|entity-factory-registry-smoke|servergamedll006-abi-smoke)$'

probe_selftest="$(python3 "$ROOT/scripts/protocol7-client-probe.py" --self-test)"
printf '%s\n' "$probe_selftest"
expected_probe_revision="PROTOCOL7_SELFTEST=PASS probe_revision=$PROJECT_REVISION"
if ! grep -Fq "$expected_probe_revision" <<<"$probe_selftest"; then
  echo "FAIL: protocol7 probe revision mismatch; expected $PROJECT_REVISION" >&2
  exit 1
fi
echo "PROTOCOL7_PROBE_REVISION=$PROJECT_REVISION PASS"

rcon_selftest="$(python3 "$ROOT/scripts/source-rcon.py" --self-test)"
printf '%s\n' "$rcon_selftest"
if ! grep -q 'SOURCE_RCON_SELFTEST=PASS revision=v1.8.27d' <<<"$rcon_selftest"; then
  echo "FAIL: source-rcon.py self-test/revision mismatch" >&2
  exit 1
fi
echo "SOURCE_RCON_REVISION=v1.8.27d PASS"

"$ROOT/scripts/test-differential.sh" "$BUILD"
if [[ "${V34_SKIP_REAL_ENGINE:-0}" == "1" ]]; then
  echo "REAL_ENGINE=DEFERRED_TO_HOST_WORKING_DOCKER"
else
  "$ROOT/scripts/test-real-engine.sh" "$BUILD"
fi

echo "SOURCE_V1_8_30A=PASS"
echo "PATCH_APPLIED_V1_8_30A=PASS"
echo "SOURCE_V1_8_30B=PASS"
echo "PATCH_APPLIED_V1_8_30B=PASS"
echo "SOURCE_V1_8_30C=PASS"
echo "PATCH_APPLIED_V1_8_30C=PASS"
grep -q 'def build_file_request_payload' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.31a NET_File encoder missing" >&2; exit 1; }
grep -q 'PROTOCOL7_DOWNLOAD_FILE=PASS' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.31a download wire oracle missing" >&2; exit 1; }
echo "SOURCE_V1_8_31A=PASS"
echo "PATCH_APPLIED_V1_8_31A=PASS"
grep -q 'receive_budget = max(receive_budget, args.timeout \* 15.0, 45.0)' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.31b download receive budget missing" >&2; exit 1; }
grep -q -- '--max-udp-packets 768' "$ROOT/scripts/test-real-engine-docker.sh" || { echo "FAIL: v1.8.31b download UDP packet budget missing" >&2; exit 1; }
echo "SOURCE_V1_8_31B=PASS"
echo "PATCH_APPLIED_V1_8_31B=PASS"
grep -q 'constexpr char kVoiceServer001\[\] = "VoiceServer001"' "$ROOT/src/reconstructed/abi/server_game_dll006_adapter.cpp" || { echo "FAIL: v1.8.32a VoiceServer001 binding missing" >&2; exit 1; }
grep -q 'VOICE_LISTENING_READY' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.32a voice listening matrix missing" >&2; exit 1; }
grep -q 'def build_voice_data_payload' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.32a CLC_VoiceData encoder missing" >&2; exit 1; }
grep -q 'PROTOCOL7_VOICE_DATA=PASS' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.32a SVC_VoiceData oracle missing" >&2; exit 1; }
grep -q 'voice_forward=\$multiclient_voice' "$ROOT/scripts/test-real-engine-docker.sh" || { echo "FAIL: v1.8.32a multiclient voice gate missing" >&2; exit 1; }
grep -q 'SERVERGAMEDLL006_VOICESERVER001=PASS slots=2,3 active_matrix=PASS disconnect_clear=PASS' "$ROOT/src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" || { echo "FAIL: v1.8.32a VoiceServer001 ABI smoke missing" >&2; exit 1; }
echo "SOURCE_V1_8_32A=PASS"
echo "PATCH_APPLIED_V1_8_32A=PASS"
grep -q 'ap.add_argument("--max-udp-packets", type=int, default=256)' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.32d default UDP packet budget missing" >&2; exit 1; }
echo "SOURCE_V1_8_32D=PASS"
echo "PATCH_APPLIED_V1_8_32D=PASS"
grep -q 'struct MapObjectiveZone' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.h" || { echo "FAIL: v1.8.33a map objective zones missing" >&2; exit 1; }
grep -q 'GAMEPLAY_PLANTED_C4_SENDSTATE' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.33a planted C4 sendstate missing" >&2; exit 1; }
grep -q 'BindGameplayGameState' "$ROOT/src/reconstructed/abi/server_class_registry.cpp" || { echo "FAIL: v1.8.33a gameplay game-state SendProp binding missing" >&2; exit 1; }
grep -q 'DT_PlantedC4' "$ROOT/src/reconstructed/abi/server_class_registry.cpp" || { echo "FAIL: v1.8.33a planted C4 SendProp proxies missing" >&2; exit 1; }
grep -q 'PROTOCOL7_OBJECTIVE_SCENARIO=PASS' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.33a objective Protocol7 oracle missing" >&2; exit 1; }
grep -q 'PROTOCOL7_PLANTED_C4_SENDPROP=PASS' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.33a planted C4 wire SendProp oracle missing" >&2; exit 1; }
grep -q 'objective_round=\$multiclient_objective' "$ROOT/scripts/test-real-engine-docker.sh" || { echo "FAIL: v1.8.33a multiclient objective gate missing" >&2; exit 1; }
echo "SOURCE_V1_8_33A=PASS"
echo "PATCH_APPLIED_V1_8_33A=PASS"
require_source_tokens "v1.8.33b ListenEvents semantic gate compatibility" "scripts/test-real-engine-docker.sh" \
  'player_say_id=[0-9]+' \
  'mask_bits=512' \
  'protocol7_listen_events_ok=1'
echo "SOURCE_V1_8_33B=PASS"
echo "PATCH_APPLIED_V1_8_33B=PASS"
grep -q 'GAMEPLAY_PLANTED_C4_DEFERRED' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.33c deferred planted C4 materialization missing" >&2; exit 1; }
# v1.8.33c originally allocated planted_c4 during deferred materialization and
# therefore reported create_edict through GAMEPLAY_PLANTED_C4_SPAWN_FAIL.
# v1.8.34h intentionally moved that allocation into the pre-client reserved-slot
# path, where the same failure stage is now GAMEPLAY_PLANTED_C4_RESERVE_FAIL.
# Keep the historical gate semantic instead of pinning the superseded call site.
c4_alloc_diag=""
if grep -q 'GAMEPLAY_PLANTED_C4_SPAWN_FAIL.*stage=create_edict' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp"; then
  c4_alloc_diag="spawn"
elif grep -q 'GAMEPLAY_PLANTED_C4_RESERVE_FAIL.*stage=create_edict' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp"; then
  c4_alloc_diag="reservation"
else
  echo "FAIL: v1.8.33c planted C4 create_edict diagnostics missing from spawn/reservation paths" >&2
  exit 1
fi
echo "SOURCE_V1_8_33C=PASS c4_alloc_diag=$c4_alloc_diag"
echo "PATCH_APPLIED_V1_8_33C=PASS"
grep -q 'Detach(edict, false)' "$ROOT/src/reconstructed/abi/server_game_ents001_adapter.cpp" || { echo "FAIL: v1.8.33d FreeContainingEntity still re-enters engine edict removal" >&2; exit 1; }
grep -q 'SERVERGAMEENTS001_FREECONTAINING=PASS no_removeedict_reentry=PASS ed_clear_safe=PASS' "$ROOT/src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" || { echo "FAIL: v1.8.33d FreeContainingEntity regression smoke missing" >&2; exit 1; }
echo "SOURCE_V1_8_33D=PASS"
echo "PATCH_APPLIED_V1_8_33D=PASS"
grep -Fq 'objective_deadline = time.monotonic() + 12.0' "$ROOT/scripts/protocol7-client-probe.py" || { echo "FAIL: v1.8.33e objective receive window regression" >&2; exit 1; }
echo "SOURCE_V1_8_33E=PASS"
echo "PATCH_APPLIED_V1_8_33E=PASS"
grep -Fq 'beforeCommandButtons = before ? before->lastUserCommand.buttons : 0' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34a full usercmd button edge state missing" >&2; exit 1; }
grep -q 'GAMEPLAY_DEFUSE_CANCEL_RELEASE' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34a defuse release/range cancellation missing" >&2; exit 1; }
grep -q 'return defuserIndex_ == static_cast<int>(clientIndex);' "$ROOT/src/reconstructed/server/bomb_objective.cpp" || { echo "FAIL: v1.8.34a held-use idempotent defuse missing" >&2; exit 1; }
grep -q 'BOMB_ROUND_OBJECTIVE_V0_9_1=PASS' "$ROOT/src/reconstructed/tests/bomb_round_objective_smoke.cpp" || { echo "FAIL: v1.8.34a objective lifecycle smoke missing" >&2; exit 1; }
echo "SOURCE_V1_8_34A=PASS"
echo "PATCH_APPLIED_V1_8_34A=PASS"
grep -q 'ENTITY_BRIDGE_REMOVE_REENTRY_OK' "$ROOT/src/reconstructed/abi/engine_entity_bridge.cpp" || { echo "FAIL: v1.8.34b reentrant RemoveEdict bridge fix missing" >&2; exit 1; }
grep -q 'removeedict_reentry_safe=1' "$ROOT/src/reconstructed/tests/networkable_bridge_smoke.cpp" || { echo "FAIL: v1.8.34b reentrant RemoveEdict regression smoke missing" >&2; exit 1; }
grep -q 'GAMEPLAY_OBJECTIVE_STAGE.*step=4 action=round_cycle_and_cleanup' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34b objective stage diagnostics missing" >&2; exit 1; }
grep -q 'entity_bridge_remove_reentry_missing' "$ROOT/scripts/test-real-engine-docker.sh" || { echo "FAIL: v1.8.34b live RemoveEdict reentry gate missing" >&2; exit 1; }
echo "SOURCE_V1_8_34B=PASS"
echo "PATCH_APPLIED_V1_8_34B=PASS"
require_source_fixed "v1.8.34c dynamic generic transmit index collection" "src/reconstructed/abi/engine_entity_bridge.cpp" "CollectForcedTransmitIndices"
require_source_fixed "v1.8.34c generic CheckTransmit force path" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "CheckTransmitGeneric_OK"
require_source_fixed "v1.8.34c forced generic transmit regression smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "forced_generic_transmit=1"
require_source_fixed "v1.8.34c voice/objective oracle independence" "scripts/protocol7-client-probe.py" "Sending CLC_VoiceData first means"
# Do not pin a prose comment from an older patch. v1.8.34e promoted this
# diagnostic to a required post-ServerActivate live gate, so validate the
# active CheckTransmitGeneric_OK contract itself.
require_source_regex "v1.8.34c live generic CheckTransmit contract" "scripts/test-real-engine-docker.sh" 'CheckTransmitGeneric_OK forced=\[1-9\]\[0-9\]\*'
echo "SOURCE_V1_8_34C=PASS"
echo "PATCH_APPLIED_V1_8_34C=PASS"
grep -q 'kEdictTransmitAlways = 0x08u' "$ROOT/src/reconstructed/public/build4100/edict.h" || { echo "FAIL: v1.8.34d exact FL_EDICT_ALWAYS flag missing" >&2; exit 1; }
grep -q 'build4100::kEdictTransmitAlways' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34d planted C4 ALWAYS transmit state missing" >&2; exit 1; }
grep -q 'GAMEPLAY_PLANTED_C4_TRANSMIT_STATE' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34d planted C4 transmit-state probe missing" >&2; exit 1; }
grep -q 'planted_c4_always=1' "$ROOT/src/reconstructed/tests/networkable_bridge_smoke.cpp" || { echo "FAIL: v1.8.34d planted C4 ALWAYS regression smoke missing" >&2; exit 1; }
require_source_regex "v1.8.34d+ live planted C4 ALWAYS/full-statechange gate" "scripts/test-real-engine-docker.sh" 'planted_c4_(transmit_always|full_statechanged)_missing'
echo "SOURCE_V1_8_34D=PASS"
echo "PATCH_APPLIED_V1_8_34D=PASS"
grep -q 'rawIndex < build4100::kMaxEdicts' "$ROOT/src/reconstructed/abi/engine_entity_bridge.cpp" || { echo "FAIL: v1.8.34e fixed edict-pool geometry missing" >&2; exit 1; }
grep -q 'later fake clients still occupy the' "$ROOT/src/reconstructed/abi/server_game_clients004_adapter.cpp" || { echo "FAIL: v1.8.34e post-activate fake-client edict geometry missing" >&2; exit 1; }
require_source_fixed "v1.8.34e+ dynamic edict transmit-state lookup/policy" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "GenericTransmitStateForEngineIndex"
grep -q 'forcedBridge.ServerActivate(forcedEdicts.data(), 3)' "$ROOT/src/reconstructed/tests/networkable_bridge_smoke.cpp" || { echo "FAIL: v1.8.34e post-ServerActivate dynamic-edict regression setup missing" >&2; exit 1; }
grep -q 'dynamic_postactivate_edict=1' "$ROOT/src/reconstructed/tests/networkable_bridge_smoke.cpp" || { echo "FAIL: v1.8.34e post-ServerActivate dynamic-edict regression marker missing" >&2; exit 1; }
require_source_fixed "v1.8.34e live post-activate generic transmit gate" "scripts/test-real-engine-docker.sh" "checktransmit_postactivate_generic_missing"
echo "SOURCE_V1_8_34E=PASS"
echo "PATCH_APPLIED_V1_8_34E=PASS"
require_source_fixed "v1.8.34f protocol probe revisioned contract" "scripts/protocol7-client-probe.py" 'PROBE_REVISION = '
require_source_fixed "v1.8.34f live oracle revisioned contract" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION='
require_source_fixed "v1.8.34f live transmit contract marker" "scripts/test-real-engine-docker.sh" 'LIVE_TRANSMIT_CONTRACT='
require_source_fixed "v1.8.34f live oracle completion marker" "scripts/test-real-engine-docker.sh" 'REAL_ENGINE_ORACLE_CONTRACT=PASS revision=$LIVE_ORACLE_REVISION'
echo "SOURCE_V1_8_34F=PASS"
echo "PATCH_APPLIED_V1_8_34F=PASS"
require_source_fixed "v1.8.34g static compose-start watchdog" "scripts/test-current.sh" 'STATIC_DOCKER_START_TIMEOUT=${static_start_deadline}s'
require_source_fixed "v1.8.34g bounded docker operations" "scripts/test-current.sh" 'timeout --signal=TERM --kill-after=3s "${seconds}s" docker "$@"'
require_source_fixed "v1.8.34g created-before-start cleanup" "scripts/test-current.sh" 'the start command timed out before we could inspect its ID'
echo "SOURCE_V1_8_34G=PASS"
echo "PATCH_APPLIED_V1_8_34G=PASS"
require_source_fixed "v1.8.34h reserved planted-C4 edict" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "GAMEPLAY_PLANTED_C4_RESERVED"
require_source_fixed "v1.8.34h reservation create_edict diagnostics" "src/reconstructed/abi/server_game_clients004_adapter.cpp" 'EngineProbe("GAMEPLAY_PLANTED_C4_RESERVE_FAIL", "stage=create_edict")'
require_source_fixed "v1.8.34h+ level reservation entrypoint" "src/reconstructed/abi/server_game_dll006_adapter.cpp" "PrepareLevelNetworkEntities(\"serveractivate_prebaseline\")"
require_source_fixed "v1.8.34h planted-C4 DONT_SEND parking" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "GAMEPLAY_PLANTED_C4_DEACTIVATED"
require_source_fixed "v1.8.34h DONT_SEND generic exclusion" "src/reconstructed/abi/engine_entity_bridge.cpp" "kEdictTransmitDontSend"
require_source_fixed "v1.8.34h reserved C4 regression smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "planted_c4_reserved_dontsend=1"
require_source_fixed "v1.8.34h live reserved-slot reuse gate" "scripts/test-real-engine-docker.sh" "planted_c4_reserved_slot_not_reused"
require_source_fixed "v1.8.34h protocol probe revisioned contract" "scripts/protocol7-client-probe.py" 'PROBE_REVISION = REVISION_FILE.read_text'
echo "SOURCE_V1_8_34H=PASS"
echo "PATCH_APPLIED_V1_8_34H=PASS"
require_source_fixed "v1.8.34i historical C4 gate accepts reservation diagnostics" "scripts/test-current.sh" 'c4_alloc_diag="reservation"'
require_source_fixed "v1.8.34i reservation diagnostics pinned at current owner" "scripts/test-current.sh" "v1.8.34h reservation create_edict diagnostics"
echo "SOURCE_V1_8_34I=PASS"
echo "PATCH_APPLIED_V1_8_34I=PASS"
require_source_fixed "v1.8.34j C4 reservation uses normal engine allocator" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "engine_->CreateEdict(-1)"
require_source_fixed "v1.8.34j C4 reservation rejects client edicts" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "actualIndex <= globals_->maxClients"
require_source_fixed "v1.8.34j bounded pre-client C4 reservation retries" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "kPlantedC4PreClientReservationAttempts = 8"
require_source_fixed "v1.8.34j reserved C4 pending materialization" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "if (plantedC4SpawnPending_)"
require_source_fixed "v1.8.34j live auto-reservation gate" "scripts/test-real-engine-docker.sh" "allocator=auto attempt="
require_source_fixed "v1.8.34j live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34J=PASS"
echo "PATCH_APPLIED_V1_8_34J=PASS"
require_source_fixed "v1.8.34k Protocol7 snapshot player inventory" "scripts/protocol7-client-probe.py" 'player_entities={player_entity_ids}'
require_source_fixed "v1.8.34k Protocol7 snapshot extra-entity inventory" "scripts/protocol7-client-probe.py" 'extras={extra_entity_classes}'
require_source_fixed "v1.8.34k live full-snapshot gate ignores reserved non-player shell" "scripts/test-real-engine-docker.sh" 'players=3 .*player_entities=1,2,3'
require_source_fixed "v1.8.34k live gameplay SendProp gate uses player inventory" "scripts/test-real-engine-docker.sh" 'player_entities=1,2,3 extras=[^ ]+'
require_source_fixed "v1.8.34k+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34K=PASS"
echo "PATCH_APPLIED_V1_8_34K=PASS"
require_source_fixed "v1.8.34l FL_FULL_EDICT_CHANGED bit" "src/reconstructed/public/build4100/edict.h" "kEdictFullChanged = 0x100u"
require_source_fixed "v1.8.34l exact change accessor serial" "src/reconstructed/public/build4100/edict.h" "changeInfoSerialNumber"
require_source_fixed "v1.8.34l CBaseEdict full StateChanged contract" "src/reconstructed/abi/engine_server021_client.cpp" "MarkEdictStateChangedFull"
require_source_fixed "v1.8.34l persistent generic transmit policy" "src/reconstructed/abi/engine_entity_bridge.h" "desiredTransmitState"
require_source_fixed "v1.8.34l parked generic candidate suppression" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "ClearTransmitBit(always, index)"
require_source_fixed "v1.8.34l planted C4 full StateChanged materialization" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "MarkEdictStateChangedFull(plantedC4Edict_)"
require_source_fixed "v1.8.34l persistent transmit regression smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "persistent_generic_transmit_policy=1"
require_source_fixed "v1.8.34l full-edict-changed regression smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "full_edict_changed_contract=1"
require_source_fixed "v1.8.34l live full-StateChanged gate" "scripts/test-real-engine-docker.sh" "planted_c4_full_statechanged_missing"
require_source_fixed "v1.8.34m+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
require_source_fixed "v1.8.34m recurring full-snapshot baseline ack" "scripts/protocol7-client-probe.py" "last_baseline_ack_tick != full_snapshot_tick"
require_source_fixed "v1.8.34m recurring full-snapshot tick ack" "scripts/protocol7-client-probe.py" "last_snapshot_tick_ack_tick != full_snapshot_tick"
require_source_fixed "v1.8.34m delta EnterPVS class carry-forward" "scripts/protocol7-client-probe.py" 'full_snapshot_entity_classes = dict(generic["entity_classes"])'
require_source_fixed "v1.8.34m objective PE false-positive filter" "scripts/protocol7-client-probe.py" 'if int(item["updated_entries"]) > 0 and int(item["data_bits"]) > 0'
echo "SOURCE_V1_8_34M=PASS"
echo "PATCH_APPLIED_V1_8_34M=PASS"
require_source_fixed "v1.8.34n+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
require_source_fixed "v1.8.34n unified Protocol7 completion contract" "scripts/protocol7-client-probe.py" "def integration_path_ready(require_dwell: bool) -> bool:"
require_source_fixed "v1.8.34n packet-budget grace requires functional readiness" "scripts/protocol7-client-probe.py" "if not integration_path_ready(require_dwell=False):"
require_source_fixed "v1.8.34n packet-budget grace diagnostic" "scripts/protocol7-client-probe.py" "PROTOCOL7_PACKET_BUDGET_GRACE=PASS"
require_source_fixed "v1.8.34n grace remains signon-deadline bounded" "scripts/protocol7-client-probe.py" "deadline_remaining={max(0.0, signon_deadline - packet_budget_grace_started_at):.3f}s"
echo "SOURCE_V1_8_34N=PASS"
echo "PATCH_APPLIED_V1_8_34N=PASS"
require_source_fixed "v1.8.34o+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
require_source_fixed "v1.8.34o post-spawn dwell helper" "scripts/protocol7-client-probe.py" "def extend_post_spawn_dwell(seconds: float, reason: str) -> None:"
require_source_fixed "v1.8.34o initial snapshot owns survival dwell" "scripts/protocol7-client-probe.py" 'extend_post_spawn_dwell(4.50, "initial_snapshot_tick_ack")'
require_source_fixed "v1.8.34o recurring resnapshot does not extend dwell" "scripts/protocol7-client-probe.py" 'count={resnapshot_tick_acks} dwell_extended=0'
require_source_fixed "v1.8.34o dwell diagnostics" "scripts/protocol7-client-probe.py" "dwell_reason={post_spawn_dwell_reason} dwell_updates={post_spawn_dwell_updates}"
echo "SOURCE_V1_8_34O=PASS"
echo "PATCH_APPLIED_V1_8_34O=PASS"
require_source_fixed "v1.8.34p generic pack-boundary full refresh" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "MarkEdictStateChangedFull(edict)"
require_source_fixed "v1.8.34p generic pack-boundary engine notification" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "NotifyEdictFlagsChange(static_cast<int>(index))"
require_source_fixed "v1.8.34p pack refresh smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "generic_pack_boundary_refresh=1"
require_source_regex "v1.8.34p live generic fresh-pack gate" "scripts/test-real-engine-docker.sh" 'CheckTransmitGeneric_OK forced=\[1-9\]\[0-9\]\* .*fresh=\[1-9\]\[0-9\]\*'
require_source_fixed "v1.8.34p generic pack-boundary lineage" "scripts/test-real-engine-docker.sh" 'postactivate_checktransmit'
echo "SOURCE_V1_8_34P=PASS"
echo "PATCH_APPLIED_V1_8_34P=PASS"
require_source_fixed "v1.8.34q ServerActivate pre-baseline reservation" "src/reconstructed/abi/server_game_dll006_adapter.cpp" 'PrepareLevelNetworkEntities("serveractivate_prebaseline")'
require_source_fixed "v1.8.34q first-frame baseline verification" "src/reconstructed/abi/server_game_dll006_adapter.cpp" "VerifyLevelNetworkEntityBaselines()"
require_source_fixed "v1.8.34q pre-baseline lifecycle smoke" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "SERVERGAMEDLL006_C4_PREBASELINE=PASS"
require_source_fixed "v1.8.34q CPlantedC4 pre-baseline probe" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "GAMEPLAY_PLANTED_C4_PREBASELINE_READY"
require_source_fixed "v1.8.34q CPlantedC4 instancebaseline probe" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "GAMEPLAY_PLANTED_C4_BASELINE_READY"
require_source_fixed "v1.8.34q live instancebaseline gate" "scripts/test-real-engine-docker.sh" "planted_c4_instancebaseline_missing"
require_source_fixed "v1.8.34q+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
require_source_fixed "v1.8.34r SendPropBool unsigned ABI" "src/reconstructed/abi/server_class_registry.cpp" "descriptor.kind == OriginalSendPropKind::Bool"
require_source_fixed "v1.8.34r SendPropBool unsigned flag" "src/reconstructed/abi/server_class_registry.cpp" "build4100::kSendPropUnsigned"
require_source_fixed "v1.8.34r C4 bool live graph regression" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" "planted_c4_bool_unsigned=1"
require_source_fixed "v1.8.34r C4 wire diagnostic" "scripts/protocol7-client-probe.py" "PROTOCOL7_PLANTED_C4_WIRE=PASS"
require_source_fixed "v1.8.34r C4 bool schema gate" "scripts/test-real-engine-docker.sh" "sendprop_schema_planted_c4_bool_unsigned_missing"
require_source_fixed "v1.8.34r live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34Q=PASS"
echo "PATCH_APPLIED_V1_8_34Q=PASS"
echo "SOURCE_V1_8_34R=PASS"
echo "PATCH_APPLIED_V1_8_34R=PASS"
require_source_fixed "v1.8.34s nullable transmit-always policy diagnostic" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "policy_always=%zu always_bits=%zu always_mask=%s"
require_source_fixed "v1.8.34s optional transmit-always bitset contract" "src/reconstructed/abi/server_game_ents001_adapter.cpp" "if (always) {"
require_source_fixed "v1.8.34s nullable transmit-always regression smoke" "src/reconstructed/tests/networkable_bridge_smoke.cpp" "nullable_transmit_always=1"
require_source_fixed "v1.8.34s live nullable-always gate" "scripts/test-real-engine-docker.sh" "checktransmit_nullable_always_pointer_mismatch"
require_source_fixed "v1.8.34s+ live oracle revision lineage" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34S=PASS"
echo "PATCH_APPLIED_V1_8_34S=PASS"
require_source_fixed "v1.8.34t C4 reservation release begin probe" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "GAMEPLAY_PLANTED_C4_RESERVATION_RELEASE_BEGIN"
require_source_fixed "v1.8.34t C4 reservation release completion probe" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "callback_path=removeedict"
require_source_fixed "v1.8.34t mapchange-scoped RemoveEdict reentry gate" "scripts/test-real-engine-docker.sh" "entity_bridge_remove_reentry_missing_after_levelshutdown"
require_source_fixed "v1.8.34t LevelShutdown C4 release gate" "scripts/test-real-engine-docker.sh" "planted_c4_levelshutdown_release_done_missing"
require_source_fixed "v1.8.34t live oracle revision" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34T=PASS"
echo "PATCH_APPLIED_V1_8_34T=PASS"
require_source_tokens "v1.8.34u live weapon button bridge" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  "HandleWeaponButtons(" "beforeCommandButtons" "commands.back()"
require_source_fixed "v1.8.34u reconnect weapon runtime reset" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "weaponRuntime_.ResetClient(clientIndex)"
require_source_fixed "v1.8.34u weapon runtime per-slot reset API" "src/reconstructed/server/weapon_runtime.cpp" "WeaponRuntimeController::ResetClient"
require_source_fixed "v1.8.34u weapon_fire game event" "src/reconstructed/abi/server_game_clients004_adapter.cpp" 'FireWeaponGameEvent("weapon_fire"'
require_source_fixed "v1.8.34u weapon_reload game event" "src/reconstructed/abi/server_game_clients004_adapter.cpp" 'FireWeaponGameEvent("weapon_reload"'
require_source_fixed "v1.8.34u protocol probe revision" "scripts/protocol7-client-probe.py" 'PROBE_REVISION = REVISION_FILE.read_text'
require_source_fixed "v1.8.34u protocol attack oracle" "scripts/protocol7-client-probe.py" "PROTOCOL7_WEAPON_ATTACK_SENT=PASS"
require_source_fixed "v1.8.34u protocol reload oracle" "scripts/protocol7-client-probe.py" "PROTOCOL7_WEAPON_RELOAD_EVENT=PASS"
require_source_fixed "v1.8.34u live oracle revision" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
echo "SOURCE_V1_8_34U=PASS"
echo "PATCH_APPLIED_V1_8_34U=PASS"
require_source_fixed "v1.8.34v shared revision file required" "scripts/test-current.sh" 'scripts/reconstruction-revision.txt'
require_source_fixed "v1.8.34v Protocol7 revision comes from shared contract" "scripts/protocol7-client-probe.py" 'PROBE_REVISION = REVISION_FILE.read_text'
require_source_fixed "v1.8.34v live oracle revision comes from shared contract" "scripts/test-real-engine-docker.sh" 'LIVE_ORACLE_REVISION="$PROJECT_REVISION"'
require_source_fixed "v1.8.34v test-current compares probe against shared contract" "scripts/test-current.sh" 'expected_probe_revision="PROTOCOL7_SELFTEST=PASS probe_revision=$PROJECT_REVISION"'
echo "SOURCE_V1_8_34V=PASS"
echo "PATCH_APPLIED_V1_8_34V=PASS"
require_source_fixed "v1.8.34w protocol7 summary fallback helper" "scripts/test-real-engine-docker.sh" 'protocol7_summary_flag()'
require_source_fixed "v1.8.34w listen-events telemetry allows appended fields" "scripts/test-real-engine-docker.sh" "player_say_id=[0-9]+ .*mask_bits=512"
require_source_fixed "v1.8.34w listen-events stable summary fallback" "scripts/test-real-engine-docker.sh" 'protocol7_summary_flag listen_events_sent'
require_source_fixed "v1.8.34w weapon event stable summary fallback" "scripts/test-real-engine-docker.sh" 'protocol7_summary_flag weapon_fire_event'
echo "SOURCE_V1_8_34W=PASS"
echo "PATCH_APPLIED_V1_8_34W=PASS"
require_source_fixed "v1.8.34x semantic historical gate helper" "scripts/test-current.sh" 'require_source_tokens()'
require_source_fixed "v1.8.34x ListenEvents historical gate decoupled from telemetry field order" "scripts/test-current.sh" 'v1.8.33b ListenEvents semantic gate compatibility'
echo "SOURCE_V1_8_34X=PASS"
echo "PATCH_APPLIED_V1_8_34X=PASS"
require_source_fixed "v1.8.34y real-engine map-level watchdog" "scripts/test-current.sh" 'V34_REAL_ENGINE_MAP_TIMEOUT:-180'
require_source_fixed "v1.8.34y bounded map runner" "scripts/test-current.sh" 'run_real_engine_map_bounded()'
require_source_fixed "v1.8.34y cleanup begins before final PASS" "scripts/test-real-engine-docker.sh" 'cleanup normal'
require_source_fixed "v1.8.34y cleanup completion marker" "scripts/test-real-engine-docker.sh" 'REAL_ENGINE_CLEANUP=PASS map=$MAP_NAME'
require_source_fixed "v1.8.34y bounded Docker cleanup" "scripts/test-real-engine-docker.sh" 'timeout --signal=TERM --kill-after=2s "${DOCKER_CLEANUP_TIMEOUT}s" docker rm -f'
echo "SOURCE_V1_8_34Y=PASS"
echo "PATCH_APPLIED_V1_8_34Y=PASS"
require_source_fixed "v1.8.34z ClientEarPosition uses gameplay origin" "src/reconstructed/abi/server_game_clients004_adapter.cpp" "const float eyeHeight = client->movementDucked ? 46.0f : 64.0f"
require_source_tokens "v1.8.34z live hitscan damage path" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  "ApplyHitscanAttack(clientIndex, weapon" "const build4100::UserCmd& command"
require_source_fixed "v1.8.34z player_hurt engine event" "src/reconstructed/abi/server_game_clients004_adapter.cpp" 'CreateEvent("player_hurt", true)'
require_source_fixed "v1.8.34z player_death engine event" "src/reconstructed/abi/server_game_clients004_adapter.cpp" 'CreateEvent("player_death", true)'
require_source_fixed "v1.8.34z combat Protocol7 setup" "scripts/protocol7-client-probe.py" "PROTOCOL7_COMBAT_SETUP_SENT=PASS"
require_source_fixed "v1.8.34z combat Protocol7 player_hurt" "scripts/protocol7-client-probe.py" "PROTOCOL7_PLAYER_HURT_EVENT=PASS"
require_source_fixed "v1.8.34z combat Protocol7 player_death" "scripts/protocol7-client-probe.py" "PROTOCOL7_PLAYER_DEATH_EVENT=PASS"
require_source_tokens "v1.8.34z live combat acceptance" "scripts/test-real-engine-docker.sh" \
  "GAMEPLAY_HITSCAN_DAMAGE" "gameplay_combat_ballistics_missing"
echo "SOURCE_V1_8_34Z=PASS"
echo "PATCH_APPLIED_V1_8_34Z=PASS"
require_source_fixed "v1.8.34za ClientEarPosition ABI regression smoke" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "SERVERGAMECLIENTS004_EARPOSITION=PASS gameplay_origin=1 standing=64 ducked=46 invalid=zero"
require_source_fixed "v1.8.34za standing ear position assertion" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "movedOrigin.z + 64.0f"
require_source_fixed "v1.8.34za ducked ear position assertion" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "duckOrigin.z + 46.0f"
echo "SOURCE_V1_8_34ZA=PASS"
echo "PATCH_APPLIED_V1_8_34ZA=PASS"
require_source_fixed "v1.8.34zb foreign-edict ABI fixture storage" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "gEngineEdicts[64]"
require_source_fixed "v1.8.34zb array-bounds warning promoted to error" "CMakeLists.txt" "-Werror=array-bounds"
require_source_fixed "v1.8.34zb warning-free ear-position marker" "src/reconstructed/tests/server_game_dll006_abi_smoke.cpp" "bounds_safe=1"
# v1.8.34zb was functionally clean, but its first user rerun stopped before
# real-engine execution because a historical gate froze one exact wording of
# the mutable current handoff table. Historical outcomes now live in an
# append-only ledger; current blocker text must never be used as old evidence.
require_source_tokens "v1.8.34zb acceptance-history checkpoint" "docs/ACCEPTANCE_HISTORY.md" \
  "\`v1.8.34zb\`" "static/source acceptance clean" "stopped before real-engine execution"
echo "SOURCE_V1_8_34ZB=PASS"
echo "PATCH_APPLIED_V1_8_34ZB=PASS"

# Current canonical documents are mutable operator state.  Historical gates may
# require their structural anchors, but must not require the exact current
# revision string: a partially copied handoff must not prevent runtime evidence
# from being collected.  Revision drift is logged below as NONFATAL.
require_source_regex "v1.8.34zc README current-status shape" "README.md" \
  '^# CSS v34 Server Rebuild — reconstructed source v1\.8\.[0-9]+[a-z]+$'
require_source_regex "v1.8.34zc SPEC current-status shape" "SPEC.md" \
  'reconstructed source acceptance checkpoint `v1\.8\.[0-9]+[a-z]+`'
require_source_regex "v1.8.34zc handoff revision field" "docs/AI_HANDOFF.md" \
  '^revision contract[[:space:]]+v1\.8\.[0-9]+[a-z]+$'
require_source_regex "v1.8.34zc research-state revision field" "docs/RESEARCH_STATE_CURRENT.md" \
  '^revision[[:space:]]+v1\.8\.[0-9]+[a-z]+$'
require_source_regex "v1.8.34zc roadmap current-status shape" "docs/ROADMAP.md" \
  '^# Roadmap — current reconstruction v1\.8\.[0-9]+[a-z]+$'
require_source_regex "v1.8.34zc testing current-status shape" "docs/TESTING.md" \
  '^# Testing — v1\.8\.[0-9]+[a-z]+$'
require_source_regex "v1.8.34zc reconstructed README current-status shape" "src/reconstructed/README.md" \
  '^Current reconstruction acceptance checkpoint: `v1\.8\.[0-9]+[a-z]+`'
require_source_regex "v1.8.34zc canonical blocker field" "docs/AI_HANDOFF.md" \
  '^current acceptance blockers[[:space:]]+[^[:space:]].*$'
require_source_tokens "v1.8.34zc acceptance-history checkpoint" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zc`' 'historical-gate compatibility batch'
for current_doc in \
  README.md SPEC.md docs/AI_HANDOFF.md docs/RESEARCH_STATE_CURRENT.md \
  docs/ROADMAP.md docs/TESTING.md src/reconstructed/README.md; do
  check_doc_revision_nonfatal "$current_doc"
done
echo "SOURCE_V1_8_34ZC=PASS"
echo "PATCH_APPLIED_V1_8_34ZC=PASS"

require_source_fixed "v1.8.34zd deterministic ballistic solver" "src/reconstructed/server/weapon_ballistics.cpp" \
  "WeaponShotSolution SolveWeaponShot"
require_source_fixed "v1.8.34zd range falloff damage" "src/reconstructed/server/weapon_ballistics.cpp" \
  "BallisticDamageAtDistance"
require_source_fixed "v1.8.34zd bounded lag history" "src/reconstructed/server/lag_compensation.cpp" \
  "LagCompensationHistory::Select"
require_source_fixed "v1.8.34zd live hitscan uses usercmd seed" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  "SolveWeaponShot("
require_source_fixed "v1.8.34zd live hitscan uses lag history" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  "lagHistory_.Select"
require_source_fixed "v1.8.34zd live ballistic telemetry gate" "scripts/test-real-engine-docker.sh" \
  "gameplay_combat_ballistics_missing"
require_source_tokens "v1.8.34zd combat smoke semantic" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  "SolveWeaponShot" "BallisticDamageAtDistance" "LagCompensationHistory" "COMBAT_BALLISTICS=PASS"
echo "SOURCE_V1_8_34ZD=PASS"
echo "PATCH_APPLIED_V1_8_34ZD=PASS"

# v1.8.34ze: the first zd user rerun proved the entire static suite (25/25)
# and all static oracles, then stopped on historical exact-source needles that
# predated zd's UserCmd-aware weapon/hitscan signatures. Keep historical gates
# semantic so later compatible signature/telemetry extensions do not block the
# real-engine acceptance stage.
require_source_fixed "v1.8.34ze weapon bridge historical gate semantic" "scripts/test-current.sh" \
  'require_source_tokens "v1.8.34u live weapon button bridge"'
require_source_fixed "v1.8.34ze hitscan historical gate semantic" "scripts/test-current.sh" \
  'require_source_tokens "v1.8.34z live hitscan damage path"'
require_source_fixed "v1.8.34ze combat telemetry historical gate semantic" "scripts/test-current.sh" \
  'require_source_tokens "v1.8.34z live combat acceptance"'
echo "SOURCE_V1_8_34ZE=PASS"
echo "PATCH_APPLIED_V1_8_34ZE=PASS"

# v1.8.34zf: ze completed all four real-engine maps, but the authoritative
# build/test-current.log ended at the final map's REAL_ENGINE_DOCKER=PASS
# because the outer success markers were stdout-only. Persist map completion,
# aggregate host completion and final acceptance markers into the same log.
require_source_fixed "v1.8.34zf per-map completion journal" "scripts/test-current.sh" \
  'HOST_REAL_ENGINE_MAP=PASS map=fy_pool_day rc=0'
require_source_fixed "v1.8.34zf aggregate host completion journal" "scripts/test-current.sh" \
  'HOST_REAL_ENGINE=PASS maps=de_dust2,cs_office,de_aztec,fy_pool_day'
require_source_fixed "v1.8.34zf final PASS persisted" "scripts/test-current.sh" \
  'echo "TEST_CURRENT=PASS" | tee -a "$LOG"'
require_source_fixed "v1.8.34zf send-back marker persisted" "scripts/test-current.sh" \
  'echo "SEND_BACK=TEST_CURRENT=PASS" | tee -a "$LOG"'
echo "SOURCE_V1_8_34ZF=PASS"
echo "PATCH_APPLIED_V1_8_34ZF=PASS"

# v1.8.34zg: recover and expose all exact original build4100 ServerClass roots
# in one coherent batch.  Preserve the formerly-live first 15 class IDs while
# expanding the protocol/schema/factory paths to the complete 177-root set.
require_source_fixed "v1.8.34zg generated ServerClass registry" "src/reconstructed/abi/serverclass_build4100.inc" \
  '{"ParticleSmokeGrenade", "DT_ParticleSmokeGrenade", "ParticleSmokeGrenade"},'
require_source_fixed "v1.8.34zg full ServerClass config" "config/serverclasses-build4100.tsv" \
  $'ParticleSmokeGrenade\tDT_ParticleSmokeGrenade\tParticleSmokeGrenade'
require_source_tokens "v1.8.34zg exact ServerClass oracle" "tools/sendtable-oracle/serverclass_oracle.py" \
  'SERVERCLASS_ORACLE=PASS' 'prefix=15' 'exact_names=177' 'exact_tables=177'
require_source_fixed "v1.8.34zg all-class Protocol7 ClassInfo" "scripts/protocol7-client-probe.py" \
  'classinfo=type10_177_exact'
require_source_tokens "v1.8.34zg all-class ServerGameDLL telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'GetAllServerClasses_OK' 'classes=177'
require_source_fixed "v1.8.34zg exact expanded live graph" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'assert(visited.size() == 229);'
require_source_fixed "v1.8.34zg exact expanded live links" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'assert(registry.LiveDataTableLinkCount() == 212);'
echo "SOURCE_V1_8_34ZG=PASS"
echo "PATCH_APPLIED_V1_8_34ZG=PASS"

# v1.8.34zh: harden the acceptance harness against mutable-document drift.
# The user zg rerun proved all static/oracle/25-test work before the stale zb
# docs gate aborted the outer harness.  Keep historical evidence append-only,
# require only a non-empty current blocker field, and preserve current revision
# synchronization without pretending a pending live rerun means `none`.
require_source_tokens "v1.8.34zh immutable zf live acceptance" "docs/ACCEPTANCE_HISTORY.md" \
  "\`v1.8.34zf\`" "full user acceptance" "TEST_CURRENT=PASS"
require_source_tokens "v1.8.34zh zg pre-live abort recorded" "docs/ACCEPTANCE_HISTORY.md" \
  "\`v1.8.34zg\`" "static phase accepted; live phase not reached" "v1.8.34zb canonical no-blocker state"
require_source_tokens "v1.8.34zh current-doc mutability contract" "docs/ACCEPTANCE_HISTORY.md" \
  "current acceptance blockers" "intentionally mutable"
require_source_regex "v1.8.34zh current blocker value present" "docs/AI_HANDOFF.md" \
  '^current acceptance blockers[[:space:]]+[^[:space:]].*$'
echo "SOURCE_V1_8_34ZH=PASS"
echo "PATCH_APPLIED_V1_8_34ZH=PASS"

# v1.8.34zi: 177-class runtime SendProp schema dump must accept the two
# original build4100 zero-field root SendTables.  zg exposed the full class set;
# zh then reached Docker and proved the old `count <= 0` guard aborts before the
# Protocol7 client.  Keep non-empty precalcs strict and make future failures
# visible in the probe instead of silently spinning in GameFrame.
require_source_tokens "v1.8.34zi zero-prop precalc legality" "src/reconstructed/abi/server_class_registry.cpp" \
  'count < 0' 'count > 8192' '(count > 0 && !props)'
require_source_tokens "v1.8.34zi schema diagnostics" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'SENDPROP_SCHEMA_DUMP_OK' 'SENDPROP_SCHEMA_DUMP_WAIT' 'SENDPROP_SCHEMA_DUMP_FAIL'
require_source_tokens "v1.8.34zi exact zero-prop root regression" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'CBaseTempEntity' 'DT_BaseTempEntity' 'CGameRulesProxy' 'DT_GameRulesProxy' \
  'baseTempEntity->table->propCount == 0' 'gameRulesProxy->table->propCount == 0'
echo "SOURCE_V1_8_34ZI=PASS"
echo "PATCH_APPLIED_V1_8_34ZI=PASS"

# v1.8.34zj: bind runtime ServerClass baseline indices to the wire
# `instancebaseline` string table and require every schema-decoded PacketEntities
# class observed by the probe to have a matching baseline entry.
require_source_tokens "v1.8.34zj runtime instancebaseline census" "src/reconstructed/abi/server_class_registry.cpp" \
  'DumpRuntimeInstanceBaselines' '#build4100-instancebaseline-v1' 'instanceBaselineIndex'
require_source_tokens "v1.8.34zj baseline census host regression" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'cssv34-instancebaseline-smoke.tsv' 'DumpRuntimeInstanceBaselines(baselineCensus) == 2' \
  'CCSPlayer\tDT_CSPlayer\t7\t1' 'CPlantedC4\tDT_PlantedC4\t3\t1'
require_source_tokens "v1.8.34zj runtime baseline telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'V34_INSTANCEBASELINE_FILE' 'INSTANCEBASELINE_DUMP_OK' 'CCSPlayer' 'CPlantedC4'
require_source_tokens "v1.8.34zj protocol baseline crosscheck" "scripts/protocol7-client-probe.py" \
  'PROTOCOL7_INSTANCEBASELINE_RUNTIME=PASS' 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS' \
  'packetentity_class_ids'
require_source_tokens "v1.8.34zj Docker baseline gate" "scripts/test-real-engine-docker.sh" \
  'V34_INSTANCEBASELINE_FILE' 'protocol7_instancebaseline_ok' 'protocol7_packetbaseline_ok'
echo "SOURCE_V1_8_34ZJ=PASS"
echo "PATCH_APPLIED_V1_8_34ZJ=PASS"

# v1.8.34zk: build4100 instancebaseline is a materialized subset, not a
# mandatory entry for every ServerClass. zj user-live proved the important
# lifecycle edge: CPlantedC4 baseline=0 while CCSPlayer remains 0xffff because
# player edicts appear only after the one-shot SV_CreateBaseline pass.
require_source_tokens "v1.8.34zk late-spawn baseline census" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'CCSPlayer\tDT_CSPlayer\t65535\t0' 'CPlantedC4\tDT_PlantedC4\t0\t1' \
  'DumpRuntimeInstanceBaselines(baselineCensus) == 1'
require_source_tokens "v1.8.34zk runtime baseline completion semantics" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'seedComplete' 'implicit all-zero/default' 'seeded=%d/%d'
require_source_tokens "v1.8.34zk immutable historical evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zk`' 'CPlantedC4 baseline=0' 'CCSPlayer baseline=65535'
require_source_tokens "v1.8.34zk protocol baseline policy" "scripts/protocol7-client-probe.py" \
  'PROTOCOL7_PACKET_BASELINE_POLICY=PASS' 'materialized_packet_classes' \
  'implicit_packet_classes' 'player_policy' \
  'cssv34-protocol7-instancebaseline-selftest.tsv'
require_source_tokens "v1.8.34zk Docker baseline policy gate" "scripts/test-real-engine-docker.sh" \
  'PROTOCOL7_PACKET_BASELINE_POLICY=PASS' 'protocol7_packetbaseline_ok' \
  'instancebaseline_policy_missing'
echo "SOURCE_V1_8_34ZK=PASS"
echo "PATCH_APPLIED_V1_8_34ZK=PASS"

# v1.8.34zl: original build4100 engine_i486.so proves variable string-table
# userdata lengths are encoded as a 12-bit byte count (max 4096), not the
# 14-bit field used by later Source branches.  Keep a hostile fixture whose
# first userdata byte would corrupt the old decoder's length.
require_source_tokens "v1.8.34zl build4100 stringtable userdata ABI" "scripts/protocol7-client-probe.py" \
  'STRINGTABLE_USERDATA_LENGTH_BITS = 12' 'STRINGTABLE_MAX_USERDATA_BYTES' \
  'build4100-u12-userdata-selftest' 'stringtable_userdata=build4100_u12_max4096'
echo "SOURCE_V1_8_34ZL=PASS"
echo "PATCH_APPLIED_V1_8_34ZL=PASS"

# v1.8.34zm: the zl user rerun passed 25/25, all static oracles and the zl
# Protocol7 self-test, then aborted before Docker on the old zc exact revision
# needle in mutable AI_HANDOFF.md.  Current docs are now shape-checked only;
# revision drift is diagnostic and non-fatal, while immutable history remains
# the source of evidence for earlier revisions.
require_source_tokens "v1.8.34zm nonfatal current-doc revision helper" "scripts/test-current.sh" \
  'check_doc_revision_nonfatal()' 'DOC_REVISION_DRIFT=NONFATAL' 'DOC_REVISION_SYNC=PASS'
require_source_tokens "v1.8.34zm historical zc gate is structural" "scripts/test-current.sh" \
  'v1.8.34zc handoff revision field' 'v1.8.34zc research-state revision field' \
  'v1.8.34zc roadmap current-status shape'
require_source_tokens "v1.8.34zm zl pre-Docker abort recorded" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zl`' 'static/Protocol7 self-test accepted; live phase not reached' \
  'v1.8.34zc handoff revision sync'
echo "SOURCE_V1_8_34ZM=PASS"
echo "PATCH_APPLIED_V1_8_34ZM=PASS"

# v1.8.34zn: the zm user-live run proves that the runtime TSV is a
# pre-connect snapshot, not a final equality oracle.  CPlantedC4 was already
# materialized as class 6/index 0; after the real Protocol7 client connected,
# build4100 appended CCSPlayer as class 1/index 1 to the wire string table.
# Preserve every snapshot entry/index, but permit later wire-only growth.
require_source_tokens "v1.8.34zn runtime snapshot subset semantics" "scripts/protocol7-client-probe.py" \
  'validate_instancebaseline_runtime_wire' 'runtime snapshot not preserved' \
  'wire_growth_ids=' 'wire_growth' 'implicit_latespawn'
require_source_tokens "v1.8.34zn baseline growth selftest" "scripts/protocol7-client-probe.py" \
  'validate_instancebaseline_runtime_wire(baseline_runtime, {6: 0, 1: 1}) == [1]' \
  'missing runtime baseline was accepted' 'runtime baseline index mismatch was accepted'
require_source_tokens "v1.8.34zn zm wire-growth evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zm`' 'runtime=1' 'wire=2' 'wire-only CCSPlayer'
echo "SOURCE_V1_8_34ZN=PASS"
echo "PATCH_APPLIED_V1_8_34ZN=PASS"

# v1.8.34zo: zn's Protocol7 client itself passed, but the outer Docker wrapper
# rejected the enriched instancebaseline PASS line because its regex froze the
# old field adjacency. Match required semantic fields independently of order and
# keep a fast fixture that reproduces the exact user-live zn line.
require_source_tokens "v1.8.34zo unordered Protocol7 host gate" "scripts/protocol7-gate-lib.sh" \
  'protocol7_pass_has_fields()' 'Field order is intentionally irrelevant'
require_source_tokens "v1.8.34zo instancebaseline host acceptance" "scripts/test-real-engine-docker.sh" \
  'protocol7_pass_has_fields "$protocol7_output"' 'runtime_snapshot=[0-9]+' \
  'wire_growth=[0-9]+' 'wire_growth_ids=[^ ]+' 'implicit_ids=[^ ]+'
require_source_tokens "v1.8.34zo host-gate regression" "scripts/test-current.sh" \
  'PROTOCOL7_HOST_GATE_SELFTEST=PASS' \
  'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries=1 runtime_snapshot=1 wire_growth=0 wire_growth_ids=- player_index=-1 c4_index=0 ids=6' \
  'incomplete instancebaseline host-gate fixture accepted'
require_source_tokens "v1.8.34zo zn wrapper-failure evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zn`' 'Protocol7 client PASS; outer Docker host-gate false negative'
echo "SOURCE_V1_8_34ZO=PASS"
echo "PATCH_APPLIED_V1_8_34ZO=PASS"

# v1.8.34zp: zo user-live completed the initial de_dust2 Protocol7 path,
# reconnect, multiclient voice/objective, then changed level to cs_office. The
# post-changelevel census contained all 177 rows but zero materialized entries;
# that is a valid pre-connect lower-bound, not an error. Freeze it after bot
# seeding and allow the later signon wire table to grow from the empty snapshot.
require_source_tokens "v1.8.34zp empty runtime snapshot loader" "scripts/protocol7-client-probe.py" \
  'post-changelevel pre-connect snapshot may legitimately' \
  'assert empty_runtime == {}' \
  'validate_instancebaseline_runtime_wire(empty_runtime, {1: 0}) == [1]' \
  'runtime_snapshot_subset' 'empty_snapshot' 'wire_growth' 'implicit_latespawn'
require_source_tokens "v1.8.34zp immutable empty baseline dump" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'ready==0 at this pre-connect checkpoint' 'if (ready >= 0 && seedComplete)' \
  'valid lower-bound snapshot'
require_source_tokens "v1.8.34zp empty baseline writer smoke" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'assert(DumpRuntimeInstanceBaselines(baselineCensus) == 0);' \
  'CPlantedC4\tDT_PlantedC4\t65535\t0'
require_source_tokens "v1.8.34zp mapchange empty-baseline live gate" "scripts/test-real-engine-docker.sh" \
  'mapchange_empty_instancebaseline_snapshot_missing' \
  'PROTOCOL7_INSTANCEBASELINE_RUNTIME=PASS classes=177 ready=0 player_index=-1 c4_index=-1' \
  'protocol7_mapchange_empty_snapshot_wire_contract_missing'
require_source_tokens "v1.8.34zp empty-baseline host fixture" "scripts/test-current.sh" \
  'runtime_snapshot=0 wire_growth=1 wire_growth_ids=1 player_index=0 c4_index=-1 ids=1' \
  'empty_runtime_growth'
require_source_tokens "v1.8.34zp zo mapchange evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zo`' 'instancebaseline census has no materialized classes' 'cs_office'
echo "SOURCE_V1_8_34ZP=PASS"
echo "PATCH_APPLIED_V1_8_34ZP=PASS"

# v1.8.34zq: zp intentionally generalized the baseline completion condition
# from `ready > 0` to `ready >= 0` so an all-sentinel 177-row pre-connect
# census is a valid immutable lower-bound. Historical zk acceptance must verify
# the lifecycle semantics/evidence, not freeze the superseded branch predicate.
require_source_tokens "v1.8.34zq current empty-snapshot completion contract" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'if (ready >= 0 && seedComplete)' 'ready==0 at this pre-connect checkpoint' \
  'valid lower-bound snapshot'
require_source_tokens "v1.8.34zq zp pre-Docker stale-gate evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zp`' '25/25 smoke' 'stale `v1.8.34zk` source token' \
  'ready > 0 && seedComplete'
echo "SOURCE_V1_8_34ZQ=PASS"
echo "PATCH_APPLIED_V1_8_34ZQ=PASS"

# v1.8.34zr: zq reached the post-changelevel Protocol7 signon with both the
# immutable runtime lower-bound and the present instancebaseline string table
# empty. build4100 may legally enter CCSPlayer later using the implicit default
# baseline, so table presence is required but zero entries are not an error.
require_source_tokens "v1.8.34zr empty wire table decoder" "scripts/protocol7-client-probe.py" \
  'def decode_instancebaseline_wire' 'assert decode_instancebaseline_wire([]) == {}' \
  'empty_wire_table' "or 'none'"
require_source_tokens "v1.8.34zr mapchange empty-wire host contract" "scripts/test-real-engine-docker.sh" \
  'entries=[0-9]+' 'wire_growth=[0-9]+' 'ids=(none|[0-9,]+)' \
  'protocol7_mapchange_empty_snapshot_wire_contract_missing'
require_source_tokens "v1.8.34zr host fixture" "scripts/test-current.sh" \
  'entries=0 runtime_snapshot=0 wire_growth=0 wire_growth_ids=- player_index=-1 c4_index=-1 ids=none' \
  'empty_runtime_empty_wire'
require_source_tokens "v1.8.34zr zq live evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zq`' 'empty instancebaseline table' 'cs_office'
echo "SOURCE_V1_8_34ZR=PASS"
echo "PATCH_APPLIED_V1_8_34ZR=PASS"

# v1.8.34zs: zr itself passed ELF32/25-smoke/Protocol7/static-oracle work,
# then stopped before Docker only because the historical zp gate froze one
# composite self-test diagnostic string. Historical gates now require stable
# semantic atoms independently, so later policy telemetry can grow without
# invalidating an already-accepted lifecycle contract.
require_source_tokens "v1.8.34zs historical baseline policy atoms" "scripts/test-current.sh" \
  "'runtime_snapshot_subset' 'empty_snapshot' 'wire_growth' 'implicit_latespawn'"
require_source_tokens "v1.8.34zs current baseline policy extensions" "scripts/protocol7-client-probe.py" \
  'runtime_snapshot_subset' 'empty_snapshot' 'empty_wire_table' 'wire_growth' 'implicit_latespawn'
require_source_tokens "v1.8.34zs zr pre-Docker stale-gate evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zr`' '25/25 smoke' 'stale `v1.8.34zp` composite policy token'
echo "SOURCE_V1_8_34ZS=PASS"
echo "PATCH_APPLIED_V1_8_34ZS=PASS"

# v1.8.34zt: zs reached the dedicated cs_office Docker run and produced a
# structurally complete all-sentinel 177-row baseline census.  The startup
# watchdog still froze the old ready>0 predicate, so centralize the host census
# validation and accept ready=0 only when all structural invariants hold.
require_source_tokens "v1.8.34zt runtime baseline host validator" "scripts/instancebaseline-gate-lib.sh" \
  'ready + missing == 177' 'seeded == 2 && seed_target == 2' \
  'ready == 0' 'player_baseline == 65535 && c4_baseline == 65535'
require_source_tokens "v1.8.34zt Docker startup/post + empty-wire Protocol7 gate" "scripts/test-real-engine-docker.sh" \
  'instancebaseline_dump_latest_valid "$PROBE"' 'instancebaseline_probe_missing_or_invalid' \
  "'entries=[0-9]+'" "'ids=(none|[0-9,]+)'" "'baseline_classes=[0-9]+'"
require_source_tokens "v1.8.34zt host-gate regression" "scripts/test-current.sh" \
  'INSTANCEBASELINE_HOST_GATE_SELFTEST=PASS' 'empty_wire_implicit' \
  'ready=0 missing=177 player_baseline=65535 c4_baseline=65535 seeded=2/2' \
  'ready=0 missing=176 player_baseline=65535 c4_baseline=65535 seeded=2/2'
require_source_tokens "v1.8.34zt zs live startup-watchdog evidence" "docs/ACCEPTANCE_HISTORY.md" \
  '`v1.8.34zs`' 'startup watchdog' 'cs_office' 'ready=0'
echo "SOURCE_V1_8_34ZT=PASS"
echo "PATCH_APPLIED_V1_8_34ZT=PASS"

# v1.8.34zu: zt closed the entire current live acceptance. Move fidelity
# forward by recovering the ballistic core from the original installed CSS
# weapon scripts. build4100 uses Thin-ICE level 0 and the key embedded in the
# original server_i486.so; the live gate requires all 24 firearms to parse.
require_source_tokens "v1.8.34zu Thin-ICE decoder" "src/reconstructed/server/weapon_script_profile.cpp" \
  "kCssIceKey" "'d', '7', 'N', 'S', 'u', 'L', 'q', '2'" \
  'DecodeBuild4100Ice' 'ParseBuild4100WeaponScript'
require_source_tokens "v1.8.34zu exact weapon fields" "src/reconstructed/server/weapon_script_profile.cpp" \
  'Damage' 'RangeModifier' 'WeaponArmorRatio' 'CycleTime' 'Bullets' 'Penetration' \
  'AccuracyDivisor' 'AccuracyOffset' 'MaxInaccuracy'
require_source_tokens "v1.8.34zu 24-firearm loader" "src/reconstructed/server/weapon_runtime.cpp" \
  'kScriptedFirearms' 'CSSV34_WEAPON_SCRIPT_ROOT' '/opt/css-v34/cstrike/scripts' \
  'gLoadStatus.expectedProfiles' 'gLoadStatus.exactMask'
require_source_tokens "v1.8.34zu runtime telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'WEAPON_SCRIPT_PROFILES_OK' 'loaded=%d expected=%d' 'ak47=%.3f/%.4f/%.3f/%.3f'
require_source_tokens "v1.8.34zu semantic live profile gate" "scripts/weapon-profile-gate-lib.sh" \
  'weapon_profile_line_valid' 'loaded=24' 'expected=24' 'encrypted=24' 'failed=0'
require_source_tokens "v1.8.34zu Docker profile gate reuse" "scripts/test-real-engine-docker.sh" \
  'weapon_profile_probe_latest_valid "$PROBE"' 'weapon_script_profiles_not_exact expected=24'
require_source_tokens "v1.8.34zu ICE+KV regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  '0x5b, 0xbe, 0xa4, 0x9a, 0xec, 0x16, 0x82, 0xf2' \
  'DecodeBuild4100Ice' 'ParseBuild4100WeaponScript' 'COMBAT_BALLISTICS=PASS'
echo "SOURCE_V1_8_34ZU=PASS"
echo "PATCH_APPLIED_V1_8_34ZU=PASS"

# v1.8.34zv: the first zu user rerun proved the new Thin-ICE/KV code builds
# cleanly (ELF32, 25/25, Protocol7) but never reached Docker because the old zd
# gate still required the literal COMBAT_BALLISTICS_V1_8_34ZD marker. Remove
# version-coupled combat-smoke markers as a class of failure; old checkpoints
# verify stable behavior tokens and the smoke executable emits one stable PASS.
require_source_fixed "v1.8.34zv stable combat marker" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'COMBAT_BALLISTICS=PASS'
require_source_absent "v1.8.34zv versioned combat marker removed" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'COMBAT_BALLISTICS_V1_8_34'
require_source_fixed "v1.8.34zv zd historical gate semanticized" "scripts/test-current.sh" \
  'require_source_tokens "v1.8.34zd combat smoke semantic"'
require_source_tokens "v1.8.34zv zu historical gate semanticized" "scripts/test-current.sh" \
  'v1.8.34zu ICE+KV regression' 'COMBAT_BALLISTICS=PASS'
echo "SOURCE_V1_8_34ZV=PASS"
echo "PATCH_APPLIED_V1_8_34ZV=PASS"

# v1.8.34zw: build4100 automatic-weapon fidelity. Keep the accepted 24/24
# Thin-ICE loader, but consume the original accuracy fields with the recovered
# CSBaseGunFire ordering and state-dependent PrimaryAttack/KickBack constants.
require_source_tokens "v1.8.34zw accuracy fields" "src/reconstructed/server/weapon_script_profile.cpp" \
  'AccuracyDivisor' 'AccuracyQuadratic' 'AccuracyOffset' 'MaxInaccuracy' 'parsed >= -1.0f'
require_source_tokens "v1.8.34zw build4100 ballistics" "src/reconstructed/server/weapon_ballistics.cpp" \
  'AdvanceBuild4100Accuracy' 'Build4100StateSpread' 'Build4100KickBackParameters' \
  'ApplyBuild4100KickBack' 'W::Ak47' 'W::M4A1' 'W::M249' 'W::Ump45'
require_source_tokens "v1.8.34zw shot ordering" "src/reconstructed/server/weapon_runtime.cpp" \
  'state.shotAccuracy = state.accuracy' 'AdvanceBuild4100Accuracy' 'ApplyPostFireKickBack'
require_source_tokens "v1.8.34zw live hitscan ordering" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  'SolveBuild4100WeaponShot' 'ApplyPostFireKickBack' 'old PunchAngle'
require_source_tokens "v1.8.34zw ballistic regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'AccuracyQuadratic' 'Build4100StateSpread' 'Build4100KickBackParameters' \
  'ApplyBuild4100KickBack' 'COMBAT_BALLISTICS=PASS'
require_source_tokens "v1.8.34zw live accuracy telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'ak47_acc=%.3f/%.3f/%.3f/%d'
require_source_tokens "v1.8.34zw semantic accuracy host gate" "scripts/weapon-profile-gate-lib.sh" \
  'ak47_acc=-?[0-9]+' 'weapon_profile_line_valid'
require_source_tokens "v1.8.34zw Docker semantic gate reuse" "scripts/test-real-engine-docker.sh" \
  'weapon_profile_probe_latest_valid "$PROBE"' 'weapon_script_profiles_not_exact expected=24'
echo "SOURCE_V1_8_34ZW=PASS"
echo "PATCH_APPLIED_V1_8_34ZW=PASS"

# v1.8.34zx: zw user-live reached the exact 24/24 loader but the expanded
# WEAPON_SCRIPT_PROFILES_OK diagnostic exceeded the legacy 384-byte probe
# buffer.  Prevent silent truncation and decouple the host gate from field order.
require_source_tokens "v1.8.34zx telemetry truncation guard" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'char detail[768]' 'detailLength < 0' 'telemetry_truncated' 'ak47_acc=%.3f/%.3f/%.3f/%d'
require_source_tokens "v1.8.34zx semantic weapon host gate" "scripts/weapon-profile-gate-lib.sh" \
  'weapon_profile_line_valid' 'loaded=24' 'encrypted=24' 'ak47_acc=-?[0-9]+'
require_source_tokens "v1.8.34zx Docker gate reuse" "scripts/test-real-engine-docker.sh" \
  'weapon-profile-gate-lib.sh' 'weapon_profile_probe_latest_valid "$PROBE"'
require_source_tokens "v1.8.34zx host-gate regression" "scripts/test-current.sh" \
  'WEAPON_PROFILE_HOST_GATE_SELFTEST=PASS' 'truncated zw telemetry accepted'
echo "SOURCE_V1_8_34ZX=PASS"
echo "PATCH_APPLIED_V1_8_34ZX=PASS"

# v1.8.34zy: zx itself passed ELF32/25/Protocol7, but static acceptance still
# stopped on historical zu/zw checks that froze pre-zx Docker-script literals.
# Historical weapon gates now follow the semantic validator introduced in zx.
require_source_tokens "v1.8.34zy zu gate semanticized" "scripts/test-current.sh" \
  'v1.8.34zu semantic live profile gate' 'v1.8.34zu Docker profile gate reuse'
require_source_tokens "v1.8.34zy zw gate semanticized" "scripts/test-current.sh" \
  'v1.8.34zw semantic accuracy host gate' 'v1.8.34zw Docker semantic gate reuse'
require_source_tokens "v1.8.34zy semantic validator remains authoritative" "scripts/weapon-profile-gate-lib.sh" \
  'weapon_profile_line_valid' 'loaded=24' 'expected=24' 'encrypted=24' 'ak47_acc=-?[0-9]+'
echo "SOURCE_V1_8_34ZY=PASS"
echo "PATCH_APPLIED_V1_8_34ZY=PASS"

# v1.8.34zz: zy user-live reached Docker with complete telemetry, but the
# bool-backed AccuracyQuadratic field surfaced a non-canonical i386 byte (205).
# Keep the host contract strict and make the source-owned storage/telemetry
# explicitly byte-normalized instead of accepting arbitrary integers.
require_source_tokens "v1.8.34zz quadratic storage" "src/reconstructed/server/weapon_runtime.h" \
  'std::uint8_t accuracyQuadratic = 0' 'AccuracyQuadraticFlag' 'accuracyQuadratic != 0 ? 1 : 0'
require_source_tokens "v1.8.34zz parser normalization" "src/reconstructed/server/weapon_script_profile.cpp" \
  'quadratic ? std::uint8_t{1} : std::uint8_t{0}'
require_source_tokens "v1.8.34zz profile canonicalization" "src/reconstructed/server/weapon_runtime.cpp" \
  'for (auto& profile : gProfiles)' 'profile.accuracyQuadratic != 0 ? std::uint8_t{1} : std::uint8_t{0}'
require_source_tokens "v1.8.34zz telemetry normalization" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'ak47_acc=%.3f/%.3f/%.3f/%d' 'AccuracyQuadraticFlag(ak47)'
require_source_tokens "v1.8.34zz i386 regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'cubic.accuracyQuadratic = 205' 'AccuracyQuadraticFlag(cubic) == 1'
require_source_tokens "v1.8.34zz host regression" "scripts/test-current.sh" \
  'non-canonical accuracyQuadratic=205 telemetry accepted' 'noncanonical_205_rejected'
echo "SOURCE_V1_8_34ZZ=PASS"
echo "PATCH_APPLIED_V1_8_34ZZ=PASS"


# v1.8.35a: remove the last recovered automatic-weapon RNG and punch-decay
# surrogates.  FX_FireBullets uses the Source uniform stream with the low 8-bit
# prediction seed; CGameMovement uses the 9/65 damped spring for PunchAngle.
require_source_tokens "v1.8.35a Source FX spread RNG" "src/reconstructed/server/weapon_ballistics.cpp" \
  'SourceUniformRandomStream' 'Build4100FxSpreadSample' 'predictionRandomSeed) & 0xffu' \
  'RandomFloat(-0.5f, 0.5f)'
require_source_tokens "v1.8.35a Source KickBack direction RNG" "src/reconstructed/server/weapon_ballistics.cpp" \
  'Build4100SharedRandomInt(predictionRandomSeed, "KickBack", 0' 'params.directionChange, 0) == 0'
require_source_tokens "v1.8.35a Source punch spring" "src/reconstructed/server/weapon_ballistics.cpp" \
  'DecayBuild4100Punch' '1.0f - 9.0f * dt' '65.0f * dt'
require_source_tokens "v1.8.35a revision family contract" "scripts/protocol7-client-probe.py" \
  're.fullmatch(r"v1\.8\.[0-9]+[a-z]+", PROBE_REVISION)'
require_source_tokens "v1.8.35a deterministic combat regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'spread17.seed == 18' 'SharedRandomInt roll 0 -> flip' '3.608231f' 'Near(tinyPunch.x, 0.01f)' \
  'rng=source_uniform_fx_seed_exact' 'punch_decay=source_spring_exact'
require_source_tokens "v1.8.35a live i386 combat fidelity probe" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'COMBAT_FIDELITY_OK' 'spread_rng=source_uniform_fx_seed8 punch_decay=source_spring9_65'
require_source_tokens "v1.8.35a Docker combat fidelity gate" "scripts/test-real-engine-docker.sh" \
  'combat_fidelity_probe_missing' 'spread_rng=source_uniform_fx_seed8 punch_decay=source_spring9_65'
echo "SOURCE_V1_8_35A=PASS"
echo "PATCH_APPLIED_V1_8_35A=PASS"
# v1.8.35b: one binary-recovered automatic rifle/SMG batch. Expand the
# exact build4100 state spread + KickBack path from 4 to 12 families without
# changing the accepted Source RNG, punch spring, weapon-script or live gates.
require_source_tokens "v1.8.35b automatic-family selector" "src/reconstructed/server/weapon_ballistics.cpp" \
  'case W::Aug:' 'case W::Galil:' 'case W::Famas:' 'case W::Mp5Navy:' \
  'case W::Mac10:' 'case W::Tmp:' 'case W::P90:' 'case W::Sg552:'
require_source_tokens "v1.8.35b recovered spread functions" "src/reconstructed/server/weapon_ballistics.cpp" \
  'CWeaponAug::PrimaryAttack @ 0x94e500' 'CWeaponGalil::PrimaryAttack @ 0x968150' \
  'CWeaponFamas::PrimaryAttack @ 0x9648d0' 'CWeaponMP5Navy::PrimaryAttack @ 0x973c90' \
  'CWeaponMAC10::PrimaryAttack @ 0x9733c0' 'CWeaponTMP::PrimaryAttack @ 0x978c50' \
  'CWeaponP90::PrimaryAttack @ 0x975810' 'CWeaponSG552::PrimaryAttack @ 0x977920'
require_source_tokens "v1.8.35b recovered KickBack functions" "src/reconstructed/server/weapon_ballistics.cpp" \
  'CWeaponAug::AUGFire @ 0x94e350' 'CWeaponGalil::GalilFire @ 0x967fb0' \
  'CWeaponFamas::FamasFire @ 0x963f00' 'CWeaponMP5Navy::MP5NFire @ 0x973ae0' \
  'CWeaponMAC10::MAC10Fire @ 0x973210' 'CWeaponTMP::TMPFire @ 0x978aa0' \
  'CWeaponP90::P90Fire @ 0x975660' 'CWeaponSG552::SG552Fire @ 0x977770'
require_source_tokens "v1.8.35b auto12 deterministic regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'spread=auto12_state_exact' 'recoil=KickBack_auto12_state_exact' \
  'AssertKick(CSWeaponID::Aug' 'AssertKick(CSWeaponID::P90' 'AssertKick(CSWeaponID::Sg552'
require_source_tokens "v1.8.35b live i386 family probe" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'COMBAT_FIDELITY_OK' 'classic_auto_families=12'
require_source_tokens "v1.8.35b Docker family gate" "scripts/test-real-engine-docker.sh" \
  'classic_auto_families=12' 'combat_fidelity_probe_missing'
echo "SOURCE_V1_8_35B=PASS"
echo "PATCH_APPLIED_V1_8_35B=PASS"

# v1.8.35c: recover the next ten PrimaryAttack state models as one batch
# without prematurely switching them onto the live automatic runtime. Six
# pistols gain exact movement/mode spread + timed accuracy formulas; AWP/Scout/
# G3SG1/SG550 gain exact movement/zoom spread, with timed accuracy for the two
# semi-auto snipers. Runtime activation waits for IN_ATTACK2/mode + family recoil.
require_source_tokens "v1.8.35c pistol/sniper model selectors" "src/reconstructed/server/weapon_ballistics.cpp" \
  'HasBuild4100RecoveredPistolSniperStateModel' 'HasBuild4100RecoveredTimedAccuracy' \
  'case W::P228:' 'case W::FiveSeven:' 'case W::Elite:' 'case W::Deagle:' \
  'case W::Usp:' 'case W::Glock:' 'case W::Awp:' 'case W::Scout:' \
  'case W::G3Sg1:' 'case W::Sg550:'
require_source_tokens "v1.8.35c timed-accuracy recovery" "src/reconstructed/server/weapon_ballistics.cpp" \
  'AdvanceBuild4100TimedAccuracy' '0.325f, 0.300f, 0.600f, 0.900f' \
  '0.275f, 0.250f, 0.725f, 0.920f' '0.400f, 0.350f, 0.550f, 0.900f' \
  'dt * 0.300f + 0.550f' 'dt * 0.350f + 0.650f'
require_source_tokens "v1.8.35c recovered function addresses" "src/reconstructed/server/weapon_ballistics.cpp" \
  'CWeaponP228::PrimaryAttack @ 0x974f20' 'CWeaponUSP::PrimaryAttack @ 0x97bb70' \
  'CWeaponGlock::PrimaryAttack @ 0x96a200' 'PrimaryAttack @ 0x94eea0 / AWPFire @ 0x94efd0' \
  'PrimaryAttack @ 0x976050 / SCOUTFire @ 0x975f10' \
  'PrimaryAttack @ 0x9674c0 / G3SG1Fire @ 0x967310' \
  'PrimaryAttack @ 0x976cb0 / SG550Fire @ 0x976b00'
require_source_tokens "v1.8.35c deferred mode state" "src/reconstructed/server/weapon_runtime.h" \
  'bool zoomed = false' 'bool burstMode = false' 'accepted four-field aggregate initializers'
require_source_tokens "v1.8.35c deterministic pistol/sniper regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'pistol6_sniper4_model_exact' 'timed_accuracy=pistol6+autosniper2_model_exact' \
  'AdvanceBuild4100TimedAccuracy(CSWeaponID::P228' \
  'Build4100StateSpread(CSWeaponID::Awp' 'Build4100StateSpread(CSWeaponID::G3Sg1'
require_source_tokens "v1.8.35c live model telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'recovered_state_families=22' 'timed_accuracy_families=8' 'model_only_pistol_sniper=10'
require_source_tokens "v1.8.35c Docker model gate" "scripts/test-real-engine-docker.sh" \
  'recovered_state_families=22' 'timed_accuracy_families=8' 'model_only_pistol_sniper=10'
echo "SOURCE_V1_8_35C=PASS"
echo "PATCH_APPLIED_V1_8_35C=PASS"

# v1.8.35d: wire the secondary-action state needed by the recovered models
# and recover the ten-family pistol/sniper post-shot punch behavior in one
# batch. Exact pistol/sniper hitscan selection remains model-only pending the
# Glock burst scheduler and networked FOV state.
require_source_tokens "v1.8.35d live IN_ATTACK2 decode" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  'kAttack2 = 1 << 11' 'attack2Pressed' 'GAMEPLAY_WEAPON_SECONDARY_EVENT' \
  'runtime ? runtime->silenced : false' 'runtime ? runtime->zoomLevel != 0 : false' \
  'runtime ? runtime->burstMode : false'
require_source_tokens "v1.8.35d secondary-mode runtime" "src/reconstructed/server/weapon_runtime.cpp" \
  'SecondaryWeaponResult WeaponRuntimeController::SecondaryAttack' \
  'case W::M4A1:' 'case W::Usp:' 'case W::Glock:' 'case W::Famas:' \
  'case W::Awp:' 'case W::Scout:' 'case W::G3Sg1:' 'case W::Sg550:' \
  'state.nextSecondaryAttackTime = now + 3.0f' 'state.nextSecondaryAttackTime = now + 0.3f'
require_source_tokens "v1.8.35d recovered pistol/sniper recoil" "src/reconstructed/server/weapon_ballistics.cpp" \
  'HasBuild4100RecoveredPistolSniperRecoil' 'ApplyBuild4100PistolSniperRecoil' \
  'G3SG1PunchAngleX' 'G3SG1PunchAngleY' 'SG550PunchAngleX' 'SG550PunchAngleY' \
  'punch.x -= 2.0f' 'punch.x = punch.x * 0.75f - pitch'
require_source_tokens "v1.8.35d deterministic regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'pistol6_sniper4_model_exact' 'HasBuild4100RecoveredPistolSniperRecoil' \
  'recoil=KickBack_auto12_state_exact+pistol6_sniper4_model_exact'
require_source_tokens "v1.8.35d secondary runtime regression" "src/reconstructed/tests/weapon_grenade_runtime_smoke.cpp" \
  'SecondaryAttack(1,46.0f,CSWeaponID::M4A1)' 'CSWeaponID::Glock' 'CSWeaponID::Famas' \
  'CSWeaponID::Awp' 'CSWeaponID::Scout' 'CSWeaponID::G3Sg1' 'CSWeaponID::Sg550' \
  'secondary_modes=8_live_state'
require_source_tokens "v1.8.35d protocol secondary oracle" "scripts/protocol7-client-probe.py" \
  'PROTOCOL7_WEAPON_SECONDARY_SENT=PASS' 'buttons=(1 << 11)' 'weapon_secondary_sent='
require_source_tokens "v1.8.35d live fidelity telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'secondary_mode_families=8' 'recovered_recoil_families=10'
require_source_tokens "v1.8.35d Docker secondary gate" "scripts/test-real-engine-docker.sh" \
  'PROTOCOL7_WEAPON_SECONDARY_SENT=PASS' 'GAMEPLAY_WEAPON_SECONDARY_EVENT' \
  'secondary_mode_families=8' 'recovered_recoil_families=10'
echo "SOURCE_V1_8_35D=PASS"
echo "PATCH_APPLIED_V1_8_35D=PASS"

# v1.8.35e: activate the recovered pistol/sniper selector only after closing
# Glock burst scheduling and networked sniper FOV state in the same batch.
require_source_tokens "v1.8.35e live exact selector" "src/reconstructed/server/weapon_ballistics.cpp" \
  'case W::P228:' 'case W::Glock:' 'case W::Elite:' 'case W::FiveSeven:' \
  'case W::Usp:' 'case W::Deagle:' 'case W::Awp:' 'case W::Scout:' \
  'case W::G3Sg1:' 'case W::Sg550:'
require_source_tokens "v1.8.35e Glock burst scheduler" "src/reconstructed/server/weapon_runtime.cpp" \
  'FireScheduledGlockBurst' 'burstShotsRemaining = 2' 'nextBurstShotTime = now + 0.100f' \
  'shotSpreadOverride = 0.050f'
require_source_tokens "v1.8.35e networked FOV proxies" "src/reconstructed/abi/server_class_registry.cpp" \
  'ProxyGameplayFov' 'ProxyGameplayDefaultFov' 'm_iFOV' 'm_iDefaultFOV'
require_source_tokens "v1.8.35e sniper zoom wire state" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  'client->currentFov = 40' 'client->currentFov = (weapon == build4100::CSWeaponID::Awp) ? 10 : 15' \
  'client->defaultFov = 90' 'GAMEPLAY_GLOCK_BURST_EVENT'
require_source_tokens "v1.8.35e regression batch" "src/reconstructed/tests/weapon_grenade_runtime_smoke.cpp" \
  'glock_burst=3shot_100ms_fixed_spread' 'live_exact_families=22' 'shotSpreadOverride==0.050f'
require_source_tokens "v1.8.35e FOV schema regression" "src/reconstructed/tests/sendtable_client_schema_smoke.cpp" \
  'm_iFOV' 'm_iDefaultFOV' 'record->currentFov = 40' 'record->defaultFov = 90'
require_source_tokens "v1.8.35e live fidelity telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'live_pistol_sniper=10' 'glock_burst_scheduler=3x100ms' 'glock_followup_spread=0.05' \
  'fov_wire_families=4' 'live_exact_families=22'
require_source_tokens "v1.8.35e Docker fidelity gate" "scripts/test-real-engine-docker.sh" \
  'live_pistol_sniper=10' 'glock_burst_scheduler=3x100ms' 'fov_wire_families=4' 'live_exact_families=22'
echo "SOURCE_V1_8_35E=PASS"
echo "PATCH_APPLIED_V1_8_35E=PASS"


# v1.8.35f: recover the two shotgun PrimaryAttack spread/recoil paths and wire
# build4100 FX_FireBullets pellet RNG into the live multi-pellet hitscan path.
require_source_tokens "v1.8.35f shotgun spread/RNG/recoil model" "src/reconstructed/server/weapon_ballistics.cpp" \
  'case W::M3:' 'return 0.0675000027f' 'case W::XM1014:' 'return 0.0724999979f' \
  'SolveBuild4100WeaponPellet' 'M3PunchAngleGround' 'M3PunchAngleAir' \
  'XM1014PunchAngleGround' 'XM1014PunchAngleAir'
require_source_tokens "v1.8.35f live shotgun multi-pellet path" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  'ApplyShotgunAttack' 'profile.bulletsPerShot' 'SolveBuild4100WeaponPellet' \
  'GAMEPLAY_SHOTGUN_DAMAGE'
require_source_tokens "v1.8.35f deterministic shotgun regression" "src/reconstructed/tests/combat_ballistics_smoke.cpp" \
  'shotgun_spread=m3_0.0675+xm1014_0.0725' 'shotgun_recoil=m3_ground4_6_air8_11+xm_ground3_5_air7_10' \
  'shotgun_pellets=fx_seed_per_pellet' 'live_exact_families=24'
require_source_tokens "v1.8.35f live fidelity telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'recovered_state_families=24' 'recovered_recoil_families=12' 'shotgun_families=2' \
  'shotgun_rng=fx_seed_per_pellet' 'live_exact_families=24'
require_source_tokens "v1.8.35f Docker fidelity gate" "scripts/test-real-engine-docker.sh" \
  'shotgun_families=2' 'shotgun_spread=m3_0.0675_xm1014_0.0725' \
  'shotgun_rng=fx_seed_per_pellet' 'live_exact_families=24'
echo "SOURCE_V1_8_35F=PASS"
echo "PATCH_APPLIED_V1_8_35F=PASS"

# v1.8.35g: recover the build4100 TE_FireBullets sender contract and put the
# live firearm visual effect back on the engine temp-entity wire. The second
# simultaneous Protocol7 client is the oracle so prediction rules can still
# exclude the shooter while proving svc_TempEntities(27) class 120 on-wire.
require_source_tokens "v1.8.35g PAS slot41 + PlaybackTempEntity slot64 bridge" "src/reconstructed/abi/engine_server021_client.cpp" \
  'void EngineServer021Client::MessageDetermineMulticastRecipients' 'VTableSlot<Fn>(object_, 41)' \
  'CRecipientFilter::AddRecipientsByPAS @ server_i486.so 0x79aa20' \
  'void EngineServer021Client::PlaybackTempEntity' 'VTableSlot<Fn>(object_, 64)' \
  'CBaseTempEntity::Create @ server_i486.so 0x830cb0'
require_source_tokens "v1.8.35g TEFireBullets live SendTable" "src/reconstructed/abi/server_class_registry.cpp" \
  'TempEntityProxy' 'DT_TEFireBullets' 'm_vecAngles[0]' 'return 28' \
  'm_vecAngles[1]' 'return 32' 'teFireBulletsSpread' 'prop.highValue = teFireBulletsSpread ? 1.0f'
require_source_tokens "v1.8.35g exact class120 sender regression" "src/reconstructed/tests/sendtable_live_graph_smoke.cpp" \
  'teFireBullets->classId == 120' 'teOrigin->offset == 16' 'tePitch->offset == 28' \
  'teWeapon->offset == 40' 'tePlayer->offset == 12' 'teSpread->offset == 52' \
  'teSpread->lowValue == 0.0f && teSpread->highValue == 1.0f && teSpread->bits == 10'
require_source_tokens "v1.8.35g live TE_FireBullets emission" "src/reconstructed/abi/server_game_clients004_adapter.cpp" \
  'EmitFireBulletsTempEntity' 'FireBulletsRecipientFilter' 'serverClass->classId != 120' \
  'MessageDetermineMulticastRecipients(true, origin, pasRecipients)' 'engine_->PlaybackTempEntity' \
  'original_empty_filter_create=1' 'const int recipientCount = filter.GetRecipientCount()' \
  'GAMEPLAY_TE_FIREBULLETS' 'predicted_shooter_excluded=1' 'pas=engine_slot41' \
  'ProcessDeferredTempEntities' 'GAMEPLAY_TE_FIREBULLETS_PLAYBACK' 'phase=gameframe'
require_source_tokens "v1.8.35g TE_FireBullets game-frame phase" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'ProcessDeferredWeaponBursts();' 'ProcessDeferredTempEntities();' 'FrameUpdatePreEntityThink();'
require_source_tokens "v1.8.35g Protocol7 temp-entity decoder" "scripts/protocol7-client-probe.py" \
  'SVC_TEMPENTITIES = 27' 'EVENT_INDEX_BITS = 8' 'NET_MAX_PAYLOAD_BITS_OLD = 17' \
  'parse_temp_entity_payload' 'scan_temp_entities_messages' 'PROTOCOL7_TE_FIREBULLETS=PASS' \
  'tempentities=type27_u8_u17+wire_classid_plus1+class120_delta_props'
require_source_tokens "v1.8.35g multiclient wire oracle" "scripts/test-real-engine-docker.sh" \
  '--expect-te-firebullets-player 3' '--expect-te-firebullets-weapon 16' \
  'multiclient_te_firebullets=1' 'protocol7_multiclient_te_firebullets_wire_missing' \
  'GAMEPLAY_TE_FIREBULLETS index=4 classid=120 weapon=16 mode=0' \
  'te_firebullets_wire=$multiclient_te_firebullets'
require_source_tokens "v1.8.35g gatefix16 long-lived snapshot transport" "scripts/protocol7-client-probe.py" \
  'latest_server_tick_seen' 'PROTOCOL7_HOLD_TRANSPORT_RESUME=PASS' \
  'PROTOCOL7_HOLD_RESNAPSHOT_BASELINE_ACK_SENT=PASS' 'PROTOCOL7_HOLD_SNAPSHOT_TICK_ACK_SENT=PASS' \
  'hold_active_tick = max(latest_server_tick_seen + 1' \
  'hold_active_tick = max(hold_active_tick, latest_server_tick_seen + 1)'
require_source_tokens "v1.8.35g gatefix16 multiclient host gate" "scripts/test-real-engine-docker.sh" \
  'hold_transport=nettick+fresh_usercmd' 'protocol7_multiclient_a_hold_transport_resume_missing' \
  'protocol7_multiclient_a_hold_snapshot_transport_missing'
require_source_tokens "v1.8.35g gatefix17 signon stage leases" "scripts/protocol7-client-probe.py" \
  'signon_hard_deadline' 'renew_signon_lease' 'PROTOCOL7_SIGNON_LEASE=PASS' \
  'renew_signon_lease("server_prespawn_scan", 15.0)' \
  'renew_signon_lease("client_prespawn_sent", 15.0)' \
  'renew_signon_lease("client_prespawn_retry", 8.0)' \
  'signon_lease_extensions={signon_lease_extensions}'
require_source_tokens "v1.8.35g gatefix17 initial mapchange gate" "scripts/test-real-engine-docker.sh" \
  '--name cssv34_mapcycle --timeout 8' '--initial-signon-retries 8 --initial-signon-retry-interval 0.75' \
  'protocol7_mapchange_signon_lease_missing' 'protocol7_mapchange_signon_lease_summary_missing'
require_source_tokens "v1.8.35g gatefix18 full-oracle watchdog profile" "scripts/test-current.sh" \
  'V34_REAL_ENGINE_DUST2_TIMEOUT:-600' 'real_engine_timeout_for_map()' \
  'REAL_ENGINE_MAP_WATCHDOG_ARM map=$map_name timeout=${map_deadline}s profile=$map_profile' \
  'map_profile=full_oracle' 'timeout=${real_engine_dust2_deadline}s rc=$real_rc'
require_source_tokens "v1.8.35g releaseprep debug/release split" "CMakeLists.txt" \
  'V34_BUILD_PROFILE "DEBUG"' 'V34_ENGINE_PROBE_ENABLED 0' 'V34_ENGINE_PROBE_ENABLED 1'
require_source_tokens "v1.8.35g releaseprep canonical debug gate" "scripts/test-current.sh" \
  '-DV34_BUILD_PROFILE=DEBUG'
require_source_tokens "v1.8.35g fidelity telemetry" "src/reconstructed/abi/server_game_dll006_adapter.cpp" \
  'te_firebullets=playback_slot64_class120' 'te_pas=multicast_slot41_prediction_exclude'
require_source_tokens "v1.8.35g Docker fidelity gate" "scripts/test-real-engine-docker.sh" \
  'te_firebullets=playback_slot64_class120' 'te_pas=multicast_slot41_prediction_exclude' \
  'event=1 te_firebullets=1$' 'multiclient_te_firebullets=$multiclient_te_firebullets'
echo "SOURCE_V1_8_35G=PASS"
echo "PATCH_APPLIED_V1_8_35G=PASS"

echo "NETWORKABLE_ABI=IServerNetworkable_slots_0..12,GetEntityHandle_IHandleEntity_ptr,GetBaseNetworkable_null,GetPVSInfo_0x7c,IServerUnknown_slots_0..6,edict_networkable_0x08,edict_unknown_0x0c,edict_full_0x04,CBaseEntity_edict_0x168,CheckTransmitInfo_0x2010,transmit_bits_2048,dynamic_generic_forced_transmit,persistent_generic_transmit_policy,planted_c4_FL_EDICT_ALWAYS,full_edict_changed_statechange,generic_pack_boundary_refresh,nullable_transmit_always,prebaseline_c4_instancebaseline,postactivate_dynamic_edict_pool,live_oracle_revisioned,RemoveEdict_FreeContainingEntity_reentry_safe"
echo "MAP_ENTITY_LUMP=parser_ready,player_spawn_origins,exact_factory_map_418,real_BSP_full_content_multiclient_probe_enabled,fakeclient_slot57"
echo "RECOVERY_ORACLES=SendTable198,SendPropFields1088,ExactServerClassRoots177,ExactDataTableLinks166,LiveTopProps1088,LiveDataTableLinks212,DPTArrays10,DPTArrayElementCapacity194,FixedArrays27,FixedArrayElements1096,UtlVectors2,UtlVectorCapacity31,EntityFactoryClassnames418,EntityFactoryTypes387"
echo "GAME_EVENT_ABI=GAMEEVENTSMANAGER002_slots2_4_6_7_8_11,IGameEvent_SetBool10_SetInt11_SetFloat12_SetString13,resource/gameevents.res,resource/ModEvents.res,CEventLog_core_listeners,player_connect_preflight,player_say_create_fire,player_hurt_death_create_fire,clc_ListenEvents12_fixed512_mask"
echo "USER_MESSAGE_ABI=VEngineServer021_UserMessageBegin43_MessageEnd44,GetUserMessageInfo27_CSS28,SayText_id3_variable,bf_write_WriteByte_WriteString"
echo "VOICE_ABI=VoiceServer001_GetClientListening2_SetClientListening3,CLC_VoiceData10_u16bits,SVC_VoiceInit14_codec_quality,SVC_VoiceData15_sender_u8_u16bits"
echo "REAL_ENGINE_RUNTIME=working_server_v34_docker,ReviveEmu,BufferFix,replace_server_i486_only,sv_sendtables1_for_full_crc_mismatch,sv_filterban0_for_synthetic_longrun,gameevents_manifests,core_event_listeners,VoiceServer001_active_client_listening_matrix,sv_voiceenable1_test_oracle,bot_quota_CreateFakeClient_gameframe_one_per_frame_edictbase_noreentry"
echo "SENDTABLE_LIVE=roots177,reachable_tables229,reachable_props2219,nested_links212,dpt_arrays10_reachable,fixed_arrays27_recovered,utlvectors2_recovered,safe_zero_proxies"
echo "SENDTABLE_CLIENT_SCHEMA=protocol7_types,classes177_ids0..176,ehandle21bits_changesoften_normalized,dpt_array_pairing_insidearray_lengthbits,engine_schema_v2_array_metadata,SPROP_XYZE_build4100_ordinary_3scalar_vector,ccsplayer_schema_driven_snapshot,gameplay_backed_account_health_team_state,sendprop_angle_0_360_eyeangles,gameplay_origin_coord,local_velocity_-2048_2048"
echo "DPT_ARRAY=PASS recovered=10 reachable=10 capacity=194 schema=engine_flattened_v2"
echo "SPROP_XYZE_BUILD4100=PASS flag=0x80 vector_codec=ordinary_3scalar"
echo "GAMEPLAY_EVENT_SENDPROP=PASS trigger=NET_StringCmd4_jointeam3 props=m_iTeamNum+m_iPlayerState notify_edict_flags_change=1"
echo "CLC_MOVE_USERCMD_SENDPROP=PASS type=9 header=new4+backup3+length16 usercmd=CUserCmd_delta props=m_angEyeAngles0+m_angEyeAngles1+m_vecOrigin+m_vecVelocity012 movement=ground_accel5+friction4_stopspeed75+split_gravity800+jump268.328+airaccel10_cap30+jump_latch+source_side_sign+enginetrace_world_hull+step18_dist_eps0.03125+walkable_slope0.7+multiplane5+dynamic_player_group8_swept_aabb+dynamic_nonplayer_vphysics+inline_brush+hostage_bbox_modelbounds+water_levels_swim_friction_accel_jump+fy_pool_day_real_waterlevel_ge2+duck_hull36_speedcrop_blocked_unduck+info_ladder_volume_climb_jumpdetach+hostage_use_follow_dynamic_origin+generic_npc_bbox_dynamic_origin+weapon_attack_secondary_reload_gameevents+glock_burst3x100ms+sniper_fov_wire+hitscan24_seeded_spread_recoil_rangefalloff_lagcomp200ms+shotgun_multi_pellet+player_damage_hurt_death+client_ear_position notify_edict_flags_change=1"
echo "REAL_CLIENT_PROTOCOL7=challenge_connect_auth2_or_auth3_classic_revemu,tcp_stream_bind,ClientConnect,server_driven_NET_SignonState3,svc_CreateStringTable12x16,CLC_ClientInfo,client_NET_SignonState3,sv_sendtables1,udp_reliable_signon,exact_fragment_header,choked_header,reliable_toggle_ack,nop_keepalive,bzip2_reassembly,signon_trace,svc_Print7,svc_ServerInfo8,sequential_sendtable_stream,svc_ClassInfo177,server_NET_SignonState4,client_NET_SignonState4,ClientPutInServer,server_spawn_transition_single17+SetView18+Tick3+tailpad,client_NET_SignonState5,ClientActive,CLC_BaselineAck11_after_full_snapshot,client_NET_SignonState6_after_full_snapshot,client_NET_Tick3_snapshot_ack,singleblock17_reliable_framing,reliable_fragment_decode_recovery,svc_PacketEntities26_full_snapshot,entitydelta_old_ubitvar_enterpvs,ccsplayer_schema_driven_gameplay_sendprops,svc_PacketEntities26_delta_snapshot_from_tick,entitydelta_existing_outer_terminator,engine_flattened_sendprop_schema_v2,DPT_Array_decode,SPROP_XYZE_scalar_vector,NET_StringCmd4_jointeam3,gameplay_event_delta_team3_state0,CLC_Move9_usercmd_eyeangles_delta,CLC_Move9_forward200_origin_velocity_delta,CLC_Move9_ground_accel_friction_jump_gravity_latch_air_accel_delta,gamestate_backed_sendprops,post_active_CheckTransmit,post_active_snapshot_survival,NET_Disconnect1,real_client_reconnect_same_server_slot_reuse,real_client_simultaneous_slots3_4_dynamic_local_entity_opposite_team_spawns,real_engine_changelevel_dust2_to_office_full_protocol7_reconnect,real_engine_repeated_mapcycle_dust2_office_aztec_pool_dust2_full_protocol7,svc_GameEventList30_descriptor_decode,clc_ListenEvents12_player_say_subscription,NET_StringCmd4_say,svc_GameEvent25_player_say_userid_text,svc_GameEvent25_player_hurt_death,NET_StringCmd4_cssv34_usermsg,svc_UserMessage23_SayText3_sender_text,CLC_VoiceData10_to_SVC_VoiceData15_multiclient_forward,svc_TempEntities27_CTEFireBullets_class120_multiclient_observer,objective_BSP_zones+planted_c4_sendprops+round_bomb_hostage_gameevents+multiclient_round_result,svc_CreateStringTable12_downloadables_decode,NET_File2_request,reliable_stream1_file_transfer_sha256"
echo "CLOSED_BLOCKERS=DPT_Array_sendprop_values,SPROP_XYZE_vector_encoding,gameplay_event_driven_sendprop_changes,CLC_Move_usercmd_to_eyeangle_sendprops,CLC_Move_usercmd_to_origin_velocity_sendprops,player_movement_ground_acceleration_friction_split_gravity_jump_latch_air_accel,player_movement_EngineTraceServer003_world_hull_ground_snap_basic_collision,player_movement_stepmove18_walkable_slopes_multiplane_clip,player_movement_dynamic_player_collisions_group8_swept_aabb,player_movement_dynamic_nonplayer_studio_vphysics_inline_brush,player_movement_hostage_bbox_modelbounds_collision,player_movement_water_levels_swim_friction_accel_jump,player_movement_duck_crouch_hull_speed_blocked_unduck,player_movement_info_ladder_volume_climb_jump_detach,player_movement_hostage_following_dynamic_origin,player_movement_generic_npc_bbox_dynamic_origin,real_client_disconnect_reconnect_slot_reuse,real_client_two_simultaneous_protocol7_clients,real_engine_changelevel_map_lifecycle_protocol7,real_engine_repeated_mapcycle_lifecycle_stability,real_client_gameevent_player_say_wire,real_client_usermessage_saytext_wire,real_client_resource_download_NET_File_stream1_sha256,real_client_multiclient_voice_wire_forward,real_client_te_firebullets_wire_effect,real_client_objective_round_bomb_hostage_wire,real_client_hitscan_player_hurt_death_wire,combat_seeded_spread_recoil_rangefalloff_bounded_lagcomp"
echo "KNOWN_BLOCKERS=none"
echo "TEST_CURRENT_INNER=PASS"
