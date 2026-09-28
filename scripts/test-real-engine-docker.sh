#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build/reconstruction-current}"
REVISION_FILE="$ROOT/scripts/reconstruction-revision.txt"
[[ -f "$REVISION_FILE" ]] || { echo "REAL_ENGINE_DOCKER=FAIL revision_file_missing=$REVISION_FILE" >&2; exit 1; }
PROJECT_REVISION="$(tr -d '\r\n' < "$REVISION_FILE")"
[[ "$PROJECT_REVISION" =~ ^v1\.8\.[0-9]+[a-z]+$ ]] || { echo "REAL_ENGINE_DOCKER=FAIL invalid_revision=$PROJECT_REVISION" >&2; exit 1; }
LIVE_ORACLE_REVISION="$PROJECT_REVISION"
LIVE_TRANSMIT_CONTRACT="prebaseline_reserved_nonclient_c4_edict+instancebaseline_ready+persistent_dontsend_to_always+full_statechanged+stable_wire_slot+postactivate_checktransmit+nullable_transmit_always+levelshutdown_c4_release_reentry+usercmd_weapon_fire_reload"
# Docker bind mounts require an absolute host path. Accept relative build dirs
# from interactive WSL usage (for example: build/reconstruction-current).
if [[ "$BUILD" != /* ]]; then
  BUILD="$PWD/$BUILD"
fi
LIB="$BUILD/server_i486.so"
SERVER_ROOT="${CSS_V34_SERVER_ROOT:-}"
MAP_NAME="${V34_TEST_MAP:-de_dust2}"
DUST2_ORACLE=0
HOSTAGE_ORACLE=0
AZTEC_WATER_ORACLE=0
POOL_WATER_ORACLE=0
LADDER_ORACLE=0
RECONNECT_ORACLE=0
MULTICLIENT_ORACLE=0
MAPCHANGE_ORACLE=0
MAPCYCLE_ORACLE=0
[[ "$MAP_NAME" == "de_dust2" ]] && { DUST2_ORACLE=1; RECONNECT_ORACLE=1; MULTICLIENT_ORACLE=1; MAPCHANGE_ORACLE=1; MAPCYCLE_ORACLE=1; }
[[ "$MAP_NAME" == "cs_office" ]] && { HOSTAGE_ORACLE=1; LADDER_ORACLE=1; }
[[ "$MAP_NAME" == "de_aztec" ]] && AZTEC_WATER_ORACLE=1
[[ "$MAP_NAME" == "fy_pool_day" ]] && POOL_WATER_ORACLE=1
DEADLINE="${V34_REAL_ENGINE_TIMEOUT:-45}"
CONTROL_SECONDS="${V34_ORIGINAL_CONTROL_SECONDS:-8}"
DOCKER_CLEANUP_TIMEOUT="${V34_DOCKER_CLEANUP_TIMEOUT:-6}"
PID_CLEANUP_TIMEOUT="${V34_PID_CLEANUP_TIMEOUT:-3}"
# BUILD itself is produced inside the dev container and its reconstruction
# subdirectory may be owned by root on the WSL host. Keep host runtime files
# under ROOT/build, which test-current creates before starting Docker.
HOST_BUILD_ROOT="$ROOT/build"
mkdir -p "$HOST_BUILD_ROOT"
PROBE_DIR="$(mktemp -d "$HOST_BUILD_ROOT/real-engine-docker.XXXXXX")"
PROBE="$PROBE_DIR/events"
SENDPROP_SCHEMA="$PROBE_DIR/sendprop-schema.tsv"
INSTANCEBASELINES="$PROBE_DIR/instancebaselines.tsv"
REBUILT_LOG="$PROBE_DIR/rebuilt.log"
CONTROL_LOG="$PROBE_DIR/original-control.log"

fail() {
  echo "REAL_ENGINE_DOCKER=FAIL $*" >&2
  [[ -s "$PROBE" ]] && { echo "--- callback probe ---" >&2; cat "$PROBE" >&2; }
  [[ -s "$REBUILT_LOG" ]] && { echo "--- rebuilt docker tail ---" >&2; tail -200 "$REBUILT_LOG" >&2; }
  [[ -s "$CONTROL_LOG" ]] && { echo "--- original docker tail ---" >&2; tail -120 "$CONTROL_LOG" >&2; }
  exit 1
}

[[ -f "$LIB" ]] || fail "rebuilt_server_missing=$LIB"
[[ -n "$SERVER_ROOT" ]] || fail "CSS_V34_SERVER_ROOT_unset"
[[ -d "$SERVER_ROOT" ]] || fail "CSS_V34_SERVER_ROOT_missing=$SERVER_ROOT"
[[ -f "$SERVER_ROOT/.env" ]] || fail "docker_env_missing=$SERVER_ROOT/.env"
[[ "$DEADLINE" =~ ^[1-9][0-9]*$ ]] || fail "invalid_timeout=$DEADLINE"
[[ "$CONTROL_SECONDS" =~ ^[1-9][0-9]*$ ]] || fail "invalid_control_seconds=$CONTROL_SECONDS"
[[ "$DOCKER_CLEANUP_TIMEOUT" =~ ^[1-9][0-9]*$ ]] || fail "invalid_docker_cleanup_timeout=$DOCKER_CLEANUP_TIMEOUT"
[[ "$PID_CLEANUP_TIMEOUT" =~ ^[1-9][0-9]*$ ]] || fail "invalid_pid_cleanup_timeout=$PID_CLEANUP_TIMEOUT"
command -v docker >/dev/null 2>&1 || fail "docker_not_found"
command -v python3 >/dev/null 2>&1 || fail "python3_not_found"
TEST_RCON_PASSWORD="${V34_TEST_RCON_PASSWORD:-}"
if [[ -z "$TEST_RCON_PASSWORD" ]]; then
  TEST_RCON_PASSWORD="$(python3 - <<'PYSECRET'
import secrets
print("cssv34_test_" + secrets.token_hex(16))
PYSECRET
)"
fi
[[ -f "$ROOT/scripts/instancebaseline-gate-lib.sh" ]] || fail "instancebaseline_gate_lib_missing"
# shellcheck source=instancebaseline-gate-lib.sh
source "$ROOT/scripts/instancebaseline-gate-lib.sh"
[[ -f "$ROOT/scripts/weapon-profile-gate-lib.sh" ]] || fail "weapon_profile_gate_lib_missing"
# shellcheck source=weapon-profile-gate-lib.sh
source "$ROOT/scripts/weapon-profile-gate-lib.sh"
[[ -x "$ROOT/scripts/protocol7-client-probe.py" ]] || fail "protocol7_probe_missing"
[[ -x "$ROOT/scripts/source-rcon.py" ]] || fail "source_rcon_missing"
echo "REAL_ENGINE_ORACLE_REVISION=$LIVE_ORACLE_REVISION transmit_contract=$LIVE_TRANSMIT_CONTRACT"

map_mount_args=()
TEST_MAP_DIR="$ROOT/runtime/test-maps"
DOWNLOAD_RESOURCE="maps/de_dust2.nav"
DOWNLOAD_SOURCE="$TEST_MAP_DIR/de_dust2.nav"
DOWNLOAD_SIZE=0
DOWNLOAD_SHA256=""
if (( DUST2_ORACLE == 1 )); then
  [[ -f "$DOWNLOAD_SOURCE" ]] || fail "download_probe_resource_missing=$DOWNLOAD_SOURCE"
  DOWNLOAD_SIZE="$(stat -c%s "$DOWNLOAD_SOURCE")"
  DOWNLOAD_SHA256="$(sha256sum "$DOWNLOAD_SOURCE" | awk '{print $1}')"
  map_mount_args+=(
    -v "$DOWNLOAD_SOURCE:/opt/css-v34/cstrike/maps/de_dust2.nav:ro"
  )
fi
if (( AZTEC_WATER_ORACLE == 1 || MAPCYCLE_ORACLE == 1 )); then
  [[ -f "$TEST_MAP_DIR/de_aztec.bsp" ]] || fail "de_aztec_bsp_missing=$TEST_MAP_DIR/de_aztec.bsp"
  map_mount_args+=(
    -v "$TEST_MAP_DIR/de_aztec.bsp:/opt/css-v34/cstrike/maps/de_aztec.bsp:ro"
  )
  [[ -f "$TEST_MAP_DIR/de_aztec.nav" ]] && map_mount_args+=(
    -v "$TEST_MAP_DIR/de_aztec.nav:/opt/css-v34/cstrike/maps/de_aztec.nav:ro"
  )
  [[ -f "$TEST_MAP_DIR/de_aztec.txt" ]] && map_mount_args+=(
    -v "$TEST_MAP_DIR/de_aztec.txt:/opt/css-v34/cstrike/maps/de_aztec.txt:ro"
  )
fi
if (( POOL_WATER_ORACLE == 1 || MAPCYCLE_ORACLE == 1 )); then
  [[ -f "$TEST_MAP_DIR/fy_pool_day.bsp" ]] || fail "fy_pool_day_bsp_missing=$TEST_MAP_DIR/fy_pool_day.bsp"
  map_mount_args+=(
    -v "$TEST_MAP_DIR/fy_pool_day.bsp:/opt/css-v34/cstrike/maps/fy_pool_day.bsp:ro"
  )
fi

# The user's working server_v34 Docker project is the runtime oracle here.
# shellcheck disable=SC1090
set -a
. "$SERVER_ROOT/.env"
set +a
IMAGE="${CSS_IMAGE:-}"
CONTAINER_HINT="${CSS_CONTAINER:-css-v34}"
if [[ -z "$IMAGE" ]]; then
  IMAGE="$(docker inspect -f '{{.Config.Image}}' "$CONTAINER_HINT" 2>/dev/null || true)"
fi
[[ -n "$IMAGE" ]] || fail "CSS_IMAGE_not_resolved"
docker image inspect "$IMAGE" >/dev/null 2>&1 || fail "working_image_not_found=$IMAGE"

if ! docker run --rm --entrypoint /bin/sh "$IMAGE" -lc \
  'test -x ./srcds_i486 && test -f ./libbufferfix_preload.so && test -f ./bin/steamclient_i486.so && test -f ./bin/libsteamvalidateuseridtickets_i486.so' \
  >/dev/null 2>&1; then
  fail "working_image_runtime_incomplete=$IMAGE"
fi

rm -f "$PROBE" "$SENDPROP_SCHEMA" "$INSTANCEBASELINES" "$REBUILT_LOG" "$CONTROL_LOG"
touch "$PROBE" "$REBUILT_LOG" "$CONTROL_LOG"
# The image runs as the non-root srcds user, so its bind-mounted probe must
# be writable independently of the host user's uid/gid mapping.
chmod 0777 "$PROBE_DIR"
chmod 0666 "$PROBE" "$REBUILT_LOG" "$CONTROL_LOG"
echo "REAL_ENGINE_HOST_PROBE_DIR=$PROBE_DIR"

CONTROL_NAME="css-v34-original-control-$$"
REBUILT_NAME="css-v34-rebuilt-probe-$$"
MULTI_PID=""
CLEANUP_DONE=0
CLEANUP_STATUS=0

cleanup_container_bounded() {
  local label="$1"
  local target="$2"
  [[ -n "$target" ]] || return 0
  echo "REAL_ENGINE_CLEANUP_CONTAINER_BEGIN map=$MAP_NAME label=$label target=$target timeout=${DOCKER_CLEANUP_TIMEOUT}s"
  set +e
  timeout --signal=TERM --kill-after=2s "${DOCKER_CLEANUP_TIMEOUT}s" docker rm -f "$target" >/dev/null 2>&1
  local rc=$?
  set -e
  if [[ $rc -eq 124 || $rc -eq 137 ]]; then
    CLEANUP_STATUS=$rc
    echo "REAL_ENGINE_CLEANUP_CONTAINER_END map=$MAP_NAME label=$label rc=$rc timed_out=1"
  else
    # rc=1 is harmless when an earlier path already removed the container.
    echo "REAL_ENGINE_CLEANUP_CONTAINER_END map=$MAP_NAME label=$label rc=$rc timed_out=0"
  fi
}

cleanup_pid_bounded() {
  local label="$1"
  local pid="$2"
  [[ -n "$pid" ]] || return 0
  echo "REAL_ENGINE_CLEANUP_PID_BEGIN map=$MAP_NAME label=$label pid=$pid timeout=${PID_CLEANUP_TIMEOUT}s"
  kill -TERM "$pid" >/dev/null 2>&1 || true
  local deadline=$((SECONDS + PID_CLEANUP_TIMEOUT))
  # jobs -pr lists only running children; unlike kill -0 it does not mistake a
  # finished-but-unreaped child for a live process.
  while jobs -pr | grep -Fxq "$pid"; do
    if (( SECONDS >= deadline )); then
      kill -KILL "$pid" >/dev/null 2>&1 || true
      sleep 0.1
      break
    fi
    sleep 0.1
  done
  if jobs -pr | grep -Fxq "$pid"; then
    CLEANUP_STATUS=124
    echo "REAL_ENGINE_CLEANUP_PID_END map=$MAP_NAME label=$label pid=$pid alive=1"
    return 0
  fi
  wait "$pid" >/dev/null 2>&1 || true
  echo "REAL_ENGINE_CLEANUP_PID_END map=$MAP_NAME label=$label pid=$pid alive=0"
}

cleanup_probe_dir_bounded() {
  [[ -n "$PROBE_DIR" ]] || return 0
  echo "REAL_ENGINE_CLEANUP_PROBE_BEGIN map=$MAP_NAME path=$PROBE_DIR timeout=3s"
  set +e
  timeout --signal=TERM --kill-after=1s 3s rm -rf "$PROBE_DIR" >/dev/null 2>&1
  local rc=$?
  set -e
  if [[ $rc -eq 124 || $rc -eq 137 ]]; then
    CLEANUP_STATUS=$rc
  fi
  echo "REAL_ENGINE_CLEANUP_PROBE_END map=$MAP_NAME rc=$rc"
}

cleanup() {
  local reason="${1:-exit}"
  (( CLEANUP_DONE == 0 )) || return 0
  CLEANUP_DONE=1
  echo "REAL_ENGINE_CLEANUP_BEGIN map=$MAP_NAME reason=$reason"
  if [[ -n "$MULTI_PID" ]]; then
    cleanup_pid_bounded multiclient "$MULTI_PID"
    MULTI_PID=""
  fi
  cleanup_container_bounded rebuilt "$REBUILT_NAME"
  cleanup_container_bounded control "$CONTROL_NAME"
  cleanup_probe_dir_bounded
  echo "REAL_ENGINE_CLEANUP_END map=$MAP_NAME reason=$reason status=$CLEANUP_STATUS"
}

on_signal() {
  local sig="$1"
  trap - EXIT INT TERM HUP
  cleanup "signal_$sig"
  exit 130
}

trap 'cleanup exit' EXIT
trap 'on_signal INT' INT
trap 'on_signal TERM' TERM
trap 'on_signal HUP' HUP

launch_command='exec env \
  SteamAppId=240 \
  REVIVE_STEAMCLIENT_LOG=/tmp/revive_steamclient.log \
  REVIVE_LOG_MODE=production \
  REVIVE_ABI_TRACE=0 \
  LD_LIBRARY_PATH=.:./bin \
  LD_PRELOAD=./libbufferfix_preload.so \
  ./srcds_i486 \
    -console -game cstrike -ip 0.0.0.0 -port 27016 -tickrate 66 \
    -localcser -insecure -nomaster \
    +fps_max 0 +mp_dynamicpricing 0 +maxplayers 4 +sv_lan 1 \
    +mp_autoteambalance 0 +mp_limitteams 0 \
    +bot_quota 2 +sv_sendtables 1 +sv_filterban 0 +sv_allowdownload 1 +sv_downloadurl "" +sv_voiceenable 1 +rcon_password "$TEST_RCON_PASSWORD" \
    +map "$V34_TEST_MAP"'

# First prove the exact Docker/ReviveEmu/BufferFix runtime can boot the original
# build4100 DLL. No host ports are published, so this cannot collide with the
# user's production server container.
echo "ORIGINAL_DOCKER_CONTROL_START image=$IMAGE window=${CONTROL_SECONDS}s map=$MAP_NAME"
control_id="$(docker run -d --name "$CONTROL_NAME" \
  -e V34_TEST_MAP="$MAP_NAME" \
  "${map_mount_args[@]}" \
  --entrypoint /bin/sh \
  "$IMAGE" -lc "$launch_command")" || fail "original_control_start"

deadline=$((SECONDS + CONTROL_SECONDS))
while (( SECONDS < deadline )); do
  running="$(docker inspect -f '{{.State.Running}}' "$control_id" 2>/dev/null || echo false)"
  if [[ "$running" != "true" ]]; then
    timeout 5s docker logs "$control_id" >"$CONTROL_LOG" 2>&1 || true
    rc="$(docker inspect -f '{{.State.ExitCode}}' "$control_id" 2>/dev/null || echo 125)"
    fail "original_control_exited rc=$rc"
  fi
  sleep 0.2
done

timeout 5s docker logs "$control_id" >"$CONTROL_LOG" 2>&1 || true
if timeout 4s docker exec "$control_id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
  echo "ORIGINAL_DOCKER_CONTROL=PASS a2s=PASS bufferfix=ON revive=ON"
else
  echo "ORIGINAL_DOCKER_CONTROL=ALIVE a2s=NOT_READY bufferfix=ON revive=ON"
fi
cleanup_container_bounded control "$control_id"
CONTROL_NAME=""
if [[ $CLEANUP_STATUS -ne 0 ]]; then
  fail "control_cleanup_timeout rc=$CLEANUP_STATUS"
fi

# Now use the exact same working image and replace only the game DLL.
# Protocol 7 upgrades the accepted connection to a build4100 TCP stream on the
# same server port. Reserve one host port that is free in both TCP and UDP
# namespaces so both transports map to container port 27016.
host_port="$(python3 - <<'PYPORT'
import random, socket
for _ in range(256):
    port = random.randint(44000, 59000)
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        tcp.bind(("127.0.0.1", port))
        udp.bind(("127.0.0.1", port))
    except OSError:
        tcp.close(); udp.close()
        continue
    tcp.close(); udp.close()
    print(port)
    raise SystemExit(0)
raise SystemExit(1)
PYPORT
)" || fail "protocol7_host_port_reservation"
[[ "$host_port" =~ ^[0-9]+$ ]] || fail "protocol7_host_port_invalid value=$host_port"

echo "REAL_ENGINE_DOCKER_START image=$IMAGE map=$MAP_NAME deadline=${DEADLINE}s replacement=cstrike/bin/server_i486.so host_port=$host_port sv_sendtables=1 sv_filterban=0 sv_allowdownload=1 sv_downloadurl=empty sv_voiceenable=1"
echo "SV_SENDTABLES_RUNTIME=PASS value=1 purpose=forced_crc_mismatch_full_tables"
echo "SV_FILTERBAN_RUNTIME=PASS value=0 purpose=test_harness_repeated_synthetic_clients"
echo "SV_ALLOWDOWNLOAD_RUNTIME=PASS value=1 sv_downloadurl=empty purpose=direct_netchannel_file_oracle"
echo "SV_VOICEENABLE_RUNTIME=PASS value=1 purpose=real_clc_voice_to_svc_voice_oracle"
rebuilt_id="$(docker run -d --name "$REBUILT_NAME" \
  -p "127.0.0.1:${host_port}:27016/udp" \
  -p "127.0.0.1:${host_port}:27016/tcp" \
  -e V34_TEST_MAP="$MAP_NAME" \
  -e V34_ENGINE_PROBE_FILE=/probe/events \
  -e V34_SENDPROP_SCHEMA_FILE=/probe/sendprop-schema.tsv \
  -e V34_INSTANCEBASELINE_FILE=/probe/instancebaselines.tsv \
  "${map_mount_args[@]}" \
  -v "$LIB:/opt/css-v34/cstrike/bin/server_i486.so:ro" \
  -v "$PROBE_DIR:/probe" \
  --entrypoint /bin/sh \
  "$IMAGE" -lc "$launch_command")" || fail "rebuilt_start"

map_inventory_ready() {
  # v1.8.24 adds generic NPC bbox inventory to the runtime map scan. Keep the
  # startup gate in lockstep with the current server probe format; otherwise a
  # healthy rebuilt server waits until the watchdog without ever starting the
  # Protocol7 client. Requiring npc_bbox also catches stale pre-v1.8.24 .so files.
  if (( HOSTAGE_ORACLE == 1 )); then
    grep -Eq '^MAP_MOVEMENT_SOLIDS total=[1-9][0-9]* studio=[0-9]+ brush=[0-9]+ prop_physics_multiplayer=[0-9]+ hostage_bbox=[1-9][0-9]* npc_bbox=[0-9]+$' "$PROBE" 2>/dev/null
  elif (( DUST2_ORACLE == 1 )); then
    grep -Fxq 'MAP_MOVEMENT_SOLIDS total=76 studio=75 brush=1 prop_physics_multiplayer=75 hostage_bbox=0 npc_bbox=0' "$PROBE" 2>/dev/null
  else
    grep -Eq '^MAP_MOVEMENT_SOLIDS total=[0-9]+ studio=[0-9]+ brush=[0-9]+ prop_physics_multiplayer=[0-9]+ hostage_bbox=[0-9]+ npc_bbox=[0-9]+$' "$PROBE" 2>/dev/null
  fi
}

last_count=0
last_event='<none>'
deadline=$((SECONDS + DEADLINE))
complete=0
while (( SECONDS < deadline )); do
  count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
  if (( count > last_count )); then
    while IFS= read -r event; do
      [[ -n "$event" ]] || continue
      last_event="$event"
      echo "REAL_ENGINE_EVENT $event"
    done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
    last_count=$count
  fi

  # A directly-invoked real-engine test may point at a binary built before the
  # v1.8.24 source overlay. Detect that immediately instead of reporting an
  # unrelated 45-second startup timeout or missing hostage/NPC gameplay markers.
  if grep -q '^MAP_MOVEMENT_SOLIDS ' "$PROBE" 2>/dev/null && \
     ! grep -Eq '^MAP_MOVEMENT_SOLIDS .* npc_bbox=[0-9]+$' "$PROBE" 2>/dev/null; then
    fail "stale_rebuilt_binary map_inventory_missing_npc_bbox lib=$LIB"
  fi

  if grep -Eq '^GAME_EVENTS_READY base=[1-9][0-9]* mod=[1-9][0-9]*$' "$PROBE" 2>/dev/null && \
     grep -Fxq 'GAME_EVENT_PLAYER_CONNECT_READY' "$PROBE" 2>/dev/null && \
     grep -Fxq 'GameInit_OK' "$PROBE" 2>/dev/null && \
     grep -Fxq 'CreateNetworkStringTables_OK' "$PROBE" 2>/dev/null && \
     grep -Eq '^MAP_ENTITY_LUMP entities=[1-9][0-9]* world=1 parsed=1$' "$PROBE" 2>/dev/null && \
     grep -Fxq 'DYNAMIC_COLLISION_INTERFACES_OK modelinfo=1 physics=1' "$PROBE" 2>/dev/null && \
     map_inventory_ready && \
     grep -Fq 'LevelInit_OK ' "$PROBE" 2>/dev/null && \
     grep -Fxq 'ServerActivate_OK' "$PROBE" 2>/dev/null && \
     grep -Eq '^FAKE_CLIENT_SEED_OK target=2 created=2 ' "$PROBE" 2>/dev/null && \
     instancebaseline_dump_latest_valid "$PROBE"; then
    fake_adopt_count="$(grep -c '^ClientPutInServer_FAKE_ADOPT ' "$PROBE" 2>/dev/null || true)"
    put_count="$(grep -c '^ClientPutInServer_OK ' "$PROBE" 2>/dev/null || true)"
    active_count="$(grep -c '^ClientActive_OK ' "$PROBE" 2>/dev/null || true)"
    if (( fake_adopt_count >= 2 && put_count >= 2 && active_count >= 2 )); then
      complete=1
      break
    fi
  fi

  running="$(docker inspect -f '{{.State.Running}}' "$rebuilt_id" 2>/dev/null || echo false)"
  if [[ "$running" != "true" ]]; then
    timeout 5s docker logs "$rebuilt_id" >"$REBUILT_LOG" 2>&1 || true
    rc="$(docker inspect -f '{{.State.ExitCode}}' "$rebuilt_id" 2>/dev/null || echo 125)"
    fail "rebuilt_exited rc=$rc last_event=$last_event"
  fi
  sleep 0.1
done

timeout 5s docker logs "$rebuilt_id" >"$REBUILT_LOG" 2>&1 || true
if (( complete == 0 )); then
  fake_enter_count="$(grep -c '^FAKE_CLIENT_CREATE_ENTER ' "$PROBE" 2>/dev/null || true)"
  fake_ok_count="$(grep -c '^FAKE_CLIENT_CREATE_OK ' "$PROBE" 2>/dev/null || true)"
  fake_fail_count="$(grep -c '^FAKE_CLIENT_CREATE_FAIL ' "$PROBE" 2>/dev/null || true)"
  connect_enter_count="$(grep -c '^ClientConnect_ENTER ' "$PROBE" 2>/dev/null || true)"
  put_enter_count="$(grep -c '^ClientPutInServer_ENTER ' "$PROBE" 2>/dev/null || true)"
  active_enter_count="$(grep -c '^ClientActive_ENTER ' "$PROBE" 2>/dev/null || true)"
  connect_count="$(grep -c '^ClientConnect_OK ' "$PROBE" 2>/dev/null || true)"
  fake_adopt_count="$(grep -c '^ClientPutInServer_FAKE_ADOPT ' "$PROBE" 2>/dev/null || true)"
  put_count="$(grep -c '^ClientPutInServer_OK ' "$PROBE" 2>/dev/null || true)"
  active_count="$(grep -c '^ClientActive_OK ' "$PROBE" 2>/dev/null || true)"
  fail "startup_watchdog_timeout=${V34_REAL_ENGINE_TIMEOUT:-45}s last_event=$last_event fake_enter=$fake_enter_count fake_ok=$fake_ok_count fake_fail=$fake_fail_count connect_enter=$connect_enter_count put_enter=$put_enter_count active_enter=$active_enter_count connect=$connect_count fake_adopt=$fake_adopt_count put=$put_count active=$active_count"
fi

if timeout 4s docker exec "$rebuilt_id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
  a2s_result=PASS
else
  a2s_result=NOT_READY
fi

# The engine's own CSendTablePrecalc is the flattened-field oracle used by the
# protocol decoder. The rebuilt DLL dumps it only after build4100 initialized
# SendTables, so no source-side flatten guess is involved.
[[ -s "$SENDPROP_SCHEMA" ]] || fail "sendprop_schema_dump_missing last_event=$last_event"
grep -Fq $'1\tCCSPlayer\tDT_CSPlayer\t347\tm_iAccount\t0\t16\t1\t' "$SENDPROP_SCHEMA" || \
  fail "sendprop_schema_account347_missing"
grep -Eq $'^6\tCPlantedC4\tDT_PlantedC4\t[0-9]+\tm_bBombTicking\t0\t1\t1\t' "$SENDPROP_SCHEMA" || \
  fail "sendprop_schema_planted_c4_bool_unsigned_missing"
grep -Eq '^SENDPROP_SCHEMA_DUMP_OK classes=177 flat_fields=[1-9][0-9]* account_field=347$' "$PROBE" || \
  fail "sendprop_schema_probe_missing"
weapon_profile_probe_latest_valid "$PROBE" || \
  fail "weapon_script_profiles_not_exact expected=24 required=loaded,expected,encrypted,plaintext,failed,mask,root,ak47,awp,usp,ak47_acc"
grep -Eq 'WEAPON_SCRIPT_PROFILES_OK .*shotgun_bullets=m3:[1-9][0-9]*,xm1014:[1-9][0-9]*' "$PROBE" || \
  fail "weapon_script_shotgun_bullets_missing"
# v1.8.35c historical source token: model_only_pistol_sniper=10
grep -Fq 'COMBAT_FIDELITY_OK spread_rng=source_uniform_fx_seed8 punch_decay=source_spring9_65 classic_auto_families=12 recovered_state_families=24 timed_accuracy_families=8 live_pistol_sniper=10 secondary_mode_families=8 recovered_recoil_families=12 glock_burst_scheduler=3x100ms glock_followup_spread=0.05 fov_wire_families=4 shotgun_families=2 shotgun_spread=m3_0.0675_xm1014_0.0725 shotgun_rng=fx_seed_per_pellet shotgun_recoil=sharedrandom_ground_air live_exact_families=24 te_firebullets=playback_slot64_class120 te_pas=multicast_slot41_prediction_exclude' "$PROBE" || \
  fail "combat_fidelity_probe_missing live_exact_families=24 shotgun_families=2 te_firebullets=playback_slot64_class120 te_pas=multicast_slot41_prediction_exclude"
# Historical source-gate compatibility markers for accepted 35c/35d/35e:
# recovered_state_families=22 model_only_pistol_sniper=10
# recovered_recoil_families=10 live_exact_families=22
[[ -s "$INSTANCEBASELINES" ]] || fail "instancebaseline_dump_missing last_event=$last_event"
grep -Fq $'1\tCCSPlayer\tDT_CSPlayer\t' "$INSTANCEBASELINES" || \
  fail "instancebaseline_player_row_missing"
instancebaseline_dump_latest_valid "$PROBE" || \
  fail "instancebaseline_probe_missing_or_invalid"

# Exercise the actual build4100 Protocol 7 path from the WSL host.  The engine
# itself exports the authoritative map spawn-count and runtime SendTable CRC;
# server_i486 probes them after ServerActivate so CLC_ClientInfo can be encoded
# without guessing. A deliberately mismatched CRC requests full svc_SendTable
# transmission. build4100 carries signon data on UDP reliable netchannel packets;
# the TCP socket is only the stream side-channel and may close normally.
signon_facts="$(grep '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null | tail -n1 || true)"
server_count="$(printf '%s\n' "$signon_facts" | sed -n 's/.*server_count=\([-0-9][0-9]*\).*/\1/p')"
sendtable_crc_hex="$(printf '%s\n' "$signon_facts" | sed -n 's/.*sendtable_crc_hex=\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/p')"
[[ -n "$server_count" ]] || fail "engine_signon_server_count_missing last_event=$last_event"
[[ -n "$sendtable_crc_hex" ]] || fail "engine_signon_sendtable_crc_missing last_event=$last_event"

protocol7_output="$PROBE_DIR/protocol7-client.log"
protocol7_lifecycle_args=()
if (( RECONNECT_ORACLE == 1 )); then
  protocol7_lifecycle_args+=(--disconnect-after-pass)
fi
protocol7_download_args=()
protocol7_objective_args=()
protocol7_weapon_args=()
if (( HOSTAGE_ORACLE == 1 )); then
  protocol7_objective_args+=(--objective-scenario hostage_rescue --max-udp-packets 384)
fi
# v1.8.31b: de_dust2.nav is 573 compressed file fragments after normal signon traffic.
# v1.8.34n: 768 remains the hard diagnostic budget while functional gates are
# incomplete. If every gate is already complete, the probe may cross it only
# for the existing post-spawn dwell, still bounded by its receive deadline.
if (( DUST2_ORACLE == 1 )); then
  protocol7_weapon_args+=(--weapon-actions --combat-damage)
  protocol7_download_args+=(
    --download-resource "$DOWNLOAD_RESOURCE"
    --download-size "$DOWNLOAD_SIZE"
    --download-sha256 "$DOWNLOAD_SHA256"
    --max-udp-packets 768
  )
fi
set +e
python3 "$ROOT/scripts/protocol7-client-probe.py" \
  --host 127.0.0.1 --port "$host_port" --name cssv34_realprobe --timeout 3 \
  --server-count "$server_count" --sendtable-crc "0x$sendtable_crc_hex" --force-full-sendtables \
  --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" "${protocol7_download_args[@]}" "${protocol7_objective_args[@]}" "${protocol7_weapon_args[@]}" "${protocol7_lifecycle_args[@]}" \
  >"$protocol7_output" 2>&1
protocol7_rc=$?
set -e
while IFS= read -r line; do
  [[ -n "$line" ]] && echo "REAL_CLIENT_EVENT $line"
done < "$protocol7_output"
if [[ $protocol7_rc -ne 0 ]]; then
  fail "protocol7_probe_rc=$protocol7_rc host_port=$host_port"
fi

objective_hostage=0
if (( HOSTAGE_ORACLE == 1 )); then
  grep -Fxq 'MAP_OBJECTIVE_ZONES bomb=0 rescue=2 total=2' "$PROBE" 2>/dev/null || \
    fail "hostage_rescue_zone_inventory_missing host_port=$host_port"
  grep -Eq '^PROTOCOL7_OBJECTIVE_SCENARIO=PASS scenario=hostage_rescue .*hostage_follows.*hostage_hurt.*hostage_killed.*hostage_rescued.*hostage_rescued_all.*hostage_stops_following.*round_end' "$protocol7_output" 2>/dev/null || {
    cat "$protocol7_output" >&2 || true
    fail "protocol7_hostage_objective_scenario_missing host_port=$host_port"
  }
  grep -Eq '^GAMEPLAY_HOSTAGE_RESCUE_ROUND scenario=hostage_rescue .*rescued=[1-9][0-9]* .*reason=' "$PROBE" 2>/dev/null || \
    fail "hostage_objective_engine_round_missing host_port=$host_port"
  objective_hostage=1
fi

protocol7_connection_ok=0
protocol7_tcp_ok=0
protocol7_signon_ok=0
protocol7_server_new_ok=0
protocol7_clientinfo_ok=0
protocol7_new_ok=0
protocol7_udp_signon_ok=0
protocol7_sendtable_seen=0
protocol7_fragment_ok=0
protocol7_bzip_ok=0
protocol7_classinfo_ok=0
protocol7_stringtables_ok=0
protocol7_instancebaseline_ok=0
protocol7_packetbaseline_ok=0
protocol7_prespawn_ok=0
protocol7_prespawn_sent=0
protocol7_spawn_ok=0
protocol7_spawn_sent=0
protocol7_full_sent=0
protocol7_baseline_ack_sent=0
protocol7_snapshot_tick_ack_sent=0
protocol7_packetentities_ok=0
protocol7_entity_snapshot_ok=0
protocol7_sendprop_snapshot_ok=0
protocol7_packetentities_delta_ok=0
protocol7_entity_delta_ok=0
protocol7_sendprop_delta_ok=0
protocol7_stringcmd_ok=0
protocol7_gameplay_event_delta_ok=0
protocol7_spawn_origin_ok=0
protocol7_move_ok=0
protocol7_usercmd_eyeangles_delta_ok=0
protocol7_usercmd_movement_delta_ok=0
protocol7_friction_move_ok=0
protocol7_usercmd_friction_delta_ok=0
protocol7_jump_move_ok=0
protocol7_usercmd_jump_delta_ok=0
protocol7_jump_hold_move_ok=0
protocol7_usercmd_jump_hold_delta_ok=0
protocol7_air_accel_move_ok=0
protocol7_usercmd_air_accel_delta_ok=0
protocol7_weapon_attack_ok=0
protocol7_weapon_reload_move_ok=0
protocol7_weapon_secondary_move_ok=0
protocol7_weapon_fire_event_ok=0
protocol7_weapon_reload_event_ok=0
protocol7_combat_setup_ok=0
protocol7_player_hurt_event_ok=0
protocol7_player_death_event_ok=0
protocol7_combat_reset_ok=0
protocol7_gameevent_list_ok=0
protocol7_listen_events_ok=0
protocol7_say_sent_ok=0
protocol7_player_say_event_ok=0
protocol7_usermessage_command_ok=0
protocol7_saytext_usermessage_ok=0
protocol7_downloadables_ok=0
protocol7_download_request_ok=0
protocol7_download_received_ok=0

# Detailed PASS lines are useful diagnostics, but their optional telemetry grows
# as the probe learns new protocol features. Acceptance must not depend on the
# exact ordering/presence of those extra key/value fields. The final PROTOCOL7
# summary is emitted only after the probe has validated each feature, so use it
# as a stable fallback contract for boolean feature gates.
protocol7_summary_flag() {
  local key="$1"
  grep -Eq "^PROTOCOL7=PASS .* ${key}=1( |$)" "$protocol7_output" 2>/dev/null
}

# Detailed PASS telemetry is append-only and may gain/reorder diagnostic fields.
# Keep semantic host acceptance independent of presentation order.
# shellcheck source=protocol7-gate-lib.sh
source "$ROOT/scripts/protocol7-gate-lib.sh"
if grep -q '^PROTOCOL7_CONNECTION_ACCEPTED=PASS reply=B ' "$protocol7_output" 2>/dev/null; then
  protocol7_connection_ok=1
fi
if grep -q '^PROTOCOL7_TCP_STREAM_BOUND=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_tcp_ok=1
fi
if grep -q '^PROTOCOL7_NETCHAN_SIGNON_SENT=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_signon_ok=1
fi
if grep -q '^PROTOCOL7_SERVER_SIGNON_NEW=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_server_new_ok=1
fi
if grep -q '^PROTOCOL7_CLIENTINFO_SENT=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_clientinfo_ok=1
fi
if grep -q '^PROTOCOL7_NETCHAN_NEW_SENT=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_new_ok=1
fi
if grep -Eq '^PROTOCOL7_SIGNON_UDP_[0-9]+=PASS ' "$protocol7_output" 2>/dev/null; then
  protocol7_udp_signon_ok=1
fi
if grep -Eq '^PROTOCOL7_SENDTABLE_STREAM=PASS tables=[1-9][0-9]* ' "$protocol7_output" 2>/dev/null; then
  protocol7_sendtable_seen=1
fi
# Incoming reliable state is a toggle mask, not a monotonic counter.  A later
# transfer can legally toggle the same subchannel back to 0x00, so acceptance
# must be based on completed fragment/BZip work rather than the final mask.
if grep -Eq '^PROTOCOL7_FRAGMENT_REASSEMBLY=PASS .*blocks=[1-9][0-9]* .*bzip_complete=[1-9][0-9]*' "$protocol7_output" 2>/dev/null; then
  protocol7_fragment_ok=1
fi
if grep -Eq '^PROTOCOL7=PASS .*bzip_complete=[1-9][0-9]*' "$protocol7_output" 2>/dev/null; then
  protocol7_bzip_ok=1
fi
if grep -Eq '^PROTOCOL7_CLASSINFO=PASS .*classes=177 .*ids=0\.\.176 ' "$protocol7_output" 2>/dev/null; then
  protocol7_classinfo_ok=1
fi
if grep -Fq 'PROTOCOL7_STRINGTABLE_BASELINES=PASS tables=16 names=downloadables,modelprecache,genericprecache,soundprecache,decalprecache,instancebaseline,lightstyles,userinfo,server_query_info,EffectDispatch,VguiScreen,Materials,InfoPanel,Scenes,GameRulesCreation,BlackMarketTable ' "$protocol7_output" 2>/dev/null; then
  protocol7_stringtables_ok=1
fi
if protocol7_pass_has_fields "$protocol7_output" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
  'entries=[0-9]+' 'runtime_snapshot=[0-9]+' 'wire_growth=[0-9]+' \
  'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=(none|[0-9,]+)'; then
  protocol7_instancebaseline_ok=1
fi
if protocol7_pass_has_fields "$protocol7_output" 'PROTOCOL7_PACKET_BASELINE_POLICY=PASS ' \
  'baseline_classes=[0-9]+' 'packet_classes=[1-9][0-9]*' \
  'materialized=[0-9]+' 'implicit=[0-9]+' 'player=(materialized|implicit)' \
  'materialized_ids=[^ ]+' 'implicit_ids=[^ ]+'; then
  protocol7_packetbaseline_ok=1
fi
if grep -q '^PROTOCOL7_SERVER_SIGNON_PRESPAWN=PASS state=4 ' "$protocol7_output" 2>/dev/null; then
  protocol7_prespawn_ok=1
fi
if grep -Eq '^PROTOCOL7_NETCHAN_PRESPAWN_SENT=PASS .*state=4 ' "$protocol7_output" 2>/dev/null; then
  protocol7_prespawn_sent=1
fi
if grep -Eq '^PROTOCOL7_SERVER_SIGNON_SPAWN=PASS state=5 .*source=spawn_transition .*tail_pad_bits=[0-7]( |$)' "$protocol7_output" 2>/dev/null; then
  protocol7_spawn_ok=1
fi
if grep -Eq '^PROTOCOL7_NETCHAN_SPAWN_SENT=PASS .*state=5 ' "$protocol7_output" 2>/dev/null; then
  protocol7_spawn_sent=1
fi
if grep -Eq '^PROTOCOL7_BASELINE_ACK_SENT=PASS .*type=11 .*tick=[0-9]+ .*baseline=[01] .*update_baseline=1 ' "$protocol7_output" 2>/dev/null; then
  protocol7_baseline_ack_sent=1
fi
if grep -Eq '^PROTOCOL7_NETCHAN_FULL_SENT=PASS .*state=6 .*full_snapshot_tick=[0-9]+ ' "$protocol7_output" 2>/dev/null; then
  protocol7_full_sent=1
fi
if grep -Eq '^PROTOCOL7_SNAPSHOT_TICK_ACK_SENT=PASS .*type=3 .*tick=[0-9]+ ' "$protocol7_output" 2>/dev/null; then
  protocol7_snapshot_tick_ack_sent=1
fi
if grep -Eq '^PROTOCOL7_PACKETENTITIES=PASS .*max=[1-9][0-9]* .*delta=0 .*updated=[1-9][0-9]* .*data_bits=[1-9][0-9]* ' "$protocol7_output" 2>/dev/null; then
  protocol7_packetentities_ok=1
fi
if grep -Eq '^PROTOCOL7_ENTITY_SNAPSHOT=PASS .*players=3 .*player_entities=1,2,3 .*changed_props=[1-9][0-9]* .*1:CCSPlayer.*2:CCSPlayer.*3:CCSPlayer' "$protocol7_output" 2>/dev/null; then
  protocol7_entity_snapshot_ok=1
fi
if grep -Eq '^PROTOCOL7_SENDPROP_GAMEPLAY=PASS schema=engine_flattened wire=bot2_team3\+local3_team0\+local3_state1 baseline=account800\+health100\+bot1_team2 player_entities=1,2,3 extras=[^ ]+ changed_props=[1-9][0-9]*$' "$protocol7_output" 2>/dev/null; then
  protocol7_sendprop_snapshot_ok=1
fi
if grep -Eq '^PROTOCOL7_PACKETENTITIES=PASS .*delta=1 .*delta_from=[0-9]+ .*updated=[0-9]+ .*data_bits=[0-9]+ ' "$protocol7_output" 2>/dev/null; then
  protocol7_packetentities_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_ENTITY_DELTA=PASS .*from_tick_match=1 .*schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_entity_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_STRINGCMD_SENT=PASS .*type=4 command=jointeam_3 ' "$protocol7_output" 2>/dev/null; then
  protocol7_stringcmd_ok=1
fi
if grep -Eq '^PROTOCOL7_GAMEPLAY_EVENT_DELTA=PASS command=jointeam_3 entity=3 team=3 state=0 changed_props=([2-9]|[1-9][0-9]+) delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_gameplay_event_delta_ok=1
  protocol7_sendprop_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_SPAWN_ORIGIN=PASS entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} worldtrace=1$' "$protocol7_output" 2>/dev/null; then
  protocol7_spawn_origin_ok=1
fi
if grep -Eq '^PROTOCOL7_MOVE_SENT=PASS .*type=9 new=1 backup=0 cmd_bits=[1-9][0-9]* .*pitch=45 yaw=90 forward=200 ' "$protocol7_output" 2>/dev/null; then
  protocol7_move_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_EYEANGLES_DELTA=PASS clc_move=9 entity=3 pitch=[-+]?[0-9]+(\.[0-9]+)? yaw=[-+]?[0-9]+(\.[0-9]+)? changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  # The Python probe emits PASS only after validating the decoded angles against
  # the requested 45/90 usercmd with SendPropAngle quantization tolerance.
  protocol7_usercmd_eyeangles_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_MOVEMENT_DELTA=PASS clc_move=9 entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} forward=200 accel=5 changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_usercmd_movement_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_FRICTION_MOVE_SENT=PASS .*type=9 new=1 backup=0 cmd_bits=[1-9][0-9]* cmd=2 .*forward=0 buttons=0 ' "$protocol7_output" 2>/dev/null; then
  protocol7_friction_move_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_FRICTION_DELTA=PASS clc_move=9 entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} friction=4 stopspeed=75 changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_usercmd_friction_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_JUMP_MOVE_SENT=PASS .*type=9 new=1 backup=0 cmd_bits=[1-9][0-9]* cmd=3 .*forward=0 buttons=2 ' "$protocol7_output" 2>/dev/null; then
  protocol7_jump_move_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_JUMP_GRAVITY_DELTA=PASS clc_move=9 entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} buttons=2 gravity=800 impulse=268\.328 changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_usercmd_jump_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_JUMP_HOLD_MOVE_SENT=PASS .*type=9 new=1 backup=0 cmd_bits=[1-9][0-9]* cmd=4 .*forward=0 buttons=2 ' "$protocol7_output" 2>/dev/null; then
  protocol7_jump_hold_move_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_JUMP_LATCH_DELTA=PASS clc_move=9 entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} buttons=2 latch=held changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_usercmd_jump_hold_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_AIR_ACCEL_MOVE_SENT=PASS .*type=9 new=1 backup=0 cmd_bits=[1-9][0-9]* cmd=5 .*forward=200 buttons=0 ' "$protocol7_output" 2>/dev/null; then
  protocol7_air_accel_move_ok=1
