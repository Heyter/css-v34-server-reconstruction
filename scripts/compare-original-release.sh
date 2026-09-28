#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_ROOT="${CSS_V34_SERVER_ROOT:-}"
RELEASE_LIB="$ROOT/dist/release/server_i486.so"
PROBE="$ROOT/scripts/external-parity-probe.py"
OUT_DIR="$ROOT/build/original-vs-rebuilt-parity"
MAP_NAME="de_dust2"
PASSWORD="${V34_PARITY_RCON:-}"
START_TIMEOUT="${V34_PARITY_START_TIMEOUT:-60}"
SETTLE_SECONDS="${V34_PARITY_SETTLE_SECONDS:-2}"
A2S_ATTEMPT_TIMEOUT="${V34_PARITY_A2S_ATTEMPT_TIMEOUT:-1.0}"
PROBE_RETRIES="${V34_PARITY_PROBE_RETRIES:-8}"
PROBE_TIMEOUT="${V34_PARITY_PROBE_TIMEOUT:-4.0}"

fail() { echo "ORIGINAL_REBUILT_PARITY=FAIL $*" >&2; exit 1; }

[[ -n "$SERVER_ROOT" ]] || fail "server_root_unset set=CSS_V34_SERVER_ROOT"
[[ -f "$RELEASE_LIB" ]] || fail "release_missing=$RELEASE_LIB run=scripts/build-release.sh"
[[ -x "$PROBE" ]] || fail "probe_missing=$PROBE"
[[ -d "$SERVER_ROOT" ]] || fail "server_root_missing=$SERVER_ROOT"
[[ -f "$SERVER_ROOT/.env" ]] || fail "docker_env_missing=$SERVER_ROOT/.env"
[[ "$START_TIMEOUT" =~ ^[1-9][0-9]*$ ]] || fail "invalid_start_timeout=$START_TIMEOUT"
[[ "$PROBE_RETRIES" =~ ^[1-9][0-9]*$ ]] || fail "invalid_probe_retries=$PROBE_RETRIES"
python3 - "$PROBE_TIMEOUT" <<'PYTIMEOUT' || fail "invalid_probe_timeout=$PROBE_TIMEOUT"
import sys
value=float(sys.argv[1])
raise SystemExit(0 if value > 0 else 1)
PYTIMEOUT
command -v docker >/dev/null 2>&1 || fail "docker_not_found"
command -v python3 >/dev/null 2>&1 || fail "python3_not_found"
if [[ -z "$PASSWORD" ]]; then
  PASSWORD="$(python3 - <<'PYSECRET'
import secrets
print("cssv34_parity_" + secrets.token_hex(16))
PYSECRET
)"
fi

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/original.json" "$OUT_DIR/rebuilt.json" "$OUT_DIR/original.log" "$OUT_DIR/rebuilt.log"

python3 "$PROBE" self-test

set -a
# shellcheck disable=SC1090
. "$SERVER_ROOT/.env"
set +a
IMAGE="${CSS_IMAGE:-}"
CONTAINER_HINT="${CSS_CONTAINER:-css-v34}"
if [[ -z "$IMAGE" ]]; then
  IMAGE="$(docker inspect -f '{{.Config.Image}}' "$CONTAINER_HINT" 2>/dev/null || true)"
fi
[[ -n "$IMAGE" ]] || fail "docker_image_not_resolved"
docker image inspect "$IMAGE" >/dev/null 2>&1 || fail "docker_image_missing=$IMAGE"
if ! docker run --rm --entrypoint /bin/sh "$IMAGE" -lc \
  'test -x ./srcds_i486 && test -f ./libbufferfix_preload.so && test -f ./bin/steamclient_i486.so && test -f ./bin/libsteamvalidateuseridtickets_i486.so' \
  >/dev/null 2>&1; then
  fail "docker_runtime_incomplete=$IMAGE"
fi

release_sha="$(sha256sum "$RELEASE_LIB" | awk '{print $1}')"
release_size="$(stat -c '%s' "$RELEASE_LIB")"
echo "PARITY_BINARY_INPUT=PASS path=$RELEASE_LIB bytes=$release_size sha256=$release_sha profile=RELEASE runtime_codegen=RelWithDebInfo stripping=none"

