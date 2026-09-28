#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build/reconstruction-current}"
LIB="$BUILD/server_i486.so"
BASE="$ROOT/runtime/original"
WORK="$BUILD/runtime-engine-probe"
CONTROL_WORK="$BUILD/runtime-engine-control"
LOG="$BUILD/real-engine.log"
CONTROL_LOG="$BUILD/real-engine-original-control.log"
PROBE="$BUILD/real-engine-probe.events"
DIAG="$BUILD/real-engine-watchdog.txt"
PIDFILE="$BUILD/real-engine.pid"
SUPERVISOR_OUT="$BUILD/real-engine-supervisor.log"
SUPERVISOR="$ROOT/scripts/real_engine_supervisor.py"

[[ -x "$SUPERVISOR" ]] || { echo "REAL_ENGINE=FAIL supervisor_missing=$SUPERVISOR" >&2; exit 1; }
[[ -f "$LIB" ]] || { echo "REAL_ENGINE=FAIL rebuilt_server_missing=$LIB" >&2; exit 1; }

cleanup_pidfile() {
  local pid=""
  if [[ -s "$PIDFILE" ]]; then
    pid="$(tr -dc '0-9' < "$PIDFILE" || true)"
  fi
  [[ -n "$pid" ]] || return 0
  if kill -0 "$pid" 2>/dev/null; then
    kill -TERM -- "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
    for _ in $(seq 1 20); do
      kill -0 "$pid" 2>/dev/null || break
      sleep 0.05
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -KILL -- "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
    fi
  fi
  : > "$PIDFILE"
}
trap cleanup_pidfile EXIT INT TERM HUP

rm -rf "$WORK"
cp -a "$BASE" "$WORK"

CONTENT_ROOT="${CSS_V34_SERVER_ROOT:-}"
CONTENT_MODE="SPARSE"
if [[ -n "$CONTENT_ROOT" && -d "$CONTENT_ROOT/cstrike" && -d "$CONTENT_ROOT/hl2" ]]; then
  cp -as --no-clobber "$CONTENT_ROOT/." "$WORK/"
  CONTENT_MODE="FULL"
fi

mkdir -p "$WORK/cstrike/bin"
rm -f "$WORK/cstrike/bin/server_i486.so"
cp -f "$LIB" "$WORK/cstrike/bin/server_i486.so"
chmod +x "$WORK/srcds_i486" "$WORK/srcds_run" || true
: > "$PROBE"
: > "$DIAG"
: > "$PIDFILE"
: > "$SUPERVISOR_OUT"
: > "$CONTROL_LOG"
export LD_LIBRARY_PATH="$WORK/bin:$WORK/cstrike/bin:${LD_LIBRARY_PATH:-}"

MAP_FILE=""
MAP_WANTED="${V34_TEST_MAP:-de_dust2}"
if [[ -n "${V34_TEST_BSP:-}" ]]; then
  [[ -f "$V34_TEST_BSP" ]] || { echo "REAL_ENGINE=FAIL V34_TEST_BSP_not_found=$V34_TEST_BSP" >&2; exit 1; }
  mkdir -p "$WORK/cstrike/maps"
  rm -f "$WORK/cstrike/maps/$(basename "$V34_TEST_BSP")"
  cp -f "$V34_TEST_BSP" "$WORK/cstrike/maps/"
  MAP_FILE="$WORK/cstrike/maps/$(basename "$V34_TEST_BSP")"
elif [[ -f "$WORK/cstrike/maps/$MAP_WANTED.bsp" ]]; then
  MAP_FILE="$WORK/cstrike/maps/$MAP_WANTED.bsp"
elif [[ -f "$ROOT/runtime/test-maps/$MAP_WANTED.bsp" ]]; then
  mkdir -p "$WORK/cstrike/maps"
  rm -f "$WORK/cstrike/maps/$MAP_WANTED.bsp"
  cp -f "$ROOT/runtime/test-maps/$MAP_WANTED.bsp" "$WORK/cstrike/maps/$MAP_WANTED.bsp"
  MAP_FILE="$WORK/cstrike/maps/$MAP_WANTED.bsp"
elif compgen -G "$ROOT/runtime/test-maps/*.bsp" >/dev/null; then
  TEST_BSP="$(find "$ROOT/runtime/test-maps" -maxdepth 1 -type f -name '*.bsp' -print -quit)"
  mkdir -p "$WORK/cstrike/maps"
  rm -f "$WORK/cstrike/maps/$(basename "$TEST_BSP")"
  cp -f "$TEST_BSP" "$WORK/cstrike/maps/"
  MAP_FILE="$WORK/cstrike/maps/$(basename "$TEST_BSP")"
fi

if [[ -n "$MAP_FILE" ]]; then
  MAP_NAME="$(basename "$MAP_FILE" .bsp)"
  MAP_MODE="BSP"
else
  MAP_NAME="__v34_rebuild_probe_missing_map__"
  MAP_MODE="NO_BSP"
fi