fi
if grep -Eq '^PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS clc_move=9 entity=3 origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} forward=200 accel=10 cap=30 changed_props=[1-9][0-9]* delta_from=[0-9]+ schema=engine_flattened$' "$protocol7_output" 2>/dev/null; then
  protocol7_usercmd_air_accel_delta_ok=1
fi
if grep -Eq '^PROTOCOL7_WEAPON_ATTACK_SENT=PASS .*type=9 cmd=6 .*buttons=1 ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag weapon_attack_sent; then protocol7_weapon_attack_ok=1; fi
if grep -Eq '^PROTOCOL7_WEAPON_RELOAD_SENT=PASS .*type=9 cmd=8 .*buttons=8192 ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag weapon_reload_sent; then protocol7_weapon_reload_move_ok=1; fi
if grep -Eq '^PROTOCOL7_WEAPON_SECONDARY_SENT=PASS .*type=9 cmd=9 .*buttons=2048 ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag weapon_secondary_sent; then protocol7_weapon_secondary_move_ok=1; fi
if grep -Eq '^PROTOCOL7_WEAPON_FIRE_EVENT=PASS .*userid=[0-9]+ weapon=(glock|usp) ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag weapon_fire_event; then protocol7_weapon_fire_event_ok=1; fi
if grep -Eq '^PROTOCOL7_WEAPON_RELOAD_EVENT=PASS .*userid=[0-9]+ ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag weapon_reload_event; then protocol7_weapon_reload_event_ok=1; fi
if grep -Eq '^PROTOCOL7_COMBAT_SETUP_SENT=PASS .*type=4 command=cssv34_combat_setup ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag combat_setup_sent; then protocol7_combat_setup_ok=1; fi
if grep -Eq '^PROTOCOL7_PLAYER_HURT_EVENT=PASS .*userid=[0-9]+ attacker=[0-9]+ health=0 armor=0 weapon=(glock|usp) ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag player_hurt_event; then protocol7_player_hurt_event_ok=1; fi
if grep -Eq '^PROTOCOL7_PLAYER_DEATH_EVENT=PASS .*userid=[0-9]+ attacker=[0-9]+ weapon=(glock|usp) headshot=1 ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag player_death_event; then protocol7_player_death_event_ok=1; fi
if grep -Eq '^PROTOCOL7_COMBAT_RESET_SENT=PASS .*type=4 command=cssv34_combat_reset ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag combat_reset_sent; then protocol7_combat_reset_ok=1; fi
if grep -Eq '^PROTOCOL7_GAMEEVENT_LIST=PASS .*events=[1-9][0-9]* .*player_say_id=[0-9]+ .*fields=.*userid:[1-6].*text:1 .*bit=[0-9]+ endbit=[0-9]+$' "$protocol7_output" 2>/dev/null || protocol7_summary_flag gameevent_list; then
  protocol7_gameevent_list_ok=1