# Verify the exact host artifact through the container bind mount before
# starting srcds. Do not require sha256sum inside the old runtime image: stream
# the mounted bytes with `cat` and hash them on the WSL host instead. parity5
# failed here only because the image does not provide sha256sum, so the runtime
# comparison never actually started.
set +e
mount_sha="$(docker run --rm \
  -v "$RELEASE_LIB:/opt/css-v34/cstrike/bin/server_i486.so:ro" \
  --entrypoint /bin/sh "$IMAGE" -lc \
  'exec cat /opt/css-v34/cstrike/bin/server_i486.so' \
  2>"$OUT_DIR/mount-read.stderr" | sha256sum | awk '{print $1}')"
mount_pipeline_rc=$?
set -e
if [[ $mount_pipeline_rc -ne 0 || "$mount_sha" != "$release_sha" ]]; then
  mount_error="$(tr '\n' ' ' < "$OUT_DIR/mount-read.stderr" 2>/dev/null | sed 's/[[:space:]]\+/ /g' | cut -c1-240)"
  fail "release_mount_mismatch host=$release_sha container=${mount_sha:-missing} read_rc=$mount_pipeline_rc error=${mount_error:-none}"
fi
rm -f "$OUT_DIR/mount-read.stderr"
echo "PARITY_BINARY_MOUNT=PASS sha256=$mount_sha verify=container_cat_host_sha256"

set +e
dep_output="$(docker run --rm \
  -v "$RELEASE_LIB:/opt/css-v34/cstrike/bin/server_i486.so:ro" \
  --entrypoint /bin/sh "$IMAGE" -lc \
  'cd /opt/css-v34 2>/dev/null || exit 0; command -v ldd >/dev/null 2>&1 || exit 0; LD_LIBRARY_PATH=.:./bin ldd ./cstrike/bin/server_i486.so 2>&1' 2>&1)"
dep_rc=$?
set -e
if [[ $dep_rc -ne 0 || "$dep_output" == *"not found"* ]]; then
  echo "PARITY_BINARY_DEPS=FAIL rc=$dep_rc" >&2
  printf '%s\n' "$dep_output" >&2
  fail "release_dependencies"
fi
echo "PARITY_BINARY_DEPS=PASS"

alloc_port() {
  python3 - <<'PY'
import random, socket
for _ in range(512):
    p=random.randint(44000,59000)
    tcp=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    try:
        tcp.bind(("127.0.0.1",p)); udp.bind(("127.0.0.1",p))
    except OSError:
        tcp.close(); udp.close(); continue
    tcp.close(); udp.close(); print(p); raise SystemExit(0)
raise SystemExit(1)
PY
}

CURRENT_NAME=""
cleanup() {
  if [[ -n "$CURRENT_NAME" ]]; then
    timeout 8s docker rm -f "$CURRENT_NAME" >/dev/null 2>&1 || true
    CURRENT_NAME=""
  fi
}
trap cleanup EXIT INT TERM HUP

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
    +mp_autoteambalance 0 +mp_limitteams 0 +bot_quota 2 \
    +sv_sendtables 1 +sv_filterban 0 +sv_voiceenable 1 \
    +rcon_password "$V34_PARITY_RCON" +map "$V34_PARITY_MAP"'

