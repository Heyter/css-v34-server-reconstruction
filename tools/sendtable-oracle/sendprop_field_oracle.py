#!/usr/bin/env python3
"""Recover selected build4100 SendProp construction graphs from the original ELF.

This is a static oracle only. It decodes the generated ServerClassInit<DT_*>
functions and records the SendProp helper calls in construction order. The
first g_SendProps element in this build is a compiler/runtime sentinel named
"should_never_see_this"; SendTable::Construct receives g_SendProps+1 and the
actual runtime property count.
"""

import argparse
import bisect
import json
import re
import subprocess
import sys
from pathlib import Path

PROP_SIZE = 0x4C
SELECTED = [
    'WORLD', 'CSPlayer', 'CSTeam', 'CSPlayerResource', 'CSRagdoll', 'CHostage',
    'PlantedC4', 'WeaponAK47', 'WeaponM4A1', 'WeaponAWP', 'WeaponC4',
    'Flashbang', 'HEGrenade', 'SmokeGrenade', 'BaseCSGrenadeProjectile',
]
EXPECTED_RUNTIME_COUNTS = {
    'WORLD': 11,
    'CSPlayer': 38,
    'CSTeam': 1,
    'CSPlayerResource': 17,
    'CSRagdoll': 11,
    'CHostage': 14,
    'PlantedC4': 6,
    'WeaponAK47': 1,
    'WeaponM4A1': 3,
    'WeaponAWP': 1,
    'WeaponC4': 4,
    'Flashbang': 1,
    'HEGrenade': 1,
    'SmokeGrenade': 1,
    'BaseCSGrenadeProjectile': 2,
}

HELPERS = (
    ('_Z11SendPropInt', 'Int'),
    ('_Z12SendPropBool', 'Bool'),
    ('_Z12SendPropTime', 'Time'),
    ('_Z13SendPropAngle', 'Angle'),
    ('_Z13SendPropFloat', 'Float'),
    ('_Z14SendPropArray3', 'Array3'),
    ('_Z14SendPropVector', 'Vector'),
    ('_Z15SendPropEHandle', 'EHandle'),
    ('_Z15SendPropExclude', 'Exclude'),
    ('_Z15SendPropStringT', 'StringT'),
    ('_Z17SendPropDataTable', 'DataTable'),
    ('_Z14SendPropString', 'String'),
    ('_Z15SendPropQAngles', 'QAngles'),
    ('_Z27SendPropIntWithMinusOneFlag', 'IntMinusOne'),
    ('_Z17SendPropUtlVector', 'UtlVector'),
    ('_Z21SendPropPredictableId', 'PredictableId'),
    ('_Z21InternalSendPropArray', 'InternalArray'),
)


def run(*args):
    return subprocess.check_output(args, text=True, errors='replace')


def parse_sections(elf: Path):
    out = []
    pat = re.compile(r'\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)')
    for line in run('readelf', '-SW', str(elf)).splitlines():
        m = pat.search(line)
        if m:
            out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
    return out


def make_string_reader(elf: Path):
    blob = elf.read_bytes()
    sections = parse_sections(elf)

    def read_string(addr):
        if not isinstance(addr, int) or addr <= 0:
            return None
        for _name, vaddr, off, size in sections:
            if not (vaddr <= addr < vaddr + size):
                continue
            pos = off + (addr - vaddr)
            end = blob.find(b'\0', pos, min(pos + 256, len(blob)))
            if end < 0:
                return None
            try:
                value = blob[pos:end].decode('ascii')
            except UnicodeDecodeError:
                return None
            if not value or any(ord(ch) < 32 or ord(ch) >= 127 for ch in value):
                return None
            return value
        return None

    return read_string


