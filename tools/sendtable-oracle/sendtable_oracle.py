#!/usr/bin/env python3
import argparse, json, math, re, subprocess
from pathlib import Path

PROP_SIZE = 0x4C
SELECTED = [
    'WORLD','CSPlayer','CSTeam','CSPlayerResource','CSRagdoll','CHostage','PlantedC4',
    'WeaponAK47','WeaponM4A1','WeaponAWP','WeaponC4','Flashbang','HEGrenade',
    'SmokeGrenade','BaseCSGrenadeProjectile'
]

def nm(path: Path):
    return subprocess.check_output(['nm','-C','-S',str(path)], text=True, errors='replace').splitlines()

def parse(path: Path):
    tables = {}
    vtables = {}
    symbols = {}
    prop_re = re.compile(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[bBdD]\s+ServerClassInit<DT_([^:]+)::ignored>\(.*\)::g_SendProps$')
    sym_re = re.compile(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\S\s+(.+)$')
    for line in nm(path):
        m = prop_re.match(line)
        if m:
            size = int(m.group(2),16)
            if size % PROP_SIZE:
                raise SystemExit(f'non-build4100 SendProp array size: DT_{m.group(3)} size=0x{size:x}')
            tables[m.group(3)] = {'address': int(m.group(1),16), 'bytes': size, 'raw_props': size // PROP_SIZE, 'props': (size // PROP_SIZE) - 1}
            continue
        m = sym_re.match(line)
        if not m: continue
        addr, size, name = int(m.group(1),16), int(m.group(2),16), m.group(3)
        if name.startswith('vtable for '): vtables[name[11:]] = size
        if name in ('SendProp::SendProp()', 'ServerClass::ServerClass(char*, SendTable*)', 'g_pServerClassHead'):
            symbols[name] = {'address': addr, 'size': size}
    return {
        'elf': str(path),
        'sendprop_size': PROP_SIZE,
        'table_count': len(tables),
        'tables': dict(sorted(tables.items())),
        'selected': {name: tables.get(name) for name in SELECTED},
        'vtables': vtables,
        'symbols': symbols,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--elf', required=True)
    ap.add_argument('--out')
    ap.add_argument('--check-selected', action='store_true')
    args = ap.parse_args()
    data = parse(Path(args.elf))
    if args.check_selected:
        missing = [x for x in SELECTED if data['selected'][x] is None]
        if missing: raise SystemExit('missing selected tables: ' + ','.join(missing))
        if data['vtables'].get('SendProp') != 0x10: raise SystemExit('SendProp vtable size mismatch')
        if data['vtables'].get('IServerNetworkable') != 0x3c: raise SystemExit('IServerNetworkable vtable size mismatch')
        if data['vtables'].get('IServerUnknown') != 0x24: raise SystemExit('IServerUnknown vtable size mismatch')
        if data['vtables'].get('IHandleEntity') != 0x18: raise SystemExit('IHandleEntity vtable size mismatch')
        if data['table_count'] < 100: raise SystemExit(f'too few original send tables: {data["table_count"]}')
    text = json.dumps(data, indent=2, sort_keys=True)
    if args.out: Path(args.out).write_text(text+'\n')
    else: print(text)

if __name__ == '__main__': main()