run_side() {
  local label="$1"
  local out_json="$2"
  local out_log="$3"
  local port name id deadline running
  port="$(alloc_port)" || fail "port_allocation label=$label"
  name="css-v34-parity-${label}-$$"
  CURRENT_NAME="$name"
  echo "PARITY_SIDE_START label=$label image=$IMAGE map=$MAP_NAME host_port=$port"

  args=(
    -d --name "$name"
    -p "127.0.0.1:${port}:27016/udp"
    -p "127.0.0.1:${port}:27016/tcp"
    -e V34_PARITY_RCON="$PASSWORD"
    -e V34_PARITY_MAP="$MAP_NAME"
    --entrypoint /bin/sh
  )
  if [[ "$label" == "rebuilt" ]]; then
    args+=( -v "$RELEASE_LIB:/opt/css-v34/cstrike/bin/server_i486.so:ro" )
  fi
  args+=( "$IMAGE" -lc "$launch_command" )

  id="$(docker run "${args[@]}")" || fail "docker_start label=$label"
  deadline=$((SECONDS + START_TIMEOUT))
  ready=0
  ready_output=""
  while (( SECONDS < deadline )); do
    running="$(docker inspect -f '{{.State.Running}}' "$id" 2>/dev/null || echo false)"
    if [[ "$running" != "true" ]]; then
      docker logs "$id" >"$out_log" 2>&1 || true
      exit_code="$(docker inspect -f '{{.State.ExitCode}}' "$id" 2>/dev/null || echo unknown)"
      oom_killed="$(docker inspect -f '{{.State.OOMKilled}}' "$id" 2>/dev/null || echo unknown)"
      state_error="$(docker inspect -f '{{.State.Error}}' "$id" 2>/dev/null || echo unknown)"
      echo "PARITY_SIDE_EXIT_DIAG label=$label exit_code=$exit_code oom_killed=$oom_killed state_error=${state_error:-none}" >&2
      echo "PARITY_SIDE_LOG_TAIL label=$label" >&2
      tail -120 "$out_log" >&2 || true
      fail "server_exited label=$label exit_code=$exit_code oom_killed=$oom_killed log=$out_log"
    fi
    if ready_output="$(python3 "$PROBE" a2s-ready \
      --host 127.0.0.1 --port "$port" --timeout "$A2S_ATTEMPT_TIMEOUT" \
      --expected-app-id 240 --expected-map "$MAP_NAME" 2>/dev/null)"; then
      ready=1
      break
    fi
    sleep 0.25
  done
  if (( ready == 0 )); then
    docker logs "$id" >"$out_log" 2>&1 || true
    internal_a2s=FAIL
    if timeout 4s docker exec "$id" /usr/local/bin/a2s-healthcheck 127.0.0.1 27016 240 >/dev/null 2>&1; then
      internal_a2s=PASS
    fi
    echo "PARITY_SIDE_START_DIAG label=$label external_a2s=FAIL internal_a2s=$internal_a2s timeout=${START_TIMEOUT}s" >&2
    echo "PARITY_SIDE_LOG_TAIL label=$label" >&2
    tail -80 "$out_log" >&2 || true
    fail "a2s_start_timeout label=$label timeout=${START_TIMEOUT}s internal_a2s=$internal_a2s log=$out_log"
  fi
  # Do not issue a second one-shot A2S query here. parity2 proved that a
  # successful readiness packet can be followed by one lost UDP reply; the
  # redundant query made that harmless loss fatal. Print the packet that
  # actually opened the startup gate instead.
  echo "$ready_output"

  sleep "$SETTLE_SECONDS"
  probe_ok=0
  for ((attempt=1; attempt<=PROBE_RETRIES; attempt++)); do
    if python3 "$PROBE" probe --host 127.0.0.1 --port "$port" --password "$PASSWORD" --timeout "$PROBE_TIMEOUT" --out "$out_json"; then
      probe_ok=1
      break
    fi
    echo "PARITY_PROBE_RETRY label=$label attempt=$attempt/$PROBE_RETRIES timeout=${PROBE_TIMEOUT}s" >&2
    sleep 0.5
  done
  if (( probe_ok == 0 )); then
    docker logs "$id" >"$out_log" 2>&1 || true
    echo "PARITY_SIDE_LOG_TAIL label=$label" >&2
    tail -80 "$out_log" >&2 || true
    fail "external_probe label=$label retries=$PROBE_RETRIES log=$out_log"
  fi
  docker logs "$id" >"$out_log" 2>&1 || true
  timeout 8s docker rm -f "$id" >/dev/null 2>&1 || fail "cleanup label=$label"
  CURRENT_NAME=""
  echo "PARITY_SIDE_DONE label=$label snapshot=$out_json log=$out_log"
}

run_side original "$OUT_DIR/original.json" "$OUT_DIR/original.log"
run_side rebuilt "$OUT_DIR/rebuilt.json" "$OUT_DIR/rebuilt.log"

python3 "$PROBE" compare --original "$OUT_DIR/original.json" --rebuilt "$OUT_DIR/rebuilt.json"

readelf -h "$RELEASE_LIB" | grep -Fq 'Class:                             ELF32' || fail "release_not_elf32"
readelf -h "$RELEASE_LIB" | grep -Fq 'Machine:                           Intel 80386' || fail "release_not_i386"
exports="$(nm -D --defined-only "$RELEASE_LIB" | awk '{print $3}' | sort -u | tr '\n' ' ')"
[[ "$exports" == "CreateInterface " ]] || fail "release_exports=$exports"
if readelf -d "$RELEASE_LIB" | grep -F 'TEXTREL' >/dev/null; then fail "release_textrel"; fi
sha="$(sha256sum "$RELEASE_LIB" | awk '{print $1}')"
echo "RC_INPUT_BINARY=PASS path=$RELEASE_LIB elf32=1 exports=CreateInterface textrel=none sha256=$sha"
echo "PARITY_ARTIFACTS=$OUT_DIR"
echo "NEXT=RC1"
