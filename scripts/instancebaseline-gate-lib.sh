#!/usr/bin/env bash

# Validate the newest runtime instancebaseline census emitted by the rebuilt DLL.
# build4100 may legitimately expose an all-sentinel pre-connect snapshot on maps
# where no class baseline has materialized yet.  The structural invariant is the
# complete 177-row census, not ready>0.
instancebaseline_dump_latest_valid() {
  local file="$1"
  local line ready missing player_baseline c4_baseline seeded seed_target

  line="$(grep '^INSTANCEBASELINE_DUMP_OK classes=177 ' "$file" 2>/dev/null | tail -n1 || true)"
  [[ -n "$line" ]] || return 1
  if ! [[ "$line" =~ ^INSTANCEBASELINE_DUMP_OK\ classes=177\ ready=([0-9]+)\ missing=([0-9]+)\ player_baseline=([0-9]+)\ c4_baseline=([0-9]+)\ seeded=([0-9]+)/([0-9]+)$ ]]; then
    return 1
  fi

  ready="${BASH_REMATCH[1]}"
  missing="${BASH_REMATCH[2]}"
  player_baseline="${BASH_REMATCH[3]}"
  c4_baseline="${BASH_REMATCH[4]}"
  seeded="${BASH_REMATCH[5]}"
  seed_target="${BASH_REMATCH[6]}"

  (( ready <= 177 && missing <= 177 )) || return 1
  (( ready + missing == 177 )) || return 1
  (( seeded == 2 && seed_target == 2 )) || return 1

  # For the legal empty snapshot every ServerClass is still sentinel/unready.
  # Keep this strict so a partial/corrupt census cannot be mistaken for the
  # cs_office lifecycle case observed on real build4100.
  if (( ready == 0 )); then
    (( missing == 177 )) || return 1
    (( player_baseline == 65535 && c4_baseline == 65535 )) || return 1
  fi
  return 0
}
