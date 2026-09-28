#!/usr/bin/env python3
import argparse
import json
import re
import subprocess
from pathlib import Path

VTABLE_RE = re.compile(r'vtable for CEntityFactory<(.*)>')
SELECTED = {
    'player': 'CCSPlayer',
    'cs_player_manager': 'CCSPlayerResource',
    'hostage_entity': 'CHostage',
    'func_hostage_rescue': 'CHostageRescueZone',
    'func_bomb_target': 'CBombTarget',
    'planted_c4': 'CPlantedC4',
    'weapon_ak47': 'CAK47',
    'weapon_m4a1': 'CWeaponM4A1',
    'weapon_awp': 'CWeaponAWP',
    'weapon_c4': 'CC4',
    'weapon_flashbang': 'CFlashbang',
    'weapon_hegrenade': 'CHEGrenade',
    'weapon_smokegrenade': 'CSmokeGrenade',
    'flashbang_projectile': 'CFlashbangProjectile',
    'hegrenade_projectile': 'CHEGrenadeProjectile',
    'smokegrenade_projectile': 'CSmokeGrenadeProjectile',
    'worldspawn': 'CWorld',
}


def run(*args):
    return subprocess.check_output(args, text=True, errors='replace')


def sections(elf: Path):
    out = []
    pat = re.compile(r'\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)')
    for line in run('readelf', '-SW', str(elf)).splitlines():
        m = pat.search(line)
        if m:
            out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
    return out


def string_reader(elf: Path):
    blob = elf.read_bytes()
    secs = sections(elf)
    allowed = {'.rodata', '.data', '.data.rel.ro'}

    def read(addr: int):
        for name, vaddr, off, size in secs:
            if name not in allowed or not (vaddr <= addr < vaddr + size):
                continue
            pos = off + addr - vaddr
            end = blob.find(b'\0', pos, min(pos + 192, len(blob)))
            if end < 0:
                return None
            try:
                return blob[pos:end].decode('ascii')
            except UnicodeDecodeError:
                return None
        return None

    return read


def factory_relocations(elf: Path):
    result = []
    for line in run('readelf', '-rW', str(elf)).splitlines():
        if '_ZTV' not in line or 'CEntityFactory' not in line:
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        site = int(parts[0], 16)
        demangled = run('c++filt', parts[4]).strip()
        m = VTABLE_RE.search(demangled)
        if m:
            result.append((site, m.group(1)))
    return sorted(result)


def recover_mapping(elf: Path):
    import bisect
    read_string = string_reader(elf)
    relocs = factory_relocations(elf)

    insns = []
    insn_re = re.compile(r'^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2}\s+)+\s*(.*)$')
    for line in run('objdump', '-drw', str(elf)).splitlines():
        m = insn_re.match(line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2).strip()))
    addresses = [x[0] for x in insns]

    mappings = []
    for site, cpp_type in relocs:
        idx = bisect.bisect_left(addresses, max(0, site - 8))
        regs = {}
        classname = None
        evidence_addr = None
        while idx < len(insns) and insns[idx][0] <= site + 420:
            _, text = insns[idx]
            m = re.match(r'mov\s+\$0x([0-9a-fA-F]+),%(e(?:ax|bx|cx|dx|si|di|bp))$', text)
            if m:
                regs[m.group(2)] = int(m.group(1), 16)
            else:
                m = re.match(r'mov\s+%(e(?:ax|bx|cx|dx|si|di|bp)),0x8\(%esp\)$', text)
                if m:
                    value = regs.get(m.group(1))
                    candidate = read_string(value) if value is not None else None
                    if candidate and 1 < len(candidate) <= 96 and all(32 <= ord(ch) < 127 for ch in candidate):
                        classname = candidate
                        evidence_addr = value
                        break
                else:
                    m = re.match(r'movl\s+\$0x([0-9a-fA-F]+),0x8\(%esp\)$', text)
                    if m:
                        value = int(m.group(1), 16)
                        candidate = read_string(value)
                        if candidate and 1 < len(candidate) <= 96 and all(32 <= ord(ch) < 127 for ch in candidate):
                            classname = candidate
                            evidence_addr = value
                            break
                    else:
                        m = re.match(r'xor\s+%(e(?:ax|bx|cx|dx|si|di|bp)),%\1$', text)
                        if m:
                            regs[m.group(1)] = 0
            idx += 1
        mappings.append({
            'classname': classname,
            'cpp_type': cpp_type,
            'vtable_relocation': site,
            'classname_address': evidence_addr,
        })
    return mappings


def parse(elf: Path):
    mappings = recover_mapping(elf)
    mapped = [x for x in mappings if x['classname']]
    by_name = {x['classname']: x['cpp_type'] for x in mapped}
    factory_types = sorted({x['cpp_type'] for x in mappings})
    return {
        'elf': str(elf),
        'registration_count': len(mappings),
        'mapped_registration_count': len(mapped),
        'unique_classname_count': len(by_name),
        'factory_type_count': len(factory_types),
        'factory_types': factory_types,
        'factory_map': dict(sorted(by_name.items())),
        'selected_map': {name: by_name.get(name) for name in SELECTED},
        'unmapped': [x for x in mappings if not x['classname']],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--elf', required=True)
    ap.add_argument('--out')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    data = parse(Path(args.elf))
    if args.check:
        if data['registration_count'] < 400:
            raise SystemExit(f"too few CEntityFactory registrations: {data['registration_count']}")
        if data['mapped_registration_count'] != data['registration_count']:
            raise SystemExit('unmapped CEntityFactory registrations: ' + ','.join(
                f"{x['cpp_type']}@0x{x['vtable_relocation']:x}" for x in data['unmapped']))
        if data['unique_classname_count'] != data['registration_count']:
            raise SystemExit('duplicate recovered classnames')
        if data['factory_type_count'] < 350:
            raise SystemExit(f"too few CEntityFactory types: {data['factory_type_count']}")
        wrong = [f'{name}:{data["selected_map"].get(name)}!={typ}'
                 for name, typ in SELECTED.items() if data['selected_map'].get(name) != typ]
        if wrong:
            raise SystemExit('selected mapping mismatch: ' + ','.join(wrong))
    text = json.dumps(data, indent=2, sort_keys=True)
    if args.out:
        Path(args.out).write_text(text + '\n')
    else:
        print(text)


if __name__ == '__main__':
    main()