if [[ "$MAP_MODE" == "BSP" && "$CONTENT_MODE" != "FULL" ]]; then
  echo "REAL_ENGINE=FAIL full_content_required_for_BSP map=$MAP_NAME set_CSS_V34_SERVER_ROOT" >&2
  exit 1
fi

STARTUP_DEADLINE="${V34_REAL_ENGINE_TIMEOUT:-45}"
if ! [[ "$STARTUP_DEADLINE" =~ ^[1-9][0-9]*$ ]]; then
  echo "REAL_ENGINE=FAIL invalid_V34_REAL_ENGINE_TIMEOUT=$STARTUP_DEADLINE" >&2
  exit 1
fi
HARD_DEADLINE=$((STARTUP_DEADLINE + 20))

srcds_args=(
  ./srcds_i486
  -game cstrike -console -insecure -nomaster -localcser -norestart
  -maxplayers 4
  +sv_lan 1 +mp_autoteambalance 0 +mp_limitteams 0
  +bot_join_after_player 0 +bot_quota_mode fill +bot_quota 2
  +map "$MAP_NAME"
)

supervisor_args=(
  "$SUPERVISOR"
  --probe "$PROBE"
  --log "$LOG"
  --diagnostic "$DIAG"
  --pid-file "$PIDFILE"
  --cwd "$WORK"
  --timeout "$STARTUP_DEADLINE"
)
if [[ "$MAP_MODE" == "BSP" ]]; then
  supervisor_args+=(--bsp)
fi

echo "REAL_ENGINE_HARNESS supervisor=${STARTUP_DEADLINE}s hard=${HARD_DEADLINE}s map=$MAP_NAME content=$CONTENT_MODE"
set +e
V34_ENGINE_PROBE_FILE="$PROBE" SteamAppId=240 \
  timeout --foreground --signal=TERM --kill-after=4s "${HARD_DEADLINE}s" \
  "${supervisor_args[@]}" -- "${srcds_args[@]}" 2>&1 | tee "$SUPERVISOR_OUT"
rc=${PIPESTATUS[0]}
set -e
cleanup_pidfile

run_original_control() {
  [[ "$CONTENT_MODE" == "FULL" ]] || { echo "ORIGINAL_CONTROL=SKIP_NO_FULL_CONTENT"; return 0; }
  rm -rf "$CONTROL_WORK"
  cp -a "$BASE" "$CONTROL_WORK"
  cp -as --no-clobber "$CONTENT_ROOT/." "$CONTROL_WORK/"
  chmod +x "$CONTROL_WORK/srcds_i486" "$CONTROL_WORK/srcds_run" || true
  echo "ORIGINAL_CONTROL_START window=8s"
  set +e
  (
    cd "$CONTROL_WORK"
    export LD_LIBRARY_PATH="$CONTROL_WORK/bin:$CONTROL_WORK/cstrike/bin:${LD_LIBRARY_PATH:-}"
    timeout --foreground --signal=TERM --kill-after=2s 8s \
      env SteamAppId=240 ./srcds_i486 \
      -game cstrike -console -insecure -nomaster -localcser -norestart \
      -maxplayers 4 +sv_lan 1 +map "$MAP_NAME"
  ) >"$CONTROL_LOG" 2>&1
  local control_rc=$?
  set -e
  if [[ $control_rc -eq 124 || $control_rc -eq 137 ]]; then
    echo "ORIGINAL_CONTROL=ALIVE_8S"
  else
    echo "ORIGINAL_CONTROL=EXIT rc=$control_rc"
  fi
  echo "--- original control tail ---"
  tail -120 "$CONTROL_LOG" || true
}

fail() {
  echo "REAL_ENGINE=FAIL $*" >&2
  if [[ -s "$PROBE" ]]; then
    echo "--- callback probe ---" >&2
    cat "$PROBE" >&2
  fi
  echo "--- srcds tail ---" >&2
  tail -200 "$LOG" >&2 || true
  if [[ -s "$DIAG" ]]; then
    echo "--- watchdog diagnostic ---" >&2
    cat "$DIAG" >&2
  fi
  exit 1
}

if [[ $rc -eq 130 ]]; then
  fail "interrupted supervisor_rc=130"
fi
if grep -Eqi 'wrong ELF class|failed to load.*server_i486|segmentation fault|core dumped' "$LOG"; then
  fail "loader_or_crash_error"
fi
if [[ $rc -eq 124 || $rc -eq 137 ]]; then
  last_event="$(tail -n 1 "$PROBE" 2>/dev/null || echo '<none>')"
  if ! grep -q '^===== GDB BACKTRACE' "$DIAG" 2>/dev/null; then
    echo "HARD_WATCHDOG_NO_SUPERVISOR_DIAGNOSTIC rc=$rc" >> "$DIAG"
  fi
  run_original_control
  fail "startup_watchdog_timeout supervisor_rc=$rc deadline=${STARTUP_DEADLINE}s hard=${HARD_DEADLINE}s last_event=$last_event"