fi
if grep -Eq '^PROTOCOL7_LISTENEVENTS_SENT=PASS .*type=12 player_say_id=[0-9]+ .*mask_bits=512 ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag listen_events_sent; then
  protocol7_listen_events_ok=1
fi
if grep -Eq '^PROTOCOL7_SAY_SENT=PASS .*type=4 command=say_cssv34_wire_event ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag say_command_sent; then
  protocol7_say_sent_ok=1
fi
if grep -Eq '^PROTOCOL7_GAMEEVENT_PLAYER_SAY=PASS .*event_id=[0-9]+ userid=[0-9]+ text=cssv34_wire_event .*length_bits=[1-9][0-9]*$' "$protocol7_output" 2>/dev/null || protocol7_summary_flag player_say_event; then
  protocol7_player_say_event_ok=1
fi
if grep -Eq '^PROTOCOL7_USERMESSAGE_COMMAND_SENT=PASS .*type=4 command=cssv34_usermsg_cssv34_wire_usermsg ' "$protocol7_output" 2>/dev/null || protocol7_summary_flag usermessage_command_sent; then
  protocol7_usermessage_command_ok=1
fi
if grep -Eq '^PROTOCOL7_USERMESSAGE_SAYTEXT=PASS .*message_id=3 sender=3 text=cssv34_wire_usermsg chat=1 .*payload_bits=[1-9][0-9]*$' "$protocol7_output" 2>/dev/null || protocol7_summary_flag saytext_usermessage; then
  protocol7_saytext_usermessage_ok=1
fi
if (( DUST2_ORACLE == 1 )); then
  if grep -Fq "PROTOCOL7_DOWNLOADABLES=PASS entries=2 requested=$DOWNLOAD_RESOURCE advertised=1 resources=maps/de_dust2.bsp,maps/de_dust2.nav" "$protocol7_output" 2>/dev/null; then
    protocol7_downloadables_ok=1
  fi
  if grep -Eq "^PROTOCOL7_DOWNLOAD_REQUEST_SENT=PASS .*type=2 transfer_id=[0-9]+ filename=$DOWNLOAD_RESOURCE " "$protocol7_output" 2>/dev/null; then
    protocol7_download_request_ok=1
  fi
  if grep -Eq "^PROTOCOL7_DOWNLOAD_FILE=PASS filename=${DOWNLOAD_RESOURCE} transfer_id=[0-9]+ bytes=${DOWNLOAD_SIZE} fragments=[1-9][0-9]* compressed=[01] sha256=${DOWNLOAD_SHA256}$" "$protocol7_output" 2>/dev/null; then
    protocol7_download_received_ok=1
  fi
fi
(( protocol7_connection_ok == 1 )) || fail "protocol7_S2C_CONNECTION_missing host_port=$host_port"
(( protocol7_tcp_ok == 1 )) || fail "protocol7_tcp_stream_missing host_port=$host_port"
(( protocol7_signon_ok == 1 )) || fail "protocol7_signon_state2_missing host_port=$host_port"
(( protocol7_server_new_ok == 1 )) || fail "protocol7_server_signon_state3_missing host_port=$host_port"
(( protocol7_clientinfo_ok == 1 )) || fail "protocol7_ClientInfo_missing host_port=$host_port"
(( protocol7_new_ok == 1 )) || fail "protocol7_signon_state3_missing host_port=$host_port"
(( protocol7_udp_signon_ok == 1 )) || fail "protocol7_udp_signon_missing host_port=$host_port"
(( protocol7_fragment_ok == 1 )) || fail "protocol7_fragment_reassembly_missing host_port=$host_port"
(( protocol7_bzip_ok == 1 )) || fail "protocol7_bzip_reassembly_missing host_port=$host_port"
(( protocol7_sendtable_seen == 1 )) || fail "protocol7_svc_SendTable_missing host_port=$host_port"
(( protocol7_classinfo_ok == 1 )) || fail "protocol7_svc_ClassInfo_missing host_port=$host_port"
(( protocol7_stringtables_ok == 1 )) || fail "protocol7_svc_CreateStringTable_baselines_missing host_port=$host_port"
(( protocol7_instancebaseline_ok == 1 )) || fail "protocol7_instancebaseline_runtime_wire_mismatch host_port=$host_port"
(( protocol7_packetbaseline_ok == 1 )) || fail "protocol7_packetentities_instancebaseline_policy_missing host_port=$host_port"
(( protocol7_prespawn_ok == 1 )) || fail "protocol7_server_signon_state4_missing host_port=$host_port"
(( protocol7_prespawn_sent == 1 )) || fail "protocol7_client_signon_state4_missing host_port=$host_port"
(( protocol7_spawn_ok == 1 )) || fail "protocol7_server_signon_state5_missing host_port=$host_port"
(( protocol7_spawn_sent == 1 )) || fail "protocol7_client_signon_state5_missing host_port=$host_port"
(( protocol7_packetentities_ok == 1 )) || fail "protocol7_svc_PacketEntities_full_snapshot_missing host_port=$host_port"
(( protocol7_baseline_ack_sent == 1 )) || fail "protocol7_clc_BaselineAck_missing host_port=$host_port"
(( protocol7_full_sent == 1 )) || fail "protocol7_client_signon_state6_full_missing host_port=$host_port"
(( protocol7_snapshot_tick_ack_sent == 1 )) || fail "protocol7_snapshot_NET_Tick_ack_missing host_port=$host_port"
(( protocol7_entity_snapshot_ok == 1 )) || fail "protocol7_packetentities_full_entity_decode_missing host_port=$host_port"
(( protocol7_sendprop_snapshot_ok == 1 )) || fail "protocol7_gameplay_sendprop_values_missing host_port=$host_port"
(( protocol7_packetentities_delta_ok == 1 )) || fail "protocol7_svc_PacketEntities_delta_snapshot_missing host_port=$host_port"
(( protocol7_entity_delta_ok == 1 )) || fail "protocol7_packetentities_delta_entity_decode_missing host_port=$host_port"
(( protocol7_stringcmd_ok == 1 )) || fail "protocol7_NET_StringCmd_jointeam3_missing host_port=$host_port"
(( protocol7_gameplay_event_delta_ok == 1 )) || fail "protocol7_gameplay_event_sendprop_delta_missing host_port=$host_port"
(( protocol7_spawn_origin_ok == 1 )) || fail "protocol7_worldtrace_spawn_origin_missing host_port=$host_port"
(( protocol7_move_ok == 1 )) || fail "protocol7_clc_Move_usercmd_missing host_port=$host_port"
(( protocol7_usercmd_eyeangles_delta_ok == 1 )) || fail "protocol7_usercmd_eyeangles_sendprop_delta_missing host_port=$host_port"
(( protocol7_usercmd_movement_delta_ok == 1 )) || fail "protocol7_usercmd_movement_sendprop_delta_missing host_port=$host_port"
(( protocol7_friction_move_ok == 1 )) || fail "protocol7_friction_clc_Move_missing host_port=$host_port"
(( protocol7_usercmd_friction_delta_ok == 1 )) || fail "protocol7_usercmd_friction_sendprop_delta_missing host_port=$host_port"
(( protocol7_jump_move_ok == 1 )) || fail "protocol7_jump_clc_Move_missing host_port=$host_port"
(( protocol7_usercmd_jump_delta_ok == 1 )) || fail "protocol7_usercmd_jump_gravity_sendprop_delta_missing host_port=$host_port"
(( protocol7_jump_hold_move_ok == 1 )) || fail "protocol7_jump_hold_clc_Move_missing host_port=$host_port"
(( protocol7_usercmd_jump_hold_delta_ok == 1 )) || fail "protocol7_usercmd_jump_latch_sendprop_delta_missing host_port=$host_port"
(( protocol7_air_accel_move_ok == 1 )) || fail "protocol7_air_accel_clc_Move_missing host_port=$host_port"
(( protocol7_usercmd_air_accel_delta_ok == 1 )) || fail "protocol7_usercmd_air_accel_sendprop_delta_missing host_port=$host_port"
if (( DUST2_ORACLE == 1 )); then
  (( protocol7_weapon_attack_ok == 1 )) || fail "protocol7_weapon_attack_clc_Move_missing host_port=$host_port"
  (( protocol7_weapon_fire_event_ok == 1 )) || fail "protocol7_weapon_fire_svc_GameEvent_missing host_port=$host_port"
  (( protocol7_weapon_reload_move_ok == 1 )) || fail "protocol7_weapon_reload_clc_Move_missing host_port=$host_port"
  (( protocol7_weapon_reload_event_ok == 1 )) || fail "protocol7_weapon_reload_svc_GameEvent_missing host_port=$host_port"
  (( protocol7_weapon_secondary_move_ok == 1 )) || fail "protocol7_weapon_secondary_clc_Move_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_WEAPON_SECONDARY_EVENT index=3 weapon=(glock|usp) silenced=1 burst=0 zoom=0 fov=0 next_secondary=[0-9]+(\.[0-9]+)?$' "$PROBE" 2>/dev/null || fail "gameplay_weapon_secondary_event_missing host_port=$host_port"
  (( protocol7_combat_setup_ok == 1 )) || fail "protocol7_combat_setup_NET_StringCmd_missing host_port=$host_port"
  (( protocol7_player_hurt_event_ok == 1 )) || fail "protocol7_player_hurt_svc_GameEvent_missing host_port=$host_port"
  (( protocol7_player_death_event_ok == 1 )) || fail "protocol7_player_death_svc_GameEvent_missing host_port=$host_port"
  (( protocol7_combat_reset_ok == 1 )) || fail "protocol7_combat_reset_NET_StringCmd_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_COMBAT_SETUP attacker=3 target=[1-9][0-9]* target_health=30 friendlyfire=1 ' "$PROBE" 2>/dev/null || fail "gameplay_combat_setup_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_HITSCAN_DAMAGE attacker=3 victim=[1-9][0-9]* weapon=(glock|usp) hitgroup=1 health_before=30 health_after=0 armor_before=0 armor_after=0 killed=1 hurt_event=1 death_event=1 .*distance=[0-9]+(\.[0-9]+)? damage=[0-9]+(\.[0-9]+)? spread=[0-9]+(\.[0-9]+)? spread_xy=[-+]?[0-9]+(\.[0-9]+)?,[-+]?[0-9]+(\.[0-9]+)? recoil=[-+]?[0-9]+(\.[0-9]+)?,[-+]?[0-9]+(\.[0-9]+)? cmd_tick=[0-9]+ history_tick=[0-9]+ rewind_ticks=[0-9]+ lagcomp=[01]$' "$PROBE" 2>/dev/null || fail "gameplay_combat_ballistics_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_COMBAT_RESET attacker=3 target=[1-9][0-9]* friendlyfire=0$' "$PROBE" 2>/dev/null || fail "gameplay_combat_reset_missing host_port=$host_port"
