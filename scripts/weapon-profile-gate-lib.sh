#!/usr/bin/env bash
# Semantic validator for the build4100 weapon-profile startup telemetry.
# Keep field checks order-independent so future diagnostic fields can be added
# without invalidating historical/live acceptance.

weapon_profile_line_has_exact_field() {
  local line="$1"
  local field="$2"
  [[ " $line " == *" $field "* ]]
}

weapon_profile_line_has_regex_field() {
  local line="$1"
  local expr="$2"
  printf '%s\n' "$line" | grep -Eq "(^|[[:space:]])${expr}($|[[:space:]])"
}

weapon_profile_line_valid() {
  local line="$1"
  [[ "$line" == WEAPON_SCRIPT_PROFILES_OK\ * ]] || return 1

  local field
  for field in loaded=24 expected=24 encrypted=24 plaintext=0 failed=0; do
    weapon_profile_line_has_exact_field "$line" "$field" || return 1
  done

  weapon_profile_line_has_regex_field "$line" 'mask=0x[0-9a-f]+' || return 1
  weapon_profile_line_has_regex_field "$line" 'root=[^[:space:]]+' || return 1
  weapon_profile_line_has_regex_field "$line" 'ak47=[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?' || return 1
  weapon_profile_line_has_regex_field "$line" 'awp=[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?' || return 1
  weapon_profile_line_has_regex_field "$line" 'usp=[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?' || return 1
  weapon_profile_line_has_regex_field "$line" 'ak47_acc=-?[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?/[0-9]+(\.[0-9]+)?/[01]' || return 1
}

weapon_profile_probe_latest_valid() {
  local probe="$1"
  local line
  [[ -f "$probe" ]] || return 1
  line="$(grep '^WEAPON_SCRIPT_PROFILES_OK ' "$probe" 2>/dev/null | tail -n1 || true)"
  [[ -n "$line" ]] || return 1
  weapon_profile_line_valid "$line"
}