fi
if [[ $rc -ne 0 ]]; then
  fail "srcds_exited_before_lifecycle_complete rc=$rc"
fi

# Prove real srcds crossed the current module ABI boundary.
grep -Fxq 'CreateInterface ServerGameDLL006' "$PROBE" \
  || fail "ServerGameDLL006 factory callback not observed"
grep -Fxq 'CreateInterface_OK ServerGameDLL006' "$PROBE" \
  || fail "ServerGameDLL006 factory lookup failed"
grep -Fxq 'CreateInterface_OK HLTVDirector001' "$PROBE" \
  || fail "HLTVDirector001 factory lookup failed"
grep -Fxq 'CreateInterface_OK VServerDllSharedAppSystems001' "$PROBE" \
  || fail "VServerDllSharedAppSystems001 factory lookup failed"
grep -Fxq 'SharedAppSystems_Count count=2' "$PROBE" \
  || fail "shared app-system count was not queried/accepted"
grep -Fxq 'SharedAppSystems_GetDllName index=0 value=soundemittersystem' "$PROBE" \
  || fail "soundemittersystem shared module was not requested"
grep -Fxq 'SharedAppSystems_GetInterfaceName index=0 value=VSoundEmitter002' "$PROBE" \
  || fail "VSoundEmitter002 shared interface was not requested"
grep -Fxq 'SharedAppSystems_GetDllName index=1 value=scenefilecache' "$PROBE" \
  || fail "scenefilecache shared module was not requested"
grep -Fxq 'SharedAppSystems_GetInterfaceName index=1 value=SceneFileCache001' "$PROBE" \
  || fail "SceneFileCache001 shared interface was not requested"
grep -Fxq 'CreateInterface_OK PluginHelpersCheck001' "$PROBE" \
  || fail "PluginHelpersCheck001 factory lookup failed"
grep -Fxq 'GetAllServerClasses_OK classes=177' "$PROBE" \
  || fail "ServerClass/SendTable registry was not accepted"
grep -Fxq 'DLLInit_ENTER' "$PROBE" \
  || fail "DLLInit was not called"
grep -Fxq 'DLLInit_OK' "$PROBE" \
  || fail "DLLInit did not complete"
grep -Fxq 'CVAR_BRIDGE_OK registered=11' "$PROBE" \
  || fail "VEngineCvar003 game-DLL registration bridge did not complete"
grep -Fxq 'GetTickInterval' "$PROBE" \
  || fail "engine did not reach GetTickInterval after send-table initialization"
grep -Fxq 'GetPlayerLimits min=1 max=64 default=32' "$PROBE" \
  || fail "engine did not query ServerGameClients player limits"
grep -Fxq 'PostInit_OK' "$PROBE" \
  || fail "PostInit did not complete"

if [[ "$MAP_MODE" == "BSP" ]]; then
  grep -Fxq 'GameInit_ENTER' "$PROBE" \
    || fail "GameInit was not called with BSP=$MAP_NAME"
  grep -Fxq 'GameInit_OK' "$PROBE" \
    || fail "GameInit did not complete with BSP=$MAP_NAME"
  grep -Fxq 'CreateNetworkStringTables_OK' "$PROBE" \
    || fail "CreateNetworkStringTables did not complete with BSP=$MAP_NAME"
  grep -Eq '^MAP_ENTITY_LUMP entities=[1-9][0-9]* world=1 parsed=1$' "$PROBE" \
    || fail "map entity lump was not parsed with worldspawn for BSP=$MAP_NAME"
  grep -Fq 'LevelInit_OK ' "$PROBE" \
    || fail "LevelInit did not complete with BSP=$MAP_NAME"
  grep -Fxq 'ServerActivate_OK' "$PROBE" \
    || fail "ServerActivate did not complete with BSP=$MAP_NAME"
  connect_count="$(grep -c '^ClientConnect_OK ' "$PROBE" || true)"
  put_count="$(grep -c '^ClientPutInServer_OK ' "$PROBE" || true)"
  active_count="$(grep -c '^ClientActive_OK ' "$PROBE" || true)"
  if (( put_count < 2 || active_count < 2 )); then
    fail "multiclient bot lifecycle incomplete connect=$connect_count put=$put_count active=$active_count BSP=$MAP_NAME"
  fi
  MAP_RESULT="GameInit+MapEntityLump+LevelInit+ServerActivate+2Bots_OK"
else
  MAP_RESULT="PostInit_OK_SKIP_NO_BSP"
fi

echo "REAL_ENGINE=PASS content=$CONTENT_MODE callbacks=CreateInterface,ServerGameDLL006,ServerClass177,HLTVDirector001,VServerDllSharedAppSystems001,SharedAppSystems2,PluginHelpersCheck001,DLLInit_OK,CVarBridge9,GetTickInterval,GetPlayerLimits,PostInit,GameInit,CreateNetworkStringTables map_phase=$MAP_RESULT"
trap - EXIT INT TERM HUP