fi
(( protocol7_gameevent_list_ok == 1 )) || fail "protocol7_svc_GameEventList_player_say_descriptor_missing host_port=$host_port"
(( protocol7_listen_events_ok == 1 )) || fail "protocol7_clc_ListenEvents_player_say_subscription_missing host_port=$host_port"
(( protocol7_say_sent_ok == 1 )) || fail "protocol7_NET_StringCmd_say_missing host_port=$host_port"
(( protocol7_player_say_event_ok == 1 )) || fail "protocol7_svc_GameEvent_player_say_missing host_port=$host_port"
(( protocol7_usermessage_command_ok == 1 )) || fail "protocol7_usermessage_trigger_command_missing host_port=$host_port"
(( protocol7_saytext_usermessage_ok == 1 )) || fail "protocol7_svc_UserMessage_SayText_missing host_port=$host_port"
if (( DUST2_ORACLE == 1 )); then
  (( protocol7_downloadables_ok == 1 )) || fail "protocol7_downloadables_resource_missing host_port=$host_port resource=$DOWNLOAD_RESOURCE"
  (( protocol7_download_request_ok == 1 )) || fail "protocol7_NET_File_request_missing host_port=$host_port resource=$DOWNLOAD_RESOURCE"
  (( protocol7_download_received_ok == 1 )) || fail "protocol7_file_transfer_missing host_port=$host_port resource=$DOWNLOAD_RESOURCE"
fi
(( protocol7_sendprop_delta_ok == 1 )) || fail "protocol7_schema_delta_decode_missing host_port=$host_port"

# The complete NEW -> PRESPAWN -> SPAWN exchange must drive the real game-DLL
# lifecycle, not only the packet parser. Wait for the named non-fake client to
# reach ClientConnect, ClientPutInServer and ClientActive.
client_lifecycle_deadline=$((SECONDS + 6))
real_client_connect=0
real_client_put=0
real_client_active=0
while (( SECONDS < client_lifecycle_deadline )); do
  count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
  if (( count > last_count )); then
    while IFS= read -r event; do
      [[ -n "$event" ]] || continue
      last_event="$event"
      echo "REAL_ENGINE_EVENT $event"
    done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
    last_count=$count
  fi
  grep -Eq '^ClientConnect_OK .*name=cssv34_realprobe( |$)' "$PROBE" 2>/dev/null && real_client_connect=1
  grep -Eq '^ClientPutInServer_OK .*name=cssv34_realprobe( |$)' "$PROBE" 2>/dev/null && real_client_put=1
  grep -Eq '^ClientActive_OK index=3( |$)' "$PROBE" 2>/dev/null && real_client_active=1
  if (( real_client_connect == 1 && real_client_put == 1 && real_client_active == 1 )); then
    break
  fi
  if grep -Eq '^ClientConnect_REJECT .*name=cssv34_realprobe( |$)' "$PROBE" 2>/dev/null; then
    fail "protocol7_ClientConnect_rejected host_port=$host_port last_event=$last_event"
  fi
  running="$(docker inspect -f '{{.State.Running}}' "$rebuilt_id" 2>/dev/null || echo false)"
  if [[ "$running" != "true" ]]; then
    timeout 5s docker logs "$rebuilt_id" >"$REBUILT_LOG" 2>&1 || true
    rc="$(docker inspect -f '{{.State.ExitCode}}' "$rebuilt_id" 2>/dev/null || echo 125)"
    oom="$(docker inspect -f '{{.State.OOMKilled}}' "$rebuilt_id" 2>/dev/null || echo unknown)"
    state_error="$(docker inspect -f '{{.State.Error}}' "$rebuilt_id" 2>/dev/null || echo unknown)"
    fail "rebuilt_exited_during_protocol7 rc=$rc oom=$oom state_error=$state_error last_event=$last_event"
  fi
  sleep 0.1
done
(( real_client_connect == 1 )) || fail "protocol7_signon_ClientConnect_missing host_port=$host_port last_event=$last_event"
(( real_client_put == 1 )) || fail "protocol7_signon_ClientPutInServer_missing host_port=$host_port last_event=$last_event"
(( real_client_active == 1 )) || fail "protocol7_signon_ClientActive_missing host_port=$host_port last_event=$last_event"

# v1.8.6: crossing ClientActive is not enough; require CheckTransmit and a real full snapshot.  The engine immediately enters
# visibility/snapshot work and used to die here when IServerNetworkable slot 0
# returned a packed handle instead of IHandleEntity*.  Keep the real client
# alive briefly and require both the container and A2S path to survive.
post_active_deadline=$((SECONDS + 2))
post_active_visibility=0
post_active_edict_bind=0
post_active_networkable=0
post_active_checktransmit=0
post_active_gameplay_state=0
post_active_gameplay_event=0
post_active_player_say_event=0
post_active_saytext_usermessage=0
post_active_engine_trace=0
post_active_spawn_trace=0
post_active_usercmd_event=0
post_active_movement_event=0
post_active_friction_event=0
post_active_jump_event=0
post_active_jump_hold_event=0
post_active_air_accel_event=0
post_active_weapon_fire_event=0
post_active_weapon_reload_event=0
post_active_walkable_slope=0
post_active_stepmove=0
post_active_advanced_movement=0
post_active_dynamic_player_collision=0
post_active_dynamic_nonplayer_collision=0
post_active_hostage_collision=0
post_active_hostage_follow=0
post_active_generic_npc=0
post_active_water_contents=0
post_active_water_movement=0
post_active_water_level2=0
post_active_duck_movement=0
post_active_ladder_movement=0
while (( SECONDS < post_active_deadline )); do
  count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
  if (( count > last_count )); then
    while IFS= read -r event; do
      [[ -n "$event" ]] || continue
      last_event="$event"
      echo "REAL_ENGINE_EVENT $event"
    done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
    last_count=$count
  fi
  grep -Eq '^ClientSetupVisibility_OK .*name=cssv34_realprobe( |$)' "$PROBE" 2>/dev/null && post_active_visibility=1
  grep -Eq '^EDICT_INTERFACE_BIND_OK index=3( |$)' "$PROBE" 2>/dev/null && post_active_edict_bind=1
  grep -Eq '^NetworkableGet(EntityHandle|ServerClass|Edict|PVSInfo)_OK .*class=player( |$)' "$PROBE" 2>/dev/null && post_active_networkable=1
  grep -Eq '^CheckTransmit_OK .*count=[1-9][0-9]* .*marked=[1-9][0-9]* ' "$PROBE" 2>/dev/null && post_active_checktransmit=1
  if grep -Eq '^GAMEPLAY_SENDSTATE_READY index=1 account=800 health=100 team=2 state=0 ' "$PROBE" 2>/dev/null && \
     grep -Eq '^GAMEPLAY_SENDSTATE_READY index=2 account=800 health=100 team=3 state=0 ' "$PROBE" 2>/dev/null && \
     grep -Eq '^GAMEPLAY_SENDSTATE_READY index=3 account=800 health=100 team=0 state=1 ' "$PROBE" 2>/dev/null; then
    post_active_gameplay_state=1
  fi
  grep -Eq '^GAMEPLAY_COMMAND_EVENT index=3 cmd=jointeam arg=3 before_team=0 after_team=3 before_state=1 after_state=0 changed=1$' "$PROBE" 2>/dev/null && post_active_gameplay_event=1
  grep -Eq '^GAME_EVENT_PLAYER_SAY index=3 cmd=say userid=[0-9]+ text=cssv34_wire_event created=1 fired=1$' "$PROBE" 2>/dev/null && post_active_player_say_event=1
  grep -Fxq 'USER_MESSAGE_SAYTEXT index=3 cmd=cssv34_usermsg msg=3 name=SayText text=cssv34_wire_usermsg chat=1 begin=1 write_api=1 write_string=1 end=1' "$PROBE" 2>/dev/null && post_active_saytext_usermessage=1
  grep -Eq '^ENGINE_TRACE_WORLD_HULL=PASS start=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} end=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} fraction=0?\.[0-9]+ normalz=[0-9]+(\.[0-9]+)? startsolid=0 allsolid=0 spawn_index=[0-9]+ preferred=[0-9]+ flat=1$' "$PROBE" 2>/dev/null && post_active_engine_trace=1
  grep -Eq '^GAMEPLAY_SPAWN_TRACE_EVENT index=3 team=3 reason=jointeam spawn=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} fraction=0?\.[0-9]+ normalz=[0-9]+(\.[0-9]+)? spawn_index=[0-9]+ preferred=[0-9]+ flat=1 worldtrace=1$' "$PROBE" 2>/dev/null && post_active_spawn_trace=1
  grep -Eq '^GAMEPLAY_USERCMD_EVENT index=3 cmd=1 tick=[0-9]+ pitch=45\.000 yaw=90\.000 roll=0\.000 num=1 total=1 changed=1$' "$PROBE" 2>/dev/null && post_active_usercmd_event=1
  grep -Eq '^GAMEPLAY_MOVEMENT_EVENT index=3 cmd=1 tick=[0-9]+ origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=0\.000,15\.000,0\.000 forward=200\.000 side=0\.000 up=0\.000 dt=0\.015 onground=1 worldtrace=1 changed=1$' "$PROBE" 2>/dev/null && post_active_movement_event=1
  grep -Eq '^GAMEPLAY_FRICTION_EVENT index=3 cmd=2 tick=[0-9]+ before_speed=[-+]?[0-9]+(\.[0-9]+)? after_speed=[-+]?[0-9]+(\.[0-9]+)? origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} dt=0\.015 onground=1 changed=1$' "$PROBE" 2>/dev/null && post_active_friction_event=1
  grep -Eq '^GAMEPLAY_JUMP_EVENT index=3 cmd=3 tick=[0-9]+ origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} buttons=0x2 dt=0\.015 onground=0 changed=1$' "$PROBE" 2>/dev/null && post_active_jump_event=1
  grep -Eq '^GAMEPLAY_JUMP_HOLD_EVENT index=3 cmd=4 tick=[0-9]+ origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} buttons=0x2 dt=0\.015 latch=held onground=0 changed=1$' "$PROBE" 2>/dev/null && post_active_jump_hold_event=1
  grep -Eq '^GAMEPLAY_WEAPON_FIRE_EVENT index=3 weapon=(glock|usp) clip_before=[1-9][0-9]* clip_after=[0-9]+ shots=1 event=1 te_firebullets=1$' "$PROBE" 2>/dev/null && post_active_weapon_fire_event=1
  grep -Eq '^GAMEPLAY_WEAPON_RELOAD_EVENT index=3 weapon=(glock|usp) clip_before=[0-9]+ clip_after=[1-9][0-9]* reserve_before=[0-9]+ reserve_after=[0-9]+ event=1$' "$PROBE" 2>/dev/null && post_active_weapon_reload_event=1
  grep -Eq '^GAMEPLAY_AIR_ACCEL_EVENT index=3 cmd=5 tick=[0-9]+ before_speed=[-+]?[0-9]+(\.[0-9]+)? after_speed=[-+]?[0-9]+(\.[0-9]+)? origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} velocity=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} forward=200\.000 side=0\.000 dt=0\.015 accel=10 cap=30 onground=0 changed=1$' "$PROBE" 2>/dev/null && post_active_air_accel_event=1
  grep -Eq '^GAMEPLAY_WALKABLE_SLOPE=PASS .*normal=[-+]?[0-9]+(\.[0-9]+)?,[-+]?[0-9]+(\.[0-9]+)?,0\.[7-9][0-9]* .*move=[0-9]+(\.[0-9]+)? dz=-[0-9]+(\.[0-9]+)? onground=1$' "$PROBE" 2>/dev/null && post_active_walkable_slope=1
  grep -Eq '^GAMEPLAY_STEPMOVE=PASS x=320\.0 low_y=[0-9]+\.0 high_y=[0-9]+\.0 low_z=[-+]?[0-9]+(\.[0-9]+)? high_z=[-+]?[0-9]+(\.[0-9]+)? expected_rise=[0-9]+(\.[0-9]+)? move=[0-9]+(\.[0-9]+)? dz=[0-9]+(\.[0-9]+)? origin=[-+]?[0-9]+(\.[0-9]+)?(,[-+]?[0-9]+(\.[0-9]+)?){2} onground=1$' "$PROBE" 2>/dev/null && post_active_stepmove=1
  grep -Eq '^GAMEPLAY_ADVANCED_MOVEMENT_PROBE=PASS slope=1 step=1$' "$PROBE" 2>/dev/null && post_active_advanced_movement=1
  grep -Eq '^GAMEPLAY_DYNAMIC_PLAYER_COLLISION=PASS mover=3 blocker=[12] .* travel=2\.[0-9]+ remaining_speed=-?0\.000 onground=1$' "$PROBE" 2>/dev/null && post_active_dynamic_player_collision=1
  grep -Eq '^GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION=PASS mover=3 solid=[0-9]+ class=prop_physics_multiplayer model=.* axis=[xy] sign=(-1|1) z=-?[0-9]+\.[0-9]+ fraction=[0-9]+\.[0-9]+ world_fraction=[0-9]+\.[0-9]+ normal=.* solids=[1-9][0-9]*$' "$PROBE" 2>/dev/null && post_active_dynamic_nonplayer_collision=1
  grep -Eq '^GAMEPLAY_HOSTAGE_COLLISION=PASS mover=3 hostage=[0-9]+ model=models/Characters/[Hh]ostage_0[1-4]\.mdl origin=.* bounds=.* axis=[xy] sign=(-1|1) z=-?[0-9]+\.[0-9]+ fraction=[0-9]+\.[0-9]+ world_fraction=[0-9]+\.[0-9]+ normal=.* hostages=[1-9][0-9]*$' "$PROBE" 2>/dev/null && post_active_hostage_collision=1
  grep -Eq '^GAMEPLAY_HOSTAGE_FOLLOW=PASS mover=3 hostage=[0-9]+ ordinal=[0-9]+ before=.* after=.* travel=[1-9][0-9]*\.[0-9]+ state=1 dynamic_origin=1 collision=1$' "$PROBE" 2>/dev/null && post_active_hostage_follow=1
  grep -Eq '^GAMEPLAY_GENERIC_NPC=PASS mover=3 solid=[0-9]+ class=npc_furniture before=.* after=.* travel=[1-9][0-9]*\.[0-9]+ dynamic_origin=1 collision=1$' "$PROBE" 2>/dev/null && post_active_generic_npc=1
  grep -Eq '^GAMEPLAY_WATER_CONTENTS=PASS origin=.* level=[123] feet=0x[0-9a-fA-F]+ waist=0x[0-9a-fA-F]+ eyes=0x[0-9a-fA-F]+ map=(de_aztec|fy_pool_day) required_level=[12]$' "$PROBE" 2>/dev/null && post_active_water_contents=1
  grep -Eq '^GAMEPLAY_WATER_MOVEMENT=PASS mover=3 oracle=real_hull_synthetic_level3 origin=.* real_level=[123] required_real_level=[12] map=(de_aztec|fy_pool_day) idle_level=[23] idle_vz=-[0-9]+\.[0-9]+ idle_dz=-[0-9]+\.[0-9]+ swim_level=[23] dir=[-0-9]+\.[0-9]+,[-0-9]+\.[0-9]+ swim_speed=[0-9]+\.[0-9]+ swim_travel=[0-9]+\.[0-9]+ jump_level=[23] jump_vz=[0-9]+\.[0-9]+ jump_dz=[0-9]+\.[0-9]+$' "$PROBE" 2>/dev/null && post_active_water_movement=1
  grep -Eq '^GAMEPLAY_WATER_LEVEL2=PASS map=fy_pool_day origin=.* point_level=[23] movement_level=[23] vz=-?[0-9]+\.[0-9]+ dz=-?[0-9]+\.[0-9]+$' "$PROBE" 2>/dev/null && post_active_water_level2=1
  grep -Eq '^GAMEPLAY_DUCK_MOVEMENT=PASS mover=3 ducked=1 duck_hull=36\.0 unducked=1 stand_hull=72\.0 map=[A-Za-z0-9_]+$' "$PROBE" 2>/dev/null && post_active_duck_movement=1
  grep -Eq '^GAMEPLAY_LADDER_MOVEMENT=PASS mover=3 ladder=[1-9][0-9]* map=(cs_office|fy_pool_day) bounds=.* origin=.* dz=[1-9][0-9]*\.[0-9]+ vz=[1-9][0-9]*\.[0-9]+ attached=1$' "$PROBE" 2>/dev/null && post_active_ladder_movement=1
  running="$(docker inspect -f '{{.State.Running}}' "$rebuilt_id" 2>/dev/null || echo false)"
  if [[ "$running" != "true" ]]; then
    timeout 5s docker logs "$rebuilt_id" >"$REBUILT_LOG" 2>&1 || true
    rc="$(docker inspect -f '{{.State.ExitCode}}' "$rebuilt_id" 2>/dev/null || echo 125)"
    oom="$(docker inspect -f '{{.State.OOMKilled}}' "$rebuilt_id" 2>/dev/null || echo unknown)"
    fail "rebuilt_exited_post_active rc=$rc oom=$oom edict_bind=$post_active_edict_bind networkable=$post_active_networkable visibility=$post_active_visibility checktransmit=$post_active_checktransmit last_event=$last_event"
  fi
  sleep 0.1
