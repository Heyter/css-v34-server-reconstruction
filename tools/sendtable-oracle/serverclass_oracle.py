#!/usr/bin/env python3
"""Recover build4100 ServerClass network-name/root-SendTable pairs from the original ELF.

The unstripped v34 Linux game DLL exposes one GetClassName/GetServerClass pair per
networked C++ type. GetClassName is a tiny `mov eax, <rodata>; ret` function and
each class's m_pClassSendTable relocation targets DT_*::g_SendTable. Combining
those two independent pieces yields the exact ServerClass name -> root table set
without loading the original DLL.
"""
from __future__ import annotations

import argparse
import re
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path

COMPAT_PREFIX = (
    "CWorld", "CCSPlayer", "CCSTeam", "CCSPlayerResource", "CCSRagdoll",
    "CHostage", "CPlantedC4", "CAK47", "CWeaponM4A1", "CWeaponAWP",
    "CC4", "CFlashbang", "CHEGrenade", "CSmokeGrenade",
    "CBaseCSGrenadeProjectile",
)

@dataclass(frozen=True)
class Entry:
    network: str
    table: str
    cpp_type: str


def run(*argv: str) -> str:
    return subprocess.check_output(argv, text=True, errors="replace")


def symbols(elf: Path):
    rows = []
    for line in run("nm", "-C", str(elf)).splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+([A-Za-z])\s+(.+)$", line)
        if match:
            rows.append((int(match.group(1), 16), match.group(2), match.group(3)))
    return rows


def load_segments(elf: Path):
    segments = []
    for line in run("readelf", "-lW", str(elf)).splitlines():
        # LOAD off vaddr paddr filesz memsz flags align
        match = re.match(
            r"^\s*LOAD\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+"
            r"0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\s+0x[0-9a-fA-F]+\s+",
            line,
        )
        if match:
            segments.append(tuple(int(value, 16) for value in match.groups()))
    if not segments:
        raise SystemExit("no PT_LOAD segments found")
    return segments


def file_offset(vaddr: int, segments) -> int:
    for offset, base, file_size in segments:
        if base <= vaddr < base + file_size:
            return offset + (vaddr - base)
    raise ValueError(f"virtual address not file-backed: 0x{vaddr:08x}")


def relocations(elf: Path):
    result = {}
    for line in run("readelf", "-rW", str(elf)).splitlines():
        match = re.match(
            r"^([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+R_386_32\s+([0-9a-fA-F]+)\s+",
            line,
        )
        if match:
            result[int(match.group(1), 16)] = int(match.group(2), 16)
    return result


def c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"unterminated string at file offset 0x{offset:x}")
    return data[offset:end].decode("ascii")


def recover(elf: Path) -> list[Entry]:
    data = elf.read_bytes()
    syms = symbols(elf)
    address_by_name = {name: address for address, _kind, name in syms}
    sendtable_by_address = {}
    for address, _kind, name in syms:
        match = re.match(r"DT_(.+)::g_SendTable$", name)
        if match:
            sendtable_by_address[address] = "DT_" + match.group(1)
    relocs = relocations(elf)
    segments = load_segments(elf)

    recovered: list[Entry] = []
    for address, _kind, name in syms:
        match = re.match(r"(.+)::GetClassName\(\)$", name)
        if not match:
            continue
        cpp_type = match.group(1)
        if cpp_type + "::GetServerClass()" not in address_by_name:
            continue
        class_name_off = file_offset(address, segments)
        code = data[class_name_off:class_name_off + 6]
        if len(code) != 6 or code[0] != 0xB8 or code[5] != 0xC3:
            raise SystemExit(f"unexpected GetClassName code for {cpp_type}: {code.hex()}")
        network_vaddr = struct.unpack_from("<I", code, 1)[0]
        network = c_string(data, file_offset(network_vaddr, segments))

        ptr_symbol = cpp_type + "::m_pClassSendTable"
        ptr_address = address_by_name.get(ptr_symbol)
        if ptr_address is None:
            raise SystemExit(f"missing {ptr_symbol}")
        sendtable_address = relocs.get(ptr_address)
        if sendtable_address is None:
            raise SystemExit(f"missing relocation for {ptr_symbol} @ 0x{ptr_address:08x}")
        table = sendtable_by_address.get(sendtable_address)
        if table is None:
            raise SystemExit(
                f"m_pClassSendTable relocation for {cpp_type} does not target DT_*::g_SendTable: "
                f"0x{sendtable_address:08x}"
            )
        recovered.append(Entry(network, table, cpp_type))

    by_network = {entry.network: entry for entry in recovered}
    if len(by_network) != len(recovered):
        raise SystemExit("duplicate network class name recovered")
    if len({entry.table for entry in recovered}) != len(recovered):
        raise SystemExit("duplicate root SendTable recovered")
    if len(recovered) != 177:
        raise SystemExit(f"unexpected build4100 ServerClass count: {len(recovered)} (expected 177)")
    missing = [name for name in COMPAT_PREFIX if name not in by_network]
    if missing:
        raise SystemExit(f"compatibility prefix classes missing: {missing}")

    prefix = [by_network[name] for name in COMPAT_PREFIX]
    prefix_names = set(COMPAT_PREFIX)
    remainder = sorted((entry for entry in recovered if entry.network not in prefix_names),
                       key=lambda entry: entry.network)
    return prefix + remainder


def write_tsv(entries: list[Entry], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as out:
        out.write("network_name\troot_table\tcpp_type\n")
        for entry in entries:
            out.write(f"{entry.network}\t{entry.table}\t{entry.cpp_type}\n")


def write_inc(entries: list[Entry], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as out:
        out.write("// Generated from original build4100 server_i486.so by serverclass_oracle.py.\n")
        out.write("// Order keeps the previously-live 15-class prefix stable, then sorts the exact remainder.\n")
        for entry in entries:
            out.write(f'    {{"{entry.network}", "{entry.table}", "{entry.cpp_type}"}},\n')


def read_tsv(path: Path) -> list[Entry]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "network_name\troot_table\tcpp_type":
        raise SystemExit(f"invalid ServerClass TSV header: {path}")
    entries = []
    for line in lines[1:]:
        if not line:
            continue
        fields = line.split("\t")
        if len(fields) != 3:
            raise SystemExit(f"invalid ServerClass TSV row: {line!r}")
        entries.append(Entry(*fields))
    return entries


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--out-tsv", type=Path)
    parser.add_argument("--out-inc", type=Path)
    parser.add_argument("--check-tsv", type=Path)
    args = parser.parse_args()
    if args.elf is None and args.check_tsv is None:
        parser.error("provide --elf and/or --check-tsv")

    entries = recover(args.elf) if args.elf else read_tsv(args.check_tsv)
    if args.check_tsv is not None and args.elf is not None:
        expected = read_tsv(args.check_tsv)
        if entries != expected:
            raise SystemExit("ServerClass TSV differs from original ELF recovery")
    if len(entries) != 177 or [entry.network for entry in entries[:15]] != list(COMPAT_PREFIX):
        raise SystemExit("ServerClass contract failed: expected 177 roots and stable 15-class prefix")
    if args.out_tsv:
        write_tsv(entries, args.out_tsv)
    if args.out_inc:
        write_inc(entries, args.out_inc)
    print(f"SERVERCLASS_ORACLE=PASS classes={len(entries)} prefix=15 exact_names=177 exact_tables=177")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