def parse_symbols(elf: Path):
    functions = {}
    arrays = {}
    sendtables = {}
    fn_re = re.compile(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[TtWw]\s+int ServerClassInit<DT_([^:]+)::ignored>\(')
    prop_re = re.compile(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[bBdD]\s+ServerClassInit<DT_([^:]+)::ignored>\(.*\)::g_SendProps$')
    table_re = re.compile(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[bBdD]\s+DT_([^:]+)::g_SendTable$')
    for line in run('nm', '-C', '-S', str(elf)).splitlines():
        m = fn_re.match(line)
        if m:
            functions[m.group(3)] = (int(m.group(1), 16), int(m.group(2), 16))
            continue
        m = prop_re.match(line)
        if m:
            size = int(m.group(2), 16)
            arrays[m.group(3)] = {
                'address': int(m.group(1), 16),
                'bytes': size,
                'raw_storage_count': size // PROP_SIZE if size % PROP_SIZE == 0 else None,
            }
            continue
        m = table_re.match(line)
        if m:
            sendtables[int(m.group(1), 16)] = m.group(3)
    return functions, arrays, sendtables


def parse_relocations(elf: Path):
    relocs = {}
    for line in run('readelf', '-rW', str(elf)).splitlines():
        parts = line.split()
        if len(parts) < 5 or not re.fullmatch(r'[0-9a-fA-F]+', parts[0]):
            continue
        value = None
        if len(parts) >= 4 and re.fullmatch(r'[0-9a-fA-F]+', parts[3]):
            value = int(parts[3], 16)
        relocs[int(parts[0], 16)] = {
            'type': parts[2],
            'value': value,
            'symbol': parts[4] if len(parts) >= 5 else '',
        }
    return relocs


def helper_kind(symbol):
    for prefix, kind in HELPERS:
        if symbol.startswith(prefix):
            return kind
    return None


def signed32(value):
    if not isinstance(value, int):
        return None
    value &= 0xffffffff
    return value - 0x100000000 if value & 0x80000000 else value


INSN_RE = re.compile(r'^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2}\s+)+\s*(.*)$')


def parse_disassembly(elf: Path, start: int, stop: int, keep_ranges=None):
    instructions = []
    ranges = sorted(keep_ranges or [(start, stop)])
    range_index = 0
    process = subprocess.Popen(
        ['objdump', '-drw', f'--start-address=0x{start:x}', f'--stop-address=0x{stop:x}', str(elf)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors='replace')
    assert process.stdout is not None
    for line in process.stdout:
        m = INSN_RE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        while range_index < len(ranges) and addr >= ranges[range_index][1]:
            range_index += 1
        if range_index >= len(ranges):
            continue
        if ranges[range_index][0] <= addr < ranges[range_index][1]:
            instructions.append((addr, m.group(2).strip()))
    stderr = process.stderr.read() if process.stderr is not None else ''
    rc = process.wait()
    if rc != 0:
        raise subprocess.CalledProcessError(rc, process.args, stderr=stderr)
    return instructions


def decode_table(elf: Path, table_name, bounds, raw_info, relocs, read_string, shared_disassembly=None):
    start, size = bounds
    if shared_disassembly is None:
        instructions = parse_disassembly(elf, start, start + size)
    else:
        addresses = shared_disassembly[0]
        all_instructions = shared_disassembly[1]
        lo = bisect.bisect_left(addresses, start)
        hi = bisect.bisect_left(addresses, start + size)
        instructions = all_instructions[lo:hi]
    regs = {name: None for name in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')}
    stack = {}
    calls = []
    construct = None
    reg_pat = r'e(?:ax|bx|cx|dx|si|di|bp)'

    for addr, ins in instructions:

        mm = re.match(rf'mov\s+\$0x([0-9a-fA-F]+),%({reg_pat})$', ins)
        if mm:
            value = int(mm.group(1), 16)
            reloc = relocs.get(addr + 1)
            if reloc and reloc.get('type') == 'R_386_32' and reloc.get('value') is not None:
                value = reloc['value']
            regs[mm.group(2)] = value
            continue
        mm = re.match(rf'mov\s+0x([0-9a-fA-F]+),%({reg_pat})$', ins)
        if mm:
            value = None
            # i386 uses either A1 moffs32 (relocation at +1, only for eax) or
            # 8B /r disp32 (relocation at +2) for absolute loads. Accept both
            # encodings so inherited m_pClassSendTable pointers resolve for all
            # original build4100 ServerClass initializers.
            reloc = relocs.get(addr + 1) or relocs.get(addr + 2)
            if reloc and reloc.get('type') == 'R_386_32' and reloc.get('value') is not None:
                pointee_reloc = relocs.get(reloc['value'])
                if pointee_reloc and pointee_reloc.get('type') == 'R_386_32' and pointee_reloc.get('value') is not None:
                    value = pointee_reloc['value']
            regs[mm.group(2)] = value
            continue
        mm = re.match(rf'mov\s+%({reg_pat}),%({reg_pat})$', ins)
        if mm:
            regs[mm.group(2)] = regs.get(mm.group(1))
            continue
        mm = re.match(rf'(?:add|sub)\s+\$0x([0-9a-fA-F]+),%({reg_pat})$', ins)
        if mm:
            current = regs.get(mm.group(2))
            if current is not None:
                delta = int(mm.group(1), 16)
                regs[mm.group(2)] = (current + delta if ins.startswith('add') else current - delta) & 0xffffffff
            continue
        mm = re.match(rf'lea\s+(-?0x[0-9a-fA-F]+)?\(%({reg_pat})\),%({reg_pat})$', ins)
        if mm:
            current = regs.get(mm.group(2))
            if current is not None:
                disp = int(mm.group(1), 16) if mm.group(1) else 0
                regs[mm.group(3)] = (current + disp) & 0xffffffff
            else:
                regs[mm.group(3)] = None
            continue
        mm = re.match(rf'xor\s+%({reg_pat}),%\1$', ins)
        if mm:
            regs[mm.group(1)] = 0
            continue

        mm = re.match(r'movl?\s+\$0x([0-9a-fA-F]+),(?:(0x[0-9a-fA-F]+)?)\(%esp\)$', ins)
        if mm:
            stack[int(mm.group(2), 16) if mm.group(2) else 0] = int(mm.group(1), 16)
            continue
        mm = re.match(rf'mov\s+%({reg_pat}),(?:(0x[0-9a-fA-F]+)?)\(%esp\)$', ins)
        if mm:
            stack[int(mm.group(2), 16) if mm.group(2) else 0] = regs.get(mm.group(1))
            continue

        if not ins.startswith('call'):
            continue
        reloc = relocs.get(addr + 1, {})
        symbol = reloc.get('symbol', '')
        kind = helper_kind(symbol)
        if kind:
            snapshot = {offset: stack.get(offset) for offset in range(0, 0x28, 4)}
            if kind == 'Exclude':
                table = read_string(snapshot.get(4))
                name = read_string(snapshot.get(8))
                record = {
                    'kind': kind,
                    'name': name,
                    'exclude_table': table,
                    'raw_offset': None,
                    'offset': None,
                    'bits': None,
                    'flags': None,
                    'call_address': addr,
                }
            elif kind == 'InternalArray':
                # InternalSendPropArray has a hidden sret pointer at stack+0.
                # The real cdecl arguments are: elements, stride, varName,
                # arrayLengthProxy.  The constructed DPT_Array itself has offset
                # zero; its paired preceding SPROP_INSIDEARRAY element carries
                # the array's memory offset.
                name = read_string(snapshot.get(0x0c))
                record = {
                    'kind': kind,
                    'name': name,
                    'exclude_table': None,
                    'raw_offset': 0,
                    'offset': 0,
                    'bits': None,
                    'flags': None,
                    'array_elements': signed32(snapshot.get(4)),
                    'array_stride': signed32(snapshot.get(8)),
                    'array_length_proxy_address': snapshot.get(0x10),
                    'call_address': addr,
                }
            else:
                name = read_string(snapshot.get(4))
                raw_offset = signed32(snapshot.get(8))
                record = {
                    'kind': kind,
                    'name': name,
                    'exclude_table': None,
                    'raw_offset': raw_offset,
                    # Normal absolute offsets in this binary are small positive values.
                    # Some props intentionally use generated negative relative offsets;
                    # preserve them in raw_offset instead of pretending they are absolute.
                    'offset': raw_offset if raw_offset is not None and 0 <= raw_offset < 0x100000 else None,
                    'bits': signed32(snapshot.get(0x10)) if kind in ('Int', 'Float', 'Angle', 'Vector', 'EHandle', 'String', 'QAngles', 'IntMinusOne') else None,
                    'flags': signed32(snapshot.get(0x14)) if kind in ('Int', 'Float', 'Angle', 'Vector', 'EHandle', 'String', 'QAngles', 'IntMinusOne') else None,
                    'call_address': addr,
                }
            record['destination_address'] = snapshot.get(0)
            if kind == 'DataTable':
                record['data_table_address'] = snapshot.get(0x0c)
                record['data_table_proxy_address'] = snapshot.get(0x10)
            calls.append(record)
        elif symbol.startswith('_ZN9SendTable9Construct'):
            construct = {
                'call_address': addr,
                'first_prop_address': stack.get(4),
                'runtime_prop_count': signed32(stack.get(8)),
                'table_name': read_string(stack.get(0x0c)),
            }

        # cdecl caller-saved registers are no longer reliable after calls.
        regs['eax'] = regs['ecx'] = regs['edx'] = None

    # SENDPROP_ARRAY-style macros first construct a temporary element prop and
    # then place the SendPropArray3 result in g_SendProps.  The helper calls are
    # adjacent in the generated initializer and share the same field name; only
    # the Array3 result is a runtime table property.
    collapsed = []
    for call in calls:
        if call['kind'] == 'Array3' and collapsed and collapsed[-1].get('name') == call.get('name'):
            collapsed.pop()
        elif call['kind'] == 'UtlVector' and collapsed:
            # SendPropUtlVector embeds a separately constructed element prop
            # immediately before the vector property itself.
            collapsed.pop()
        collapsed.append(call)

    sentinel = [x for x in collapsed if x.get('name') == 'should_never_see_this']
    runtime_props = [x for x in collapsed if x.get('name') != 'should_never_see_this']
    for index, prop in enumerate(runtime_props):
        prop['index'] = index

    raw_count = raw_info.get('raw_storage_count') if raw_info else None
    return {
        'name': table_name,
        'function_address': start,
        'function_size': size,
        'g_sendprops_address': raw_info.get('address') if raw_info else None,
        'raw_storage_count': raw_count,
        'sentinel_count': len(sentinel),
        'runtime_prop_count': construct.get('runtime_prop_count') if construct else None,
        'construct': construct,
        'props': runtime_props,
    }


def parse(elf: Path, all_tables=False):
    functions, arrays, sendtables = parse_symbols(elf)
    relocs = parse_relocations(elf)
    read_string = make_string_reader(elf)
    names = sorted(arrays) if all_tables else SELECTED
    shared = None
    if all_tables and functions:
        ranges = [(functions[name][0], functions[name][0] + functions[name][1])
                  for name in names if name in functions]
        start = min(x[0] for x in ranges)
        stop = max(x[1] for x in ranges)
        instructions = parse_disassembly(elf, start, stop, ranges)
        shared = ([x[0] for x in instructions], instructions)
    tables = {}
    for name in names:
        if name not in functions:
            tables[name] = None
            continue
        tables[name] = decode_table(elf, name, functions[name], arrays.get(name), relocs, read_string, shared)
    for table in tables.values():
        if not table:
            continue
        for prop in table.get('props', []):
            if prop.get('kind') == 'DataTable':
                target = sendtables.get(prop.get('data_table_address'))
                prop['data_table_target'] = ('DT_' + target) if target else None
    return {
        'elf': str(elf),
        'sendprop_size': PROP_SIZE,
        'selected_table_count': len(SELECTED),
        'table_count': len(tables),
        'tables': tables,
    }


def validate(data):
    for name, expected in EXPECTED_RUNTIME_COUNTS.items():
        table = data['tables'].get(name)
        if not table:
            raise SystemExit(f'missing ServerClassInit for DT_{name}')
        if table['sentinel_count'] != 1:
            raise SystemExit(f'DT_{name}: expected exactly one g_SendProps sentinel, got {table["sentinel_count"]}')
        if table['runtime_prop_count'] != expected:
            raise SystemExit(f'DT_{name}: runtime count {table["runtime_prop_count"]} != {expected}')
        if len(table['props']) != expected:
            raise SystemExit(f'DT_{name}: decoded prop calls {len(table["props"])} != {expected}')
        if table['raw_storage_count'] != expected + 1:
            raise SystemExit(f'DT_{name}: raw g_SendProps count {table["raw_storage_count"]} != {expected + 1}')
        if any(not prop.get('name') for prop in table['props']):
            raise SystemExit(f'DT_{name}: unnamed recovered property')

    # High-value field/order checks from the original build4100 graph.
    planted = data['tables']['PlantedC4']['props']
    expected_planted = [
        ('baseclass', 'DataTable'), ('m_bBombTicking', 'Bool'), ('m_flC4Blow', 'Float'),
        ('m_flTimerLength', 'Float'), ('m_flDefuseLength', 'Float'),
        ('m_flDefuseCountDown', 'Float'),
    ]
    if [(x['name'], x['kind']) for x in planted] != expected_planted:
        raise SystemExit('DT_PlantedC4 field graph mismatch')
    offsets = {x['name']: x['offset'] for x in planted}
    for key, value in {'m_bBombTicking':0x47c, 'm_flC4Blow':0x480, 'm_flTimerLength':0x484,
                       'm_flDefuseLength':0x498, 'm_flDefuseCountDown':0x49c}.items():
        if offsets.get(key) != value:
            raise SystemExit(f'DT_PlantedC4 {key} offset mismatch: {offsets.get(key)} != {value}')

    player = data['tables']['CSPlayer']['props']
    names = [x['name'] for x in player]
    for required in ('m_iAccount', 'm_bInBombZone', 'm_bInBuyZone', 'm_ArmorValue',
                     'm_bHasDefuser', 'm_bHasHelmet', 'm_flFlashDuration', 'm_hRagdoll'):
        if required not in names:
            raise SystemExit(f'DT_CSPlayer missing {required}')
    player_offsets = {x['name']: x['offset'] for x in player}
    for key, value in {'m_iAccount':0xe30, 'm_bInBombZone':0x126c, 'm_bInBuyZone':0x126d,
                       'm_ArmorValue':0xc0c, 'm_bHasDefuser':0xe5c, 'm_bHasHelmet':0xe20,
                       'm_flFlashDuration':0x12b8, 'm_hRagdoll':0x12cc}.items():
        if player_offsets.get(key) != value:
            raise SystemExit(f'DT_CSPlayer {key} offset mismatch: {player_offsets.get(key)} != {value}')


def validate_all(data):
    if data.get('table_count') != 198:
        raise SystemExit(f'expected 198 original send tables, got {data.get("table_count")}')
    for name, table in data['tables'].items():
        if not table:
            raise SystemExit(f'DT_{name}: missing initializer')
        raw = table['raw_storage_count']
        runtime = table['runtime_prop_count']
        decoded = len(table['props'])
        if raw is None or runtime != raw - 1 or decoded != runtime or table['sentinel_count'] != 1:
            raise SystemExit(
                f'DT_{name}: graph mismatch raw={raw} runtime={runtime} decoded={decoded} '
                f'sentinel={table["sentinel_count"]}')
        if any(not prop.get('name') for prop in table['props']):
            raise SystemExit(f'DT_{name}: unnamed recovered property')

    expected_arrays = {
        ('BasePlayer', 'm_hViewModel'): (2, 4, 0),
        ('BaseViewModel', 'm_flPoseParameter'): (24, 4, 0),
        ('Beam', 'm_hAttachEntity'): (10, 4, 0),
        ('Beam', 'm_nAttachIndex'): (10, 4, 0),
        ('EnvScreenOverlay', 'm_iszOverlayNames'): (10, 4, 0),
        ('EnvScreenOverlay', 'm_flOverlayTimes'): (10, 4, 0),
        ('Ragdoll', 'm_ragAngles'): (24, 12, 0),
        ('Ragdoll', 'm_ragPos'): (24, 12, 0),
        ('TEBeamSpline', 'm_vecPoints'): (16, 12, 0),
        ('Team', '"player_array"'): (64, 0, 0x7dbe50),
    }
    recovered_arrays = {}
    for table_name, table in data['tables'].items():
        for prop in table['props']:
            if prop.get('kind') != 'InternalArray':
                continue
            key = (table_name, prop.get('name'))
            recovered_arrays[key] = (
                prop.get('array_elements'),
                prop.get('array_stride'),
                prop.get('array_length_proxy_address') or 0,
            )
    if recovered_arrays != expected_arrays:
        raise SystemExit(f'InternalSendPropArray mismatch: {recovered_arrays!r}')
    validate(data)



def write_links_include(data, path: Path):
    links = []
    for table_name, table in data["tables"].items():
        if not table:
            continue
        for prop in table.get("props", []):
            if prop.get("kind") != "DataTable":
                continue
            target = prop.get("data_table_target")
            if not target:
                raise SystemExit(f"DT_{table_name}.{prop.get('name')}: unresolved DataTable target")
            links.append(("DT_" + table_name, prop.get("name"), target))
    links.sort()
    with path.open("w", encoding="utf-8", newline="\n") as out:
        out.write("// Generated from original build4100 server_i486.so by sendprop_field_oracle.py --all.\n")
        out.write("// Exact relocation-backed SendPropDataTable links across all 198 recovered SendTables.\n\n")
        out.write("static constexpr DataTableLinkDescriptor kLiveDataTableLinks[] = {\n")
        for owner, prop, target in links:
            out.write(f'    {{"{owner}", "{prop}", "{target}"}},\n')
        out.write("};\n")
    return len(links)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--elf', required=True)
    ap.add_argument('--out')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--all', action='store_true', help='recover all 198 build4100 SendTables')
    ap.add_argument('--out-links', type=Path, help='write exact DataTable-link C++ include (requires --all)')
    args = ap.parse_args()
    data = parse(Path(args.elf), all_tables=args.all)
    if args.check:
        validate_all(data) if args.all else validate(data)
    if args.out_links:
        if not args.all:
            raise SystemExit('--out-links requires --all')
        count = write_links_include(data, args.out_links)
        print(f'SENDTABLE_LINK_ORACLE=PASS links={count}', file=sys.stderr)
    text = json.dumps(data, indent=2, sort_keys=True)
    if args.out:
        Path(args.out).write_text(text + '\n')
    else:
        print(text)


if __name__ == '__main__':
    main()