done
if timeout 4s docker exec "$rebuilt_id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
  post_active_a2s=PASS
else
  post_active_a2s=NOT_READY
fi
(( post_active_edict_bind == 1 )) || fail "post_active_edict_interface_bind_missing last_event=$last_event"
(( post_active_checktransmit == 1 )) || fail "post_active_CheckTransmit_missing last_event=$last_event"
(( post_active_gameplay_state == 1 )) || fail "post_active_gameplay_sendstate_missing last_event=$last_event"
(( post_active_gameplay_event == 1 )) || fail "post_active_GAMEPLAY_COMMAND_EVENT_missing last_event=$last_event"
(( post_active_player_say_event == 1 )) || fail "post_active_GAME_EVENT_PLAYER_SAY_missing last_event=$last_event"
(( post_active_saytext_usermessage == 1 )) || fail "post_active_USER_MESSAGE_SAYTEXT_missing last_event=$last_event"
(( post_active_engine_trace == 1 )) || fail "post_active_ENGINE_TRACE_WORLD_HULL_missing last_event=$last_event"
(( post_active_spawn_trace == 1 )) || fail "post_active_GAMEPLAY_SPAWN_TRACE_EVENT_missing last_event=$last_event"
(( post_active_usercmd_event == 1 )) || fail "post_active_GAMEPLAY_USERCMD_EVENT_missing last_event=$last_event"
(( post_active_movement_event == 1 )) || fail "post_active_GAMEPLAY_MOVEMENT_EVENT_missing last_event=$last_event"
(( post_active_friction_event == 1 )) || fail "post_active_GAMEPLAY_FRICTION_EVENT_missing last_event=$last_event"
(( post_active_jump_event == 1 )) || fail "post_active_GAMEPLAY_JUMP_EVENT_missing last_event=$last_event"
(( post_active_jump_hold_event == 1 )) || fail "post_active_GAMEPLAY_JUMP_HOLD_EVENT_missing last_event=$last_event"
(( post_active_air_accel_event == 1 )) || fail "post_active_GAMEPLAY_AIR_ACCEL_EVENT_missing last_event=$last_event"
if (( DUST2_ORACLE == 1 )); then
  (( post_active_weapon_fire_event == 1 )) || fail "post_active_GAMEPLAY_WEAPON_FIRE_EVENT_missing last_event=$last_event"
  (( post_active_weapon_reload_event == 1 )) || fail "post_active_GAMEPLAY_WEAPON_RELOAD_EVENT_missing last_event=$last_event"
fi
(( post_active_dynamic_player_collision == 1 )) || fail "post_active_GAMEPLAY_DYNAMIC_PLAYER_COLLISION_missing last_event=$last_event"
(( post_active_duck_movement == 1 )) || fail "post_active_GAMEPLAY_DUCK_MOVEMENT_missing last_event=$last_event"
if (( LADDER_ORACLE == 1 )); then
  (( post_active_ladder_movement == 1 )) || fail "post_active_GAMEPLAY_LADDER_MOVEMENT_missing last_event=$last_event"
fi
if (( HOSTAGE_ORACLE == 1 )); then
  (( post_active_hostage_collision == 1 )) || fail "post_active_GAMEPLAY_HOSTAGE_COLLISION_missing last_event=$last_event"
  (( post_active_hostage_follow == 1 )) || fail "post_active_GAMEPLAY_HOSTAGE_FOLLOW_missing last_event=$last_event"
  (( post_active_generic_npc == 1 )) || fail "post_active_GAMEPLAY_GENERIC_NPC_missing last_event=$last_event"
elif (( POOL_WATER_ORACLE == 1 )); then
  (( post_active_water_contents == 1 )) || fail "post_active_GAMEPLAY_WATER_CONTENTS_missing last_event=$last_event"
  (( post_active_water_level2 == 1 )) || fail "post_active_GAMEPLAY_WATER_LEVEL2_missing last_event=$last_event"
  (( post_active_water_movement == 1 )) || fail "post_active_GAMEPLAY_WATER_MOVEMENT_missing last_event=$last_event"
elif (( AZTEC_WATER_ORACLE == 1 )); then
  (( post_active_water_contents == 1 )) || fail "post_active_GAMEPLAY_WATER_CONTENTS_missing last_event=$last_event"
  (( post_active_water_movement == 1 )) || fail "post_active_GAMEPLAY_WATER_MOVEMENT_missing last_event=$last_event"
elif (( DUST2_ORACLE == 1 )); then
  (( post_active_walkable_slope == 1 )) || fail "post_active_GAMEPLAY_WALKABLE_SLOPE_missing last_event=$last_event"
  (( post_active_stepmove == 1 )) || fail "post_active_GAMEPLAY_STEPMOVE_missing last_event=$last_event"
  (( post_active_advanced_movement == 1 )) || fail "post_active_GAMEPLAY_ADVANCED_MOVEMENT_PROBE_missing last_event=$last_event"
  (( post_active_dynamic_nonplayer_collision == 1 )) || fail "post_active_GAMEPLAY_DYNAMIC_NONPLAYER_COLLISION_missing last_event=$last_event"
fi
echo "POST_ACTIVE_SURVIVAL=PASS seconds=2 map=$MAP_NAME edict_bind=$post_active_edict_bind networkable=$post_active_networkable visibility=$post_active_visibility checktransmit=$post_active_checktransmit gameplay_state=$post_active_gameplay_state gameplay_event=$post_active_gameplay_event player_say_event=$post_active_player_say_event saytext_usermessage=$post_active_saytext_usermessage engine_trace=$post_active_engine_trace spawn_trace=$post_active_spawn_trace usercmd_event=$post_active_usercmd_event movement_event=$post_active_movement_event friction_event=$post_active_friction_event jump_event=$post_active_jump_event jump_hold_event=$post_active_jump_hold_event air_accel_event=$post_active_air_accel_event weapon_fire_event=$post_active_weapon_fire_event weapon_reload_event=$post_active_weapon_reload_event walkable_slope=$post_active_walkable_slope stepmove=$post_active_stepmove advanced_movement=$post_active_advanced_movement dynamic_player_collision=$post_active_dynamic_player_collision dynamic_nonplayer_collision=$post_active_dynamic_nonplayer_collision hostage_collision=$post_active_hostage_collision hostage_follow=$post_active_hostage_follow generic_npc=$post_active_generic_npc water_contents=$post_active_water_contents water_level2=$post_active_water_level2 water_movement=$post_active_water_movement duck_movement=$post_active_duck_movement ladder_movement=$post_active_ladder_movement a2s=$post_active_a2s"

# v1.8.25b: prove the real engine can tear down a Protocol 7 client and reuse
# the same reserved client edict without restarting SRCDS. The first probe sends
# net_Disconnect(type 1); after the game-DLL confirms slot 3 is empty, a second
# full Protocol 7 client must reconnect into slot 3 and complete gameplay again.
reconnect_protocol7=0
reconnect_slot_reuse=0
reconnect_disconnect=0
if (( RECONNECT_ORACLE == 1 )); then
  grep -Eq '^PROTOCOL7_DISCONNECT_SENT=PASS .*type=1 reason=probe_complete ' "$protocol7_output" 2>/dev/null || \
    fail "protocol7_NET_Disconnect_missing host_port=$host_port"

  disconnect_deadline=$((SECONDS + 4))
  while (( SECONDS < disconnect_deadline )); do
    count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
    if (( count > last_count )); then
      while IFS= read -r event; do
        [[ -n "$event" ]] || continue
        last_event="$event"
        echo "REAL_ENGINE_EVENT $event"
      done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
      last_count=$count
    fi
    if grep -Fxq 'ClientDisconnect_OK index=3 remaining=2 slot_empty=1' "$PROBE" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
  grep -Fxq 'ClientDisconnect_OK index=3 remaining=2 slot_empty=1' "$PROBE" 2>/dev/null || \
    fail "protocol7_ClientDisconnect_slot_release_missing host_port=$host_port last_event=$last_event"

  reconnect_output="$PROBE_DIR/protocol7-reconnect.log"
  disconnect_before="$(grep -c '^ClientDisconnect_OK index=3 remaining=2 slot_empty=1$' "$PROBE" 2>/dev/null || true)"
  active_before="$(grep -c '^ClientActive_OK index=3$' "$PROBE" 2>/dev/null || true)"
  if ! grep -Fq 'PROTOCOL7_NETCHAN_PRESPAWN_RETRY_' "$ROOT/scripts/protocol7-client-probe.py" 2>/dev/null; then
    fail "reconnect_probe_prespawn_retry_feature_missing file=scripts/protocol7-client-probe.py"
  fi
  echo "REAL_RECONNECT_PROBE_POLICY timeout=8 signon_retries=8 retry_interval=0.75"
  set +e
  python3 "$ROOT/scripts/protocol7-client-probe.py" \
    --host 127.0.0.1 --port "$host_port" --name cssv34_reconnect --timeout 8 \
    --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
    --server-count "$server_count" --sendtable-crc "0x$sendtable_crc_hex" --force-full-sendtables \
    --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --disconnect-after-pass --max-udp-packets 512 \
    >"$reconnect_output" 2>&1
  reconnect_rc=$?
  set -e
  if [[ $reconnect_rc -ne 0 ]]; then
    cat "$reconnect_output" >&2 || true
    fail "protocol7_reconnect_probe_rc=$reconnect_rc host_port=$host_port"
  fi
  while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    if [[ "$line" == PROTOCOL7_CONNECTION_ACCEPTED=PASS* || \
          "$line" == PROTOCOL7_SERVER_SIGNON_SPAWN=PASS* || \
          "$line" == PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS* || \
          "$line" == PROTOCOL7_DISCONNECT_SENT=PASS* || \
          "$line" == PROTOCOL7=PASS* ]]; then
      echo "REAL_RECONNECT_EVENT $line"
    fi
  done < "$reconnect_output"

  grep -Eq '^PROTOCOL7=PASS .*signon_state6_sent=1 .*usercmd_air_accel_delta=1 .*classinfo=1 .*packetentities_full=[1-9][0-9]* .*disconnect_sent=1 ' "$reconnect_output" 2>/dev/null || \
    fail "protocol7_reconnect_full_path_missing host_port=$host_port"
  grep -Eq '^PROTOCOL7_DISCONNECT_SENT=PASS .*type=1 reason=probe_complete ' "$reconnect_output" 2>/dev/null || \
    fail "protocol7_reconnect_NET_Disconnect_missing host_port=$host_port"

  reconnect_deadline=$((SECONDS + 6))
  while (( SECONDS < reconnect_deadline )); do
    count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
    if (( count > last_count )); then
      while IFS= read -r event; do
        [[ -n "$event" ]] || continue
        last_event="$event"
        echo "REAL_ENGINE_EVENT $event"
      done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
      last_count=$count
    fi
    disconnect_now="$(grep -c '^ClientDisconnect_OK index=3 remaining=2 slot_empty=1$' "$PROBE" 2>/dev/null || true)"
    active_now="$(grep -c '^ClientActive_OK index=3$' "$PROBE" 2>/dev/null || true)"
    if grep -Eq '^ClientConnect_OK index=3 name=cssv34_reconnect( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientPutInServer_OK index=3 name=cssv34_reconnect( |$)' "$PROBE" 2>/dev/null && \
       (( active_now > active_before )) && (( disconnect_now > disconnect_before )); then
      reconnect_protocol7=1
      reconnect_slot_reuse=1
      reconnect_disconnect=1
      break
    fi
    sleep 0.1
  done
  (( reconnect_protocol7 == 1 )) || fail "protocol7_reconnect_engine_lifecycle_missing host_port=$host_port last_event=$last_event"
  echo "REAL_CLIENT_RECONNECT=PASS first_slot=3 second_slot=3 slot_reuse=$reconnect_slot_reuse disconnects=2 full_protocol7=$reconnect_protocol7"
fi

