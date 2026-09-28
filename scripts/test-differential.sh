#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build/reconstruction-current}"
LIB="$BUILD/server_i486.so"
RUNTIME="$ROOT/runtime/original"
ORIGINAL="$RUNTIME/cstrike/bin/server_i486.so"
mkdir -p "$BUILD"

# The original build4100 game DLL is a static oracle outside srcds. Its global
# constructors assume the engine process, so never dlopen it from this helper.
python3 "$ROOT/tools/abi-inventory/abi_inventory.py" \
  --original "$ORIGINAL" \
  --rebuilt "$LIB" \
  --out "$BUILD/abi-inventory.json"

python3 "$ROOT/tools/sendtable-oracle/sendtable_oracle.py" \
  --elf "$ORIGINAL" \
  --out "$BUILD/sendtable-oracle.json" \
  --check-selected

python3 "$ROOT/tools/sendtable-oracle/sendprop_field_oracle.py" \
  --elf "$ORIGINAL" \
  --out "$BUILD/sendprop-field-oracle.json" \
  --all --check

python3 "$ROOT/tools/entity-factory-oracle.py" \
  --elf "$ORIGINAL" \
  --out "$BUILD/entity-factory-oracle.json" \
  --check

oracle_line="$(python3 - "$BUILD/sendtable-oracle.json" <<'PY'
import json, sys
x=json.load(open(sys.argv[1]))
sel=x['selected']
ordered=['WORLD','CSPlayer','CSTeam','CSPlayerResource','CSRagdoll','CHostage','PlantedC4','WeaponAK47','WeaponM4A1','WeaponAWP','WeaponC4','Flashbang','HEGrenade','SmokeGrenade','BaseCSGrenadeProjectile']
print('SENDTABLE_ORACLE=PASS tables=%d sendprop=0x%x iservernetworkable=0x%x iserverunknown=0x%x ihandleentity=0x%x runtime_selected=%s' % (
    x['table_count'], x['sendprop_size'], x['vtables']['IServerNetworkable'],
    x['vtables']['IServerUnknown'], x['vtables']['IHandleEntity'],
    ','.join('%s:%d' % (n, sel[n]['props']) for n in ordered)))
PY
)"
echo "$oracle_line"

field_line="$(python3 - "$BUILD/sendprop-field-oracle.json" <<'PY'
import json, sys
x=json.load(open(sys.argv[1]))
props=sum(len(t['props']) for t in x['tables'].values())
links=sum(1 for t in x['tables'].values() for p in t['props'] if p.get('data_table_target'))
selected=['WORLD','CSPlayer','CSPlayerResource','PlantedC4','WeaponM4A1','BaseEntity']
checks={
    ('WORLD','baseclass'):'DT_BaseEntity',
    ('CSPlayer','baseclass'):'DT_BasePlayer',
    ('CSPlayer','cslocaldata'):'DT_CSLocalPlayerExclusive',
    ('CSPlayer','csnonlocaldata'):'DT_CSNonLocalPlayerExclusive',
    ('PlantedC4','baseclass'):'DT_BaseAnimating',
    ('BaseEntity','AnimTimeMustBeFirst'):'DT_AnimTimeMustBeFirst',
    ('BaseEntity','m_Collision'):'DT_CollisionProperty',
    ('BaseEntity','predictable_id'):'DT_PredictableId',
}
for (table,name), expected in checks.items():
    prop=next((p for p in x['tables'][table]['props'] if p['name']==name), None)
    if not prop or prop.get('data_table_target') != expected:
        raise SystemExit('datatable link mismatch %s.%s -> %r expected %r' % (table,name, prop and prop.get('data_table_target'), expected))
print('SENDPROP_FIELD_ORACLE=PASS tables=%d runtime_props=%d datatable_links=%d selected=%s' % (
    x['table_count'], props, links,
    ','.join('%s:%d' % (n, x['tables'][n]['runtime_prop_count']) for n in selected)))
PY
)"
echo "$field_line"

factory_line="$(python3 - "$BUILD/entity-factory-oracle.json" <<'PY'
import json, sys
x=json.load(open(sys.argv[1]))
print('ENTITY_FACTORY_ORACLE=PASS registrations=%d mapped=%d classnames=%d cpp_types=%d selected=%d' % (
    x['registration_count'], x['mapped_registration_count'], x['unique_classname_count'],
    x['factory_type_count'], sum(1 for v in x['selected_map'].values() if v)))
PY
)"
echo "$factory_line"

"$BUILD/factory-differential-smoke" "$LIB"

echo "DIFFERENTIAL=PASS original=static_elf rebuilt=runtime_contract+sendprop_field_oracle+exact_entity_factory_map"
