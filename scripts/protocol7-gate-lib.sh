#!/usr/bin/env bash

# Match one diagnostic PASS line by prefix and a set of required ERE fields.
# Field order is intentionally irrelevant: Protocol7 telemetry grows over time,
# while the semantic acceptance contract is the presence/value of each field.
protocol7_pass_has_fields() {
  local file="$1"
  local prefix="$2"
  shift 2

  local line pattern ok
  while IFS= read -r line; do
    [[ "$line" == "$prefix"* ]] || continue
    ok=1
    for pattern in "$@"; do
      if ! [[ "$line" =~ $pattern ]]; then
        ok=0
        break
      fi
    done
    if (( ok == 1 )); then
      return 0
    fi
  done < "$file"
  return 1
}