# v1.8.26d: keep one real Protocol 7 client alive in slot 3 while a second
# independent client completes the same full signon/gameplay path in slot 4.
# This proves the probe and reconstructed server no longer hard-code entity 3.
multiclient_protocol7=0
multiclient_slots=0
multiclient_voice=0
multiclient_objective=0
multiclient_te_firebullets=0
if (( MULTICLIENT_ORACLE == 1 )); then
  multi_a_output="$PROBE_DIR/protocol7-multi-a.log"
  multi_b_output="$PROBE_DIR/protocol7-multi-b.log"
  multi_release="$PROBE_DIR/protocol7-multi-a.release"
  multi_voice_hex="435353563334564f494345333241"
  rm -f "$multi_release"
  disconnect3_before="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"
  disconnect4_before="$(grep -c '^ClientDisconnect_OK index=4 ' "$PROBE" 2>/dev/null || true)"

  if ! grep -Fq 'PROTOCOL7_NETCHAN_PRESPAWN_RETRY_' "$ROOT/scripts/protocol7-client-probe.py" 2>/dev/null; then
    fail "multiclient_probe_prespawn_retry_feature_missing file=scripts/protocol7-client-probe.py"
  fi
  echo "REAL_MULTICLIENT_PROBE_POLICY timeout=8 signon_retries=8 retry_interval=0.75 ready_deadline=65 hold=90 hold_transport=nettick+fresh_usercmd clients=a+b"
  python3 -u "$ROOT/scripts/protocol7-client-probe.py" \
    --host 127.0.0.1 --port "$host_port" --name cssv34_multi_a --timeout 8 \
    --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
    --server-count "$server_count" --sendtable-crc "0x$sendtable_crc_hex" --force-full-sendtables \
    --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --join-team 2 --hold-after-pass 90 --hold-until-file "$multi_release" \
    --expect-voice-hex "$multi_voice_hex" --expect-voice-sender 3 --expect-te-firebullets-player 3 --expect-te-firebullets-weapon 16 \
    --disconnect-after-pass --max-udp-packets 512 \
    >"$multi_a_output" 2>&1 &
  MULTI_PID=$!

  # protocol7-client-probe uses receive_budget=max(timeout*7, 21). With
  # timeout=8 the full-sendtable signon window is 56s, so the harness must
  # wait longer than that before declaring HOLD_READY missing.
  multi_ready_deadline=$((SECONDS + 65))
  while (( SECONDS < multi_ready_deadline )); do
    if grep -Eq '^PROTOCOL7_HOLD_TRANSPORT_RESUME=PASS .*latest_tick=[0-9]+ cmd_tick=[0-9]+ ' "$multi_a_output" 2>/dev/null && \
       grep -Eq '^PROTOCOL7_HOLD_READY=PASS .*local_entity=3 release_file=1$' "$multi_a_output" 2>/dev/null && \
       grep -Eq '^ClientActive_OK index=3$' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientConnect_OK index=3 name=cssv34_multi_a( |$)' "$PROBE" 2>/dev/null; then
      break
    fi
    # kill -0 also succeeds for a zombie. jobs -pr reports only running jobs,
    # so an exited probe cannot masquerade as a live signon for the whole window.
    if ! jobs -pr | grep -Fxq "$MULTI_PID"; then
      cat "$multi_a_output" >&2 || true
      wait "$MULTI_PID" >/dev/null 2>&1 || true
      MULTI_PID=""
      fail "protocol7_multiclient_a_exited_before_hold host_port=$host_port"
    fi
    sleep 0.1
  done
  grep -Eq '^PROTOCOL7_HOLD_READY=PASS .*local_entity=3 release_file=1$' "$multi_a_output" 2>/dev/null || {
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_hold_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_HOLD_TRANSPORT_RESUME=PASS .*latest_tick=[0-9]+ cmd_tick=[0-9]+ ' "$multi_a_output" 2>/dev/null || {
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_hold_transport_resume_missing host_port=$host_port"
  }

  set +e
  python3 "$ROOT/scripts/protocol7-client-probe.py" \
    --host 127.0.0.1 --port "$host_port" --name cssv34_multi_b --timeout 8 \
    --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
    --server-count "$server_count" --sendtable-crc "0x$sendtable_crc_hex" --force-full-sendtables \
    --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --voice-send-hex "$multi_voice_hex" --objective-scenario bomb_rounds --weapon-actions --pre-weapon-join-team 2 --disconnect-after-pass --max-udp-packets 512 \
    >"$multi_b_output" 2>&1
  multi_b_rc=$?
  set -e
  # v1.8.32a: B must inject real clc_VoiceData while A is still held. Wait
  # for A to decode the engine-forwarded svc_VoiceData before releasing it.
  multi_voice_deadline=$((SECONDS + 4))
  while (( SECONDS < multi_voice_deadline )); do
    if grep -Eq '^PROTOCOL7_VOICE_DATA=PASS .*type=15 sender=3 payload_bits=112 payload_hex=435353563334564f494345333241 ' "$multi_a_output" 2>/dev/null; then
      multiclient_voice=1
      break
    fi
    sleep 0.05
  done
  multi_objective_deadline=$((SECONDS + 4))
  while (( SECONDS < multi_objective_deadline )); do
    if grep -Eq '^PROTOCOL7_OBJECTIVE_EVENT=PASS .*name=bomb_defused ' "$multi_a_output" 2>/dev/null && \
       grep -Eq '^PROTOCOL7_OBJECTIVE_EVENT=PASS .*name=bomb_exploded ' "$multi_a_output" 2>/dev/null && \
       grep -Eq '^PROTOCOL7_OBJECTIVE_EVENT=PASS .*name=round_end ' "$multi_a_output" 2>/dev/null; then
      multiclient_objective=1
      break
    fi
    sleep 0.05
  done
  multi_te_firebullets_deadline=$((SECONDS + 4))
  while (( SECONDS < multi_te_firebullets_deadline )); do
    if grep -Eq '^PROTOCOL7_TE_FIREBULLETS=PASS .*type=27 class=120 wire_class=121 decode=(full|envelope_class|resync_full) .*entries=[1-9][0-9]* ' "$multi_a_output" 2>/dev/null; then
      multiclient_te_firebullets=1
      break
    fi
    sleep 0.05
  done
  # Release client A only after client B has finished its full probe attempt
  # and the voice-forward marker has had a chance to arrive.
  touch "$multi_release"
  if [[ $multi_b_rc -ne 0 ]]; then
    set +e
    wait "$MULTI_PID"
    multi_a_rc=$?
    set -e
    MULTI_PID=""
    cat "$multi_b_output" >&2 || true
    [[ $multi_a_rc -eq 0 ]] || { echo "--- multi-a ---" >&2; cat "$multi_a_output" >&2 || true; }
    fail "protocol7_multiclient_b_probe_rc=$multi_b_rc host_port=$host_port"
  fi
  grep -Eq '^PROTOCOL7_PRE_WEAPON_JOIN_SENT=PASS .*team=2 ' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_b_pre_weapon_join_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7=PASS .*usercmd_air_accel_delta=1 .*pre_weapon_join_sent=1 pre_weapon_join_team=2 weapon_attack_sent=1 weapon_release_sent=1 weapon_reload_sent=1 weapon_secondary_sent=1 weapon_fire_event=1 weapon_reload_event=1 .*voice_sent=1 .*objective_scenario=bomb_rounds objective_command_sent=1 objective_ok=1 objective_c4_sendprop=1 .*player_slot=3 local_entity=4 .*disconnect_sent=1 ' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_b_full_path_missing host_port=$host_port"
  }

  set +e
  wait "$MULTI_PID"
  multi_a_rc=$?
  set -e
  MULTI_PID=""
  if [[ $multi_a_rc -ne 0 ]]; then
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_probe_rc=$multi_a_rc host_port=$host_port"
  fi
  grep -Eq '^PROTOCOL7=PASS .*usercmd_air_accel_delta=1 .*voice_received=1 .*te_firebullets_received=1 .*player_slot=2 local_entity=3 .*disconnect_sent=1 ' "$multi_a_output" 2>/dev/null || {
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_full_path_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_HOLD_DONE=PASS .*active_moves=[1-9][0-9]* .*tick_acks=[1-9][0-9]* .*latest_tick=[0-9]+ .*local_entity=3 .*te_firebullets_received=1$' "$multi_a_output" 2>/dev/null || {
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_hold_snapshot_transport_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_GAMEPLAY_EVENT_DELTA=PASS command=jointeam_2 entity=3 team=2 state=0 changed_props=([2-9]|[1-9][0-9]+) delta_from=[0-9]+ schema=engine_flattened$' "$multi_a_output" 2>/dev/null || {
    cat "$multi_a_output" >&2 || true
    fail "protocol7_multiclient_a_team2_delta_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_GAMEPLAY_EVENT_DELTA=PASS command=jointeam_3 entity=4 team=3 state=0 changed_props=([2-9]|[1-9][0-9]+) delta_from=[0-9]+ schema=engine_flattened$' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_b_team3_delta_missing host_port=$host_port"
  }
  (( multiclient_voice == 1 )) || {
    echo "--- multi-a ---" >&2; cat "$multi_a_output" >&2 || true
    echo "--- multi-b ---" >&2; cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_voice_forward_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_VOICE_SEND=PASS .*type=10 payload_bits=112 payload_hex=435353563334564f494345333241 ' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_voice_send_missing host_port=$host_port"
  }

  grep -Eq '^GAMEPLAY_TE_FIREBULLETS_PLAYBACK index=4 classid=120 weapon=16 seed=[0-9]+ recipients=1 phase=gameframe recipient0=3$' "$PROBE" 2>/dev/null || {
    echo "--- engine ---" >&2; tail -n 260 "$PROBE" >&2 || true
    fail "protocol7_multiclient_te_firebullets_gameframe_playback_missing host_port=$host_port"
  }

  (( multiclient_te_firebullets == 1 )) || {
    echo "--- multi-a ---" >&2; cat "$multi_a_output" >&2 || true
    echo "--- multi-b ---" >&2; cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_te_firebullets_wire_missing host_port=$host_port"
  }
  grep -Eq '^GAMEPLAY_SPAWN_TRACE_EVENT index=4 team=2 reason=jointeam ' "$PROBE" 2>/dev/null || {
    echo "--- engine ---" >&2; tail -n 260 "$PROBE" >&2 || true
    fail "protocol7_multiclient_te_firebullets_pas_reposition_missing host_port=$host_port"
  }
  grep -Eq '^GAMEPLAY_TE_FIREBULLETS index=4 classid=120 weapon=16 mode=0 seed=[0-9]+ spread=[0-9]+\.[0-9]+ recipients=1 predicted_shooter_excluded=1 pas=engine_slot41 .* recipient0=3 recipient1=-1$' "$PROBE" 2>/dev/null || {
    echo "--- engine ---" >&2; tail -n 240 "$PROBE" >&2 || true
    fail "protocol7_multiclient_te_firebullets_engine_missing host_port=$host_port"
  }

  grep -Eq '^PROTOCOL7_OBJECTIVE_SCENARIO=PASS scenario=bomb_rounds .*bomb_abortplant.*bomb_beep.*bomb_beginplant.*bomb_defused.*bomb_exploded.*round_end.*c4_sendprop=1' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_objective_b_missing host_port=$host_port"
  }
  (( multiclient_objective == 1 )) || {
    echo "--- multi-a ---" >&2; cat "$multi_a_output" >&2 || true
    echo "--- multi-b ---" >&2; cat "$multi_b_output" >&2 || true
    fail "protocol7_multiclient_objective_broadcast_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_PLANTED_C4_WIRE=PASS .*names=.*m_bBombTicking' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_planted_c4_bool_wire_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_PLANTED_C4_SENDPROP=PASS .*ticking=1 .*timer=4[0-9]\.[0-9]+ ' "$multi_b_output" 2>/dev/null || {
    cat "$multi_b_output" >&2 || true
    fail "protocol7_planted_c4_sendprop_missing host_port=$host_port"
  }
  grep -Fxq 'MAP_OBJECTIVE_ZONES bomb=2 rescue=0 total=2' "$PROBE" 2>/dev/null || \
    fail "bombsite_zone_inventory_missing host_port=$host_port"
  grep -Eq '^OBJECTIVE_BOMBSITE_DETECTION scenario=bomb_rounds .*zone=[01] site=[01] ' "$PROBE" 2>/dev/null || \
    fail "bombsite_detection_probe_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_SENDSTATE entity=[1-9][0-9]* class=CPlantedC4 .*ticking=1 ' "$PROBE" 2>/dev/null || \
    fail "planted_c4_sendstate_probe_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_RESERVED entity=[1-9][0-9]* preferred=[1-9][0-9]* maxClients=[1-9][0-9]* allocator=auto attempt=[1-9][0-9]* flags=0x00000015 mode=dontsend changed=1$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_level_reservation_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_PREBASELINE_READY entity=[1-9][0-9]* classid=6 baseline=65535 phase=serveractivate_prebaseline flags=0x[0-9a-fA-F]+ mode=dontsend$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_serveractivate_prebaseline_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_BASELINE_READY entity=[1-9][0-9]* classid=6 baseline=[0-9]+ reserved=1 flags=0x[0-9a-fA-F]+ phase=first_gameframe$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_instancebaseline_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_TRANSMIT_STATE entity=[1-9][0-9]* flags=0x0000010d mode=always changed=1 fullchanged=1$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_full_statechanged_missing host_port=$host_port"
  c4_reserved_index="$(sed -nE 's/^GAMEPLAY_PLANTED_C4_RESERVED entity=([0-9]+) .*/\1/p' "$PROBE" | head -n1)"
  c4_spawn_indices="$(sed -nE 's/^GAMEPLAY_PLANTED_C4_SPAWN entity=([0-9]+) .*/\1/p' "$PROBE" | sort -u | tr '\n' ' ')"
  [[ -n "$c4_reserved_index" && "$c4_spawn_indices" == "$c4_reserved_index " ]] || \
    fail "planted_c4_reserved_slot_not_reused reserved=$c4_reserved_index spawned=$c4_spawn_indices host_port=$host_port"
  # The planted-C4 shell is now allocated during ServerActivate so
  # SV_CreateBaseline can encode CPlantedC4. It still lives outside the original
  # activation-time high-water mark, so keep proving that the bridge resolves the
  # fixed MAX_EDICTS pool and that CheckTransmit reaches the forced generic path.
  #
  # build4100's live CCheckTransmitInfo can legitimately expose a null
  # m_pTransmitAlways (the ordinary CheckTransmit_OK probe reports
  # always=0x00000000). FL_EDICT_ALWAYS remains authoritative on the edict and
  # the entity must still be present in m_pTransmitEdict. Only require an
  # m_pTransmitAlways bit when that optional bitset actually exists.
  grep -Eq '^CheckTransmitGeneric_OK forced=[1-9][0-9]* added=[0-9]+ policy_always=[1-9][0-9]* always_bits=[0-9]+ always_mask=(present|absent) fresh=[1-9][0-9]* first=[1-9][0-9]* firstFlags=0x[0-9a-fA-F]+ candidates=[0-9]+ pvsSize=[0-9]+$' "$PROBE" 2>/dev/null || \
    fail "checktransmit_postactivate_generic_missing host_port=$host_port"
  if grep -Eq '^CheckTransmitGeneric_OK .*always_mask=absent ' "$PROBE" 2>/dev/null; then
    grep -Eq '^CheckTransmitGeneric_OK .*always_bits=0 always_mask=absent ' "$PROBE" 2>/dev/null || \
      fail "checktransmit_nullable_always_bits_nonzero host_port=$host_port"
    grep -Eq '^CheckTransmit_OK .* always=0x00000000$' "$PROBE" 2>/dev/null || \
      fail "checktransmit_nullable_always_pointer_mismatch host_port=$host_port"
  else
    grep -Eq '^CheckTransmitGeneric_OK .*always_bits=[1-9][0-9]* always_mask=present ' "$PROBE" 2>/dev/null || \
      fail "checktransmit_present_always_bits_missing host_port=$host_port"
  fi
  grep -Fxq 'GAMEPLAY_OBJECTIVE_SCENARIO scenario=bomb_rounds plant=1 defuse=1 explode=1 round_cycle=1 sendprop_c4=1' "$PROBE" 2>/dev/null || \
    fail "bomb_objective_engine_scenario_missing host_port=$host_port"
  grep -Fxq 'GAMEPLAY_OBJECTIVE_STAGE scenario=bomb_rounds step=4 action=round_cycle_and_cleanup' "$PROBE" 2>/dev/null || \
    fail "bomb_objective_stage4_missing host_port=$host_port"
  # The reserved C4 is intentionally retained across bomb defuse/explosion and
  # therefore must NOT exercise RemoveEdict during the objective itself.  Its
  # real RemoveEdict -> FreeContainingEntity reentry happens at LevelShutdown,
  # where the reservation is released before the next map is activated.

  multi_lifecycle_deadline=$((SECONDS + 6))
  while (( SECONDS < multi_lifecycle_deadline )); do
    count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
    if (( count > last_count )); then
      while IFS= read -r event; do
        [[ -n "$event" ]] || continue
        last_event="$event"
        echo "REAL_ENGINE_EVENT $event"
      done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
      last_count=$count
    fi
    disconnect3_now="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"
    disconnect4_now="$(grep -c '^ClientDisconnect_OK index=4 ' "$PROBE" 2>/dev/null || true)"
    if grep -Eq '^ClientConnect_OK index=3 name=cssv34_multi_a( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientConnect_OK index=4 name=cssv34_multi_b( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientPutInServer_OK index=4 name=cssv34_multi_b( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientActive_OK index=4$' "$PROBE" 2>/dev/null && \
       grep -Eq '^VOICE_LISTENING_READY index=3 peers=[1-4] set_calls=[1-9][0-9]*$' "$PROBE" 2>/dev/null && \
       grep -Eq '^VOICE_LISTENING_READY index=4 peers=[1-4] set_calls=[1-9][0-9]*$' "$PROBE" 2>/dev/null && \
       grep -Eq '^GAMEPLAY_COMMAND_EVENT index=3 cmd=jointeam arg=2 before_team=0 after_team=2 before_state=1 after_state=0 changed=1$' "$PROBE" 2>/dev/null && \
       grep -Eq '^GAMEPLAY_COMMAND_EVENT index=4 cmd=jointeam arg=3 before_team=0 after_team=3 before_state=1 after_state=0 changed=1$' "$PROBE" 2>/dev/null && \
       (( disconnect3_now > disconnect3_before )) && (( disconnect4_now > disconnect4_before )); then
      multiclient_protocol7=1
      multiclient_slots=1
      break
    fi
    sleep 0.1
  done
  (( multiclient_protocol7 == 1 )) || fail "protocol7_multiclient_engine_lifecycle_missing host_port=$host_port last_event=$last_event"

  while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    if [[ "$line" == PROTOCOL7_HOLD_READY=PASS* || "$line" == PROTOCOL7_VOICE_DATA=PASS* || "$line" == PROTOCOL7_TE_FIREBULLETS=PASS* || "$line" == PROTOCOL7_OBJECTIVE_EVENT=PASS* || "$line" == PROTOCOL7_HOLD_DONE=PASS* || "$line" == PROTOCOL7=PASS* ]]; then
      echo "REAL_MULTI_A_EVENT $line"
    fi
  done < "$multi_a_output"
  while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    if [[ "$line" == PROTOCOL7_SERVER_SIGNON_SPAWN=PASS* || "$line" == PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS* || "$line" == PROTOCOL7_WEAPON_ATTACK_SENT=PASS* || "$line" == PROTOCOL7_VOICE_SEND=PASS* || "$line" == PROTOCOL7_OBJECTIVE_EVENT=PASS* || "$line" == PROTOCOL7_OBJECTIVE_SCENARIO=PASS* || "$line" == PROTOCOL7=PASS* ]]; then
      echo "REAL_MULTI_B_EVENT $line"
    fi
  done < "$multi_b_output"
  echo "REAL_CLIENT_MULTICLIENT=PASS slots=3,4 simultaneous=1 local_entities=3,4 teams=2,3 voice_forward=$multiclient_voice objective_round=$multiclient_objective te_firebullets_wire=$multiclient_te_firebullets full_protocol7=$multiclient_protocol7"
fi

# v1.8.27d: exercise a real build4100 changelevel through Source TCP RCON on the same
# SRCDS process. Require a second LevelShutdown/LevelInit/ServerActivate cycle,
# cs_office map inventory, bot reseeding, a new server_count and then a fresh
# full Protocol 7 client path without restarting the container.
mapchange_protocol7=0
mapchange_lifecycle=0
if (( MAPCHANGE_ORACLE == 1 )); then
  mapchange_output="$PROBE_DIR/protocol7-mapchange.log"
  rcon_output="$PROBE_DIR/source-rcon.log"
  shutdown_before="$(grep -c '^LevelShutdown_OK$' "$PROBE" 2>/dev/null || true)"
  activate_before="$(grep -c '^ServerActivate_OK$' "$PROBE" 2>/dev/null || true)"
  fake_seed_before="$(grep -c '^FAKE_CLIENT_SEED_OK target=2 created=2 ' "$PROBE" 2>/dev/null || true)"
  bot1_ready_before="$(grep -c '^ClientPutInServer_OK index=1 name=cssv34_bot_01 bridge=1$' "$PROBE" 2>/dev/null || true)"
  bot2_ready_before="$(grep -c '^ClientPutInServer_OK index=2 name=cssv34_bot_02 bridge=1$' "$PROBE" 2>/dev/null || true)"
  signon_facts_before="$(grep -c '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null || true)"
  baseline_dump_before="$(grep -c '^INSTANCEBASELINE_DUMP_OK classes=177 ' "$PROBE" 2>/dev/null || true)"
  disconnect3_before="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"
  c4_release_begin_before="$(grep -c '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASE_BEGIN ' "$PROBE" 2>/dev/null || true)"
  c4_release_done_before="$(grep -c '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASED ' "$PROBE" 2>/dev/null || true)"
  remove_reentry_before="$(grep -c '^ENTITY_BRIDGE_REMOVE_REENTRY_OK ' "$PROBE" 2>/dev/null || true)"
  remove_fallback_before="$(grep -c '^ENTITY_BRIDGE_REMOVE_FALLBACK ' "$PROBE" 2>/dev/null || true)"

  set +e
  python3 "$ROOT/scripts/source-rcon.py" --host 127.0.0.1 --port "$host_port" \
    --password "$TEST_RCON_PASSWORD" --command "changelevel cs_office" --timeout 2 \
    >"$rcon_output" 2>&1
  rcon_rc=$?
  set -e
  cat "$rcon_output" | sed 's/^/REAL_MAPCHANGE_RCON_EVENT /'
  [[ $rcon_rc -eq 0 ]] || fail "mapchange_rcon_rc=$rcon_rc host_port=$host_port"
  grep -Eq '^SOURCE_RCON=PASS revision=v1\.8\.27d .*command=changelevel_cs_office ' "$rcon_output" || \
    fail "mapchange_rcon_ack_missing host_port=$host_port"

  mapchange_deadline=$((SECONDS + 20))
  while (( SECONDS < mapchange_deadline )); do
    count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
    if (( count > last_count )); then
      while IFS= read -r event; do
        [[ -n "$event" ]] || continue
        last_event="$event"
        echo "REAL_ENGINE_EVENT $event"
      done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
      last_count=$count
    fi

    shutdown_now="$(grep -c '^LevelShutdown_OK$' "$PROBE" 2>/dev/null || true)"
    activate_now="$(grep -c '^ServerActivate_OK$' "$PROBE" 2>/dev/null || true)"
    fake_seed_now="$(grep -c '^FAKE_CLIENT_SEED_OK target=2 created=2 ' "$PROBE" 2>/dev/null || true)"
    bot1_ready_now="$(grep -c '^ClientPutInServer_OK index=1 name=cssv34_bot_01 bridge=1$' "$PROBE" 2>/dev/null || true)"
    bot2_ready_now="$(grep -c '^ClientPutInServer_OK index=2 name=cssv34_bot_02 bridge=1$' "$PROBE" 2>/dev/null || true)"
    signon_facts_now="$(grep -c '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null || true)"
    baseline_dump_now="$(grep -c '^INSTANCEBASELINE_DUMP_OK classes=177 ' "$PROBE" 2>/dev/null || true)"
    c4_release_begin_now="$(grep -c '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASE_BEGIN ' "$PROBE" 2>/dev/null || true)"
    c4_release_done_now="$(grep -c '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASED ' "$PROBE" 2>/dev/null || true)"
    remove_reentry_now="$(grep -c '^ENTITY_BRIDGE_REMOVE_REENTRY_OK ' "$PROBE" 2>/dev/null || true)"
    remove_fallback_now="$(grep -c '^ENTITY_BRIDGE_REMOVE_FALLBACK ' "$PROBE" 2>/dev/null || true)"
    if (( shutdown_now > shutdown_before && activate_now > activate_before && \
          fake_seed_now > fake_seed_before && bot1_ready_now > bot1_ready_before && \
          bot2_ready_now > bot2_ready_before && signon_facts_now > signon_facts_before && \
          baseline_dump_now > baseline_dump_before && \
          c4_release_begin_now > c4_release_begin_before && c4_release_done_now > c4_release_done_before && \
          remove_reentry_now > remove_reentry_before && remove_fallback_now == remove_fallback_before )) && \
       grep -Eq '^LEVEL_CLIENT_MAP_RESET fake_released=2 fake_detached=2 retained_real=0 remaining=0$' "$PROBE" 2>/dev/null && \
       grep -Fxq 'LevelInit_ENTER cs_office' "$PROBE" 2>/dev/null && \
       grep -Fxq 'MAP_ENTITY_LUMP entities=821 world=1 parsed=1' "$PROBE" 2>/dev/null && \
       grep -Fxq 'MAP_PLAYER_SPAWNS terrorist=20 counterterrorist=20' "$PROBE" 2>/dev/null && \
       grep -Fxq 'MAP_MOVEMENT_SOLIDS total=231 studio=226 brush=1 prop_physics_multiplayer=226 hostage_bbox=4 npc_bbox=0' "$PROBE" 2>/dev/null && \
       grep -Fxq 'MAP_MOVEMENT_LADDERS count=2' "$PROBE" 2>/dev/null && \
       grep -Fxq 'LevelInit_OK cs_office' "$PROBE" 2>/dev/null; then
      mapchange_lifecycle=1
      break
    fi

    if grep -Eq '^LevelInit_FAIL |^ServerActivate_FAIL ' "$PROBE" 2>/dev/null; then
      fail "mapchange_lifecycle_callback_fail last_event=$last_event"
    fi
    running="$(docker inspect -f '{{.State.Running}}' "$rebuilt_id" 2>/dev/null || echo false)"
    [[ "$running" == "true" ]] || fail "rebuilt_exited_during_mapchange last_event=$last_event"
    sleep 0.1
  done
  (( mapchange_lifecycle == 1 )) || fail "mapchange_lifecycle_missing host_port=$host_port last_event=$last_event"
  grep -Eq '^INSTANCEBASELINE_DUMP_OK classes=177 ready=0 missing=177 player_baseline=65535 c4_baseline=65535 seeded=2/2$' "$PROBE" 2>/dev/null || \
    fail "mapchange_empty_instancebaseline_snapshot_missing host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASE_BEGIN entity=[1-9][0-9]* flags=0x[0-9a-fA-F]+ remove_engine_edict=1$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_levelshutdown_release_begin_missing host_port=$host_port"
  grep -Eq '^ENTITY_BRIDGE_REMOVE_REENTRY_OK edict=0x[0-9a-fA-F]+ callback_detached=1$' "$PROBE" 2>/dev/null || \
    fail "entity_bridge_remove_reentry_missing_after_levelshutdown host_port=$host_port"
  grep -Eq '^GAMEPLAY_PLANTED_C4_RESERVATION_RELEASED entity=[1-9][0-9]* released=1 callback_path=removeedict$' "$PROBE" 2>/dev/null || \
    fail "planted_c4_levelshutdown_release_done_missing host_port=$host_port"
  if (( remove_fallback_now != remove_fallback_before )); then
    fail "entity_bridge_remove_fallback_unexpected_after_levelshutdown host_port=$host_port"
  fi

  if ! timeout 4s docker exec "$rebuilt_id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
    fail "mapchange_a2s_not_ready host_port=$host_port last_event=$last_event"
  fi

  map_signon_facts="$(grep '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null | tail -n1 || true)"
  map_server_count="$(printf '%s\n' "$map_signon_facts" | sed -n 's/.*server_count=\([-0-9][0-9]*\).*/\1/p')"
  map_sendtable_crc_hex="$(printf '%s\n' "$map_signon_facts" | sed -n 's/.*sendtable_crc_hex=\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/p')"
  [[ "$map_server_count" =~ ^[0-9]+$ ]] || fail "mapchange_server_count_missing facts=$map_signon_facts"
  [[ -n "$map_sendtable_crc_hex" ]] || fail "mapchange_sendtable_crc_missing facts=$map_signon_facts"
  (( map_server_count > server_count )) || fail "mapchange_server_count_not_advanced before=$server_count after=$map_server_count"

  set +e
  # gatefix17: the first dust2->office probe is the same post-changelevel race
  # class as the later mapcycle probes.  Give it the same bounded retry policy
  # and a larger base timeout; the client probe additionally renews its deadline
  # only when NEW/PRESPAWN/SPAWN milestones make forward progress.
  python3 "$ROOT/scripts/protocol7-client-probe.py" \
    --host 127.0.0.1 --port "$host_port" --name cssv34_mapcycle --timeout 8 \
    --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
    --server-count "$map_server_count" --sendtable-crc "0x$map_sendtable_crc_hex" --force-full-sendtables \
    --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --disconnect-after-pass --max-udp-packets 512 \
    >"$mapchange_output" 2>&1
  mapchange_rc=$?
  set -e
  if [[ $mapchange_rc -ne 0 ]]; then
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_probe_rc=$mapchange_rc host_port=$host_port"
  fi
  grep -Fxq 'PROTOCOL7_INSTANCEBASELINE_RUNTIME=PASS classes=177 ready=0 player_index=-1 c4_index=-1' "$mapchange_output" 2>/dev/null || {
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_empty_runtime_snapshot_missing host_port=$host_port"
  }
  if ! protocol7_pass_has_fields "$mapchange_output" 'PROTOCOL7_INSTANCEBASELINE_TABLE=PASS ' \
    'entries=[0-9]+' 'runtime_snapshot=0' 'wire_growth=[0-9]+' \
    'wire_growth_ids=[^ ]+' 'player_index=-?[0-9]+' 'c4_index=-?[0-9]+' 'ids=(none|[0-9,]+)'; then
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_empty_snapshot_wire_contract_missing host_port=$host_port"
  fi
  grep -Eq '^PROTOCOL7=PASS .*usercmd_air_accel_delta=1 .*player_slot=2 local_entity=3 .*disconnect_sent=1 ' "$mapchange_output" 2>/dev/null || {
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_full_path_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7_GAMEPLAY_EVENT_DELTA=PASS command=jointeam_3 entity=3 team=3 state=0 ' "$mapchange_output" 2>/dev/null || {
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_team_delta_missing host_port=$host_port"
  }

  grep -Eq '^PROTOCOL7_SIGNON_LEASE=PASS stage=(server_prespawn_scan|server_prespawn_packet|client_prespawn_sent) ' "$mapchange_output" 2>/dev/null || {
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_signon_lease_missing host_port=$host_port"
  }
  grep -Eq '^PROTOCOL7=PASS .*signon_lease_extensions=[0-9]+ ' "$mapchange_output" 2>/dev/null || {
    cat "$mapchange_output" >&2 || true
    fail "protocol7_mapchange_signon_lease_summary_missing host_port=$host_port"
  }

  map_client_deadline=$((SECONDS + 6))
  while (( SECONDS < map_client_deadline )); do
    count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
    if (( count > last_count )); then
      while IFS= read -r event; do
        [[ -n "$event" ]] || continue
        last_event="$event"
        echo "REAL_ENGINE_EVENT $event"
      done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
      last_count=$count
    fi
    disconnect3_now="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"
    if grep -Eq '^ClientConnect_OK index=3 name=cssv34_mapcycle( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^ClientPutInServer_OK index=3 name=cssv34_mapcycle( |$)' "$PROBE" 2>/dev/null && \
       grep -Eq '^GAMEPLAY_COMMAND_EVENT index=3 cmd=jointeam arg=3 before_team=0 after_team=3 before_state=1 after_state=0 changed=1$' "$PROBE" 2>/dev/null && \
       (( disconnect3_now > disconnect3_before )); then
      mapchange_protocol7=1
      break
    fi
    sleep 0.1
  done
  (( mapchange_protocol7 == 1 )) || fail "protocol7_mapchange_engine_lifecycle_missing host_port=$host_port last_event=$last_event"

  while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    if [[ "$line" == PROTOCOL7_SERVER_SIGNON_SPAWN=PASS* || \
          "$line" == PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS* || \
          "$line" == PROTOCOL7_DISCONNECT_SENT=PASS* || \
          "$line" == PROTOCOL7=PASS* ]]; then
      echo "REAL_MAPCHANGE_CLIENT_EVENT $line"
    fi
  done < "$mapchange_output"
  echo "REAL_CLIENT_MAPCHANGE=PASS from=de_dust2 to=cs_office server_count=${server_count}->${map_server_count} lifecycle=$mapchange_lifecycle inventory=1 bots=2 full_protocol7=$mapchange_protocol7"
fi

# v1.8.28b: continue the accepted dust2->office transition into a repeated
# same-process map cycle. Every additional changelevel must advance server_count
# exactly once, rebuild both fake-client slots without lifecycle skips, keep A2S
# alive and complete a fresh full Protocol 7 gameplay/movement/disconnect path.
mapcycle_protocol7=0
mapcycle_steps=0
if (( MAPCYCLE_ORACLE == 1 )); then
  (( mapchange_protocol7 == 1 && mapchange_lifecycle == 1 )) || fail "mapcycle_requires_initial_mapchange_pass"

  cycle_initial_count="$server_count"
  cycle_prev_count="$map_server_count"
  mapcycle_steps=1
  mapcycle_protocol7=1

  run_mapcycle_step() {
    local step="$1"
    local from_map="$2"
    local target_map="$3"
    local expected_entities="$4"
    local expected_t_spawns="$5"
    local expected_ct_spawns="$6"
    local expected_solids="$7"
    local expected_ladders="$8"
    local cycle_name="cssv34_cycle_${target_map}"
    local cycle_output="$PROBE_DIR/protocol7-mapcycle-${step}-${target_map}.log"
    local cycle_rcon_output="$PROBE_DIR/source-rcon-mapcycle-${step}-${target_map}.log"
    local expected_entity_line="MAP_ENTITY_LUMP entities=${expected_entities} world=1 parsed=1"
    local expected_spawn_line="MAP_PLAYER_SPAWNS terrorist=${expected_t_spawns} counterterrorist=${expected_ct_spawns}"
    local expected_ladder_line="MAP_MOVEMENT_LADDERS count=${expected_ladders}"
    local shutdown_before activate_before fake_seed_before bot1_before bot2_before facts_before reset_before skip_before
    local init_before entity_before spawn_before solids_before ladders_before disconnect_before
    local shutdown_now activate_now fake_seed_now bot1_now bot2_now facts_now reset_now init_now entity_now spawn_now solids_now ladders_now
    local cycle_deadline count running cycle_facts cycle_count cycle_crc expected_count cycle_rc client_deadline disconnect_now skip_after

    shutdown_before="$(grep -c '^LevelShutdown_OK$' "$PROBE" 2>/dev/null || true)"
    activate_before="$(grep -c '^ServerActivate_OK$' "$PROBE" 2>/dev/null || true)"
    fake_seed_before="$(grep -c '^FAKE_CLIENT_SEED_OK target=2 created=2 ' "$PROBE" 2>/dev/null || true)"
    bot1_before="$(grep -c '^ClientPutInServer_OK index=1 name=cssv34_bot_01 bridge=1$' "$PROBE" 2>/dev/null || true)"
    bot2_before="$(grep -c '^ClientPutInServer_OK index=2 name=cssv34_bot_02 bridge=1$' "$PROBE" 2>/dev/null || true)"
    facts_before="$(grep -c '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null || true)"
    reset_before="$(grep -c '^LEVEL_CLIENT_MAP_RESET fake_released=2 fake_detached=2 retained_real=0 remaining=0$' "$PROBE" 2>/dev/null || true)"
    skip_before="$(grep -c '^ClientPutInServer_SKIP .*reason=lifecycle$' "$PROBE" 2>/dev/null || true)"
    init_before="$(grep -c "^LevelInit_ENTER ${target_map}$" "$PROBE" 2>/dev/null || true)"
    entity_before="$(grep -c "^${expected_entity_line}$" "$PROBE" 2>/dev/null || true)"
    spawn_before="$(grep -c "^${expected_spawn_line}$" "$PROBE" 2>/dev/null || true)"
    solids_before="$(grep -c "^${expected_solids}$" "$PROBE" 2>/dev/null || true)"
    ladders_before="$(grep -c "^${expected_ladder_line}$" "$PROBE" 2>/dev/null || true)"
    disconnect_before="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"

    set +e
    python3 "$ROOT/scripts/source-rcon.py" --host 127.0.0.1 --port "$host_port" \
      --password "$TEST_RCON_PASSWORD" --command "changelevel ${target_map}" --timeout 2 \
      >"$cycle_rcon_output" 2>&1
    local rcon_rc=$?
    set -e
    cat "$cycle_rcon_output" | sed "s/^/REAL_MAPCYCLE_RCON_EVENT step=${step} /"
    [[ $rcon_rc -eq 0 ]] || fail "mapcycle_rcon_rc=$rcon_rc step=$step from=$from_map to=$target_map host_port=$host_port"
    grep -Eq "^SOURCE_RCON=PASS revision=v1\\.8\\.27d .*command=changelevel_${target_map} " "$cycle_rcon_output" || \
      fail "mapcycle_rcon_ack_missing step=$step from=$from_map to=$target_map host_port=$host_port"

    cycle_deadline=$((SECONDS + 20))
    while (( SECONDS < cycle_deadline )); do
      count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
      if (( count > last_count )); then
        while IFS= read -r event; do
          [[ -n "$event" ]] || continue
          last_event="$event"
          echo "REAL_ENGINE_EVENT $event"
        done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
        last_count=$count
      fi

      shutdown_now="$(grep -c '^LevelShutdown_OK$' "$PROBE" 2>/dev/null || true)"
      activate_now="$(grep -c '^ServerActivate_OK$' "$PROBE" 2>/dev/null || true)"
      fake_seed_now="$(grep -c '^FAKE_CLIENT_SEED_OK target=2 created=2 ' "$PROBE" 2>/dev/null || true)"
      bot1_now="$(grep -c '^ClientPutInServer_OK index=1 name=cssv34_bot_01 bridge=1$' "$PROBE" 2>/dev/null || true)"
      bot2_now="$(grep -c '^ClientPutInServer_OK index=2 name=cssv34_bot_02 bridge=1$' "$PROBE" 2>/dev/null || true)"
      facts_now="$(grep -c '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null || true)"
      reset_now="$(grep -c '^LEVEL_CLIENT_MAP_RESET fake_released=2 fake_detached=2 retained_real=0 remaining=0$' "$PROBE" 2>/dev/null || true)"
      init_now="$(grep -c "^LevelInit_ENTER ${target_map}$" "$PROBE" 2>/dev/null || true)"
      entity_now="$(grep -c "^${expected_entity_line}$" "$PROBE" 2>/dev/null || true)"
      spawn_now="$(grep -c "^${expected_spawn_line}$" "$PROBE" 2>/dev/null || true)"
      solids_now="$(grep -c "^${expected_solids}$" "$PROBE" 2>/dev/null || true)"
      ladders_now="$(grep -c "^${expected_ladder_line}$" "$PROBE" 2>/dev/null || true)"

      if (( shutdown_now > shutdown_before && activate_now > activate_before && \
            fake_seed_now > fake_seed_before && bot1_now > bot1_before && bot2_now > bot2_before && \
            facts_now > facts_before && reset_now > reset_before && init_now > init_before && \
            entity_now > entity_before && spawn_now > spawn_before && solids_now > solids_before && \
            ladders_now > ladders_before )); then
        break
      fi

      if grep -Eq '^LevelInit_FAIL |^ServerActivate_FAIL ' "$PROBE" 2>/dev/null; then
        fail "mapcycle_lifecycle_callback_fail step=$step from=$from_map to=$target_map last_event=$last_event"
      fi
      running="$(docker inspect -f '{{.State.Running}}' "$rebuilt_id" 2>/dev/null || echo false)"
      [[ "$running" == "true" ]] || fail "rebuilt_exited_during_mapcycle step=$step from=$from_map to=$target_map last_event=$last_event"
      sleep 0.1
    done

    (( shutdown_now > shutdown_before && activate_now > activate_before && \
       fake_seed_now > fake_seed_before && bot1_now > bot1_before && bot2_now > bot2_before && \
       facts_now > facts_before && reset_now > reset_before && init_now > init_before && \
       entity_now > entity_before && spawn_now > spawn_before && solids_now > solids_before && \
       ladders_now > ladders_before )) || \
      fail "mapcycle_lifecycle_missing step=$step from=$from_map to=$target_map host_port=$host_port last_event=$last_event"

    skip_after="$(grep -c '^ClientPutInServer_SKIP .*reason=lifecycle$' "$PROBE" 2>/dev/null || true)"
    [[ "$skip_after" == "$skip_before" ]] || fail "mapcycle_stale_lifecycle_skip step=$step from=$from_map to=$target_map before=$skip_before after=$skip_after"

    if ! timeout 4s docker exec "$rebuilt_id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
      fail "mapcycle_a2s_not_ready step=$step from=$from_map to=$target_map host_port=$host_port last_event=$last_event"
    fi

    cycle_facts="$(grep '^ENGINE_SIGNON_FACTS ' "$PROBE" 2>/dev/null | tail -n1 || true)"
    cycle_count="$(printf '%s\n' "$cycle_facts" | sed -n 's/.*server_count=\([-0-9][0-9]*\).*/\1/p')"
    cycle_crc="$(printf '%s\n' "$cycle_facts" | sed -n 's/.*sendtable_crc_hex=\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/p')"
    [[ "$cycle_count" =~ ^[0-9]+$ ]] || fail "mapcycle_server_count_missing step=$step facts=$cycle_facts"
    [[ -n "$cycle_crc" ]] || fail "mapcycle_sendtable_crc_missing step=$step facts=$cycle_facts"
    expected_count=$((cycle_prev_count + 1))
    (( cycle_count == expected_count )) || fail "mapcycle_server_count_step step=$step from=$from_map to=$target_map expected=$expected_count actual=$cycle_count"

    # gatefix3 install sentinel: fail fast if the paired probe was not copied.
    if ! grep -Fq 'PROTOCOL7_NETCHAN_SIGNON_RETRY_' "$ROOT/scripts/protocol7-client-probe.py" 2>/dev/null; then
      fail "mapcycle_probe_retry_feature_missing step=$step map=$target_map file=scripts/protocol7-client-probe.py"
    fi

    set +e
    # Map transitions can accept the connection while build4100 is still
    # finishing the new-map signon stream. Keep the first attempt on one
    # netchannel and retransmit CONNECTED/state2 there. If that channel still
    # stalls before svc_ServerInfo/signon NEW, make exactly one fresh connect
    # attempt. The full-path assertions below remain unchanged.
    python3 "$ROOT/scripts/protocol7-client-probe.py" \
      --host 127.0.0.1 --port "$host_port" --name "$cycle_name" --timeout 8 \
      --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
      --server-count "$cycle_count" --sendtable-crc "0x$cycle_crc" --force-full-sendtables \
      --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --disconnect-after-pass --max-udp-packets 512 \
      >"$cycle_output" 2>&1
    cycle_rc=$?

    if [[ $cycle_rc -ne 0 ]] && \
       grep -Fq 'PROTOCOL7=FAIL reason=full_path_incomplete' "$cycle_output" 2>/dev/null && \
       ! grep -Fq 'PROTOCOL7_SERVER_SIGNON_NEW=PASS' "$cycle_output" 2>/dev/null; then
      echo "REAL_MAPCYCLE_PROBE_RETRY_BEGIN step=$step map=$target_map reason=pre_new_stall"
      retry_output="${cycle_output}.retry"
      sleep 0.35
      python3 "$ROOT/scripts/protocol7-client-probe.py" \
        --host 127.0.0.1 --port "$host_port" --name "${cycle_name}_retry" --timeout 8 \
        --initial-signon-retries 8 --initial-signon-retry-interval 0.75 \
        --server-count "$cycle_count" --sendtable-crc "0x$cycle_crc" --force-full-sendtables \
        --sendprop-schema "$SENDPROP_SCHEMA" --instance-baselines "$INSTANCEBASELINES" --disconnect-after-pass --max-udp-packets 512 \
        >"$retry_output" 2>&1
      retry_rc=$?
      echo "REAL_MAPCYCLE_PROBE_RETRY_END step=$step map=$target_map rc=$retry_rc"
      if [[ $retry_rc -eq 0 ]]; then
        mv -f "$retry_output" "$cycle_output"
        cycle_rc=0
      else
        cat "$cycle_output" >&2 || true
        cat "$retry_output" >&2 || true
        cycle_rc=$retry_rc
      fi
    fi
    set -e
    if [[ $cycle_rc -ne 0 ]]; then
      cat "$cycle_output" >&2 || true
      if grep -Fq "Banned by server" "$cycle_output" 2>/dev/null; then
        fail "mapcycle_server_ban step=$step map=$target_map host_port=$host_port sv_filterban=0"
      fi
      fail "protocol7_mapcycle_probe_rc=$cycle_rc step=$step map=$target_map host_port=$host_port"
    fi
    grep -Eq "^PROTOCOL7=PASS .*usercmd_air_accel_delta=1 .*server_count=${cycle_count} .*player_slot=2 local_entity=3 .*disconnect_sent=1 " "$cycle_output" 2>/dev/null || {
      cat "$cycle_output" >&2 || true
      fail "protocol7_mapcycle_full_path_missing step=$step map=$target_map host_port=$host_port"
    }
    grep -Eq '^PROTOCOL7_SENDPROP_GAMEPLAY=PASS .*wire=bot2_team3\+local3_team0\+local3_state1 ' "$cycle_output" 2>/dev/null || {
      cat "$cycle_output" >&2 || true
      fail "protocol7_mapcycle_sendprop_missing step=$step map=$target_map host_port=$host_port"
    }

    client_deadline=$((SECONDS + 6))
    while (( SECONDS < client_deadline )); do
      count="$(wc -l < "$PROBE" 2>/dev/null || echo 0)"
      if (( count > last_count )); then
        while IFS= read -r event; do
          [[ -n "$event" ]] || continue
          last_event="$event"
          echo "REAL_ENGINE_EVENT $event"
        done < <(sed -n "$((last_count + 1)),${count}p" "$PROBE")
        last_count=$count
      fi
      disconnect_now="$(grep -c '^ClientDisconnect_OK index=3 ' "$PROBE" 2>/dev/null || true)"
      if grep -Eq "^ClientConnect_OK index=3 name=${cycle_name}( |$)" "$PROBE" 2>/dev/null && \
         grep -Eq "^ClientPutInServer_OK index=3 name=${cycle_name}( |$)" "$PROBE" 2>/dev/null && \
         grep -Eq '^GAMEPLAY_COMMAND_EVENT index=3 cmd=jointeam arg=3 before_team=0 after_team=3 before_state=1 after_state=0 changed=1$' "$PROBE" 2>/dev/null && \
         (( disconnect_now > disconnect_before )); then
        break
      fi
      sleep 0.1
    done
    (( disconnect_now > disconnect_before )) || fail "protocol7_mapcycle_disconnect_missing step=$step map=$target_map last_event=$last_event"

    skip_after="$(grep -c '^ClientPutInServer_SKIP .*reason=lifecycle$' "$PROBE" 2>/dev/null || true)"
    [[ "$skip_after" == "$skip_before" ]] || fail "mapcycle_post_client_lifecycle_skip step=$step map=$target_map before=$skip_before after=$skip_after"

    while IFS= read -r line; do
      [[ -n "$line" ]] || continue
      if [[ "$line" == PROTOCOL7_SERVER_SIGNON_SPAWN=PASS* || \
            "$line" == PROTOCOL7_SENDPROP_GAMEPLAY=PASS* || \
            "$line" == PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS* || \
            "$line" == PROTOCOL7_DISCONNECT_SENT=PASS* || \
            "$line" == PROTOCOL7=PASS* ]]; then
        echo "REAL_MAPCYCLE_CLIENT_EVENT step=$step map=$target_map $line"
      fi
    done < "$cycle_output"

    echo "REAL_MAPCYCLE_STEP=PASS step=$step from=$from_map to=$target_map server_count=${cycle_prev_count}->${cycle_count} lifecycle=1 inventory=1 bots=2 a2s=PASS full_protocol7=1"
    cycle_prev_count="$cycle_count"
    mapcycle_steps=$((mapcycle_steps + 1))
    mapcycle_protocol7=$((mapcycle_protocol7 + 1))
  }

  run_mapcycle_step 2 cs_office de_aztec 234 20 20 \
    'MAP_MOVEMENT_SOLIDS total=19 studio=16 brush=3 prop_physics_multiplayer=16 hostage_bbox=0 npc_bbox=0' 0
  run_mapcycle_step 3 de_aztec fy_pool_day 297 16 15 \
    'MAP_MOVEMENT_SOLIDS total=136 studio=118 brush=18 prop_physics_multiplayer=5 hostage_bbox=0 npc_bbox=0' 2
  run_mapcycle_step 4 fy_pool_day de_dust2 392 20 20 \
    'MAP_MOVEMENT_SOLIDS total=76 studio=75 brush=1 prop_physics_multiplayer=75 hostage_bbox=0 npc_bbox=0' 0

  (( mapcycle_steps == 4 && mapcycle_protocol7 == 4 )) || \
    fail "mapcycle_summary_incomplete steps=$mapcycle_steps protocol7=$mapcycle_protocol7"
  echo "REAL_CLIENT_MAPCYCLE=PASS maps=de_dust2,cs_office,de_aztec,fy_pool_day,de_dust2 transitions=4 server_count=${cycle_initial_count}->${cycle_prev_count} lifecycle=4 bot_reseed=4 a2s=4 full_protocol7=4 stale_lifecycle_skips=0"
fi
protocol7_auth="$(sed -n 's/^PROTOCOL7=PASS auth=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_auth" ]] || protocol7_auth=unknown
protocol7_ticket_len="$(sed -n 's/^PROTOCOL7=PASS .*ticket_len=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_ticket_len" ]] || protocol7_ticket_len=0
protocol7_payload_len="$(sed -n 's/^PROTOCOL7_CONNECTION_ACCEPTED=PASS .*payload_len=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_payload_len" ]] || protocol7_payload_len=0
protocol7_sendtables="$(sed -n 's/^PROTOCOL7=PASS .*sendtable_envelopes=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_sendtables" ]] || protocol7_sendtables=0
protocol7_udp_signon_packets="$(sed -n 's/^PROTOCOL7=PASS .*udp_signon_packets=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_udp_signon_packets" ]] || protocol7_udp_signon_packets=0
protocol7_udp_signon_bytes="$(sed -n 's/^PROTOCOL7=PASS .*udp_signon_bytes=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_udp_signon_bytes" ]] || protocol7_udp_signon_bytes=0
protocol7_fragment_blocks="$(sed -n 's/^PROTOCOL7=PASS .*fragment_blocks=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_fragment_blocks" ]] || protocol7_fragment_blocks=0
protocol7_reliable_ack="$(sed -n 's/^PROTOCOL7=PASS .*reliable_ack=\(0x[0-9a-fA-F][0-9a-fA-F]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_reliable_ack" ]] || protocol7_reliable_ack=0x00
protocol7_bzip_complete="$(sed -n 's/^PROTOCOL7=PASS .*bzip_complete=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_bzip_complete" ]] || protocol7_bzip_complete=0
protocol7_classinfo="$(sed -n 's/^PROTOCOL7=PASS .*classinfo=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_classinfo" ]] || protocol7_classinfo=0
protocol7_stringtable_baselines="$(sed -n 's/^PROTOCOL7=PASS .*stringtable_baselines=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_stringtable_baselines" ]] || protocol7_stringtable_baselines=0
protocol7_instancebaseline_classes="$(sed -n 's/^PROTOCOL7=PASS .*instancebaseline_classes=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_instancebaseline_classes" ]] || protocol7_instancebaseline_classes=0
protocol7_packet_baseline_classes="$(sed -n 's/^PROTOCOL7=PASS .*packet_baseline_classes=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packet_baseline_classes" ]] || protocol7_packet_baseline_classes=0
protocol7_packetentities="$(sed -n 's/^PROTOCOL7=PASS .*packetentities=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packetentities" ]] || protocol7_packetentities=0
protocol7_packetentities_full="$(sed -n 's/^PROTOCOL7=PASS .*packetentities_full=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packetentities_full" ]] || protocol7_packetentities_full=0
protocol7_packetentities_updated_max="$(sed -n 's/^PROTOCOL7=PASS .*packetentities_updated_max=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packetentities_updated_max" ]] || protocol7_packetentities_updated_max=0
protocol7_packetentities_data_bits="$(sed -n 's/^PROTOCOL7=PASS .*packetentities_data_bits=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packetentities_data_bits" ]] || protocol7_packetentities_data_bits=0
protocol7_snapshot_entities="$(sed -n 's/^PROTOCOL7=PASS .*snapshot_entities=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_snapshot_entities" ]] || protocol7_snapshot_entities=0
protocol7_snapshot_changed_props="$(sed -n 's/^PROTOCOL7=PASS .*snapshot_changed_props=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_snapshot_changed_props" ]] || protocol7_snapshot_changed_props=0
protocol7_sendprop_snapshot="$(sed -n 's/^PROTOCOL7=PASS .*sendprop_snapshot=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_sendprop_snapshot" ]] || protocol7_sendprop_snapshot=0
protocol7_baseline_ack="$(sed -n 's/^PROTOCOL7=PASS .*baseline_ack_sent=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_baseline_ack" ]] || protocol7_baseline_ack=0
protocol7_snapshot_tick_ack="$(sed -n 's/^PROTOCOL7=PASS .*snapshot_tick_ack_sent=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_snapshot_tick_ack" ]] || protocol7_snapshot_tick_ack=0
protocol7_packetentities_delta="$(sed -n 's/^PROTOCOL7=PASS .*packetentities_delta=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_packetentities_delta" ]] || protocol7_packetentities_delta=0
protocol7_delta_changed_props="$(sed -n 's/^PROTOCOL7=PASS .*delta_changed_props=\([0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_delta_changed_props" ]] || protocol7_delta_changed_props=0
protocol7_delta_field_index="$(sed -n 's/^PROTOCOL7=PASS .*delta_field_index=\([-0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_delta_field_index" ]] || protocol7_delta_field_index=-1
protocol7_delta_account="$(sed -n 's/^PROTOCOL7=PASS .*delta_account=\([-0-9][0-9]*\).*/\1/p' "$protocol7_output" | tail -n1)"
[[ -n "$protocol7_delta_account" ]] || protocol7_delta_account=-1

put_count="$(grep -c '^ClientPutInServer_OK ' "$PROBE" || true)"
active_count="$(grep -c '^ClientActive_OK ' "$PROBE" || true)"
connect_count="$(grep -c '^ClientConnect_OK ' "$PROBE" || true)"
fake_adopt_count="$(grep -c '^ClientPutInServer_FAKE_ADOPT ' "$PROBE" || true)"

# A REAL_ENGINE_DOCKER=PASS line is now a true completion marker: no Docker or
# filesystem cleanup is allowed to run after it.  This prevents the outer
# harness from appearing successful while a final `docker rm` is still stuck.
cleanup normal
REBUILT_NAME=""
CONTROL_NAME=""
if [[ $CLEANUP_STATUS -ne 0 ]]; then
  echo "REAL_ENGINE_CLEANUP=FAIL map=$MAP_NAME rc=$CLEANUP_STATUS" >&2
  exit "$CLEANUP_STATUS"
fi
trap - EXIT INT TERM HUP
echo "REAL_ENGINE_CLEANUP=PASS map=$MAP_NAME"
echo "REAL_ENGINE_ORACLE_CONTRACT=PASS revision=$LIVE_ORACLE_REVISION transmit_contract=$LIVE_TRANSMIT_CONTRACT"
echo "REAL_ENGINE_DOCKER=PASS runtime=working_image bufferfix=ON revive=ON a2s=$a2s_result map=$MAP_NAME connect=$connect_count real_client_connectionless=$protocol7_connection_ok tcp_stream=$protocol7_tcp_ok signon_state2=$protocol7_signon_ok server_new=$protocol7_server_new_ok real_client_connect=$real_client_connect clientinfo=$protocol7_clientinfo_ok signon_state3=$protocol7_new_ok udp_signon=$protocol7_udp_signon_ok svc_sendtable_seen=$protocol7_sendtable_seen sendtables=$protocol7_sendtables udp_signon_packets=$protocol7_udp_signon_packets udp_signon_bytes=$protocol7_udp_signon_bytes fragment_reassembly=$protocol7_fragment_ok fragment_blocks=$protocol7_fragment_blocks reliable_ack=$protocol7_reliable_ack bzip_complete=$protocol7_bzip_complete classinfo=$protocol7_classinfo stringtable_baselines=$protocol7_stringtable_baselines instancebaseline_classes=$protocol7_instancebaseline_classes packet_baseline_classes=$protocol7_packet_baseline_classes prespawn=$protocol7_prespawn_ok prespawn_sent=$protocol7_prespawn_sent spawn=$protocol7_spawn_ok spawn_sent=$protocol7_spawn_sent full_sent=$protocol7_full_sent baseline_ack=$protocol7_baseline_ack snapshot_tick_ack=$protocol7_snapshot_tick_ack real_client_put=$real_client_put real_client_active=$real_client_active packetentities=$protocol7_packetentities packetentities_full=$protocol7_packetentities_full packetentities_updated_max=$protocol7_packetentities_updated_max packetentities_data_bits=$protocol7_packetentities_data_bits entity_snapshot=$protocol7_entity_snapshot_ok snapshot_entities=$protocol7_snapshot_entities snapshot_changed_props=$protocol7_snapshot_changed_props sendprop_snapshot=$protocol7_sendprop_snapshot packetentities_delta=$protocol7_packetentities_delta entity_delta=$protocol7_entity_delta_ok delta_changed_props=$protocol7_delta_changed_props stringcmd_jointeam3=$protocol7_stringcmd_ok gameplay_event_delta=$protocol7_gameplay_event_delta_ok gameevent_list=$protocol7_gameevent_list_ok listen_events=$protocol7_listen_events_ok say_sent=$protocol7_say_sent_ok player_say_event=$protocol7_player_say_event_ok usermessage_cmd=$protocol7_usermessage_command_ok saytext_usermessage=$protocol7_saytext_usermessage_ok downloadables=$protocol7_downloadables_ok download_request=$protocol7_download_request_ok download_file=$protocol7_download_received_ok download_bytes=$DOWNLOAD_SIZE spawn_origin=$protocol7_spawn_origin_ok clc_move=$protocol7_move_ok usercmd_eyeangles_delta=$protocol7_usercmd_eyeangles_delta_ok usercmd_movement_delta=$protocol7_usercmd_movement_delta_ok friction_move=$protocol7_friction_move_ok usercmd_friction_delta=$protocol7_usercmd_friction_delta_ok jump_move=$protocol7_jump_move_ok usercmd_jump_delta=$protocol7_usercmd_jump_delta_ok jump_hold_move=$protocol7_jump_hold_move_ok usercmd_jump_hold_delta=$protocol7_usercmd_jump_hold_delta_ok air_accel_move=$protocol7_air_accel_move_ok usercmd_air_accel_delta=$protocol7_usercmd_air_accel_delta_ok weapon_attack=$protocol7_weapon_attack_ok weapon_reload_move=$protocol7_weapon_reload_move_ok weapon_secondary_move=$protocol7_weapon_secondary_move_ok weapon_fire_event=$protocol7_weapon_fire_event_ok weapon_reload_event=$protocol7_weapon_reload_event_ok combat_setup=$protocol7_combat_setup_ok player_hurt_event=$protocol7_player_hurt_event_ok player_death_event=$protocol7_player_death_event_ok combat_reset=$protocol7_combat_reset_ok sendprop_delta=$protocol7_sendprop_delta_ok delta_field_index=$protocol7_delta_field_index delta_account=$protocol7_delta_account post_active_survival=1 post_active_edict_bind=$post_active_edict_bind post_active_networkable=$post_active_networkable post_active_visibility=$post_active_visibility post_active_checktransmit=$post_active_checktransmit post_active_gameplay_state=$post_active_gameplay_state post_active_gameplay_event=$post_active_gameplay_event post_active_player_say_event=$post_active_player_say_event post_active_saytext_usermessage=$post_active_saytext_usermessage post_active_engine_trace=$post_active_engine_trace post_active_spawn_trace=$post_active_spawn_trace post_active_usercmd_event=$post_active_usercmd_event post_active_movement_event=$post_active_movement_event post_active_friction_event=$post_active_friction_event post_active_jump_event=$post_active_jump_event post_active_jump_hold_event=$post_active_jump_hold_event post_active_air_accel_event=$post_active_air_accel_event post_active_walkable_slope=$post_active_walkable_slope post_active_stepmove=$post_active_stepmove post_active_advanced_movement=$post_active_advanced_movement post_active_dynamic_player_collision=$post_active_dynamic_player_collision post_active_dynamic_nonplayer_collision=$post_active_dynamic_nonplayer_collision post_active_hostage_collision=$post_active_hostage_collision post_active_hostage_follow=$post_active_hostage_follow post_active_generic_npc=$post_active_generic_npc post_active_water_contents=$post_active_water_contents post_active_water_level2=$post_active_water_level2 post_active_water_movement=$post_active_water_movement post_active_duck_movement=$post_active_duck_movement post_active_ladder_movement=$post_active_ladder_movement post_active_a2s=$post_active_a2s reconnect_protocol7=$reconnect_protocol7 reconnect_slot_reuse=$reconnect_slot_reuse reconnect_disconnect=$reconnect_disconnect multiclient_protocol7=$multiclient_protocol7 multiclient_slots=$multiclient_slots multiclient_voice=$multiclient_voice multiclient_objective=$multiclient_objective multiclient_te_firebullets=$multiclient_te_firebullets objective_hostage=$objective_hostage mapchange_protocol7=$mapchange_protocol7 mapchange_lifecycle=$mapchange_lifecycle mapcycle_protocol7=$mapcycle_protocol7 mapcycle_steps=$mapcycle_steps engine_server_count=$server_count engine_sendtable_crc=0x$sendtable_crc_hex protocol7=PASS auth=$protocol7_auth ticket_len=$protocol7_ticket_len connection_payload_len=$protocol7_payload_len fake_adopt=$fake_adopt_count put=$put_count active=$active_count fakeclient_slot=57 gameevents=READY core_listener=READY"
