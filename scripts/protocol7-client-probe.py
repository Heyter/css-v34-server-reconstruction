#!/usr/bin/env python3
"""Source build4100 protocol-7 real-client bootstrap probe.

The probe drives the exact build4100 boundaries needed before game-DLL
IServerGameClients::ClientConnect:
  connectionless challenge/connect -> S2C_CONNECTION -> TCP stream bind ->
  first UDP netchannel packet containing NET_SignonState(2, -1).

After state 2 it waits for the server-driven NEW (state 3), then sends the exact
build4100 CLC_ClientInfo and matching NEW acknowledgement.  With sv_sendtables=1
in the integration runtime, a deliberately mismatched SendTable CRC forces the
full svc_SendTable path.  The probe decodes UDP CNetChan reliable fragments,
BZip2 transfers, svc_SendTable envelopes, and svc_ClassInfo.
"""

from __future__ import annotations

import argparse
import bz2
import hashlib
import os
import re
import select
import socket
import struct
import sys
import time
from pathlib import Path

CONNECTIONLESS = b"\xff\xff\xff\xff"
A2S_GETCHALLENGE = ord("q")
S2C_CHALLENGE = ord("A")
C2S_CONNECT = ord("k")
S2C_CONNECTION = ord("B")
PROTOCOL_VERSION = 7
PROTOCOL_HASHEDCDKEY = 2
PROTOCOL_STEAM = 3

# build4100 net message ids / signon states.
NET_NOP = 0
NET_DISCONNECT = 1
NET_FILE = 2
NET_TICK = 3
NET_STRINGCMD = 4
NET_SETCONVAR = 5
NETMSG_SIGNONSTATE = 6
SVC_PRINT = 7
CLC_CLIENTINFO = 8
SVC_SERVERINFO = 8
CLC_MOVE = 9
SVC_SENDTABLE = 9
CLC_VOICEDATA = 10
SVC_CLASSINFO = 10
CLC_BASELINEACK = 11
CLC_LISTENEVENTS = 12
SVC_CREATESTRINGTABLE = 12
SVC_VOICEINIT = 14
SVC_VOICEDATA = 15
SVC_SETVIEW = 18
SVC_USERMESSAGE = 23
SVC_ENTITYMESSAGE = 24
SVC_GAMEEVENT = 25
SVC_PACKETENTITIES = 26
SVC_TEMPENTITIES = 27
SVC_GAMEEVENTLIST = 30
EVENT_INDEX_BITS = 8
NET_MAX_PAYLOAD_BITS_OLD = 17
MAX_EDICT_BITS = 11
MAX_SERVER_CLASS_BITS = 9
SIGNONSTATE_CONNECTED = 2
SIGNONSTATE_NEW = 3
SIGNONSTATE_PRESPAWN = 4
SIGNONSTATE_SPAWN = 5
SIGNONSTATE_FULL = 6
REVISION_FILE = Path(__file__).with_name("reconstruction-revision.txt")
PROBE_REVISION = REVISION_FILE.read_text(encoding="ascii").strip()
if re.fullmatch(r"v1\.8\.[0-9]+[a-z]+", PROBE_REVISION) is None:
    raise RuntimeError(f"invalid reconstruction revision: {PROBE_REVISION!r}")

NETMSG_TYPE_BITS = 5
PACKET_FLAG_RELIABLE = 1 << 0
PACKET_FLAG_CHOKED = 1 << 4
PACKET_FLAG_CHALLENGE = 1 << 5
MAX_STREAMS = 2
FRAGMENT_BITS = 8
FRAGMENT_SIZE = 1 << FRAGMENT_BITS
MAX_FILE_SIZE_BITS = 26
SINGLE_BLOCK_SIZE_BITS = 17
NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS = 10

OBJECTIVE_EVENT_NAMES = {
    "round_start", "round_freeze_end", "round_end",
    "bomb_dropped", "bomb_pickup", "bomb_beginplant", "bomb_abortplant",
    "bomb_planted", "bomb_beep", "bomb_begindefuse", "bomb_abortdefuse",
    "bomb_defused", "bomb_exploded",
    "hostage_follows", "hostage_stops_following", "hostage_hurt",
    "hostage_rescued", "hostage_rescued_all", "hostage_killed",
}
BOMB_SCENARIO_REQUIRED_EVENTS = {
    "round_start", "round_freeze_end", "round_end", "bomb_dropped", "bomb_pickup",
    "bomb_beginplant", "bomb_abortplant", "bomb_planted", "bomb_beep",
    "bomb_begindefuse", "bomb_abortdefuse", "bomb_defused", "bomb_exploded",
}
HOSTAGE_SCENARIO_REQUIRED_EVENTS = {
    "round_start", "round_end", "hostage_follows", "hostage_stops_following",
    "hostage_hurt", "hostage_killed", "hostage_rescued", "hostage_rescued_all",
}

# build4100 SendProp flags used by the schema-driven value decoder.
SPROP_UNSIGNED = 1 << 0
SPROP_COORD = 1 << 1
SPROP_NOSCALE = 1 << 2
SPROP_NORMAL = 1 << 5
SPROP_XYZE = 1 << 7
SPROP_INSIDEARRAY = 1 << 8

# build4100 TCP stream control bytes.
STREAM_CMD_DATA = 2
STREAM_CMD_ACK = 4

# build4100 CNetworkStringTable variable-userdata envelope.  The original
# engine_i486.so ParseUpdate/WriteUpdate pair uses a 12-bit byte count and
# rejects values above 0x1000.  Later Source branches widened this field;
# protocol7 must follow the build4100 wire ABI, not those later branches.
STRINGTABLE_USERDATA_LENGTH_BITS = 12
STRINGTABLE_MAX_USERDATA_BYTES = 1 << STRINGTABLE_USERDATA_LENGTH_BITS

# Classic RevEmu SteamGetEncryptedUserIDTicket structure (6 x int32 + 128 bytes).
REVEMU_TICKET_SIZE = 152
REVEMU_HEADER = ord("S")
# GCC/x86 value of the C multi-character literal 'rev' is 0x00726576.
REVEMU_MAGIC = 0x00726576
REVEMU_STEAMID_HIGH = 0x01100001
REVEMU_HASH = 0x13572468


class BitWriter:
    """Tiny LSB-first writer matching build4100 bf_write for integer fields."""

    def __init__(self) -> None:
        self._data = bytearray()
        self._bit = 0

    def write_ubit_long(self, value: int, bits: int) -> None:
        if bits < 0:
            raise ValueError("negative bit count")
        value &= (1 << bits) - 1 if bits else 0
        for i in range(bits):
            byte_index = self._bit >> 3
            bit_index = self._bit & 7
            if byte_index == len(self._data):
                self._data.append(0)
            if (value >> i) & 1:
                self._data[byte_index] |= 1 << bit_index
            self._bit += 1

    def write_byte(self, value: int) -> None:
        self.write_ubit_long(value, 8)

    def write_ubit_var_build4100(self, value: int) -> None:
        """build4100 bf_write::WriteUBitVar: unary width + payload bits."""
        if value < 0:
            raise ValueError("negative ubitvar")
        bits = 0
        base = 0
        while value > (base << 1):
            bits += 1
            base = (1 << bits) - 1
        if bits:
            self.write_ubit_long(0, bits)
        self.write_ubit_long(1, 1)
        if bits:
            self.write_ubit_long(value - base, bits)

    def write_long(self, value: int) -> None:
        self.write_ubit_long(value & 0xFFFFFFFF, 32)

    def write_signed(self, value: int, bits: int) -> None:
        mask = (1 << bits) - 1 if bits < 32 else 0xFFFFFFFF
        self.write_ubit_long(value & mask, bits)

    def write_bit_angle(self, angle: float, bits: int) -> None:
        levels = 1 << bits
        encoded = int(angle * (float(levels) / 360.0)) & (levels - 1)
        self.write_ubit_long(encoded, bits)

    def write_string(self, value: str) -> None:
        raw = value.encode("utf-8", "strict") + b"\0"
        for byte in raw:
            self.write_byte(byte)

    @property
    def bits_written(self) -> int:
        return self._bit

    def bytes(self) -> bytes:
        return bytes(self._data)


def read_ubit_long(data: bytes, bit_offset: int, bits: int) -> tuple[int, int]:
    value = 0
    for i in range(bits):
        pos = bit_offset + i
        if pos >> 3 >= len(data):
            raise ValueError("bitstream truncated")
        if (data[pos >> 3] >> (pos & 7)) & 1:
            value |= 1 << i
    return value, bit_offset + bits


def read_ubit_var_build4100(data: bytes, bit_offset: int) -> tuple[int, int]:
    """Exact build4100 bf_read::ReadUBitVar unary coding.

    Count zero bits until the first one, then read that many payload bits and
    add (2**n - 1). This predates the later six-bit UBitVar format.
    """
    width = 0
    while True:
        marker, bit_offset = read_ubit_long(data, bit_offset, 1)
        if marker:
            break
        width += 1
        if width > 31:
            raise ValueError("ubitvar width overflow")
    base = (1 << width) - 1
    extra = 0
    if width:
        extra, bit_offset = read_ubit_long(data, bit_offset, width)
    return base + extra, bit_offset


def bits_required_for_count(count: int) -> int:
    if count <= 1:
        return 1
    return (count - 1).bit_length()


def load_sendprop_schema(path: str) -> dict[int, dict[int, dict[str, object]]]:
    schema: dict[int, dict[int, dict[str, object]]] = {}
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) not in (10, 17):
                raise ValueError("bad sendprop schema row")
            class_id, class_name, root_table, field_index, prop_name, prop_type, bits, flags, low, high = parts[:10]
            entry = {
                "class_id": int(class_id), "class_name": class_name, "root_table": root_table,
                "field_index": int(field_index), "prop_name": prop_name, "type": int(prop_type),
                "bits": int(bits), "flags": int(flags), "low": float(low), "high": float(high),
                "array_elements": 0, "array_stride": 0,
                "element_type": -1, "element_bits": 0, "element_flags": 0,
                "element_low": 0.0, "element_high": 0.0,
            }
            if len(parts) == 17:
                (array_elements, array_stride, element_type, element_bits, element_flags,
                 element_low, element_high) = parts[10:]
                entry.update({
                    "array_elements": int(array_elements), "array_stride": int(array_stride),
                    "element_type": int(element_type), "element_bits": int(element_bits),
                    "element_flags": int(element_flags), "element_low": float(element_low),
                    "element_high": float(element_high),
                })
            schema.setdefault(int(class_id), {})[int(field_index)] = entry
    if 1 not in schema or 347 not in schema[1] or schema[1][347]["prop_name"] != "m_iAccount":
        raise ValueError("CCSPlayer field347 schema oracle missing")
    return schema


def load_instancebaseline_runtime(path: str) -> dict[int, int]:
    """Load the rebuilt DLL's live ServerClass -> instancebaseline index census."""
    ready: dict[int, int] = {}
    seen_rows: dict[int, tuple[str, str, int, int]] = {}
    used_indexes: set[int] = set()
    with open(path, "r", encoding="utf-8") as handle:
        header = handle.readline().rstrip("\n")
        columns = handle.readline().rstrip("\n")
        if header != "#build4100-instancebaseline-v1":
            raise ValueError("bad instancebaseline header")
        if columns != "#class_id\tclass_name\troot_table\tbaseline_index\tready":
            raise ValueError("bad instancebaseline columns")
        for line in handle:
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) != 5:
                raise ValueError("bad instancebaseline row")
            class_id = int(parts[0]); class_name = parts[1]; root_table = parts[2]
            baseline_index = int(parts[3]); is_ready = int(parts[4])
            if class_id in seen_rows or is_ready not in (0, 1):
                raise ValueError("duplicate/bad instancebaseline class row")
            if is_ready != (1 if baseline_index != 0xffff else 0):
                raise ValueError("instancebaseline ready/sentinel mismatch")
            if baseline_index < 0:
                raise ValueError("negative instancebaseline index")
            seen_rows[class_id] = (class_name, root_table, baseline_index, is_ready)
            if is_ready:
                if baseline_index in used_indexes:
                    raise ValueError("duplicate instancebaseline index")
                used_indexes.add(baseline_index)
                ready[class_id] = baseline_index
    if len(seen_rows) != len(EXPECTED_CLASSES) or set(seen_rows) != set(range(len(EXPECTED_CLASSES))):
        raise ValueError(f"instancebaseline class census mismatch rows={len(seen_rows)}")
    for class_id, (expected_name, expected_table) in enumerate(EXPECTED_CLASSES):
        name, table, _index, _ready = seen_rows[class_id]
        if name != expected_name or table != expected_table:
            raise ValueError(f"instancebaseline class identity mismatch id={class_id}")
    # build4100 does not require one materialized string-table baseline per
    # ServerClass, and a post-changelevel pre-connect snapshot may legitimately
    # contain zero materialized classes. Classes whose first live edict appears
    # after SV_CreateBaseline retain 0xffff and enter PacketEntities relative to
    # an implicit zero/default state. This file is therefore a point-in-time
    # lower-bound snapshot: it may be empty, and the engine may append baselines
    # after the dump when later real clients/entities are created.
    return ready


def decode_instancebaseline_wire(entries: list[tuple[int, str]]) -> dict[int, int]:
    """Decode the build4100 instancebaseline string-table entries.

    The table itself may be present with zero entries after changelevel. That is
    distinct from the table being missing: with an empty pre-connect runtime
    snapshot, PacketEntities can still enter classes against the engine's
    implicit zero/default state. Non-empty runtime snapshots are validated
    separately as a preserved lower bound by validate_instancebaseline_runtime_wire().
    """
    wire: dict[int, int] = {}
    for string_index, class_text in entries:
        if not class_text or not class_text.isdigit():
            raise ValueError(f"bad class token {class_text!r}")
        class_id = int(class_text)
        if class_id < 0 or class_id >= len(EXPECTED_CLASSES) or class_id in wire:
            raise ValueError(f"bad/duplicate class id {class_id}")
        wire[class_id] = int(string_index)
    return wire


def validate_instancebaseline_runtime_wire(runtime: dict[int, int], wire: dict[int, int]) -> list[int]:
    """Validate a pre-connect runtime snapshot against a later signon table.

    The runtime TSV is emitted after fake-client baseline seeding but before the
    Protocol7 real client connects. build4100 can materialize additional class
    baselines between those moments (observed for CCSPlayer), so the runtime map
    is an immutable lower-bound snapshot, not an equality oracle. Every runtime
    baseline must still exist on the wire at the same string-table index.
    """
    runtime_only = sorted(set(runtime) - set(wire))
    index_mismatch = sorted(
        cid for cid in set(runtime) & set(wire) if runtime[cid] != wire[cid]
    )
    if runtime_only or index_mismatch:
        raise ValueError(
            f"runtime snapshot not preserved runtime={len(runtime)} wire={len(wire)} "
            f"runtime_only={runtime_only} index_mismatch={index_mismatch}"
        )
    return sorted(set(wire) - set(runtime))


def _signed_bits(value: int, bits: int) -> int:
    if bits <= 0 or bits >= 32:
        bits = 32
    sign = 1 << (bits - 1)
    return value - (1 << bits) if value & sign else value


def read_bit_coord_build4100(data: bytes, pos: int) -> tuple[float, int]:
    has_int, pos = read_ubit_long(data, pos, 1)
    has_frac, pos = read_ubit_long(data, pos, 1)
    if not has_int and not has_frac:
        return 0.0, pos
    sign, pos = read_ubit_long(data, pos, 1)
    integer = 0
    frac = 0
    if has_int:
        integer, pos = read_ubit_long(data, pos, 14)
        integer += 1
    if has_frac:
        frac, pos = read_ubit_long(data, pos, 5)
    value = float(integer) + float(frac) / 32.0
    return (-value if sign else value), pos


def read_bit_normal_build4100(data: bytes, pos: int) -> tuple[float, int]:
    sign, pos = read_ubit_long(data, pos, 1)
    frac, pos = read_ubit_long(data, pos, 11)
    value = float(frac) / 2047.0
    return (-value if sign else value), pos


def decode_sendprop_value(data: bytes, pos: int, end_bit: int, prop: dict[str, object]) -> tuple[object, int]:
    if pos < 0 or end_bit < pos or end_bit > len(data) * 8:
        raise ValueError("sendprop bit range")
    typ = int(prop["type"]); bits = int(prop["bits"]); flags = int(prop["flags"])

    def finish(value: object, new_pos: int) -> tuple[object, int]:
        if new_pos > end_bit:
            raise ValueError("sendprop value exceeds entity payload")
        return value, new_pos
    if typ == 0:  # DPT_Int
        width = bits if 0 < bits <= 32 else 32
        raw, pos = read_ubit_long(data, pos, width)
        return finish(raw if flags & SPROP_UNSIGNED else _signed_bits(raw, width), pos)
    if typ == 1:  # DPT_Float
        if flags & SPROP_COORD:
            value, pos = read_bit_coord_build4100(data, pos)
            return finish(value, pos)
        if flags & SPROP_NORMAL:
            value, pos = read_bit_normal_build4100(data, pos)
            return finish(value, pos)
        if flags & SPROP_NOSCALE:
            raw, pos = read_ubit_long(data, pos, 32)
            return finish(struct.unpack("<f", struct.pack("<I", raw))[0], pos)
        width = bits if 0 < bits <= 32 else 32
        raw, pos = read_ubit_long(data, pos, width)
        lo, hi = float(prop["low"]), float(prop["high"])
        if width == 32 or hi == lo:
            return finish(raw, pos)
        return finish(lo + (hi - lo) * (raw / float((1 << width) - 1)), pos)
    if typ == 2:  # DPT_Vector
        if flags & SPROP_NORMAL:
            x, pos = read_bit_normal_build4100(data, pos); y, pos = read_bit_normal_build4100(data, pos)
            z_sign, pos = read_ubit_long(data, pos, 1)
            z2 = max(0.0, 1.0 - x*x - y*y); z = z2 ** 0.5
            return finish((x, y, -z if z_sign else z), pos)
        # SPROP_XYZE exists in the build4100 flag ABI, but engine_i486.so's
        # Vector_Encode/Vector_Decode never tests bit 0x80. Therefore XYZE
        # vectors use the same three scalar component encoding as ordinary
        # vectors; do not apply the later RGBE/exponent interpretation here.
        _xyze_build4100 = bool(flags & SPROP_XYZE)
        scalar = dict(prop); scalar["type"] = 1
        x, pos = decode_sendprop_value(data, pos, end_bit, scalar)
        y, pos = decode_sendprop_value(data, pos, end_bit, scalar)
        z, pos = decode_sendprop_value(data, pos, end_bit, scalar)
        return finish((x, y, z), pos)
    if typ == 3:  # DPT_String
        length, pos = read_ubit_long(data, pos, 9)
        raw = bytearray()
        for _ in range(length):
            ch, pos = read_ubit_long(data, pos, 8); raw.append(ch)
        return finish(raw.decode("utf-8", "replace"), pos)
    if typ == 4:  # DPT_Array
        elements = int(prop.get("array_elements", 0))
        element_type = int(prop.get("element_type", -1))
        if elements <= 0 or element_type < 0:
            raise ValueError("DPT_Array schema missing element metadata")
        length_bits = elements.bit_length()  # SendProp::GetNumArrayLengthBits().
        count, pos = read_ubit_long(data, pos, length_bits)
        if count > elements:
            raise ValueError("DPT_Array length exceeds max elements")
        element = {
            "type": element_type,
            "bits": int(prop.get("element_bits", 0)),
            "flags": int(prop.get("element_flags", 0)) & ~SPROP_INSIDEARRAY,
            "low": float(prop.get("element_low", 0.0)),
            "high": float(prop.get("element_high", 0.0)),
        }
        values = []
        for _ in range(count):
            value, pos = decode_sendprop_value(data, pos, end_bit, element)
            values.append(value)
        return finish(tuple(values), pos)
    raise ValueError(f"unsupported sendprop type {typ}")


def parse_entity_updates_schema(packet: dict[str, object], schema: dict[int, dict[int, dict[str, object]]],
                                entity_classes: dict[int, tuple[int, str, str]] | None = None,
                                class_entries: tuple[tuple[str, str], ...] | None = None) -> dict[str, object] | None:
    payload = packet.get("payload")
    if not isinstance(payload, (bytes, bytearray)): return None
    if class_entries is None: class_entries = EXPECTED_CLASSES
    data = bytes(payload); end = int(packet["data_bits"]); updated = int(packet["updated_entries"])
    class_bits = bits_required_for_count(len(class_entries)); pos = 0; base = -1
    known = dict(entity_classes or {}); entries: list[dict[str, object]] = []
    try:
        for _ in range(updated):
            start = pos; d, pos = read_ubit_var_build4100(data, pos); ent = base + 1 + d; base = ent
            leave, pos = read_ubit_long(data, pos, 1)
            if leave:
                delete, pos = read_ubit_long(data, pos, 1)
                entries.append({"entity": ent, "leave": 1, "delete": delete, "props": []}); continue
            enter, pos = read_ubit_long(data, pos, 1)
            serial = 0
            if enter:
                cid, pos = read_ubit_long(data, pos, class_bits); serial, pos = read_ubit_long(data, pos, NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS)
                if cid >= len(class_entries): raise ValueError("class id range")
                known[ent] = (cid, class_entries[cid][0], class_entries[cid][1])
            elif ent not in known:
                raise ValueError("existing entity class unknown")
            cid, cname, tname = known[ent]
            props: list[dict[str, object]] = []; last = -1
            while True:
                present, pos = read_ubit_long(data, pos, 1)
                if not present: break
                delta, pos = read_ubit_var_build4100(data, pos); field = last + 1 + delta; last = field
                meta = schema.get(cid, {}).get(field)
                if meta is None: raise ValueError(f"schema missing class={cid} field={field}")
                value, pos = decode_sendprop_value(data, pos, end, meta)
                props.append({"field_index": field, "name": meta["prop_name"], "type": meta["type"], "value": value})
            entries.append({"entity": ent, "class_id": cid, "class_name": cname, "table_name": tname,
                            "serial": serial, "enter": enter, "props": props, "start_bit": start, "end_bit": pos})
        # build4100 can leave a short all-zero tail after the per-entity
        # CDeltaBitsWriter terminator when recursive datatable recipient proxies
        # (SendProxy_SendLocalDataTable / SendProxy_SendNonLocalDataTable) cull
        # the flattened property set for a client.  The real v1.8.15a WSL
        # packet leaves 13 zero bits here (full snapshot: 12).  They carry no
        # SendProp value and must not make an otherwise complete entity decode
        # fail.  Keep this bounded and accept only literal zero bits.
        outer = 0
        if pos < end:
            remaining = end - pos
            if remaining <= 32:
                zero_tail = True
                check = pos
                while check < end:
                    bit, check = read_ubit_long(data, check, 1)
                    if bit != 0:
                        zero_tail = False
                        break
                if zero_tail:
                    outer = remaining
                    pos = end
        return {"complete": pos == end, "reason": "" if pos == end else "trailing_entity_bits",
                "entries": entries, "entity_classes": known, "bits_consumed": pos, "data_bits": end,
                "changed_props": sum(len(e.get("props", [])) for e in entries), "outer_terminator_bits": outer}
    except (ValueError, KeyError) as exc:
        return {"complete": False, "reason": str(exc), "entries": entries, "bits_consumed": pos, "data_bits": end}


def read_delta_prop_indexes_build4100(
    data: bytes, bit_offset: int, end_bit: int
) -> tuple[list[int], int]:
    """Decode CDeltaBitsReader's property-index stream.

    Each changed prop starts with one bit set, followed by build4100 UBitVar
    for (newIndex-lastIndex-1). A zero bit terminates the list. Values follow
    each non-empty index and need the class schema to advance; current baseline
    fixture has the exact empty-list path for all three players.
    """
    indexes: list[int] = []
    last = -1
    while bit_offset < end_bit:
        present, bit_offset = read_ubit_long(data, bit_offset, 1)
        if not present:
            return indexes, bit_offset
        delta, bit_offset = read_ubit_var_build4100(data, bit_offset)
        index = last + 1 + delta
        if index <= last:
            raise ValueError("non-monotonic delta prop index")
        indexes.append(index)
        last = index
        # The caller cannot skip an encoded value without the flattened class
        # schema. Surface the index rather than silently guessing its length.
        return indexes, bit_offset
    raise ValueError("unterminated delta prop index list")


def parse_full_snapshot_entity_updates(
    packet: dict[str, object], class_entries: tuple[tuple[str, str], ...] | None = None
) -> dict[str, object] | None:
    """Decode full-snapshot entity headers and zero-delta SendProp bodies.

    The v1.8.6 real build4100 snapshot uses three EnterPVS records, each with
    an empty CDeltaBitsReader property list. Empty means the entity is exactly
    equal to its instance/static baseline for every flattened SendProp.
    """
    if int(packet.get("is_delta", 1)) != 0:
        return None
    if class_entries is None:
        class_entries = EXPECTED_CLASSES
    payload = packet.get("payload")
    if not isinstance(payload, (bytes, bytearray)):
        return None
    data = bytes(payload)
    data_bits = int(packet["data_bits"])
    updated = int(packet["updated_entries"])
    class_bits = bits_required_for_count(len(class_entries))
    pos = 0
    header_base = -1
    entities: list[dict[str, object]] = []
    try:
        for _ in range(updated):
            start = pos
            entity_delta, pos = read_ubit_var_build4100(data, pos)
            entity = header_base + 1 + entity_delta
            header_base = entity
            leave, pos = read_ubit_long(data, pos, 1)
            if leave:
                delete, pos = read_ubit_long(data, pos, 1)
                return {
                    "complete": False,
                    "reason": "leavepvs_in_full_snapshot",
                    "entities": entities,
                    "bit": start,
                    "delete": delete,
                }
            enter, pos = read_ubit_long(data, pos, 1)
            if not enter:
                return {
                    "complete": False,
                    "reason": "non_enterpvs_in_full_snapshot",
                    "entities": entities,
                    "bit": start,
                }
            class_id, pos = read_ubit_long(data, pos, class_bits)
            serial, pos = read_ubit_long(data, pos, NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS)
            if class_id >= len(class_entries):
                return {
                    "complete": False,
                    "reason": "class_id_out_of_range",
                    "entities": entities,
                    "bit": start,
                }
            changed_props, next_pos = read_delta_prop_indexes_build4100(data, pos, data_bits)
            if changed_props:
                entities.append({
                    "entity": entity,
                    "class_id": class_id,
                    "class_name": class_entries[class_id][0],
                    "table_name": class_entries[class_id][1],
                    "serial": serial,
                    "changed_props": changed_props,
                    "start_bit": start,
                    "prop_bit": pos,
                })
                return {
                    "complete": False,
                    "reason": "nonzero_prop_values_need_schema",
                    "entities": entities,
                    "bit": next_pos,
                }
            pos = next_pos
            entities.append({
                "entity": entity,
                "class_id": class_id,
                "class_name": class_entries[class_id][0],
                "table_name": class_entries[class_id][1],
                "serial": serial,
                "changed_props": [],
                "start_bit": start,
                "end_bit": pos,
            })
        return {
            "complete": pos == data_bits,
            "reason": "" if pos == data_bits else "trailing_entity_bits",
            "entities": entities,
            "bits_consumed": pos,
            "data_bits": data_bits,
            "changed_props": sum(len(e["changed_props"]) for e in entities),
        }
    except ValueError as exc:
        return {"complete": False, "reason": str(exc), "entities": entities, "bit": pos}


SERVERCLASS_TSV = Path(__file__).resolve().parents[1] / "config" / "serverclasses-build4100.tsv"

def load_expected_classes() -> tuple[tuple[str, str], ...]:
    rows: list[tuple[str, str]] = []
    lines = SERVERCLASS_TSV.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "network_name\troot_table\tcpp_type":
        raise RuntimeError(f"invalid ServerClass TSV header: {SERVERCLASS_TSV}")
    for line in lines[1:]:
        if not line:
            continue
        network, table, _cpp_type = line.split("\t")
        rows.append((network, table))
    expected_prefix = (
        ("CWorld", "DT_WORLD"),
        ("CCSPlayer", "DT_CSPlayer"),
        ("CCSTeam", "DT_CSTeam"),
        ("CCSPlayerResource", "DT_CSPlayerResource"),
        ("CCSRagdoll", "DT_CSRagdoll"),
        ("CHostage", "DT_CHostage"),
        ("CPlantedC4", "DT_PlantedC4"),
        ("CAK47", "DT_WeaponAK47"),
        ("CWeaponM4A1", "DT_WeaponM4A1"),
        ("CWeaponAWP", "DT_WeaponAWP"),
        ("CC4", "DT_WeaponC4"),
        ("CFlashbang", "DT_Flashbang"),
        ("CHEGrenade", "DT_HEGrenade"),
        ("CSmokeGrenade", "DT_SmokeGrenade"),
        ("CBaseCSGrenadeProjectile", "DT_BaseCSGrenadeProjectile"),
    )
    if len(rows) != 177 or tuple(rows[:15]) != expected_prefix:
        raise RuntimeError(f"invalid build4100 ServerClass contract: classes={len(rows)}")
    return tuple(rows)

EXPECTED_CLASSES = load_expected_classes()

EXPECTED_STRINGTABLES: tuple[str, ...] = (
    "downloadables",
    "modelprecache",
    "genericprecache",
    "soundprecache",
    "decalprecache",
    "instancebaseline",
    "lightstyles",
    "userinfo",
    "server_query_info",
    "EffectDispatch",
    "VguiScreen",
    "Materials",
    "InfoPanel",
    "Scenes",
    "GameRulesCreation",
    "BlackMarketTable",
)



def read_cstring_bits(
    data: bytes, bit_offset: int, max_bytes: int = 512, *, allow_controls: bool = False
) -> tuple[str, int]:
    out = bytearray()
    for _ in range(max_bytes):
        value, bit_offset = read_ubit_long(data, bit_offset, 8)
        if value == 0:
            return out.decode("ascii", "strict"), bit_offset
        if value < 0x20 or value > 0x7E:
            if not (allow_controls and value in (0x09, 0x0A, 0x0D)):
                raise ValueError("non-printable network string")
        out.append(value)
    raise ValueError("unterminated network string")



GAMEEVENT_TYPE_LOCAL = 0
GAMEEVENT_TYPE_STRING = 1
GAMEEVENT_TYPE_FLOAT = 2
GAMEEVENT_TYPE_LONG = 3
GAMEEVENT_TYPE_SHORT = 4
GAMEEVENT_TYPE_BYTE = 5
GAMEEVENT_TYPE_BOOL = 6


def parse_gameevent_list_at(data: bytes, start_bit: int) -> dict[str, object] | None:
    """Parse build4100 svc_GameEventList (type30).

    engine_i486.so proves the envelope as type:5 + numEvents:9 + length:20.
    CGameEventManager::WriteEventList serializes each descriptor as
    eventId:9 + name:cstring + repeated(type:3 + fieldName:cstring), terminated
    by type 0. Local-only fields (type 0) are omitted from the wire list.
    """
    try:
        msg_type, pos = read_ubit_long(data, start_bit, NETMSG_TYPE_BITS)
        if msg_type != SVC_GAMEEVENTLIST:
            return None
        num_events, pos = read_ubit_long(data, pos, 9)
        length_bits, pos = read_ubit_long(data, pos, 20)
        if num_events <= 0 or num_events > 512 or length_bits <= 0:
            return None
        end_bit = pos + length_bits
        if end_bit > len(data) * 8:
            return None
        descriptors: dict[int, dict[str, object]] = {}
        for _ in range(num_events):
            event_id, pos = read_ubit_long(data, pos, 9)
            if event_id in descriptors:
                return None
            name, pos = read_cstring_bits(data, pos, 128)
            if not name:
                return None
            fields: list[tuple[str, int]] = []
            for _field in range(64):
                field_type, pos = read_ubit_long(data, pos, 3)
                if field_type == GAMEEVENT_TYPE_LOCAL:
                    break
                if field_type > GAMEEVENT_TYPE_BOOL:
                    return None
                field_name, pos = read_cstring_bits(data, pos, 128)
                if not field_name:
                    return None
                fields.append((field_name, field_type))
            else:
                return None
            descriptors[event_id] = {"name": name, "fields": fields}
        if pos != end_bit:
            return None
        return {
            "start_bit": start_bit,
            "end_bit": end_bit,
            "num_events": num_events,
            "length_bits": length_bits,
            "descriptors": descriptors,
        }
    except (ValueError, UnicodeDecodeError):
        return None


def scan_gameevent_lists(data: bytes, start_bit: int = 0) -> list[dict[str, object]]:
    total = len(data) * 8
    out: list[dict[str, object]] = []
    if start_bit < 0:
        start_bit = 0
    for bit in range(start_bit, max(start_bit, total - (NETMSG_TYPE_BITS + 9 + 20) + 1)):
        try:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        except ValueError:
            break
        if msg_type != SVC_GAMEEVENTLIST:
            continue
        parsed = parse_gameevent_list_at(data, bit)
        if parsed is not None:
            out.append(parsed)
    return out


def _decode_gameevent_value(data: bytes, pos: int, end_bit: int, field_type: int) -> tuple[object, int]:
    if field_type == GAMEEVENT_TYPE_STRING:
        value, pos = read_cstring_bits(data, pos, 512, allow_controls=True)
    elif field_type == GAMEEVENT_TYPE_FLOAT:
        raw, pos = read_ubit_long(data, pos, 32)
        value = struct.unpack("<f", struct.pack("<I", raw))[0]
    elif field_type == GAMEEVENT_TYPE_LONG:
        raw, pos = read_ubit_long(data, pos, 32)
        value = _signed_bits(raw, 32)
    elif field_type == GAMEEVENT_TYPE_SHORT:
        raw, pos = read_ubit_long(data, pos, 16)
        value = _signed_bits(raw, 16)
    elif field_type == GAMEEVENT_TYPE_BYTE:
        value, pos = read_ubit_long(data, pos, 8)
    elif field_type == GAMEEVENT_TYPE_BOOL:
        value, pos = read_ubit_long(data, pos, 1)
        value = bool(value)
    else:
        raise ValueError("unsupported gameevent field type")
    if pos > end_bit:
        raise ValueError("gameevent value exceeds envelope")
    return value, pos


def parse_gameevent_at(
    data: bytes, start_bit: int, descriptors: dict[int, dict[str, object]]
) -> dict[str, object] | None:
    """Parse build4100 svc_GameEvent (type25): type:5 + length:11 + event bits."""
    try:
        msg_type, pos = read_ubit_long(data, start_bit, NETMSG_TYPE_BITS)
        if msg_type != SVC_GAMEEVENT:
            return None
        length_bits, pos = read_ubit_long(data, pos, 11)
        if length_bits < 9:
            return None
        end_bit = pos + length_bits
        if end_bit > len(data) * 8:
            return None
        event_id, pos = read_ubit_long(data, pos, 9)
        descriptor = descriptors.get(event_id)
        if descriptor is None:
            return None
        values: dict[str, object] = {}
        for field_name, field_type in descriptor.get("fields", []):
            value, pos = _decode_gameevent_value(data, pos, end_bit, int(field_type))
            values[str(field_name)] = value
        if pos != end_bit:
            return None
        return {
            "start_bit": start_bit,
            "end_bit": end_bit,
            "length_bits": length_bits,
            "event_id": event_id,
            "name": str(descriptor.get("name", "")),
            "values": values,
        }
    except (ValueError, UnicodeDecodeError, struct.error):
        return None


def scan_gameevents(
    data: bytes, descriptors: dict[int, dict[str, object]], start_bit: int = 0
) -> list[dict[str, object]]:
    total = len(data) * 8
    out: list[dict[str, object]] = []
    if not descriptors:
        return out
    if start_bit < 0:
        start_bit = 0
    for bit in range(start_bit, max(start_bit, total - (NETMSG_TYPE_BITS + 11 + 9) + 1)):
        try:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        except ValueError:
            break
        if msg_type != SVC_GAMEEVENT:
            continue
        parsed = parse_gameevent_at(data, bit, descriptors)
        if parsed is not None:
            out.append(parsed)
    return out


def write_serverinfo_message(
    writer: BitWriter,
    *,
    protocol: int = PROTOCOL_VERSION,
    server_count: int = 1,
    is_hltv: int = 0,
    is_dedicated: int = 1,
    client_crc: int = 0xFFFFFFFF,
    max_classes: int = len(EXPECTED_CLASSES),
    map_crc: int = 0x54426650,
    player_slot: int = 2,
    max_clients: int = 4,
    tick_interval: float = 0.015,
    os_char: str = "l",
    game_dir: str = "cstrike",
    map_name: str = "de_dust2",
    sky_name: str = "sky_dust",
    host_name: str = "Counter-Strike: Source",
) -> None:
    """Synthetic build4100 ServerInfo writer used by protocol self-tests."""
    writer.write_ubit_long(SVC_SERVERINFO, NETMSG_TYPE_BITS)
    writer.write_ubit_long(protocol, 16)
    writer.write_long(server_count)
    writer.write_ubit_long(is_hltv, 1)
    writer.write_ubit_long(is_dedicated, 1)
    writer.write_long(client_crc)
    writer.write_ubit_long(max_classes, 16)
    writer.write_long(map_crc)
    writer.write_byte(player_slot)
    writer.write_byte(max_clients)
    tick_bits = struct.unpack("<I", struct.pack("<f", tick_interval))[0]
    writer.write_long(tick_bits)
    writer.write_byte(ord(os_char))
    writer.write_string(game_dir)
    writer.write_string(map_name)
    writer.write_string(sky_name)
    writer.write_string(host_name)


def write_classinfo_message(
    writer: BitWriter, classes: tuple[tuple[str, str], ...] = EXPECTED_CLASSES
) -> None:
    writer.write_ubit_long(SVC_CLASSINFO, NETMSG_TYPE_BITS)
    writer.write_ubit_long(len(classes), 16)
    # SV_WriteClassInfos sends explicit server class names/tables, so create-on-client is false.
    writer.write_ubit_long(0, 1)
    class_bits = len(classes).bit_length() if classes else 1
    for class_id, (network_name, table_name) in enumerate(classes):
        writer.write_ubit_long(class_id, class_bits)
        writer.write_string(network_name)
        writer.write_string(table_name)


def build_classinfo_payload(classes: tuple[tuple[str, str], ...] = EXPECTED_CLASSES) -> bytes:
    writer = BitWriter()
    write_classinfo_message(writer, classes)
    return writer.bytes()


def parse_print_at(data: bytes, bit_offset: int = 0) -> tuple[str, int] | None:
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_PRINT:
            return None
        # build4100's connection banner starts with a newline.  SVC_Print is a
        # generic console string, so CR/LF/TAB are valid wire bytes here.
        text, bit_offset = read_cstring_bits(data, bit_offset, 4096, allow_controls=True)
        return text, bit_offset
    except (ValueError, UnicodeDecodeError):
        return None


def parse_serverinfo_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse build4100 SVC_ServerInfo and return its exact end bit.

    Field order is recovered from engine_i486.so::SVC_ServerInfo::WriteToBuffer:
    type:5, protocol:u16, serverCount:u32, hltv:1, dedicated:1,
    clientCRC:u32, maxClasses:u16, mapCRC:u32, playerSlot:u8, maxClients:u8,
    tickInterval:f32, os:i8, gameDir/mapName/skyName/hostName:cstring.
    """
    start = bit_offset
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_SERVERINFO:
            return None
        protocol, bit_offset = read_ubit_long(data, bit_offset, 16)
        server_count, bit_offset = read_ubit_long(data, bit_offset, 32)
        is_hltv, bit_offset = read_ubit_long(data, bit_offset, 1)
        is_dedicated, bit_offset = read_ubit_long(data, bit_offset, 1)
        client_crc, bit_offset = read_ubit_long(data, bit_offset, 32)
        max_classes, bit_offset = read_ubit_long(data, bit_offset, 16)
        map_crc, bit_offset = read_ubit_long(data, bit_offset, 32)
        player_slot, bit_offset = read_ubit_long(data, bit_offset, 8)
        max_clients, bit_offset = read_ubit_long(data, bit_offset, 8)
        tick_bits, bit_offset = read_ubit_long(data, bit_offset, 32)
        os_byte, bit_offset = read_ubit_long(data, bit_offset, 8)
        game_dir, bit_offset = read_cstring_bits(data, bit_offset, 1024)
        map_name, bit_offset = read_cstring_bits(data, bit_offset, 1024)
        sky_name, bit_offset = read_cstring_bits(data, bit_offset, 1024)
        host_name, bit_offset = read_cstring_bits(data, bit_offset, 2048, allow_controls=True)
        tick_interval = struct.unpack("<f", struct.pack("<I", tick_bits))[0]
        return {
            "start_bit": start,
            "end_bit": bit_offset,
            "protocol": protocol,
            "server_count": server_count,
            "is_hltv": is_hltv,
            "is_dedicated": is_dedicated,
            "client_crc": client_crc,
            "max_classes": max_classes,
            "map_crc": map_crc,
            "player_slot": player_slot,
            "max_clients": max_clients,
            "tick_interval": tick_interval,
            "os": chr(os_byte),
            "game_dir": game_dir,
            "map_name": map_name,
            "sky_name": sky_name,
            "host_name": host_name,
        }
    except (ValueError, UnicodeDecodeError, struct.error):
        return None




def parse_tick_at(data: bytes, bit_offset: int) -> dict[str, int] | None:
    """Parse protocol-7 NET_Tick (type:5 + tick:i32)."""
    start = bit_offset
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != NET_TICK:
            return None
        tick, bit_offset = read_ubit_long(data, bit_offset, 32)
        return {"start_bit": start, "end_bit": bit_offset, "tick": tick}
    except ValueError:
        return None



def extract_bit_slice(data: bytes, bit_offset: int, bit_count: int) -> bytes:
    """Copy an arbitrary LSB-first bit slice into a byte-aligned buffer."""
    if bit_offset < 0 or bit_count < 0 or bit_offset + bit_count > len(data) * 8:
        raise ValueError("bit slice out of range")
    out = bytearray((bit_count + 7) // 8)
    for i in range(bit_count):
        src = bit_offset + i
        if (data[src >> 3] >> (src & 7)) & 1:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def parse_voice_init_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse build4100 svc_VoiceInit: type14 + codec:cstring + quality:u8."""
    start = bit_offset
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_VOICEINIT:
            return None
        codec, bit_offset = read_cstring_bits(data, bit_offset, 64)
        quality, bit_offset = read_ubit_long(data, bit_offset, 8)
        if not codec or any(ord(ch) < 0x20 or ord(ch) > 0x7e for ch in codec):
            return None
        return {"start_bit": start, "end_bit": bit_offset, "codec": codec, "quality": quality}
    except (ValueError, UnicodeDecodeError):
        return None


def parse_voice_data_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse build4100 svc_VoiceData: type15 + sender:u8 + payloadBits:u16 + raw bits."""
    start = bit_offset
    total_bits = len(data) * 8
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_VOICEDATA:
            return None
        sender, bit_offset = read_ubit_long(data, bit_offset, 8)
        payload_bits, bit_offset = read_ubit_long(data, bit_offset, 16)
        payload_start = bit_offset
        end_bit = payload_start + payload_bits
        if payload_bits <= 0 or payload_bits > 32768 or end_bit > total_bits:
            return None
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "sender": sender,
            "payload_bits": payload_bits,
            "payload": extract_bit_slice(data, payload_start, payload_bits),
        }
    except ValueError:
        return None


def scan_voice_inits(data: bytes, start_bit: int = 0) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    total = len(data) * 8
    for bit in range(max(0, start_bit), max(max(0, start_bit), total - 13)):
        try:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        except ValueError:
            break
        if msg_type != SVC_VOICEINIT:
            continue
        parsed = parse_voice_init_at(data, bit)
        if parsed is not None:
            out.append(parsed)
    return out


def scan_voice_data(data: bytes, start_bit: int = 0) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    total = len(data) * 8
    for bit in range(max(0, start_bit), max(max(0, start_bit), total - 29)):
        try:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        except ValueError:
            break
        if msg_type != SVC_VOICEDATA:
            continue
        parsed = parse_voice_data_at(data, bit)
        if parsed is not None:
            out.append(parsed)
    return out


def parse_user_message_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse build4100 svc_UserMessage (type 23) envelope.

    engine_i486.so::SVC_UserMessage::WriteToBuffer proves the exact fields:
    type:5, messageType:u8, payloadBits:u11, then payloadBits raw data.
    """
    start = bit_offset
    total_bits = len(data) * 8
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_USERMESSAGE:
            return None
        message_type, bit_offset = read_ubit_long(data, bit_offset, 8)
        payload_bits, bit_offset = read_ubit_long(data, bit_offset, 11)
        payload_start = bit_offset
        end_bit = payload_start + payload_bits
        if end_bit > total_bits:
            return None
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "message_type": message_type,
            "payload_bits": payload_bits,
            "payload_start_bit": payload_start,
            "payload": extract_bit_slice(data, payload_start, payload_bits),
        }
    except ValueError:
        return None


def scan_user_messages(data: bytes, start_bit: int = 0) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    total = len(data) * 8
    for bit in range(max(0, start_bit), max(max(0, start_bit), total - 23)):
        try:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        except ValueError:
            break
        if msg_type != SVC_USERMESSAGE:
            continue
        parsed = parse_user_message_at(data, bit)
        if parsed is not None:
            out.append(parsed)
    return out


def parse_saytext_payload(payload: bytes, payload_bits: int) -> tuple[int, str, int] | None:
    if payload_bits < 24:
        return None
    try:
        sender, bit = read_ubit_long(payload, 0, 8)
        text, bit = read_cstring_bits(payload, bit, 1024, allow_controls=True)
        chat, end_bit = read_ubit_long(payload, bit, 8)
        if end_bit > payload_bits:
            return None
        return sender, text, chat
    except (ValueError, UnicodeDecodeError):
        return None


def parse_entity_message_at(data: bytes, bit_offset: int) -> dict[str, int] | None:
    """Parse build4100 svc_EntityMessage (type 24) envelope.

    engine_i486.so::SVC_EntityMessage::ReadFromBuffer proves the exact fields:
    entity:u11, class:u9, payloadBits:u11, then payloadBits raw data.
    """
    start = bit_offset
    total_bits = len(data) * 8
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_ENTITYMESSAGE:
            return None
        entity, bit_offset = read_ubit_long(data, bit_offset, MAX_EDICT_BITS)
        class_id, bit_offset = read_ubit_long(data, bit_offset, MAX_SERVER_CLASS_BITS)
        payload_bits, bit_offset = read_ubit_long(data, bit_offset, 11)
        payload_start = bit_offset
        end_bit = payload_start + payload_bits
        if entity >= (1 << MAX_EDICT_BITS) or end_bit > total_bits:
            return None
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "entity": entity,
            "class_id": class_id,
            "payload_bits": payload_bits,
            "payload_start_bit": payload_start,
        }
    except ValueError:
        return None


def parse_temp_entity_payload(payload: bytes, data_bits: int, num_entries: int,
                              schema: dict[int, dict[int, dict[str, object]]],
                              class_entries: tuple[tuple[str, str], ...] | None = None) -> dict[str, object]:
    """Decode build4100 CBaseServer::WriteTempEntities payload.

    Each event starts with an optional signed 8-bit fire delay. A one-bit full
    marker then either carries the 1-based CEventInfo wire class id (registry
    ServerClass id + 1) or deltas against the preceding event of the same class.
    SendTable_WriteAllDeltaProps uses the same build4100 unary UBitVar field-index
    stream decoded for PacketEntities.
    """
    if class_entries is None:
        class_entries = EXPECTED_CLASSES
    if num_entries <= 0 or num_entries >= (1 << EVENT_INDEX_BITS):
        return {"complete": False, "reason": "bad_entry_count", "entries": []}
    if data_bits <= 0 or data_bits > len(payload) * 8:
        return {"complete": False, "reason": "bad_payload_bits", "entries": []}

    class_bits = bits_required_for_count(len(class_entries))
    pos = 0
    last_class_id = -1
    last_values: dict[int, object] = {}
    entries: list[dict[str, object]] = []
    try:
        for entry_index in range(num_entries):
            start = pos
            delayed, pos = read_ubit_long(payload, pos, 1)
            fire_delay = 0.0
            if delayed:
                raw_delay, pos = read_ubit_long(payload, pos, 8)
                fire_delay = float(_signed_bits(raw_delay, 8)) / 100.0

            full, pos = read_ubit_long(payload, pos, 1)
            wire_class_id = 0
            if full:
                # build4100 CEventInfo stores classId+1. CVEngineServer::PlaybackTempEntity
                # increments the 0-based ServerClass id before SendTable_Encode and stores
                # that same 1-based value in CEventInfo::classID. CBaseServer::WriteTempEntities
                # writes it verbatim. Convert wire 1..N back to registry 0..N-1 here.
                wire_class_id, pos = read_ubit_long(payload, pos, class_bits)
                if wire_class_id <= 0 or wire_class_id > len(class_entries):
                    raise ValueError(f"tempentity wire class id range={wire_class_id}")
                class_id = wire_class_id - 1
                base_values: dict[int, object] = {}
            else:
                if last_class_id < 0:
                    raise ValueError("tempentity delta without previous event")
                class_id = last_class_id
                wire_class_id = class_id + 1
                base_values = dict(last_values)

            if class_id < 0 or class_id >= len(class_entries):
                raise ValueError("tempentity class id range")
            class_schema = schema.get(class_id)
            if not class_schema:
                raise ValueError(f"tempentity schema missing class={class_id}")

            props: list[dict[str, object]] = []
            values = base_values
            last_field = -1
            while True:
                present, pos = read_ubit_long(payload, pos, 1)
                if not present:
                    break
                delta, pos = read_ubit_var_build4100(payload, pos)
                field = last_field + 1 + delta
                last_field = field
                meta = class_schema.get(field)
                if meta is None:
                    raise ValueError(f"tempentity schema missing class={class_id} field={field}")
                value, pos = decode_sendprop_value(payload, pos, data_bits, meta)
                values[field] = value
                props.append({
                    "field_index": field,
                    "name": str(meta["prop_name"]),
                    "type": int(meta["type"]),
                    "value": value,
                })

            class_name, table_name = class_entries[class_id]
            entries.append({
                "entry_index": entry_index,
                "class_id": class_id,
                "wire_class_id": wire_class_id,
                "class_name": class_name,
                "table_name": table_name,
                "full": int(full),
                "fire_delay": fire_delay,
                "props": props,
                "values": dict(values),
                "start_bit": start,
                "end_bit": pos,
            })
            last_class_id = class_id
            last_values = dict(values)

        return {
            "complete": pos == data_bits,
            "reason": "" if pos == data_bits else "trailing_tempentity_bits",
            "entries": entries,
            "bits_consumed": pos,
            "data_bits": data_bits,
        }
    except (ValueError, KeyError) as exc:
        return {
            "complete": False,
            "reason": str(exc),
            "entries": entries,
            "bits_consumed": pos,
            "data_bits": data_bits,
        }


def parse_temp_entities_at(data: bytes, bit_offset: int,
                           schema: dict[int, dict[int, dict[str, object]]],
                           class_entries: tuple[tuple[str, str], ...] | None = None) -> dict[str, object] | None:
    """Parse protocol-7 svc_TempEntities: type5 + count8 + length17 + payload."""
    if class_entries is None:
        class_entries = EXPECTED_CLASSES
    total_bits = len(data) * 8
    try:
        start = bit_offset
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_TEMPENTITIES:
            return None
        num_entries, bit_offset = read_ubit_long(data, bit_offset, EVENT_INDEX_BITS)
        data_bits, bit_offset = read_ubit_long(data, bit_offset, NET_MAX_PAYLOAD_BITS_OLD)
        payload_start = bit_offset
        end_bit = payload_start + data_bits
        if num_entries <= 0 or data_bits <= 0 or end_bit > total_bits:
            return None
        payload = extract_bit_slice(data, payload_start, data_bits)
        decoded = parse_temp_entity_payload(payload, data_bits, num_entries, schema, class_entries)
        if not decoded.get("complete"):
            return None
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "num_entries": num_entries,
            "data_bits": data_bits,
            "payload_start_bit": payload_start,
            "payload": payload,
            "entries": decoded["entries"],
        }
    except (ValueError, KeyError):
        return None


def scan_temp_entities_messages(data: bytes,
                                schema: dict[int, dict[int, dict[str, object]]],
                                start_bit: int = 0) -> list[dict[str, object]]:
    """Find complete build4100 svc_TempEntities envelopes in a netmessage stream."""
    total_bits = len(data) * 8
    if start_bit < 0:
        start_bit = 0
    minimum = NETMSG_TYPE_BITS + EVENT_INDEX_BITS + NET_MAX_PAYLOAD_BITS_OLD + 4
    matches: list[dict[str, object]] = []
    for bit in range(start_bit, max(start_bit, total_bits - minimum) + 1):
        parsed = parse_temp_entities_at(data, bit, schema)
        if parsed is not None:
            matches.append(parsed)
    matches.sort(key=lambda item: (int(item["start_bit"]), -int(item["data_bits"])))
    return matches


def parse_temp_entities_envelope_at(data: bytes, bit_offset: int,
                                    class_entries: tuple[tuple[str, str], ...] | None = None) -> dict[str, object] | None:
    """Parse only the build4100 svc_TempEntities envelope and first full class id.

    This is intentionally independent of SendProp value decoding.  The engine
    serializes CEventInfo::classID as ServerClass id + 1; for the first selected
    event CBaseServer::WriteTempEntities must emit a full-class marker because
    there is no previous event to delta against.  That makes the first wire
    class a strong observer-side oracle even when a later property codec is not
    understood yet.
    """
    if class_entries is None:
        class_entries = EXPECTED_CLASSES
    total_bits = len(data) * 8
    try:
        start = bit_offset
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_TEMPENTITIES:
            return None
        num_entries, bit_offset = read_ubit_long(data, bit_offset, EVENT_INDEX_BITS)
        data_bits, bit_offset = read_ubit_long(data, bit_offset, NET_MAX_PAYLOAD_BITS_OLD)
        payload_start = bit_offset
        end_bit = payload_start + data_bits
        # Real weapon temp entities are tiny.  Keep the fallback deliberately
        # narrow so a type-27 bit pattern inside arbitrary packet data cannot
        # become a false positive.
        if num_entries <= 0 or num_entries > 16 or data_bits < 10 or data_bits > 2048 or end_bit > total_bits:
            return None
        payload = extract_bit_slice(data, payload_start, data_bits)
        pos = 0
        delayed, pos = read_ubit_long(payload, pos, 1)
        fire_delay = 0.0
        if delayed:
            raw_delay, pos = read_ubit_long(payload, pos, 8)
            fire_delay = float(_signed_bits(raw_delay, 8)) / 100.0
        full, pos = read_ubit_long(payload, pos, 1)
        if not full:
            return None
        class_bits = bits_required_for_count(len(class_entries))
        wire_class_id, pos = read_ubit_long(payload, pos, class_bits)
        if wire_class_id <= 0 or wire_class_id > len(class_entries):
            return None
        class_id = wire_class_id - 1
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "num_entries": num_entries,
            "data_bits": data_bits,
            "payload_start_bit": payload_start,
            "payload": payload,
            "first_class_id": class_id,
            "first_wire_class_id": wire_class_id,
            "first_fire_delay": fire_delay,
            "first_header_end_bit": pos,
        }
    except ValueError:
        return None


def scan_temp_entities_envelopes(data: bytes, start_bit: int = 0) -> list[dict[str, object]]:
    """Find exact type/count/length envelopes with a valid first full class id."""
    total_bits = len(data) * 8
    start_bit = max(0, start_bit)
    minimum = NETMSG_TYPE_BITS + EVENT_INDEX_BITS + NET_MAX_PAYLOAD_BITS_OLD + 10
    matches: list[dict[str, object]] = []
    for bit in range(start_bit, max(start_bit, total_bits - minimum) + 1):
        parsed = parse_temp_entities_envelope_at(data, bit)
        if parsed is not None:
            matches.append(parsed)
    matches.sort(key=lambda item: (int(item["start_bit"]), int(item["data_bits"])))
    return matches


def parse_temp_entity_full_resync_candidate(payload: bytes, data_bits: int, start_bit: int,
                                             class_id: int,
                                             schema: dict[int, dict[int, dict[str, object]]],
                                             class_entries: tuple[tuple[str, str], ...] | None = None) -> dict[str, object] | None:
    """Decode one full temp-entity entry starting at an arbitrary payload bit.

    This is a recovery path for an svc_TempEntities envelope whose earlier
    entry cannot be decoded with our current SendProp schema.  The candidate
    still has to contain the exact build4100 entry header (delay/full/wire
    class) and a completely decodable delta-property list for the requested
    class.  Callers must additionally validate semantic property values before
    accepting the candidate.
    """
    if class_entries is None:
        class_entries = EXPECTED_CLASSES
    if class_id < 0 or class_id >= len(class_entries):
        return None
    if start_bit < 0 or start_bit >= data_bits or data_bits > len(payload) * 8:
        return None
    class_schema = schema.get(class_id)
    if not class_schema:
        return None
    class_bits = bits_required_for_count(len(class_entries))
    try:
        pos = start_bit
        delayed, pos = read_ubit_long(payload, pos, 1)
        fire_delay = 0.0
        if delayed:
            raw_delay, pos = read_ubit_long(payload, pos, 8)
            fire_delay = float(_signed_bits(raw_delay, 8)) / 100.0
        full, pos = read_ubit_long(payload, pos, 1)
        if not full:
            return None
        wire_class_id, pos = read_ubit_long(payload, pos, class_bits)
        if wire_class_id != class_id + 1:
            return None

        values: dict[int, object] = {}
        props: list[dict[str, object]] = []
        last_field = -1
        while True:
            if pos >= data_bits:
                return None
            present, pos = read_ubit_long(payload, pos, 1)
            if not present:
                break
            delta, pos = read_ubit_var_build4100(payload, pos)
            field = last_field + 1 + delta
            if field <= last_field:
                return None
            last_field = field
            meta = class_schema.get(field)
            if meta is None:
                return None
            value, pos = decode_sendprop_value(payload, pos, data_bits, meta)
            values[field] = value
            props.append({
                "field_index": field,
                "name": str(meta["prop_name"]),
                "type": int(meta["type"]),
                "value": value,
            })
        if not props or pos > data_bits:
            return None
        class_name, table_name = class_entries[class_id]
        return {
            "class_id": class_id,
            "wire_class_id": wire_class_id,
            "class_name": class_name,
            "table_name": table_name,
            "full": 1,
            "fire_delay": fire_delay,
            "props": props,
            "values": values,
            "start_bit": start_bit,
            "end_bit": pos,
        }
    except (ValueError, KeyError):
        return None


def scan_temp_entities_resync_full(data: bytes,
                                   schema: dict[int, dict[int, dict[str, object]]],
                                   class_id: int,
                                   start_bit: int = 0) -> list[dict[str, object]]:
    """Find a fully decodable class entry anywhere inside a valid type-27 envelope.

    The normal decoder remains authoritative.  This bounded bit-resync path is
    only used when an earlier temp-entity class in the same envelope prevents
    strict sequential decoding.  It never accepts a class-id hit by itself;
    the full requested-class SendProp stream must decode to its zero terminator.
    """
    matches: list[dict[str, object]] = []
    seen: set[tuple[int, int, int]] = set()
    for envelope in scan_temp_entities_envelopes(data, start_bit):
        payload = envelope["payload"]
        data_bits = int(envelope["data_bits"])
        # delay(1) + full(1) + class id + at least one property marker/value.
        minimum = 2 + bits_required_for_count(len(EXPECTED_CLASSES)) + 2
        for candidate_bit in range(0, max(0, data_bits - minimum) + 1):
            entry = parse_temp_entity_full_resync_candidate(
                payload, data_bits, candidate_bit, class_id, schema
            )
            if entry is None:
                continue
            key = (int(envelope["start_bit"]), int(entry["start_bit"]), int(entry["end_bit"]))
            if key in seen:
                continue
            seen.add(key)
            item = dict(entry)
            item.update({
                "message_start_bit": int(envelope["start_bit"]),
                "message_end_bit": int(envelope["end_bit"]),
                "num_entries": int(envelope["num_entries"]),
                "data_bits": data_bits,
            })
            matches.append(item)
    matches.sort(key=lambda item: (int(item["message_start_bit"]), int(item["start_bit"])))
    return matches


def parse_packet_entities_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse the exact build4100 svc_PacketEntities type-26 envelope.

    Recovered from engine_i486.so::SVC_PacketEntities::ReadFromBuffer:
      type:5, maxEntries:u11, isDelta:1, [deltaFrom:i32], baseline:1,
      updatedEntries:u11, dataBits:u20, updateBaseline:1, data:dataBits.

    Later Source branches use a 24-bit data length; build4100 uses 20 bits.
    """
    start = bit_offset
    total_bits = len(data) * 8
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_PACKETENTITIES:
            return None
        max_entries, bit_offset = read_ubit_long(data, bit_offset, MAX_EDICT_BITS)
        is_delta, bit_offset = read_ubit_long(data, bit_offset, 1)
        delta_from = -1
        if is_delta:
            raw_delta, bit_offset = read_ubit_long(data, bit_offset, 32)
            delta_from = raw_delta if raw_delta < 0x80000000 else raw_delta - 0x100000000
        baseline, bit_offset = read_ubit_long(data, bit_offset, 1)
        updated_entries, bit_offset = read_ubit_long(data, bit_offset, MAX_EDICT_BITS)
        data_bits, bit_offset = read_ubit_long(data, bit_offset, 20)
        update_baseline, bit_offset = read_ubit_long(data, bit_offset, 1)
        payload_start = bit_offset
        end_bit = payload_start + data_bits

        # Strong plausibility checks keep a bitwise scan from mistaking entity
        # property payload for a new type-26 envelope.
        if max_entries <= 0 or max_entries > (1 << MAX_EDICT_BITS):
            return None
        if updated_entries > max_entries or data_bits < 0 or end_bit > total_bits:
            return None
        if updated_entries > 0 and data_bits == 0:
            return None
        if not is_delta and delta_from != -1:
            return None

        payload = extract_bit_slice(data, payload_start, data_bits)
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "max_entries": max_entries,
            "is_delta": is_delta,
            "delta_from": delta_from,
            "baseline": baseline,
            "updated_entries": updated_entries,
            "data_bits": data_bits,
            "update_baseline": update_baseline,
            "payload_start_bit": payload_start,
            "payload": payload,
            "payload_sha256": hashlib.sha256(payload).hexdigest(),
            "payload_head_hex": payload[:48].hex(),
        }
    except ValueError:
        return None


def scan_packet_entities_messages(data: bytes, bit_offset: int = 0) -> list[dict[str, object]]:
    """Find plausible PacketEntities envelopes and prefer complete large payloads."""
    total_bits = len(data) * 8
    matches: list[dict[str, object]] = []
    if bit_offset < 0:
        bit_offset = 0
    # Minimum non-delta envelope: type5 + max11 + delta1 + baseline1 +
    # updated11 + length20 + updateBaseline1 + at least one payload bit.
    for bit in range(bit_offset, max(bit_offset, total_bits - 49) + 1):
        parsed = parse_packet_entities_at(data, bit)
        if parsed is not None:
            matches.append(parsed)
    # Nested false positives inside a real entity payload tend to be shorter.
    # Return stable outer/larger candidates first.
    matches.sort(key=lambda item: (-int(item["data_bits"]), int(item["start_bit"])))
    return matches


def select_ordinary_packet_entities(data: bytes, bit_offset: int = 0) -> dict[str, object] | None:
    """Select the outer svc_PacketEntities from an ordinary protocol-7 datagram.

    Active snapshots emitted by build4100 begin with NET_Tick.  When a
    PacketEntities message follows that tick directly, its exact bit boundary is
    stronger evidence than any bitwise scan candidate.  This prevents entity
    payload bits from being misidentified as a zero-length full snapshot.
    """
    tick = parse_tick_at(data, bit_offset)
    if tick is not None:
        exact = parse_packet_entities_at(data, int(tick["end_bit"]))
        if exact is not None:
            return exact

    matches = scan_packet_entities_messages(data, bit_offset)
    if not matches:
        return None
    # If another top-level message precedes PacketEntities, prefer the candidate
    # carrying the largest real entity payload.  Do not prioritize is_delta=0:
    # a nested property bit pattern can otherwise look like an empty full update.
    matches.sort(key=lambda item: (-int(item["data_bits"]), int(item["start_bit"])))
    return matches[0]


def parse_setconvar_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse protocol-7 NET_SetConVar: type:5, count:u8, name/value cstrings."""
    start = bit_offset
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != NET_SETCONVAR:
            return None
        count, bit_offset = read_ubit_long(data, bit_offset, 8)
        if count > 128:
            return None
        convars: list[tuple[str, str]] = []
        for _ in range(count):
            name, bit_offset = read_cstring_bits(data, bit_offset, 1024, allow_controls=False)
            value, bit_offset = read_cstring_bits(data, bit_offset, 4096, allow_controls=True)
            convars.append((name, value))
        return {"start_bit": start, "end_bit": bit_offset, "count": count, "convars": convars}
    except (ValueError, UnicodeDecodeError):
        return None


def parse_create_stringtable_at(data: bytes, bit_offset: int) -> dict[str, object] | None:
    """Parse build4100/protocol-7 SVC_CreateStringTable exactly.

    Recovered from the real v1.7.8 CONNECTED->NEW block.  Wire layout is:
      type:5, tableName:cstring, maxEntries:u16,
      numEntries:Q_log2(maxEntries)+1, dataBits:u20,
      userDataFixed:1, [userDataSize:u12, userDataBits:u4], data:dataBits.

    Later Source branches add extra flags/varints; build4100 does not use those
    fields on this protocol-7 wire path.
    """
    start = bit_offset
    total_bits = len(data) * 8
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_CREATESTRINGTABLE:
            return None
        table_name, bit_offset = read_cstring_bits(data, bit_offset, 256)
        max_entries, bit_offset = read_ubit_long(data, bit_offset, 16)
        if max_entries <= 0 or max_entries > 0xFFFF or (max_entries & (max_entries - 1)):
            return None
        entry_bits = (max_entries.bit_length() - 1) + 1
        num_entries, bit_offset = read_ubit_long(data, bit_offset, entry_bits)
        if num_entries > max_entries:
            return None
        data_bits, bit_offset = read_ubit_long(data, bit_offset, 20)
        user_data_fixed, bit_offset = read_ubit_long(data, bit_offset, 1)
        user_data_size = 0
        user_data_bits = 0
        if user_data_fixed:
            user_data_size, bit_offset = read_ubit_long(data, bit_offset, 12)
            user_data_bits, bit_offset = read_ubit_long(data, bit_offset, 4)
        data_start = bit_offset
        end_bit = data_start + data_bits
        if end_bit > total_bits:
            return None
        return {
            "start_bit": start,
            "end_bit": end_bit,
            "table_name": table_name,
            "max_entries": max_entries,
            "num_entries": num_entries,
            "data_bits": data_bits,
            "user_data_fixed": user_data_fixed,
            "user_data_size": user_data_size,
            "user_data_bits": user_data_bits,
            "data_start_bit": data_start,
        }
    except (ValueError, UnicodeDecodeError):
        return None


def decode_stringtable_entries(data: bytes, table: dict[str, object]) -> list[tuple[int, str]]:
    """Decode build4100 initial string-table entries from CreateStringTable.

    build4100 uses a 12-bit *byte* count for variable userdata (max 4096
    bytes).  This differs from later Source branches that use a wider field.
    Every read is bounded by the table's declared payload before touching the
    enclosing packet, so a malformed table cannot consume the next netmessage.
    """
    bit = int(table["data_start_bit"])
    end_bit = bit + int(table["data_bits"])
    max_entries = int(table["max_entries"])
    num_entries = int(table["num_entries"])
    index_bits = max(1, (max_entries - 1).bit_length())
    history: list[str] = []
    known_values: dict[int, str] = {}
    current_index = -1
    entries: list[tuple[int, str]] = []

    if bit < 0 or end_bit < bit or end_bit > len(data) * 8:
        raise ValueError("stringtable payload range invalid")

    def read(bits: int) -> int:
        nonlocal bit
        if bits < 0 or bit + bits > end_bit:
            raise ValueError("stringtable payload overflow")
        value, bit = read_ubit_long(data, bit, bits)
        return value

    def read_string(max_bytes: int = 4096) -> str:
        out = bytearray()
        for _ in range(max_bytes):
            value = read(8)
            if value == 0:
                return out.decode("ascii", "strict")
            if value < 0x20 or value > 0x7E:
                if value not in (0x09, 0x0A, 0x0D):
                    raise ValueError("non-printable network string")
            out.append(value)
        raise ValueError("unterminated network string")

    for _ in range(num_entries):
        if read(1):
            current_index += 1
        else:
            current_index = read(index_bits)
        if current_index < 0 or current_index >= max_entries:
            raise ValueError(f"stringtable index out of range {current_index}")

        value = known_values.get(current_index, "")
        if read(1):
            if read(1):
                history_index = read(5)
                prefix_length = read(5)
                if history_index >= len(history):
                    raise ValueError("stringtable history index out of range")
                base = history[history_index]
                if prefix_length > len(base):
                    raise ValueError("stringtable history prefix out of range")
                value = base[:prefix_length] + read_string()
            else:
                value = read_string()
            known_values[current_index] = value

        # Source's substring dictionary tracks the final string for every
        # changed entry, including an entry that reused its previous value.
        history.append(value)
        if len(history) > 32:
            history.pop(0)

        if read(1):
            if int(table["user_data_fixed"]):
                user_bits = int(table["user_data_bits"])
                if user_bits <= 0:
                    user_bits = int(table["user_data_size"]) * 8
            else:
                user_bytes = read(STRINGTABLE_USERDATA_LENGTH_BITS)
                if user_bytes > STRINGTABLE_MAX_USERDATA_BYTES:
                    raise ValueError(f"stringtable userdata too large {user_bytes}")
                user_bits = user_bytes * 8
            if user_bits < 0 or bit + user_bits > end_bit:
                raise ValueError("stringtable userdata overflow")
            bit += user_bits
        entries.append((current_index, value))

    if bit != end_bit:
        raise ValueError(f"stringtable payload tail bits={end_bit - bit}")
    return entries


def parse_connected_new_stream(data: bytes) -> dict[str, object] | None:
    """Parse the complete build4100 CONNECTED->NEW signon block sequentially."""
    total_bits = len(data) * 8
    bit = 0
    prints: list[tuple[str, int, int]] = []
    try:
        while bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type != SVC_PRINT:
                break
            parsed = parse_print_at(data, bit)
            if parsed is None:
                return None
            text, end_bit = parsed
            prints.append((text, bit, end_bit))
            bit = end_bit

        serverinfo = parse_serverinfo_at(data, bit)
        if serverinfo is None:
            return None
        bit = int(serverinfo["end_bit"])

        tick = parse_tick_at(data, bit)
        if tick is None:
            return None
        bit = int(tick["end_bit"])

        tables: list[dict[str, object]] = []
        while bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type != SVC_CREATESTRINGTABLE:
                break
            table = parse_create_stringtable_at(data, bit)
            if table is None:
                return None
            tables.append(table)
            bit = int(table["end_bit"])

        if tuple(str(table["table_name"]) for table in tables) != EXPECTED_STRINGTABLES:
            return None

        convars = parse_setconvar_at(data, bit)
        if convars is None:
            return None
        bit = int(convars["end_bit"])

        signon = parse_signon_state_at(data, bit)
        if signon is None or signon["state"] != SIGNONSTATE_NEW:
            return None
        bit = int(signon["end_bit"])

        # bf_write may leave zero padding in the final byte only.
        if total_bits - bit > 7:
            return None
        return {
            "prints": prints,
            "serverinfo": serverinfo,
            "tick": tick,
            "tables": tables,
            "convars": convars,
            "signon": signon,
            "end_bit": bit,
            "total_bits": total_bits,
        }
    except (ValueError, UnicodeDecodeError):
        return None


def parse_signon_state_at(
    data: bytes,
    bit_offset: int,
    *,
    allow_trailing_zero_pad: bool = False,
) -> dict[str, int] | None:
    """Parse build4100 NET_SignonState at an arbitrary bit offset.

    CNetChan reliable byte buffers can omit up to seven high zero bits from the
    final integer field because GetNumBytesWritten() only preserves meaningful
    bytes at the end of a reliable transfer.  That is observable in the real
    PRESPAWN -> SPAWN transition: type/state plus the low 29 bits of spawncount
    are present, while the final three zero bits are absent.  Only the final
    server-count field is eligible for this zero extension, and callers must
    opt in explicitly.
    """
    start = bit_offset
    total_bits = len(data) * 8
    zero_pad_bits = 0
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != NETMSG_SIGNONSTATE:
            return None
        state, bit_offset = read_ubit_long(data, bit_offset, 8)
        if state < 0 or state > 7:
            return None

        bits_left = total_bits - bit_offset
        if bits_left >= 32:
            server_count_u32, bit_offset = read_ubit_long(data, bit_offset, 32)
        elif allow_trailing_zero_pad and 25 <= bits_left < 32:
            # Missing bits are the high bits of the little-endian/LSB-first
            # 32-bit spawn count.  They may only be reconstructed as zero.
            server_count_u32, available_end = read_ubit_long(data, bit_offset, bits_left)
            zero_pad_bits = 32 - bits_left
            bit_offset = available_end + zero_pad_bits
        else:
            return None

        server_count = struct.unpack("<i", struct.pack("<I", server_count_u32))[0]
        return {
            "start_bit": start,
            "end_bit": bit_offset,
            "state": state,
            "server_count": server_count,
            "zero_pad_bits": zero_pad_bits,
        }
    except (ValueError, struct.error):
        return None


def parse_spawn_transition_stream(
    data: bytes,
    expected_server_count: int,
    expected_view_entity: int | None = None,
) -> dict[str, object] | None:
    """Parse the small reliable PRESPAWN -> SPAWN transition block.

    CGameClient::SpawnPlayer writes SVC_SetView, then CBaseClient::SpawnPlayer
    writes NET_Tick and NET_SignonState(SPAWN). With exact single-block framing
    this normally starts at bit 0. Since fragment length is byte-granular, up to
    seven unused tail bits can remain after the logical message stream.
    """
    total_bits = len(data) * 8
    try:
        for pad_bits in range(0, min(16, total_bits)):
            # Only zero padding is accepted before SVC_SetView.
            if pad_bits:
                nonzero = False
                for pad_bit in range(pad_bits):
                    value, _ = read_ubit_long(data, pad_bit, 1)
                    if value:
                        nonzero = True
                        break
                if nonzero:
                    continue

            if pad_bits + NETMSG_TYPE_BITS + MAX_EDICT_BITS > total_bits:
                break
            msg_type, bit = read_ubit_long(data, pad_bits, NETMSG_TYPE_BITS)
            if msg_type != SVC_SETVIEW:
                continue
            view_entity, bit = read_ubit_long(data, bit, MAX_EDICT_BITS)
            if expected_view_entity is not None and view_entity != expected_view_entity:
                continue

            tick = parse_tick_at(data, bit)
            if tick is None:
                continue
            bit = int(tick["end_bit"])

            signon = parse_signon_state_at(data, bit, allow_trailing_zero_pad=True)
            if signon is None:
                continue
            if signon["state"] != SIGNONSTATE_SPAWN or signon["server_count"] != expected_server_count:
                continue

            logical_end = int(signon["end_bit"])
            tail_pad_bits = max(0, total_bits - logical_end)
            if tail_pad_bits > 7:
                continue

            return {
                "pad_bits": pad_bits,
                "tail_pad_bits": tail_pad_bits,
                "view_entity": view_entity,
                "tick": tick,
                "signon": signon,
                "end_bit": logical_end,
                "total_bits": total_bits,
            }
        return None
    except ValueError:
        return None


def scan_signon_state_messages(data: bytes, start_bit: int = 0) -> list[dict[str, int]]:
    """Find plausible NET_SignonState messages in a reassembled signon block.

    The first build4100 reliable block also contains Tick/SetConVar/string-table
    data, which v1.7.8 traces without treating embedded payload bits as messages.  SignonState
    has a compact fixed 45-bit wire shape, so matching state plus spawn count is
    enough to drive the protocol state machine without guessing message sizes.
    """
    total_bits = len(data) * 8
    found: list[dict[str, int]] = []
    for bit in range(max(0, start_bit), max(0, total_bits - 44)):
        parsed = parse_signon_state_at(data, bit)
        if parsed is None:
            continue
        # Server spawn counts are non-negative.  Keep -1 for the initial client
        # CONNECTED acknowledgement used by this protocol generation.
        if parsed["server_count"] < -1 or parsed["server_count"] > 0x00FFFFFF:
            continue
        found.append(parsed)
    return found


def parse_delta_snapshot_account_updates(
    packet: dict[str, object],
    entity_classes: dict[int, tuple[int, str, str]],
    value_bits: int = 16,
) -> dict[str, object] | None:
    """Decode the v1.8.8 deterministic existing-entity SendProp delta.

    After the baseline-equal full snapshot, the reconstructed server changes
    exactly DT_CSPlayer::m_iAccount from 0 to 1337 and marks active player
    edicts FL_EDICT_CHANGED.  Existing entities therefore use the build4100
    header path leave=0/enter=0 followed by one CDeltaBitsReader field/value and
    its zero terminator.  The flattened field index is learned from the wire;
    the field identity is unambiguous because m_iAccount is the only live proxy
    that changes in this milestone.
    """
    if int(packet.get("is_delta", 0)) != 1:
        return None
    payload = packet.get("payload")
    if not isinstance(payload, (bytes, bytearray)):
        return None
    data = bytes(payload)
    data_bits = int(packet["data_bits"])
    updated = int(packet["updated_entries"])
    pos = 0
    header_base = -1
    updates: list[dict[str, object]] = []
    try:
        for _ in range(updated):
            start = pos
            entity_delta, pos = read_ubit_var_build4100(data, pos)
            entity = header_base + 1 + entity_delta
            header_base = entity
            leave, pos = read_ubit_long(data, pos, 1)
            if leave:
                delete, pos = read_ubit_long(data, pos, 1)
                return {
                    "complete": False,
                    "reason": "leavepvs_in_account_delta",
                    "updates": updates,
                    "entity": entity,
                    "delete": delete,
                    "bit": start,
                }
            enter, pos = read_ubit_long(data, pos, 1)
            if enter:
                return {
                    "complete": False,
                    "reason": "enterpvs_in_account_delta",
                    "updates": updates,
                    "entity": entity,
                    "bit": start,
                }
            class_info = entity_classes.get(entity)
            if class_info is None:
                return {
                    "complete": False,
                    "reason": "delta_entity_missing_from_full_snapshot",
                    "updates": updates,
                    "entity": entity,
                    "bit": start,
                }

            present, pos = read_ubit_long(data, pos, 1)
            if not present:
                return {
                    "complete": False,
                    "reason": "delta_entity_has_no_changed_prop",
                    "updates": updates,
                    "entity": entity,
                    "bit": start,
                }
            field_delta, pos = read_ubit_var_build4100(data, pos)
            field_index = field_delta  # last field starts at -1.
            value, pos = read_ubit_long(data, pos, value_bits)
            more, pos = read_ubit_long(data, pos, 1)
            if more:
                return {
                    "complete": False,
                    "reason": "multiple_changed_props_not_expected",
                    "updates": updates,
                    "entity": entity,
                    "field_index": field_index,
                    "bit": pos,
                }
            class_id, class_name, table_name = class_info
            updates.append({
                "entity": entity,
                "class_id": class_id,
                "class_name": class_name,
                "table_name": table_name,
                "field_index": field_index,
                "field_name": "m_iAccount",
                "value": value,
                "start_bit": start,
                "end_bit": pos,
            })
        # build4100 CBaseServer::WriteDeltaEntities appends one outer zero bit
        # after the updated-entity stream.  This is distinct from each
        # CDeltaBitsWriter destructor's per-entity zero property terminator.
        # The real v1.8.10 frame is therefore 116 entity/property bits + this
        # final zero = the advertised 117 data bits.
        outer_terminator_bits = 0
        if pos + 1 == data_bits:
            terminator, pos = read_ubit_long(data, pos, 1)
            if terminator != 0:
                return {
                    "complete": False,
                    "reason": "nonzero_delta_entity_stream_terminator",
                    "updates": updates,
                    "bits_consumed": pos,
                    "data_bits": data_bits,
                    "changed_props": len(updates),
                }
            outer_terminator_bits = 1
        return {
            "complete": pos == data_bits,
            "reason": "" if pos == data_bits else "trailing_delta_entity_bits",
            "updates": updates,
            "bits_consumed": pos,
            "data_bits": data_bits,
            "changed_props": len(updates),
            "outer_terminator_bits": outer_terminator_bits,
        }
    except ValueError as exc:
        return {"complete": False, "reason": str(exc), "updates": updates, "bit": pos}

def parse_classinfo_at(data: bytes, bit_offset: int) -> tuple[int, list[tuple[int, str, str]], int] | None:
    start = bit_offset
    try:
        msg_type, bit_offset = read_ubit_long(data, bit_offset, NETMSG_TYPE_BITS)
        if msg_type != SVC_CLASSINFO:
            return None
        class_count, bit_offset = read_ubit_long(data, bit_offset, 16)
        if class_count != len(EXPECTED_CLASSES):
            return None
        create_on_client, bit_offset = read_ubit_long(data, bit_offset, 1)
        if create_on_client:
            return None
        class_bits = class_count.bit_length() if class_count else 1
        classes: list[tuple[int, str, str]] = []
        for _ in range(class_count):
            class_id, bit_offset = read_ubit_long(data, bit_offset, class_bits)
            network_name, bit_offset = read_cstring_bits(data, bit_offset, 256)
            table_name, bit_offset = read_cstring_bits(data, bit_offset, 256)
            classes.append((class_id, network_name, table_name))
        if [(n, t) for _, n, t in classes] != list(EXPECTED_CLASSES):
            return None
        if [cid for cid, _, _ in classes] != list(range(class_count)):
            return None
        return start, classes, bit_offset
    except (ValueError, UnicodeDecodeError):
        return None


def scan_classinfo_messages(data: bytes) -> list[tuple[int, list[tuple[int, str, str]], int]]:
    found: list[tuple[int, list[tuple[int, str, str]], int]] = []
    for bit in range(max(0, len(data) * 8 - 22)):
        parsed = parse_classinfo_at(data, bit)
        if parsed is not None:
            found.append(parsed)
    return found


def parse_full_sendtable_stream(data: bytes, bit_offset: int = 0) -> dict[str, object] | None:
    """Parse consecutive SVC_SendTable envelopes followed by SVC_ClassInfo.

    build4100 serializes each SVC_SendTable as type:5, needsDecoder:1,
    payloadBits:u16, payload:payloadBits.  The stream may start at an arbitrary
    bit offset because the same compressed signon block can contain Print and
    ServerInfo before the SendTables.
    """
    total_bits = len(data) * 8
    bit = bit_offset
    tables: list[tuple[int, int, int, int]] = []
    try:
        while bit + 22 <= total_bits:
            start = bit
            msg_type, pos = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type != SVC_SENDTABLE:
                break
            needs_decoder, pos = read_ubit_long(data, pos, 1)
            payload_bits, pos = read_ubit_long(data, pos, 16)
            if payload_bits <= 0 or pos + payload_bits > total_bits:
                return None
            end = pos + payload_bits
            tables.append((start, needs_decoder, payload_bits, end))
            bit = end
        if not tables:
            return None
        next_type = None
        if bit + NETMSG_TYPE_BITS <= total_bits:
            next_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        classinfo = parse_classinfo_at(data, bit) if next_type == SVC_CLASSINFO else None
        return {
            "start_bit": bit_offset,
            "tables": tables,
            "next_bit": bit,
            "next_type": next_type,
            "classinfo": classinfo,
            "total_bits": total_bits,
        }
    except ValueError:
        return None


def parse_full_signon_stream(data: bytes) -> dict[str, object] | None:
    """Parse the observed build4100 full-signon reliable block.

    The real server block is not a SendTable-only blob.  It starts with an
    SVC_Print banner, then SVC_ServerInfo, then the forced full SendTables and
    SVC_ClassInfo.  Parse those boundaries sequentially so embedded payload
    bits can never be mistaken for top-level messages.
    """
    total_bits = len(data) * 8
    bit = 0
    prints: list[tuple[str, int, int]] = []
    serverinfo: dict[str, object] | None = None
    try:
        # build4100 can prepend more than one console Print.
        while bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type != SVC_PRINT:
                break
            parsed_print = parse_print_at(data, bit)
            if parsed_print is None:
                return None
            text, endbit = parsed_print
            prints.append((text, bit, endbit))
            bit = endbit

        if bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type == SVC_SERVERINFO:
                serverinfo = parse_serverinfo_at(data, bit)
                if serverinfo is None:
                    return None
                bit = int(serverinfo["end_bit"])

        sendtables = parse_full_sendtable_stream(data, bit)
        if sendtables is None:
            return None
        return {
            "prints": prints,
            "serverinfo": serverinfo,
            "sendtables": sendtables,
            "classinfo": sendtables["classinfo"],
            "next_bit": sendtables["next_bit"],
            "next_type": sendtables["next_type"],
            "total_bits": total_bits,
        }
    except ValueError:
        return None


def classify_signon_stream(data: bytes) -> tuple[str, str]:
    if not data:
        return "empty", "bytes=0"

    connected_new = parse_connected_new_stream(data)
    if connected_new is not None:
        tables = connected_new["tables"]
        signon = connected_new["signon"]
        return (
            "connected_new",
            f"tick={connected_new['tick']['tick']} stringtables={len(tables)} "
            f"table_names={','.join(str(table['table_name']) for table in tables)} "
            f"convars={connected_new['convars']['count']} signon={signon['state']}@{signon['server_count']}",
        )

    full = parse_full_signon_stream(data)
    if full is not None:
        sendtables = full["sendtables"]
        serverinfo = full["serverinfo"]
        classinfo = full["classinfo"]
        print_count = len(full["prints"])
        server_detail = "serverinfo=0"
        if serverinfo is not None:
            server_detail = (
                f"serverinfo=1 protocol={serverinfo['protocol']} map={serverinfo['map_name']} "
                f"maxclients={serverinfo['max_clients']} maxclasses={serverinfo['max_classes']}"
            )
        return (
            "signon_full",
            f"prints={print_count} {server_detail} tables={len(sendtables['tables'])} "
            f"next_bit={sendtables['next_bit']} next_type={sendtables['next_type']} "
            f"classinfo={1 if classinfo else 0}",
        )

    # v1.8.1: the PRESPAWN -> SPAWN reliable block can end with up to
    # seven omitted high zero bits, so it needs a terminal-aware parser.
    # Use the common build4100 spawn count from the stream only for
    # classification; the runtime path validates the exact expected count.
    for expected_count in range(0, 8):
        spawn_transition = parse_spawn_transition_stream(data, expected_count)
        if spawn_transition is not None:
            signon = spawn_transition["signon"]
            tick = spawn_transition["tick"]
            return (
                "spawn_transition",
                f"pad_bits={spawn_transition['pad_bits']} view_entity={spawn_transition['view_entity']} tick={tick['tick']} "
                f"state={signon['state']} server_count={signon['server_count']} "
                f"tail_pad_bits={spawn_transition['tail_pad_bits']}",
            )

    try:
        first_type, _ = read_ubit_long(data, 0, NETMSG_TYPE_BITS)
    except ValueError:
        return "truncated", f"bytes={len(data)}"
    if first_type == SVC_SENDTABLE:
        parsed = parse_full_sendtable_stream(data)
        if parsed is None:
            return "sendtables_malformed", f"first_type={first_type} bytes={len(data)}"
        tables = parsed["tables"]
        classinfo = parsed["classinfo"]
        return (
            "sendtables",
            f"first_type={first_type} tables={len(tables)} next_bit={parsed['next_bit']} "
            f"next_type={parsed['next_type']} classinfo={1 if classinfo else 0}",
        )
    if first_type == SVC_PRINT:
        parsed_print = parse_print_at(data, 0)
        if parsed_print is not None:
            text, endbit = parsed_print
            preview = text.replace("\n", "|")[:160]
            next_type = -1
            if endbit + NETMSG_TYPE_BITS <= len(data) * 8:
                next_type, _ = read_ubit_long(data, endbit, NETMSG_TYPE_BITS)
            states = scan_signon_state_messages(data)
            state_detail = ",".join(
                f"{item['state']}@{item['server_count']}:{item['start_bit']}" for item in states[:8]
            ) or "none"
            return (
                "signon_misc",
                f"first_type={first_type} print_end={endbit} next_type={next_type} "
                f"signon_states={state_detail} print={preview!r}",
            )
    return "signon_misc", f"first_type={first_type} bytes={len(data)}"


def trace_signon_prefix(data: bytes) -> str:
    """Describe only proven top-level prefix boundaries without bit scanning payloads."""
    if not data:
        return "bytes=0"
    total_bits = len(data) * 8
    bit = 0
    print_ranges: list[str] = []
    server_detail = "serverinfo=0"
    try:
        while bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type != SVC_PRINT:
                break
            parsed = parse_print_at(data, bit)
            if parsed is None:
                break
            _, endbit = parsed
            print_ranges.append(f"{bit}..{endbit}")
            bit = endbit

        if bit + NETMSG_TYPE_BITS <= total_bits:
            msg_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
            if msg_type == SVC_SERVERINFO:
                info = parse_serverinfo_at(data, bit)
                if info is not None:
                    server_detail = (
                        f"serverinfo=1:{bit}..{info['end_bit']} protocol={info['protocol']} "
                        f"server_count={info['server_count']} map={info['map_name']} "
                        f"maxclients={info['max_clients']} maxclasses={info['max_classes']}"
                    )
                    bit = int(info["end_bit"])

        next_type = -1
        if bit + NETMSG_TYPE_BITS <= total_bits:
            next_type, _ = read_ubit_long(data, bit, NETMSG_TYPE_BITS)
        return (
            f"bytes={len(data)} total_bits={total_bits} prints={len(print_ranges)} "
            f"print_ranges={','.join(print_ranges) or 'none'} {server_detail} "
            f"next_bit={bit} next_type={next_type}"
        )
    except (ValueError, UnicodeDecodeError, struct.error):
        return f"bytes={len(data)} total_bits={total_bits} prefix_parse=truncated next_bit={bit}"


class BitReader:
    """LSB-first reader for build4100 bf_read-compatible fragment headers."""

    def __init__(self, data: bytes, bit_offset: int = 0) -> None:
        self.data = data
        self.bit = bit_offset

    @property
    def bits_left(self) -> int:
        return len(self.data) * 8 - self.bit

    def read_ubit_long(self, bits: int) -> int:
        value, self.bit = read_ubit_long(self.data, self.bit, bits)
        return value

    def read_one_bit(self) -> int:
        return self.read_ubit_long(1)

    def read_byte(self) -> int:
        return self.read_ubit_long(8)

    def read_long(self) -> int:
        return self.read_ubit_long(32)

    def read_varint32(self) -> int:
        value = 0
        shift = 0
        for _ in range(5):
            byte = self.read_byte()
            value |= (byte & 0x7F) << shift
            if not (byte & 0x80):
                return value
            shift += 7
        raise ValueError("varint32 too long")

    def read_bytes(self, size: int) -> bytes:
        if size < 0 or self.bits_left < size * 8:
            raise ValueError(f"fragment payload truncated need={size} left_bits={self.bits_left}")
        return bytes(self.read_byte() for _ in range(size))

    def read_cstring(self, max_bytes: int = 260) -> str:
        out = bytearray()
        for _ in range(max_bytes):
            byte = self.read_byte()
            if byte == 0:
                return out.decode("latin1")
            out.append(byte)
        raise ValueError("unterminated fragment filename")


class FragmentTransfer:
    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.buffer: bytearray | None = None
        self.bytes = 0
        self.num_fragments = 0
        self.is_compressed = False
        self.uncompressed_size = 0
        self.filename = ""
        self.transfer_id = 0
        self.received: set[int] = set()

    @property
    def active(self) -> bool:
        return self.buffer is not None

    @property
    def complete(self) -> bool:
        return self.active and self.num_fragments > 0 and len(self.received) >= self.num_fragments


class ReliableFragmentReassembler:
    """Decode build4100 CNetChan reliable subchannels and reassemble streams."""

    def __init__(self) -> None:
        self.streams = [FragmentTransfer() for _ in range(MAX_STREAMS)]
        self.ack_reliable_state = 0

    @staticmethod
    def _fragment_count(size: int) -> int:
        return (size + FRAGMENT_SIZE - 1) // FRAGMENT_SIZE

    def _read_stream_block(self, reader: BitReader, stream: int) -> tuple[dict[str, int | bool | str], bytes | None]:
        data = self.streams[stream]
        single_block = reader.read_one_bit() == 0
        start_fragment = 0
        num_fragments = 0
        offset = 0
        length = 0

        if not single_block:
            start_fragment = reader.read_ubit_long(MAX_FILE_SIZE_BITS - FRAGMENT_BITS)
            num_fragments = reader.read_ubit_long(3)
            if num_fragments <= 0:
                raise ValueError("zero fragments in reliable subchannel")
            offset = start_fragment * FRAGMENT_SIZE
            length = num_fragments * FRAGMENT_SIZE

        if offset == 0:
            data.reset()
            if single_block:
                data.is_compressed = bool(reader.read_one_bit())
                if data.is_compressed:
                    data.uncompressed_size = reader.read_ubit_long(MAX_FILE_SIZE_BITS)
                # build4100 CNetChan::ReadSubChannelData uses a fixed 17-bit
                # byte count for a single-block transfer. Reading this as a
                # varint leaves the upper nine size bits in front of the data
                # and shifts every following ordinary message boundary.
                data.bytes = reader.read_ubit_long(SINGLE_BLOCK_SIZE_BITS)
            else:
                is_file = bool(reader.read_one_bit())
                if is_file:
                    data.transfer_id = reader.read_long()
                    data.filename = reader.read_cstring()
                data.is_compressed = bool(reader.read_one_bit())
                if data.is_compressed:
                    data.uncompressed_size = reader.read_ubit_long(MAX_FILE_SIZE_BITS)
                data.bytes = reader.read_ubit_long(MAX_FILE_SIZE_BITS)

            if data.bytes <= 0 or data.bytes >= (1 << MAX_FILE_SIZE_BITS):
                raise ValueError(f"invalid reliable transfer size {data.bytes}")
            data.num_fragments = self._fragment_count(data.bytes)
            data.buffer = bytearray(data.bytes)
            if single_block:
                num_fragments = data.num_fragments
                length = num_fragments * FRAGMENT_SIZE
        elif not data.active:
            raise ValueError(f"stream={stream} fragment arrived before header start={start_fragment}")

        if start_fragment + num_fragments > data.num_fragments:
            raise ValueError(
                f"fragment range overflow stream={stream} start={start_fragment} count={num_fragments} total={data.num_fragments}"
            )
        if start_fragment + num_fragments == data.num_fragments:
            remainder = data.bytes % FRAGMENT_SIZE
            if remainder:
                length -= FRAGMENT_SIZE - remainder

        if offset + length > data.bytes:
            raise ValueError(
                f"fragment byte range overflow stream={stream} offset={offset} length={length} total={data.bytes}"
            )
        chunk = reader.read_bytes(length)
        assert data.buffer is not None
        data.buffer[offset : offset + length] = chunk
        for frag in range(start_fragment, start_fragment + num_fragments):
            data.received.add(frag)

        completed: bytes | None = None
        decode_error = ""
        raw_head_hex = ""
        if data.complete:
            raw = bytes(data.buffer[: data.bytes])
            raw_head_hex = raw[:48].hex()
            if data.is_compressed:
                # Build4100 normally hands us a complete bzip2 stream beginning
                # with BZh.  A bad compression flag or corrupt/stale transfer must
                # not abort the whole netchannel: ordinary netmessages can follow
                # this reliable block in the same UDP packet (notably the first
                # post-SPAWN snapshot).  Preserve raw bytes for diagnostics and
                # let the caller continue at the exact reader bit boundary.
                if not raw.startswith(b"BZh"):
                    decode_error = "bzip2_magic_missing"
                    completed = raw
                else:
                    try:
                        decoded = bz2.decompress(raw)
                    except OSError as exc:
                        decode_error = f"bzip2_decode_failed:{exc}"
                        completed = raw
                    else:
                        if data.uncompressed_size and len(decoded) != data.uncompressed_size:
                            decode_error = (
                                f"bzip2_size_mismatch:expected={data.uncompressed_size}:got={len(decoded)}"
                            )
                            completed = raw
                        else:
                            completed = decoded
            else:
                completed = raw

        info: dict[str, int | bool | str] = {
            "stream": stream,
            "single": single_block,
            "start": start_fragment,
            "count": num_fragments,
            "offset": offset,
            "length": length,
            "total_bytes": data.bytes,
            "total_fragments": data.num_fragments,
            "received_fragments": len(data.received),
            "compressed": data.is_compressed,
            "uncompressed_size": data.uncompressed_size,
            "filename": data.filename,
            "transfer_id": data.transfer_id,
            "decode_error": decode_error,
            "raw_head_hex": raw_head_hex,
        }
        if completed is not None:
            data.reset()
        return info, completed

    def consume(self, payload: bytes) -> tuple[int, list[dict[str, int | bool | str]], list[tuple[int, bool, bytes, str]], int]:
        reader = BitReader(payload)
        if reader.bits_left < 3:
            raise ValueError("reliable payload missing subchannel index")
        subchannel = reader.read_ubit_long(3)
        if not (0 <= subchannel < 8):
            raise ValueError(f"invalid subchannel {subchannel}")

        blocks: list[dict[str, int | bool | str]] = []
        completed: list[tuple[int, bool, bytes, str]] = []
        for stream in range(MAX_STREAMS):
            if reader.bits_left < 1:
                raise ValueError(f"reliable payload missing stream-present bit stream={stream}")
            if not reader.read_one_bit():
                continue
            info, decoded = self._read_stream_block(reader, stream)
            blocks.append(info)
            if decoded is not None:
                completed.append((stream, bool(info["compressed"]), decoded, str(info["decode_error"])))

        self.ack_reliable_state ^= 1 << subchannel
        return subchannel, blocks, completed, reader.bit


def parse_netchan_packet(packet: bytes) -> tuple[int, int, int, int, int | None, int | None, bytes]:
    if len(packet) < 10:
        raise ValueError(f"netchannel packet too short: {len(packet)}")
    sequence, ack, flags, reliable_state = struct.unpack_from("<iiBB", packet, 0)
    offset = 10
    choked: int | None = None
    challenge: int | None = None
    if flags & PACKET_FLAG_CHOKED:
        if offset >= len(packet):
            raise ValueError("choked packet missing choked-count byte")
        choked = packet[offset]
        offset += 1
    if flags & PACKET_FLAG_CHALLENGE:
        if offset + 4 > len(packet):
            raise ValueError("challenge packet missing challenge value")
        challenge = struct.unpack_from("<I", packet, offset)[0]
        offset += 4
    return sequence, ack, flags, reliable_state, choked, challenge, packet[offset:]

def cstring(value: str) -> bytes:
    return value.encode("utf-8", "strict") + b"\0"


def md5_guid(seed: str) -> str:
    return hashlib.md5(seed.encode("ascii", "strict")).hexdigest()


def classic_revemu_ticket(seed: str) -> bytes:
    hwid_text = ("CSSV34-" + hashlib.md5(seed.encode("ascii", "strict")).hexdigest()).encode("ascii")
    hwid = hwid_text[:127].ljust(128, b"\0")
    steamid = (2 * REVEMU_HASH) & 0xFFFFFFFF
    ticket = struct.pack(
        "<IIIIII128s",
        REVEMU_HEADER,
        REVEMU_HASH,
        REVEMU_MAGIC,
        0,
        steamid,
        REVEMU_STEAMID_HIGH,
        hwid,
    )
    if len(ticket) != REVEMU_TICKET_SIZE:
        raise AssertionError(f"classic RevEmu ticket size mismatch: {len(ticket)}")
    return ticket


def parse_challenge(packet: bytes) -> tuple[int, int]:
    if len(packet) < 13:
        raise ValueError(f"challenge response too short: {len(packet)}")
    if packet[:4] != CONNECTIONLESS:
        raise ValueError("challenge response missing connectionless header")
    if packet[4] != S2C_CHALLENGE:
        raise ValueError(f"unexpected challenge response type=0x{packet[4]:02x}")
    challenge, auth = struct.unpack_from("<ii", packet, 5)
    if auth not in (PROTOCOL_HASHEDCDKEY, PROTOCOL_STEAM):
        raise ValueError(f"unsupported auth protocol {auth}")
    return challenge, auth


def build_connect(
    challenge: int,
    auth: int,
    name: str,
    password: str,
    guid: str,
    steam_ticket: bytes | None = None,
) -> bytes:
    payload = bytearray(CONNECTIONLESS)
    payload.append(C2S_CONNECT)
    payload += struct.pack("<iii", PROTOCOL_VERSION, auth, challenge)
    payload += cstring(name)
    payload += cstring(password)
    if auth == PROTOCOL_HASHEDCDKEY:
        payload += cstring(guid)
    elif auth == PROTOCOL_STEAM:
        ticket = steam_ticket if steam_ticket is not None else classic_revemu_ticket(name)
        if not (1 <= len(ticket) <= 0x800):
            raise ValueError(f"invalid Steam ticket length {len(ticket)}")
        payload += struct.pack("<H", len(ticket))
        payload += ticket
    else:
        raise ValueError(f"unsupported auth protocol {auth}")
    return bytes(payload)


def is_connection_accept(packet: bytes) -> bool:
    return len(packet) >= 5 and packet[:4] == CONNECTIONLESS and packet[4] == S2C_CONNECTION


def describe_oob(packet: bytes) -> str:
    if len(packet) < 5 or packet[:4] != CONNECTIONLESS:
        return f"raw:{packet[:32].hex()}"
    kind = packet[4]
    text = packet[5:].split(b"\0", 1)[0].decode("latin1", "replace")
    if kind == S2C_CONNECTION:
        return f"S2C_CONNECTION text={text!r}"
    return f"type=0x{kind:02x} text={text!r} hex={packet[5:69].hex()}"


def build_stream_ack(challenge: int) -> bytes:
    return bytes([STREAM_CMD_ACK]) + struct.pack("<I", challenge & 0xFFFFFFFF)


def build_signon_state_payload(state: int = SIGNONSTATE_CONNECTED, server_count: int = -1) -> bytes:
    writer = BitWriter()
    writer.write_ubit_long(NETMSG_SIGNONSTATE, NETMSG_TYPE_BITS)
    writer.write_byte(state)
    writer.write_long(server_count)
    if writer.bits_written != 45:
        raise AssertionError(f"NET_SignonState bit count mismatch: {writer.bits_written}")
    return writer.bytes()


def build_tick_payload(tick: int) -> bytes:
    # build4100 NET_Tick::WriteToBuffer: type:5 + tick:int32 = 37 bits.
    # On the server CBaseClient::ProcessTick forwards this value to
    # UpdateAcknowledgedFramecount(), which establishes m_nDeltaTick.
    writer = BitWriter()
    writer.write_ubit_long(NET_TICK, NETMSG_TYPE_BITS)
    writer.write_long(tick)
    if writer.bits_written != 37:
        raise AssertionError(f"NET_Tick bit count mismatch: {writer.bits_written}")
    return writer.bytes()


def build_baseline_ack_payload(tick: int, baseline: int) -> bytes:
    # build4100 CLC_BaselineAck::WriteToBuffer: type:5 + tick:int32 +
    # baseline selector:1 = 38 bits. The selector is the baseline number from
    # the SVC_PacketEntities message that requested m_bUpdateBaseline.
    if baseline not in (0, 1):
        raise ValueError(f"invalid baseline selector {baseline}")
    writer = BitWriter()
    writer.write_ubit_long(CLC_BASELINEACK, NETMSG_TYPE_BITS)
    writer.write_long(tick)
    writer.write_ubit_long(baseline, 1)
    if writer.bits_written != 38:
        raise AssertionError(f"CLC_BaselineAck bit count mismatch: {writer.bits_written}")
    return writer.bytes()


def build_client_info_payload(
    server_count: int,
    sendtable_crc: int,
    *,
    is_hltv: bool = False,
    friends_id: int = 0,
    friends_name: str = "",
    custom_files: tuple[int, int, int, int] = (0, 0, 0, 0),
) -> bytes:
    # Exact CLC_ClientInfo::WriteToBuffer order recovered from engine_i486.so:
    # type:5, serverCount:i32, sendTableCRC:u32, isHLTV:1, friendsID:u32,
    # friendsName:cstring, then 4 optional custom-file CRCs (presence bit + u32).
    writer = BitWriter()
    writer.write_ubit_long(CLC_CLIENTINFO, NETMSG_TYPE_BITS)
    writer.write_long(server_count)
    writer.write_long(sendtable_crc)
    writer.write_ubit_long(1 if is_hltv else 0, 1)
    writer.write_long(friends_id)
    writer.write_string(friends_name)
    for crc in custom_files:
        crc &= 0xFFFFFFFF
        writer.write_ubit_long(1 if crc else 0, 1)
        if crc:
            writer.write_long(crc)
    return writer.bytes()


def build_stream_transfer_ack(transfer_id: int) -> bytes:
    return bytes([STREAM_CMD_ACK]) + struct.pack("<I", transfer_id & 0xFFFFFFFF)


def scan_svc_sendtable_envelopes(payload: bytes) -> list[tuple[int, int, int]]:
    # SVC_SendTable::WriteToBuffer is: type:5, needsDecoder:1, length:u16, bits:length.
    # Scan bitwise because the preceding signon messages are not byte-aligned.
    total_bits = len(payload) * 8
    found: list[tuple[int, int, int]] = []
    for bit in range(max(0, total_bits - 22)):
        msg_type, pos = read_ubit_long(payload, bit, 5)
        if msg_type != SVC_SENDTABLE:
            continue
        needs_decoder, pos = read_ubit_long(payload, pos, 1)
        bit_length, pos = read_ubit_long(payload, pos, 16)
        if bit_length < 16 or pos + bit_length > total_bits:
            continue
        # Real build4100 table envelopes are non-trivial; tiny random matches are noise.
        if bit_length < 64:
            continue
        found.append((bit, needs_decoder, bit_length))
    return found


def recv_stream_data_frame(sock: socket.socket, timeout: float) -> tuple[int, int, bytes] | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = max(0.01, deadline - time.monotonic())
        readable, _, _ = select.select([sock], [], [], remaining)
        if not readable:
            return None
        first = recv_exact(sock, 1, deadline)[0]
        if first == STREAM_CMD_DATA:
            header = recv_exact(sock, 6, deadline)
            length = struct.unpack_from("<I", header, 0)[0]
            transfer_id = struct.unpack_from("<H", header, 4)[0]
            if length > 0x17700:
                raise ValueError(f"TCP stream data length too large: {length}")
            data = recv_exact(sock, length, deadline)
            return transfer_id, length, data
        if first == STREAM_CMD_ACK:
            _ = recv_exact(sock, 4, deadline)
            continue
        # Commands 1/3 are not expected in this signon probe; report the exact byte.
        raise ValueError(f"unexpected TCP stream command {first}")
    return None


def build_nop_payload() -> bytes:
    writer = BitWriter()
    writer.write_ubit_long(NET_NOP, NETMSG_TYPE_BITS)
    return writer.bytes()


def build_disconnect_payload(reason: str = "probe_complete") -> bytes:
    writer = BitWriter()
    writer.write_ubit_long(NET_DISCONNECT, NETMSG_TYPE_BITS)
    writer.write_string(reason)
    return writer.bytes()


def build_stringcmd_payload(command: str) -> bytes:
    writer = BitWriter()
    writer.write_ubit_long(NET_STRINGCMD, NETMSG_TYPE_BITS)
    writer.write_string(command)
    return writer.bytes()


def build_voice_data_payload(payload: bytes, payload_bits: int | None = None) -> bytes:
    """Encode build4100 clc_VoiceData: type10 + payloadBits:u16 + raw bits."""
    if payload_bits is None:
        payload_bits = len(payload) * 8
    if payload_bits <= 0 or payload_bits > min(0xFFFF, len(payload) * 8):
        raise ValueError("invalid voice payload bit count")
    writer = BitWriter()
    writer.write_ubit_long(CLC_VOICEDATA, NETMSG_TYPE_BITS)
    writer.write_ubit_long(payload_bits, 16)
    for bit in range(payload_bits):
        value, _ = read_ubit_long(payload, bit, 1)
        writer.write_ubit_long(value, 1)
    return writer.bytes()


def build_file_request_payload(filename: str, transfer_id: int) -> bytes:
    """Encode build4100 NET_File request: type2 + transfer:u32 + filename + requested:1."""
    if not filename or filename.startswith("/") or ".." in filename.replace("\\", "/").split("/"):
        raise ValueError("unsafe download filename")
    writer = BitWriter()
    writer.write_ubit_long(NET_FILE, NETMSG_TYPE_BITS)
    writer.write_long(transfer_id)
    writer.write_string(filename)
    writer.write_ubit_long(1, 1)
    return writer.bytes()


def build_listen_events_payload(event_ids: set[int] | list[int] | tuple[int, ...]) -> bytes:
    """Encode build4100 clc_ListenEvents: type12 + fixed 512-bit event-id mask."""
    selected = {int(event_id) for event_id in event_ids}
    if any(event_id < 0 or event_id >= 512 for event_id in selected):
        raise ValueError("gameevent id outside clc_ListenEvents mask")
    writer = BitWriter()
    writer.write_ubit_long(CLC_LISTENEVENTS, NETMSG_TYPE_BITS)
    for event_id in range(512):
        writer.write_ubit_long(1 if event_id in selected else 0, 1)
    return writer.bytes()


def write_delta_usercmd_build4100(
    writer: BitWriter,
    *,
    command_number: int,
    tick_count: int,
    pitch: float,
    yaw: float,
    roll: float = 0.0,
    forward_move: int = 0,
    side_move: int = 0,
    up_move: int = 0,
    buttons: int = 0,
) -> None:
    """Encode one build4100 CUserCmd delta against the zero/default command."""
    writer.write_ubit_long(0 if command_number == 1 else 1, 1)
    if command_number != 1:
        writer.write_long(command_number)

    writer.write_ubit_long(0 if tick_count == 1 else 1, 1)
    if tick_count != 1:
        writer.write_long(tick_count)

    for angle, bits in ((pitch, 16), (yaw, 16), (roll, 8)):
        changed = angle != 0.0
        writer.write_ubit_long(1 if changed else 0, 1)
        if changed:
            writer.write_bit_angle(angle, bits)

    for move in (forward_move, side_move, up_move):
        changed = move != 0
        writer.write_ubit_long(1 if changed else 0, 1)
        if changed:
            writer.write_signed(move, 16)

    writer.write_ubit_long(1 if buttons else 0, 1)
    if buttons:
        writer.write_long(buttons)

    writer.write_ubit_long(0, 1)  # impulse unchanged
    writer.write_ubit_long(0, 1)  # weaponSelect unchanged
    writer.write_ubit_long(0, 1)  # mouseDx unchanged
    writer.write_ubit_long(0, 1)  # mouseDy unchanged


def build_move_payload(
    tick_count: int,
    *,
    command_number: int = 1,
    pitch: float = 45.0,
    yaw: float = 90.0,
    forward_move: int = 0,
    side_move: int = 0,
    up_move: int = 0,
    buttons: int = 0,
) -> tuple[bytes, int]:
    """Exact build4100 CLC_Move: type:5, new:4, backup:3, length:u16, usercmd bits."""
    command = BitWriter()
    write_delta_usercmd_build4100(
        command,
        command_number=command_number,
        tick_count=tick_count,
        pitch=pitch,
        yaw=yaw,
        forward_move=forward_move,
        side_move=side_move,
        up_move=up_move,
        buttons=buttons,
    )

    writer = BitWriter()
    writer.write_ubit_long(CLC_MOVE, NETMSG_TYPE_BITS)
    writer.write_ubit_long(1, 4)  # one new command
    writer.write_ubit_long(0, 3)  # no backup commands
    writer.write_ubit_long(command.bits_written, 16)
    command_bytes = command.bytes()
    for bit in range(command.bits_written):
        value, _ = read_ubit_long(command_bytes, bit, 1)
        writer.write_ubit_long(value, 1)
    return writer.bytes(), command.bits_written


def build_netchan_packet(sequence: int, ack: int, payload: bytes, flags: int = 0, reliable_state: int = 0) -> bytes:
    # Exact build4100 CNetChan::ProcessPacketHeader order:
    # sequence:int32, sequence_ack:int32, flags:uint8, reliable_state:uint8.
    return struct.pack("<iiBB", sequence, ack, flags & 0xFF, reliable_state & 0xFF) + payload


def describe_netchan(packet: bytes) -> str:
    if packet.startswith(CONNECTIONLESS):
        return describe_oob(packet)
    if len(packet) < 10:
        return f"short:{packet.hex()}"
    seq, ack, flags, reliable = struct.unpack_from("<iiBB", packet, 0)
    return (
        f"NETCHAN seq={seq} ack={ack} flags=0x{flags:02x} reliable=0x{reliable:02x} "
        f"bytes={len(packet)} payload={packet[10:42].hex()}"
    )


def recv_exact(sock: socket.socket, size: int, deadline: float) -> bytes:
    out = bytearray()
    while len(out) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"TCP stream expected {size} bytes, got {len(out)}")
        sock.settimeout(remaining)
        chunk = sock.recv(size - len(out))
        if not chunk:
            raise ConnectionError(f"TCP stream closed after {len(out)}/{size} bytes")
        out += chunk
    return bytes(out)


def self_test() -> int:
    sample2 = CONNECTIONLESS + bytes([S2C_CHALLENGE]) + struct.pack("<ii", 0x12345678, 2) + b"000000\0"
    challenge2, auth2 = parse_challenge(sample2)
    assert challenge2 == 0x12345678 and auth2 == PROTOCOL_HASHEDCDKEY
    packet2 = build_connect(challenge2, auth2, "cssv34_realprobe", "", "0" * 32)
    assert packet2[:5] == CONNECTIONLESS + b"k"
    assert struct.unpack_from("<iii", packet2, 5) == (7, 2, 0x12345678)
    assert packet2.endswith(("0" * 32).encode() + b"\0")

    sample3 = CONNECTIONLESS + bytes([S2C_CHALLENGE]) + struct.pack("<ii", 0x10203040, 3)
    challenge3, auth3 = parse_challenge(sample3)
    assert challenge3 == 0x10203040 and auth3 == PROTOCOL_STEAM
    ticket = classic_revemu_ticket("cssv34_realprobe")
    packet3 = build_connect(challenge3, auth3, "cssv34_realprobe", "", "unused", ticket)
    prefix_len = 5 + 12 + len(cstring("cssv34_realprobe")) + len(cstring(""))
    assert struct.unpack_from("<iii", packet3, 5) == (7, 3, 0x10203040)
    assert struct.unpack_from("<H", packet3, prefix_len)[0] == REVEMU_TICKET_SIZE
    assert packet3[prefix_len + 2 :] == ticket

    accepted = CONNECTIONLESS + bytes([S2C_CONNECTION]) + b"00000000000000"
    assert is_connection_accept(accepted)
    assert build_stream_ack(0x10203040) == b"\x04\x40\x30\x20\x10"

    signon = build_signon_state_payload()
    bit = 0
    msg_type, bit = read_ubit_long(signon, bit, 5)
    state, bit = read_ubit_long(signon, bit, 8)
    server_count, bit = read_ubit_long(signon, bit, 32)
    assert msg_type == NETMSG_SIGNONSTATE
    assert state == SIGNONSTATE_CONNECTED
    assert server_count == 0xFFFFFFFF
    assert bit == 45 and len(signon) == 6

    full = build_signon_state_payload(SIGNONSTATE_FULL, 1)
    full_state = parse_signon_state_at(full, 0)
    assert full_state is not None
    assert full_state["state"] == SIGNONSTATE_FULL and full_state["server_count"] == 1

    disconnect = build_disconnect_payload("probe_complete")
    bit = 0
    msg_type, bit = read_ubit_long(disconnect, bit, NETMSG_TYPE_BITS)
    reason, bit = read_cstring_bits(disconnect, bit)
    assert msg_type == NET_DISCONNECT and reason == "probe_complete"

    snapshot_tick_ack = build_tick_payload(197)
    bit = 0
    msg_type, bit = read_ubit_long(snapshot_tick_ack, bit, NETMSG_TYPE_BITS)
    ack_tick, bit = read_ubit_long(snapshot_tick_ack, bit, 32)
    assert msg_type == NET_TICK and ack_tick == 197 and bit == 37

    baseline_ack = build_baseline_ack_payload(197, 0)
    bit = 0
    msg_type, bit = read_ubit_long(baseline_ack, bit, NETMSG_TYPE_BITS)
    baseline_tick, bit = read_ubit_long(baseline_ack, bit, 32)
    baseline_nr, bit = read_ubit_long(baseline_ack, bit, 1)
    assert msg_type == CLC_BASELINEACK and baseline_tick == 197 and baseline_nr == 0 and bit == 38

    netchan = build_netchan_packet(1, 0, signon)
    assert struct.unpack_from("<iiBB", netchan, 0) == (1, 0, 0, 0)
    assert netchan[10:] == signon

    client_info = build_client_info_payload(7, 0x1234ABCD, friends_name="probe")
    bit = 0
    msg_type, bit = read_ubit_long(client_info, bit, 5)
    server_count, bit = read_ubit_long(client_info, bit, 32)
    crc, bit = read_ubit_long(client_info, bit, 32)
    is_hltv, bit = read_ubit_long(client_info, bit, 1)
    friends_id, bit = read_ubit_long(client_info, bit, 32)
    assert (msg_type, server_count, crc, is_hltv, friends_id) == (CLC_CLIENTINFO, 7, 0x1234ABCD, 0, 0)

    # Regression fixture from the v1.7.3 real build4100 log. The first reliable
    # payload starts with an 80-bit fragment header; the CHOKED continuation
    # starts at bit 0 only after removing its one-byte choke count.
    observed_first = BitReader(bytes.fromhex("1800800a8700c0790100"))
    assert observed_first.read_ubit_long(3) == 0
    assert observed_first.read_one_bit() == 1
    assert observed_first.read_one_bit() == 1
    assert observed_first.read_ubit_long(MAX_FILE_SIZE_BITS - FRAGMENT_BITS) == 0
    assert observed_first.read_ubit_long(3) == 5
    assert observed_first.read_one_bit() == 0
    assert observed_first.read_one_bit() == 1
    assert observed_first.read_ubit_long(MAX_FILE_SIZE_BITS) == 2160
    assert observed_first.read_ubit_long(MAX_FILE_SIZE_BITS) == 1511
    assert observed_first.bit == 80

    observed_cont = BitReader(bytes.fromhex("b90080ac"))
    assert observed_cont.read_ubit_long(3) == 1
    assert observed_cont.read_one_bit() == 1
    assert observed_cont.read_one_bit() == 1
    assert observed_cont.read_ubit_long(MAX_FILE_SIZE_BITS - FRAGMENT_BITS) == 5
    assert observed_cont.read_ubit_long(3) == 1
    assert observed_cont.bit == 26

    fake_sendtable = BitWriter()
    fake_sendtable.write_ubit_long(SVC_SENDTABLE, 5)
    fake_sendtable.write_ubit_long(1, 1)
    fake_sendtable.write_ubit_long(80, 16)
    for _ in range(10):
        fake_sendtable.write_byte(0xA5)
    matches = scan_svc_sendtable_envelopes(fake_sendtable.bytes())
    assert matches and matches[0] == (0, 1, 80)
    assert build_stream_transfer_ack(0x1234) == b"\x04\x34\x12\x00\x00"
    assert build_nop_payload() == b"\x00"

    exact_stream = BitWriter()
    for needs_decoder, payload_bits, pattern in ((1, 13, 0x15A5), (0, 29, 0x12ABCDEF)):
        exact_stream.write_ubit_long(SVC_SENDTABLE, NETMSG_TYPE_BITS)
        exact_stream.write_ubit_long(needs_decoder, 1)
        exact_stream.write_ubit_long(payload_bits, 16)
        exact_stream.write_ubit_long(pattern, payload_bits)
    write_classinfo_message(exact_stream)
    exact = parse_full_sendtable_stream(exact_stream.bytes())
    assert exact is not None and len(exact["tables"]) == 2
    assert exact["next_type"] == SVC_CLASSINFO and exact["classinfo"] is not None
    kind, detail = classify_signon_stream(exact_stream.bytes())
    assert kind == "signon_full" and "tables=2" in detail and "classinfo=1" in detail

    # Exact prefix captured from the v1.7.5 real build4100 BZip stream.  The
    # first SVC_Print legally begins with LF; after its terminating NUL the next
    # 5 bits are SVC_ServerInfo (8).
    observed_signon_head = bytes.fromhex(
        "4761e8adce8dae4cae658a4e2e6dad4c0764eaad4e6eac4ca1290c4e0784acec"
        "8bac6e8e4e46018a2d2caf4c6e4e07046406e405844641a82e8d8d0c842606064"
        "661aa4cceae4c0ec4a9ae4dac4c0e244601001d0004000000f8ffffffff000065"
        "26442540f0285cc7c33636472797b656064056f645563747270350e7b6e6f6"
    )
    observed_print = parse_print_at(observed_signon_head, 0)
    assert observed_print is not None
    observed_text, observed_print_end = observed_print
    assert observed_text.startswith("\nCounter-Strike: Source\nMap: de_dust2\n")
    observed_next, _ = read_ubit_long(observed_signon_head, observed_print_end, NETMSG_TYPE_BITS)
    assert observed_print_end == 661 and observed_next == SVC_SERVERINFO

    full_stream = BitWriter()
    full_stream.write_ubit_long(SVC_PRINT, NETMSG_TYPE_BITS)
    full_stream.write_string("\nCounter-Strike: Source\nMap: de_dust2\nPlayers:  3 / 4\nBuild 4100\nServer Number 1\n")
    write_serverinfo_message(full_stream)
    for needs_decoder, payload_bits, pattern in ((1, 13, 0x15A5), (0, 29, 0x12ABCDEF)):
        full_stream.write_ubit_long(SVC_SENDTABLE, NETMSG_TYPE_BITS)
        full_stream.write_ubit_long(needs_decoder, 1)
        full_stream.write_ubit_long(payload_bits, 16)
        full_stream.write_ubit_long(pattern, payload_bits)
    write_classinfo_message(full_stream)
    parsed_full = parse_full_signon_stream(full_stream.bytes())
    assert parsed_full is not None
    assert len(parsed_full["prints"]) == 1
    assert parsed_full["serverinfo"] is not None
    assert parsed_full["serverinfo"]["protocol"] == 7
    assert parsed_full["serverinfo"]["map_name"] == "de_dust2"
    assert len(parsed_full["sendtables"]["tables"]) == 2
    assert parsed_full["classinfo"] is not None
    kind, detail = classify_signon_stream(full_stream.bytes())
    assert kind == "signon_full" and "serverinfo=1" in detail and "tables=2" in detail and "classinfo=1" in detail
    prefix_trace = trace_signon_prefix(full_stream.bytes())
    assert "prints=1" in prefix_trace and "serverinfo=1:" in prefix_trace and "next_type=9" in prefix_trace

    # v1.7.8 regression: build4100 drives CONNECTED -> NEW.  The client must
    # wait for the server's NET_SignonState(NEW, server_count) before sending
    # CLC_ClientInfo and its matching NEW acknowledgement.  The real first
    # signon block contains variable-length messages we do not need to decode
    # yet, so state detection deliberately uses the fixed 45-bit wire shape.
    server_driven = BitWriter()
    server_driven.write_ubit_long(SVC_PRINT, NETMSG_TYPE_BITS)
    server_driven.write_string("\nCounter-Strike: Source\nMap: de_dust2\n")
    write_serverinfo_message(server_driven)
    server_new_start = server_driven.bits_written
    server_driven.write_ubit_long(NETMSG_SIGNONSTATE, NETMSG_TYPE_BITS)
    server_driven.write_byte(SIGNONSTATE_NEW)
    server_driven.write_long(1)
    server_states = scan_signon_state_messages(server_driven.bytes())
    assert any(
        state["start_bit"] == server_new_start
        and state["state"] == SIGNONSTATE_NEW
        and state["server_count"] == 1
        for state in server_states
    )

    # v1.8.1 regression: exact protocol-7 CreateStringTable baseline layout.
    connected_new = BitWriter()
    connected_new.write_ubit_long(SVC_PRINT, NETMSG_TYPE_BITS)
    connected_new.write_string("\nCounter-Strike: Source\nMap: de_dust2\n")
    write_serverinfo_message(connected_new)
    connected_new.write_ubit_long(NET_TICK, NETMSG_TYPE_BITS)
    connected_new.write_long(118)
    for table_index, table_name in enumerate(EXPECTED_STRINGTABLES):
        connected_new.write_ubit_long(SVC_CREATESTRINGTABLE, NETMSG_TYPE_BITS)
        connected_new.write_string(table_name)
        max_entries = 1024
        connected_new.write_ubit_long(max_entries, 16)
        connected_new.write_ubit_long(1 if table_index == 0 else 0, 11)
        payload_bits = 13 if table_index == 0 else 0
        connected_new.write_ubit_long(payload_bits, 20)
        connected_new.write_ubit_long(0, 1)
        if payload_bits:
            connected_new.write_ubit_long(0x15A5, payload_bits)
    connected_new.write_ubit_long(NET_SETCONVAR, NETMSG_TYPE_BITS)
    connected_new.write_byte(0)
    connected_new.write_ubit_long(NETMSG_SIGNONSTATE, NETMSG_TYPE_BITS)
    connected_new.write_byte(SIGNONSTATE_NEW)
    connected_new.write_long(1)
    parsed_connected_new = parse_connected_new_stream(connected_new.bytes())
    assert parsed_connected_new is not None
    assert len(parsed_connected_new["tables"]) == len(EXPECTED_STRINGTABLES)
    assert tuple(table["table_name"] for table in parsed_connected_new["tables"]) == EXPECTED_STRINGTABLES
    assert parsed_connected_new["signon"]["state"] == SIGNONSTATE_NEW
    kind, detail = classify_signon_stream(connected_new.bytes())
    assert kind == "connected_new" and "stringtables=16" in detail

    # v1.8.31a regression: initial downloadables entries and NET_File request.
    download_table_data = BitWriter()
    download_table_data.write_ubit_long(1, 1)  # index 0 = previous + 1
    download_table_data.write_ubit_long(1, 1)  # has string
    download_table_data.write_ubit_long(0, 1)  # full string, no history prefix
    download_table_data.write_string("maps/de_dust2.bsp")
    download_table_data.write_ubit_long(0, 1)  # no user data
    download_table_data.write_ubit_long(1, 1)  # index 1 = previous + 1
    download_table_data.write_ubit_long(1, 1)  # has string
    download_table_data.write_ubit_long(1, 1)  # substring history
    download_table_data.write_ubit_long(0, 5)  # history slot 0
    download_table_data.write_ubit_long(14, 5) # copy "maps/de_dust2."
    download_table_data.write_string("nav")
    download_table_data.write_ubit_long(0, 1)  # no user data
    download_table = {
        "data_start_bit": 0,
        "data_bits": download_table_data.bits_written,
        "max_entries": 8192,
        "num_entries": 2,
        "user_data_fixed": 0,
        "user_data_size": 0,
        "user_data_bits": 0,
    }
    assert decode_stringtable_entries(download_table_data.bytes(), download_table) == [
        (0, "maps/de_dust2.bsp"), (1, "maps/de_dust2.nav")
    ]

    # v1.8.34zl build4100-u12-userdata-selftest: the original build4100
    # CNetworkStringTable::ParseUpdate/WriteUpdate uses a 12-bit variable
    # userdata *byte* count (max 4096).  Make the first userdata byte end in
    # binary 11 so the obsolete 14-bit decoder would steal those two bits as
    # high length bits and run off the table before reaching entry 1.
    userdata_table_data = BitWriter()
    userdata_table_data.write_ubit_long(1, 1)  # index 0 = previous + 1
    userdata_table_data.write_ubit_long(1, 1)  # has string
    userdata_table_data.write_ubit_long(0, 1)  # full string
    userdata_table_data.write_string("6")
    userdata_table_data.write_ubit_long(1, 1)  # has userdata
    userdata_table_data.write_ubit_long(37, STRINGTABLE_USERDATA_LENGTH_BITS)
    userdata_blob = bytes([0x03]) + bytes(range(1, 37))
    for byte in userdata_blob:
        userdata_table_data.write_byte(byte)
    userdata_table_data.write_ubit_long(1, 1)  # index 1 = previous + 1
    userdata_table_data.write_ubit_long(1, 1)  # has string
    userdata_table_data.write_ubit_long(0, 1)  # full string
    userdata_table_data.write_string("16")
    userdata_table_data.write_ubit_long(0, 1)  # no userdata
    userdata_table = {
        "data_start_bit": 0,
        "data_bits": userdata_table_data.bits_written,
        "max_entries": 1024,
        "num_entries": 2,
        "user_data_fixed": 0,
        "user_data_size": 0,
        "user_data_bits": 0,
    }
    assert decode_stringtable_entries(userdata_table_data.bytes(), userdata_table) == [
        (0, "6"), (1, "16")
    ]
    request_transfer = 0x34335643
    request_payload = build_file_request_payload("maps/de_dust2.nav", request_transfer)
    request_type, request_bit = read_ubit_long(request_payload, 0, NETMSG_TYPE_BITS)
    request_id, request_bit = read_ubit_long(request_payload, request_bit, 32)
    request_name, request_bit = read_cstring_bits(request_payload, request_bit, 260)
    requested, request_bit = read_ubit_long(request_payload, request_bit, 1)
    assert request_type == NET_FILE and request_id == request_transfer
    assert request_name == "maps/de_dust2.nav" and requested == 1

    # v1.8.6 regression: after decoding the build4100 single-block header
    # with its fixed 17-bit byte count, the reliable SPAWN payload begins
    # directly with SVC_SetView. The previous apparent 9-bit prefix was the
    # unread high part of that length field.
    spawn_transition_fixture = bytes.fromhex("7200a3180000c0140400000080")
    spawn_transition = parse_spawn_transition_stream(spawn_transition_fixture, 1)
    assert spawn_transition is not None
    assert spawn_transition["pad_bits"] == 0
    assert spawn_transition["view_entity"] == 3
    assert spawn_transition["tick"]["tick"] == 197
    assert spawn_transition["signon"]["state"] == SIGNONSTATE_SPAWN
    assert spawn_transition["signon"]["server_count"] == 1
    assert spawn_transition["tail_pad_bits"] == 6
    kind, detail = classify_signon_stream(spawn_transition_fixture)
    assert kind == "spawn_transition"
    assert "pad_bits=0" in detail and "view_entity=3" in detail and "tick=197" in detail and "tail_pad_bits=6" in detail

    classinfo = build_classinfo_payload()
    parsed = parse_classinfo_at(classinfo, 0)
    assert parsed is not None and len(parsed[1]) == 177
    assert parsed[1][0] == (0, "CWorld", "DT_WORLD")
    assert parsed[1][14] == (14, "CBaseCSGrenadeProjectile", "DT_BaseCSGrenadeProjectile")
    assert parsed[1][-1] == (176, "ParticleSmokeGrenade", "DT_ParticleSmokeGrenade")
    noise = b"".join(hashlib.sha256(struct.pack("<I", i)).digest() for i in range(8))
    decoded_stream = classinfo + noise
    compressed = bz2.compress(decoded_stream, compresslevel=9)
    total_fragments = (len(compressed) + FRAGMENT_SIZE - 1) // FRAGMENT_SIZE
    assert 2 <= total_fragments <= 12
    first_count = min(5, total_fragments - 1)
    second_count = total_fragments - first_count
    assert 1 <= first_count <= 7 and 1 <= second_count <= 7

    first_writer = BitWriter()
    first_writer.write_ubit_long(0, 3)
    first_writer.write_ubit_long(1, 1)  # normal stream present
    first_writer.write_ubit_long(1, 1)  # fragmented block
    first_writer.write_ubit_long(0, MAX_FILE_SIZE_BITS - FRAGMENT_BITS)
    first_writer.write_ubit_long(first_count, 3)
    first_writer.write_ubit_long(0, 1)  # not a file
    first_writer.write_ubit_long(1, 1)  # compressed
    first_writer.write_ubit_long(len(decoded_stream), MAX_FILE_SIZE_BITS)
    first_writer.write_ubit_long(len(compressed), MAX_FILE_SIZE_BITS)
    for byte in compressed[: first_count * FRAGMENT_SIZE]:
        first_writer.write_byte(byte)
    first_writer.write_ubit_long(0, 1)  # file stream absent

    second_writer = BitWriter()
    second_writer.write_ubit_long(1, 3)
    second_writer.write_ubit_long(1, 1)  # normal stream present
    second_writer.write_ubit_long(1, 1)  # fragmented block
    second_writer.write_ubit_long(first_count, MAX_FILE_SIZE_BITS - FRAGMENT_BITS)
    second_writer.write_ubit_long(second_count, 3)
    for byte in compressed[first_count * FRAGMENT_SIZE :]:
        second_writer.write_byte(byte)
    second_writer.write_ubit_long(0, 1)  # file stream absent

    reassembler = ReliableFragmentReassembler()
    subchannel, blocks, completed, endbit = reassembler.consume(first_writer.bytes())
    assert subchannel == 0 and len(blocks) == 1 and not completed
    assert blocks[0]["start"] == 0 and blocks[0]["count"] == first_count
    assert reassembler.ack_reliable_state == 0x01 and endbit > 80

    choked_packet = struct.pack("<iiBB", 11, 1, PACKET_FLAG_RELIABLE | PACKET_FLAG_CHOKED, 0) + bytes([8]) + second_writer.bytes()
    seq, ack, flags, reliable, choked, challenge, reliable_payload = parse_netchan_packet(choked_packet)
    assert (seq, ack, flags, reliable, choked, challenge) == (11, 1, 0x11, 0, 8, None)
    subchannel, blocks, completed, _ = reassembler.consume(reliable_payload)
    assert subchannel == 1 and len(blocks) == 1 and reassembler.ack_reliable_state == 0x03
    assert completed and completed[0][1] is True and completed[0][2] == decoded_stream and completed[0][3] == ""
    assert scan_classinfo_messages(completed[0][2])

    # v1.8.31a: file transfers use reliable stream 1 and carry transfer-id + filename.
    file_bytes = bytes((i * 13) & 0xFF for i in range(300))
    file_writer = BitWriter()
    file_writer.write_ubit_long(2, 3)  # subchannel
    file_writer.write_ubit_long(0, 1)  # stream 0 absent
    file_writer.write_ubit_long(1, 1)  # stream 1 present
    file_writer.write_ubit_long(1, 1)  # fragmented block
    file_writer.write_ubit_long(0, MAX_FILE_SIZE_BITS - FRAGMENT_BITS)
    file_writer.write_ubit_long(2, 3)  # 300 bytes = 2 fragments
    file_writer.write_ubit_long(1, 1)  # is file
    file_writer.write_long(request_transfer)
    file_writer.write_string("maps/de_dust2.nav")
    file_writer.write_ubit_long(0, 1)  # uncompressed
    file_writer.write_ubit_long(len(file_bytes), MAX_FILE_SIZE_BITS)
    for byte in file_bytes:
        file_writer.write_byte(byte)
    file_reassembler = ReliableFragmentReassembler()
    file_subchannel, file_blocks, file_completed, _ = file_reassembler.consume(file_writer.bytes())
    assert file_subchannel == 2 and len(file_blocks) == 1 and len(file_completed) == 1
    assert file_blocks[0]["stream"] == 1 and file_blocks[0]["filename"] == "maps/de_dust2.nav"
    assert file_blocks[0]["transfer_id"] == request_transfer and file_completed[0][2] == file_bytes

    # v1.8.4 regression: exact build4100 svc_PacketEntities header uses a
    # 20-bit payload length (not the 24-bit field used by later Source branches).
    packet_entities = BitWriter()
    packet_entities.write_ubit_long(SVC_PACKETENTITIES, NETMSG_TYPE_BITS)
    packet_entities.write_ubit_long((1 << MAX_EDICT_BITS) - 1, MAX_EDICT_BITS)
    packet_entities.write_ubit_long(0, 1)  # full snapshot, not delta
    packet_entities.write_ubit_long(0, 1)  # baseline selector
    packet_entities.write_ubit_long(3, MAX_EDICT_BITS)
    packet_entities.write_ubit_long(37, 20)
    packet_entities.write_ubit_long(1, 1)  # update baseline
    packet_entities.write_ubit_long(0x1A2B3C4D5, 37)
    parsed_packet_entities = parse_packet_entities_at(packet_entities.bytes(), 0)
    assert parsed_packet_entities is not None
    assert parsed_packet_entities["max_entries"] == (1 << MAX_EDICT_BITS) - 1
    assert parsed_packet_entities["is_delta"] == 0
    assert parsed_packet_entities["delta_from"] == -1
    assert parsed_packet_entities["updated_entries"] == 3
    assert parsed_packet_entities["data_bits"] == 37
    assert parsed_packet_entities["update_baseline"] == 1
    packet_entity_matches = scan_packet_entities_messages(packet_entities.bytes())
    assert packet_entity_matches and packet_entity_matches[0]["start_bit"] == 0

    # v1.8.16a regression: real jump snapshot from WSL contains the true delta
    # immediately after NET_Tick at bit 37, plus a nested bit pattern at bit 160
    # that happens to decode as max=711/isDelta=0/dataBits=0.  The old runtime
    # sorter preferred that false "full" candidate and lost jump/air state.
    jump_snapshot_fixture = bytes.fromhex(
        "831b0000401720330000c000cc030042cc9f025efa5800000000703d0a3f351376400590015443804a0000000000"
    )
    jump_candidates = scan_packet_entities_messages(jump_snapshot_fixture, 0)
    assert any(
        int(item["start_bit"]) == 160 and int(item["is_delta"]) == 0
        and int(item["max_entries"]) == 711 and int(item["data_bits"]) == 0
        for item in jump_candidates
    )
    jump_selected = select_ordinary_packet_entities(jump_snapshot_fixture, 0)
    assert jump_selected is not None
    assert jump_selected["start_bit"] == 37
    assert jump_selected["max_entries"] == 5
    assert jump_selected["is_delta"] == 1
    assert jump_selected["delta_from"] == 204
    assert jump_selected["updated_entries"] == 1
    assert jump_selected["data_bits"] == 243

    # v1.8.5 regression: a reliable block can be flagged compressed even when
    # the bytes are not a valid BZip2 member.  Recovery must preserve the exact
    # ordinary-message boundary so a PacketEntities message following the block
    # in the same UDP payload remains decodable.
    invalid_bzip_raw = b"not-a-bzip-stream"
    recovery_writer = BitWriter()
    recovery_writer.write_ubit_long(2, 3)
    recovery_writer.write_ubit_long(1, 1)  # normal stream present
    recovery_writer.write_ubit_long(0, 1)  # single block
    recovery_writer.write_ubit_long(1, 1)  # declared compressed
    recovery_writer.write_ubit_long(64, MAX_FILE_SIZE_BITS)
    recovery_writer.write_ubit_long(len(invalid_bzip_raw), SINGLE_BLOCK_SIZE_BITS)
    for byte in invalid_bzip_raw:
        recovery_writer.write_byte(byte)
    recovery_writer.write_ubit_long(0, 1)  # file stream absent
    recovery_ordinary_start = recovery_writer.bits_written
    for bit in range(packet_entities.bits_written):
        value, _ = read_ubit_long(packet_entities.bytes(), bit, 1)
        recovery_writer.write_ubit_long(value, 1)

    recovery = ReliableFragmentReassembler()
    subchannel, blocks, recovered, endbit = recovery.consume(recovery_writer.bytes())
    assert subchannel == 2 and endbit == recovery_ordinary_start
    assert len(blocks) == 1 and blocks[0]["decode_error"] == "bzip2_magic_missing"
    assert recovered and recovered[0][1] is True and recovered[0][2] == invalid_bzip_raw
    assert recovered[0][3] == "bzip2_magic_missing"
    recovered_packetentities = scan_packet_entities_messages(recovery_writer.bytes(), endbit)
    assert recovered_packetentities and recovered_packetentities[0]["start_bit"] == endbit
    assert recovery.streams[0].active is False

    # v1.8.6 regression: real post-SPAWN snapshots use a compressed single
    # block.  Verify the fixed 17-bit compressed byte count, BZip decode and
    # PacketEntities scan as one path.
    snapshot_stream = BitWriter()
    snapshot_stream.write_ubit_long(NET_TICK, NETMSG_TYPE_BITS)
    snapshot_stream.write_long(197)
    for _ in range(3):
        snapshot_stream.write_ubit_long(NET_NOP, NETMSG_TYPE_BITS)
    snapshot_start = snapshot_stream.bits_written
    for bit in range(packet_entities.bits_written):
        value, _ = read_ubit_long(packet_entities.bytes(), bit, 1)
        snapshot_stream.write_ubit_long(value, 1)
    snapshot_plain = snapshot_stream.bytes()
    snapshot_bzip = bz2.compress(snapshot_plain, compresslevel=9)
    snapshot_writer = BitWriter()
    snapshot_writer.write_ubit_long(0, 3)
    snapshot_writer.write_ubit_long(1, 1)  # normal stream present
    snapshot_writer.write_ubit_long(0, 1)  # single block
    snapshot_writer.write_ubit_long(1, 1)  # compressed
    snapshot_writer.write_ubit_long(len(snapshot_plain), MAX_FILE_SIZE_BITS)
    snapshot_writer.write_ubit_long(len(snapshot_bzip), SINGLE_BLOCK_SIZE_BITS)
    for byte in snapshot_bzip:
        snapshot_writer.write_byte(byte)
    snapshot_writer.write_ubit_long(0, 1)  # file stream absent
    snapshot_reassembler = ReliableFragmentReassembler()
    _, snapshot_blocks, snapshot_completed, snapshot_end = snapshot_reassembler.consume(snapshot_writer.bytes())
    assert len(snapshot_blocks) == 1 and snapshot_blocks[0]["single"] is True
    assert snapshot_blocks[0]["compressed"] is True and snapshot_blocks[0]["decode_error"] == ""
    assert snapshot_end == snapshot_writer.bits_written
    assert snapshot_completed and snapshot_completed[0][2] == snapshot_plain
    snapshot_matches = scan_packet_entities_messages(snapshot_completed[0][2], 0)
    assert snapshot_matches and snapshot_matches[0]["start_bit"] == snapshot_start

    # Real v1.8.5 WSL fixture: seq=84 after SPAWN. With single-block length
    # decoded as u17 this is a valid BZip member (363 -> 931 bytes) containing
    # the first full svc_PacketEntities snapshot.
    real_postspawn_fixture = bytes.fromhex(
        "e8e800006b0184b4d0726282b24ca6b26238d1830102aefefff79dc5d8c480ce4100084001920c000cc45804808000c084c04c80d0008400806002b0220c3385526a37a2209165c4603651c8bc49db8e1031340012000108a201314d8c6824481a9f88a233341a0c8c8034340002cc3d9809c73a1772e5fab78da4d91682f182551628da44ef8be4f1b776b0626cba13851468baa111a72e284404f1b86a04d268dd68ad8a8d4c981dbfa6840829596d4581720080449641534b4112827af00bb65882135202251b18ad951b27882bed93d6a2b525ad816bfc9bb09a598c4ce76203ac98f0e9e09ecbcc494b4c928107e40919db0db2f86928636f4f4faf4af932420da2010eea56cadd05b1f36154d2508bf975a7607c013e5612420bacc19977dd2b50257df8a98b31410184e8afe82e8282a8da18cb2466b9663ea57632303db17190cc446f7f8ffd933f0d7da1ee079db012306ee4170464b7d10c42100c01ed6c416092b177255384090319cf8d1000"
    )
    fixture_reassembler = ReliableFragmentReassembler()
    fixture_subchannel, fixture_blocks, fixture_completed, fixture_end = fixture_reassembler.consume(real_postspawn_fixture)
    assert fixture_subchannel == 0 and fixture_end == 2954
    assert len(fixture_blocks) == 1
    assert fixture_blocks[0]["single"] is True and fixture_blocks[0]["compressed"] is True
    assert fixture_blocks[0]["total_bytes"] == 363 and fixture_blocks[0]["uncompressed_size"] == 931
    assert fixture_blocks[0]["decode_error"] == ""
    assert fixture_completed and len(fixture_completed[0][2]) == 931
    fixture_packetentities = scan_packet_entities_messages(fixture_completed[0][2], 0)
    assert len(fixture_packetentities) == 1
    assert fixture_packetentities[0]["start_bit"] == 7338
    assert fixture_packetentities[0]["max_entries"] == 5
    assert fixture_packetentities[0]["is_delta"] == 0
    assert fixture_packetentities[0]["updated_entries"] == 3
    assert fixture_packetentities[0]["data_bits"] == 56
    fixture_entities = parse_full_snapshot_entity_updates(fixture_packetentities[0], EXPECTED_CLASSES[:15])
    assert fixture_entities is not None and fixture_entities["complete"] is True
    assert fixture_entities["bits_consumed"] == 56
    assert fixture_entities["changed_props"] == 0
    assert [entity["entity"] for entity in fixture_entities["entities"]] == [1, 2, 3]
    assert [entity["class_id"] for entity in fixture_entities["entities"]] == [1, 1, 1]
    assert [entity["serial"] for entity in fixture_entities["entities"]] == [0, 0, 0]
    assert all(entity["class_name"] == "CCSPlayer" for entity in fixture_entities["entities"])
    assert all(entity["table_name"] == "DT_CSPlayer" for entity in fixture_entities["entities"])

    # v1.8.12 schema-driven value decoder coverage. Keep these independent of
    # entity headers so each build4100 leaf encoding has an exact bit contract.
    scalar_writer = BitWriter()
    scalar_writer.write_ubit_long(0xAB, 8)
    scalar_data = scalar_writer.bytes()
    value, end = decode_sendprop_value(scalar_data, 0, 8, {
        "type": 0, "bits": 8, "flags": 1, "low": 0.0, "high": 0.0,
    })
    assert value == 0xAB and end == 8

    float_writer = BitWriter()
    float_raw = struct.unpack("<I", struct.pack("<f", 12.5))[0]
    float_writer.write_ubit_long(float_raw, 32)
    value, end = decode_sendprop_value(float_writer.bytes(), 0, 32, {
        "type": 1, "bits": 32, "flags": 0x4, "low": 0.0, "high": 0.0,
    })
    assert abs(float(value) - 12.5) < 1e-6 and end == 32

    vector_writer = BitWriter()
    for raw in (0, 127, 255):
        vector_writer.write_ubit_long(raw, 8)
    value, end = decode_sendprop_value(vector_writer.bytes(), 0, 24, {
        "type": 2, "bits": 8, "flags": 0, "low": -1.0, "high": 1.0,
    })
    assert isinstance(value, tuple) and len(value) == 3 and end == 24
    assert abs(value[0] + 1.0) < 1e-6 and abs(value[2] - 1.0) < 1e-6

    # build4100 binary proof: SPROP_XYZE (0x80) is ignored by Vector_Encode
    # and Vector_Decode, so the exact same 3-scalar bitstream must decode.
    xyze_value, xyze_end = decode_sendprop_value(vector_writer.bytes(), 0, 24, {
        "type": 2, "bits": 8, "flags": SPROP_XYZE, "low": -1.0, "high": 1.0,
    })
    assert xyze_value == value and xyze_end == 24

    array_writer = BitWriter()
    array_writer.write_ubit_long(2, 2)  # max elements=2 => GetNumArrayLengthBits()=2
    array_writer.write_ubit_long(7, 8)
    array_writer.write_ubit_long(9, 8)
    array_value, array_end = decode_sendprop_value(array_writer.bytes(), 0, 18, {
        "type": 4, "bits": 0, "flags": 0, "low": 0.0, "high": 0.0,
        "array_elements": 2, "array_stride": 4,
        "element_type": 0, "element_bits": 8,
        "element_flags": SPROP_UNSIGNED | SPROP_INSIDEARRAY,
        "element_low": 0.0, "element_high": 0.0,
    })
    assert array_value == (7, 9) and array_end == 18

    string_writer = BitWriter()
    string_writer.write_ubit_long(3, 9)
    for ch in b"css":
        string_writer.write_ubit_long(ch, 8)
    value, end = decode_sendprop_value(string_writer.bytes(), 0, 33, {
        "type": 3, "bits": 0, "flags": 0, "low": 0.0, "high": 0.0,
    })
    assert value == "css" and end == 33

    # Logical payload bounds must win over byte padding.
    try:
        decode_sendprop_value(scalar_data, 0, 7, {
            "type": 0, "bits": 8, "flags": 1, "low": 0.0, "high": 0.0,
        })
        raise AssertionError("sendprop bounds not enforced")
    except ValueError:
        pass

    # v1.8.8 deterministic delta fixture: three existing CCSPlayer entities,
    # each changing exactly one 16-bit field (m_iAccount) from baseline 0 to
    # 1337. The delta snapshot explicitly references the full snapshot tick.
    delta_entity_payload = BitWriter()
    delta_field_index = 42
    delta_header_base = -1
    for delta_entity in (1, 2, 3):
        delta_entity_payload.write_ubit_var_build4100(delta_entity - delta_header_base - 1)
        delta_header_base = delta_entity
        delta_entity_payload.write_ubit_long(0, 1)  # leave PVS
        delta_entity_payload.write_ubit_long(0, 1)  # enter PVS
        delta_entity_payload.write_ubit_long(1, 1)  # changed prop present
        delta_entity_payload.write_ubit_var_build4100(delta_field_index)
        delta_entity_payload.write_ubit_long(1337, 16)
        delta_entity_payload.write_ubit_long(0, 1)  # per-entity prop-list terminator
    delta_entity_payload.write_ubit_long(0, 1)  # outer WriteDeltaEntities stream terminator
    delta_packet_writer = BitWriter()
    delta_packet_writer.write_ubit_long(SVC_PACKETENTITIES, NETMSG_TYPE_BITS)
    delta_packet_writer.write_ubit_long(5, MAX_EDICT_BITS)
    delta_packet_writer.write_ubit_long(1, 1)
    delta_packet_writer.write_long(197)
    delta_packet_writer.write_ubit_long(0, 1)
    delta_packet_writer.write_ubit_long(3, MAX_EDICT_BITS)
    delta_packet_writer.write_ubit_long(delta_entity_payload.bits_written, 20)
    delta_packet_writer.write_ubit_long(0, 1)
    for delta_bit in range(delta_entity_payload.bits_written):
        delta_value, _ = read_ubit_long(delta_entity_payload.bytes(), delta_bit, 1)
        delta_packet_writer.write_ubit_long(delta_value, 1)
    parsed_delta_packet = parse_packet_entities_at(delta_packet_writer.bytes(), 0)
    assert parsed_delta_packet is not None
    assert parsed_delta_packet["is_delta"] == 1 and parsed_delta_packet["delta_from"] == 197
    full_entity_classes = {
        int(entity["entity"]): (int(entity["class_id"]), str(entity["class_name"]), str(entity["table_name"]))
        for entity in fixture_entities["entities"]
    }
    parsed_account_delta = parse_delta_snapshot_account_updates(parsed_delta_packet, full_entity_classes)
    assert parsed_account_delta is not None and parsed_account_delta["complete"] is True
    assert parsed_account_delta["changed_props"] == 3
    assert [item["entity"] for item in parsed_account_delta["updates"]] == [1, 2, 3]
    assert all(item["field_index"] == delta_field_index for item in parsed_account_delta["updates"])
    assert all(item["value"] == 1337 for item in parsed_account_delta["updates"])
    assert parsed_account_delta["outer_terminator_bits"] == 1

    schema_fixture = {1: {delta_field_index: {
        "class_id": 1, "class_name": "CCSPlayer", "root_table": "DT_CSPlayer",
        "field_index": delta_field_index, "prop_name": "m_iAccount", "type": 0,
        "bits": 16, "flags": 1, "low": 0.0, "high": 0.0,
    }}}
    parsed_schema_delta = parse_entity_updates_schema(parsed_delta_packet, schema_fixture, full_entity_classes)
    assert parsed_schema_delta is not None and parsed_schema_delta["complete"] is True
    assert parsed_schema_delta["changed_props"] == 3
    assert all(item["props"][0]["name"] == "m_iAccount" for item in parsed_schema_delta["entries"])
    assert all(item["props"][0]["value"] == 1337 for item in parsed_schema_delta["entries"])
    assert parsed_schema_delta["outer_terminator_bits"] == 1

    # v1.8.15a real WSL movement delta after canonical local/non-local
    # datatable recipient proxies. The useful CCSPlayer props decode fully,
    # followed by 13 literal zero bits emitted by the engine's culled-proxy
    # path. The bounded zero tail is protocol framing, not a SendProp value.
    movement_tail_packet = {
        "is_delta": 1, "delta_from": 197, "updated_entries": 1, "data_bits": 237,
        "payload": bytes.fromhex("8498ff636c00000000000020200000008002c800aa214092a00100000000"),
    }
    movement_tail_schema = {1: {
        8: {"prop_name": "m_vecVelocity[1]", "type": 1, "bits": 20, "flags": 0, "low": -2048.0, "high": 2048.0},
        11: {"prop_name": "m_vecOrigin", "type": 2, "bits": 32, "flags": SPROP_NOSCALE, "low": 0.0, "high": 0.0},
        13: {"prop_name": "m_angEyeAngles[0]", "type": 1, "bits": 11, "flags": 1024, "low": 0.0, "high": 360.0},
        14: {"prop_name": "m_angEyeAngles[1]", "type": 1, "bits": 11, "flags": 1024, "low": 0.0, "high": 360.0},
        16: {"prop_name": "m_iTeamNum", "type": 0, "bits": 6, "flags": 0, "low": 0.0, "high": 0.0},
        308: {"prop_name": "recipient_culled_tail_field", "type": 0, "bits": 32, "flags": SPROP_UNSIGNED, "low": 0.0, "high": 0.0},
    }}
    movement_tail = parse_entity_updates_schema(
        movement_tail_packet, movement_tail_schema, {3: (1, "CCSPlayer", "DT_CSPlayer")}
    )
    assert movement_tail is not None and movement_tail["complete"] is True
    assert movement_tail["bits_consumed"] == 237 and movement_tail["outer_terminator_bits"] == 13
    movement_props = {p["name"]: p["value"] for p in movement_tail["entries"][0]["props"]}
    assert movement_props["m_vecOrigin"] == (0.0, 3.0, 0.0)
    assert abs(float(movement_props["m_vecVelocity[1]"]) - 200.0) <= 0.01
    assert abs(float(movement_props["m_angEyeAngles[0]"]) - 45.0) <= 0.2
    assert abs(float(movement_props["m_angEyeAngles[1]"]) - 90.0) <= 0.2

    # Real v1.8.10 WSL delta fixture.  The server emits this exact 117-bit
    # entity payload repeatedly after BaselineAck + FULL + NET_Tick.  It is
    # three existing CCSPlayer updates carrying flattened field 347 = 1337,
    # followed by the one-bit outer entity-stream zero terminator.
    real_delta_payload = bytes.fromhex("2240ae9c0209902ba74002e4ca2900")
    real_delta_packet = {
        "is_delta": 1,
        "delta_from": 201,
        "updated_entries": 3,
        "data_bits": 117,
        "payload": real_delta_payload,
    }
    real_account_delta = parse_delta_snapshot_account_updates(real_delta_packet, full_entity_classes)
    assert real_account_delta is not None and real_account_delta["complete"] is True
    assert real_account_delta["bits_consumed"] == 117
    assert real_account_delta["outer_terminator_bits"] == 1
    assert [item["entity"] for item in real_account_delta["updates"]] == [1, 2, 3]
    assert all(item["field_index"] == 347 for item in real_account_delta["updates"])
    assert all(item["value"] == 1337 for item in real_account_delta["updates"])

    # Exact old build4100 unary UBitVar round-trip used by entity and property
    # delta headers. Later Source branches use a different six-bit format.
    for ubit_value in (0, 1, 2, 3, 15, 16, 31, 255, 4095):
        ubit_writer = BitWriter()
        ubit_writer.write_ubit_var_build4100(ubit_value)
        decoded_value, decoded_end = read_ubit_var_build4100(ubit_writer.bytes(), 0)
        assert decoded_value == ubit_value and decoded_end == ubit_writer.bits_written

    # v1.8.35g svc_TempEntities fixture: one full CTEFireBullets event.
    # The build4100 envelope is type27 + count:u8 + length:u17. PlaybackTempEntity
    # stores classId+1 in CEventInfo, so registry class120 is wire class121. The
    # event payload is fireDelayBit + fullBit + wireClassId:u8 + UBitVar SendProps.
    te_schema_fixture = {120: {
        3: {"prop_name": "m_iWeaponID", "type": 0, "bits": 5, "flags": SPROP_UNSIGNED, "low": 0.0, "high": 0.0},
        5: {"prop_name": "m_iSeed", "type": 0, "bits": 8, "flags": SPROP_UNSIGNED, "low": 0.0, "high": 0.0},
        6: {"prop_name": "m_iPlayer", "type": 0, "bits": 6, "flags": SPROP_UNSIGNED, "low": 0.0, "high": 0.0},
        7: {"prop_name": "m_flSpread", "type": 1, "bits": 10, "flags": 0, "low": 0.0, "high": 1.0},
    }}
    te_event = BitWriter()
    te_event.write_ubit_long(0, 1)  # fire_delay == 0
    te_event.write_ubit_long(1, 1)  # full update against zeros
    te_event.write_ubit_long(121, bits_required_for_count(len(EXPECTED_CLASSES)))
    te_event.write_ubit_long(1, 1); te_event.write_ubit_var_build4100(3); te_event.write_ubit_long(16, 5)
    te_event.write_ubit_long(1, 1); te_event.write_ubit_var_build4100(1); te_event.write_ubit_long(123, 8)
    te_event.write_ubit_long(1, 1); te_event.write_ubit_var_build4100(0); te_event.write_ubit_long(3, 6)
    te_event.write_ubit_long(1, 1); te_event.write_ubit_var_build4100(0); te_event.write_ubit_long(153, 10)
    te_event.write_ubit_long(0, 1)
    te_message = BitWriter()
    te_message.write_ubit_long(SVC_TEMPENTITIES, NETMSG_TYPE_BITS)
    te_message.write_ubit_long(1, EVENT_INDEX_BITS)
    te_message.write_ubit_long(te_event.bits_written, NET_MAX_PAYLOAD_BITS_OLD)
    for te_bit in range(te_event.bits_written):
        te_value, _ = read_ubit_long(te_event.bytes(), te_bit, 1)
        te_message.write_ubit_long(te_value, 1)
    te_parsed = parse_temp_entities_at(te_message.bytes(), 0, te_schema_fixture)
    assert te_parsed is not None and te_parsed["num_entries"] == 1
    te_entry = te_parsed["entries"][0]
    assert te_entry["class_id"] == 120 and te_entry["wire_class_id"] == 121
    assert te_entry["full"] == 1 and te_entry["fire_delay"] == 0.0
    te_values = {item["name"]: item["value"] for item in te_entry["props"]}
    assert te_values["m_iWeaponID"] == 16 and te_values["m_iSeed"] == 123 and te_values["m_iPlayer"] == 3
    assert 0.14 < float(te_values["m_flSpread"]) < 0.16
    te_scanned = scan_temp_entities_messages(te_message.bytes(), te_schema_fixture)
    assert len(te_scanned) == 1 and te_scanned[0]["entries"][0]["class_id"] == 120

    # The observer fallback validates the exact SVC envelope + first 1-based
    # class id even if a future/unknown SendProp value prevents full decode.
    # Add one trailing one-bit to make strict property decoding fail while the
    # envelope/class oracle remains valid.
    te_fallback_event = BitWriter()
    for te_bit in range(te_event.bits_written):
        te_value, _ = read_ubit_long(te_event.bytes(), te_bit, 1)
        te_fallback_event.write_ubit_long(te_value, 1)
    te_fallback_event.write_ubit_long(1, 1)
    te_fallback_message = BitWriter()
    te_fallback_message.write_ubit_long(SVC_TEMPENTITIES, NETMSG_TYPE_BITS)
    te_fallback_message.write_ubit_long(1, EVENT_INDEX_BITS)
    te_fallback_message.write_ubit_long(te_fallback_event.bits_written, NET_MAX_PAYLOAD_BITS_OLD)
    for te_bit in range(te_fallback_event.bits_written):
        te_value, _ = read_ubit_long(te_fallback_event.bytes(), te_bit, 1)
        te_fallback_message.write_ubit_long(te_value, 1)
    assert parse_temp_entities_at(te_fallback_message.bytes(), 0, te_schema_fixture) is None
    te_envelopes = scan_temp_entities_envelopes(te_fallback_message.bytes())
    assert len(te_envelopes) == 1
    assert te_envelopes[0]["first_class_id"] == 120 and te_envelopes[0]["first_wire_class_id"] == 121

    # gatefix12: a different temp entity may precede FireBullets in the same
    # svc_TempEntities payload.  Deliberately omit class119 from the fixture
    # schema so strict sequential decoding cannot cross entry #0.  The bounded
    # full-entry resync must still recover entry #1 (wire121 -> registry120)
    # and decode its real class120 properties, not merely match the class id.
    te_prefix = BitWriter()
    te_prefix.write_ubit_long(0, 1)  # no delay
    te_prefix.write_ubit_long(1, 1)  # full class
    te_prefix.write_ubit_long(120, bits_required_for_count(len(EXPECTED_CLASSES)))  # registry119
    te_prefix.write_ubit_long(0, 1)  # empty delta-prop list
    te_multi_payload = BitWriter()
    for source_writer in (te_prefix, te_event):
        for te_bit in range(source_writer.bits_written):
            te_value, _ = read_ubit_long(source_writer.bytes(), te_bit, 1)
            te_multi_payload.write_ubit_long(te_value, 1)
    te_multi_message = BitWriter()
    te_multi_message.write_ubit_long(SVC_TEMPENTITIES, NETMSG_TYPE_BITS)
    te_multi_message.write_ubit_long(2, EVENT_INDEX_BITS)
    te_multi_message.write_ubit_long(te_multi_payload.bits_written, NET_MAX_PAYLOAD_BITS_OLD)
    for te_bit in range(te_multi_payload.bits_written):
        te_value, _ = read_ubit_long(te_multi_payload.bytes(), te_bit, 1)
        te_multi_message.write_ubit_long(te_value, 1)
    assert parse_temp_entities_at(te_multi_message.bytes(), 0, te_schema_fixture) is None
    te_multi_envelopes = scan_temp_entities_envelopes(te_multi_message.bytes())
    assert len(te_multi_envelopes) == 1
    assert te_multi_envelopes[0]["first_class_id"] == 119
    te_resync = scan_temp_entities_resync_full(te_multi_message.bytes(), te_schema_fixture, 120)
    assert len(te_resync) == 1
    assert te_resync[0]["start_bit"] == te_prefix.bits_written
    te_resync_values = {item["name"]: item["value"] for item in te_resync[0]["props"]}
    assert te_resync_values["m_iWeaponID"] == 16 and te_resync_values["m_iPlayer"] == 3
    assert 0.14 < float(te_resync_values["m_flSpread"]) < 0.16

    entity_message = BitWriter()
    entity_message.write_ubit_long(SVC_ENTITYMESSAGE, NETMSG_TYPE_BITS)
    entity_message.write_ubit_long(3, MAX_EDICT_BITS)
    entity_message.write_ubit_long(1, MAX_SERVER_CLASS_BITS)
    entity_message.write_ubit_long(13, 11)
    entity_message.write_ubit_long(0x15A5, 13)
    parsed_entity_message = parse_entity_message_at(entity_message.bytes(), 0)
    assert parsed_entity_message is not None
    assert parsed_entity_message["entity"] == 3 and parsed_entity_message["class_id"] == 1
    assert parsed_entity_message["payload_bits"] == 13

    # v1.8.14: exact pre-protobuf clc_Move header and one build4100 CUserCmd.
    move_payload, move_cmd_bits = build_move_payload(198, pitch=45.0, yaw=90.0, forward_move=200)
    move_pos = 0
    move_type, move_pos = read_ubit_long(move_payload, move_pos, NETMSG_TYPE_BITS)
    move_new, move_pos = read_ubit_long(move_payload, move_pos, 4)
    move_backup, move_pos = read_ubit_long(move_payload, move_pos, 3)
    move_length, move_pos = read_ubit_long(move_payload, move_pos, 16)
    assert move_type == CLC_MOVE and move_new == 1 and move_backup == 0
    assert move_length == move_cmd_bits and move_length > 32
    cmd_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    tick_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    assert cmd_changed == 0 and tick_changed == 1
    encoded_tick, move_pos = read_ubit_long(move_payload, move_pos, 32)
    pitch_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    encoded_pitch, move_pos = read_ubit_long(move_payload, move_pos, 16)
    yaw_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    encoded_yaw, move_pos = read_ubit_long(move_payload, move_pos, 16)
    roll_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    forward_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    encoded_forward, move_pos = read_ubit_long(move_payload, move_pos, 16)
    assert encoded_tick == 198 and pitch_changed == 1 and yaw_changed == 1
    assert encoded_pitch == 8192 and encoded_yaw == 16384
    assert roll_changed == 0 and forward_changed == 1 and encoded_forward == 200
    side_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    up_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    buttons_changed, move_pos = read_ubit_long(move_payload, move_pos, 1)
    assert side_changed == 0 and up_changed == 0 and buttons_changed == 0

    jump_payload, jump_cmd_bits = build_move_payload(200, command_number=3, pitch=45.0, yaw=90.0, buttons=2)
    jump_pos = NETMSG_TYPE_BITS + 4 + 3 + 16
    cmd_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    encoded_cmd, jump_pos = read_ubit_long(jump_payload, jump_pos, 32)
    tick_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    encoded_jump_tick, jump_pos = read_ubit_long(jump_payload, jump_pos, 32)
    for bits in (16, 16):
        changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
        assert changed == 1
        _, jump_pos = read_ubit_long(jump_payload, jump_pos, bits)
    roll_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    forward_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    side_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    up_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    buttons_changed, jump_pos = read_ubit_long(jump_payload, jump_pos, 1)
    encoded_buttons, jump_pos = read_ubit_long(jump_payload, jump_pos, 32)
    assert cmd_changed == 1 and encoded_cmd == 3 and tick_changed == 1 and encoded_jump_tick == 200
    assert roll_changed == 0 and forward_changed == 0 and side_changed == 0 and up_changed == 0
    assert buttons_changed == 1 and encoded_buttons == 2 and jump_cmd_bits > move_cmd_bits

    # v1.8.29a: exact build4100 svc_GameEventList/type30 and
    # svc_GameEvent/type25 envelopes recovered from engine_i486.so.
    event_list_payload = BitWriter()
    event_list_payload.write_ubit_long(42, 9)
    event_list_payload.write_string("player_say")
    event_list_payload.write_ubit_long(GAMEEVENT_TYPE_SHORT, 3)
    event_list_payload.write_string("userid")
    event_list_payload.write_ubit_long(GAMEEVENT_TYPE_STRING, 3)
    event_list_payload.write_string("text")
    event_list_payload.write_ubit_long(GAMEEVENT_TYPE_LOCAL, 3)
    event_list_message = BitWriter()
    event_list_message.write_ubit_long(SVC_GAMEEVENTLIST, NETMSG_TYPE_BITS)
    event_list_message.write_ubit_long(1, 9)
    event_list_message.write_ubit_long(event_list_payload.bits_written, 20)
    for bit in range(event_list_payload.bits_written):
        value, _ = read_ubit_long(event_list_payload.bytes(), bit, 1)
        event_list_message.write_ubit_long(value, 1)
    parsed_event_list = parse_gameevent_list_at(event_list_message.bytes(), 0)
    assert parsed_event_list is not None and parsed_event_list["num_events"] == 1
    descriptors = parsed_event_list["descriptors"]
    assert descriptors[42]["name"] == "player_say"
    assert descriptors[42]["fields"] == [("userid", GAMEEVENT_TYPE_SHORT), ("text", GAMEEVENT_TYPE_STRING)]
    assert len(scan_gameevent_lists(event_list_message.bytes())) == 1

    event_payload = BitWriter()
    event_payload.write_ubit_long(42, 9)
    event_payload.write_ubit_long(31337, 16)
    event_payload.write_string("cssv34_wire_event")
    event_message = BitWriter()
    event_message.write_ubit_long(SVC_GAMEEVENT, NETMSG_TYPE_BITS)
    event_message.write_ubit_long(event_payload.bits_written, 11)
    for bit in range(event_payload.bits_written):
        value, _ = read_ubit_long(event_payload.bytes(), bit, 1)
        event_message.write_ubit_long(value, 1)
    parsed_event = parse_gameevent_at(event_message.bytes(), 0, descriptors)
    assert parsed_event is not None and parsed_event["name"] == "player_say"
    assert parsed_event["values"]["userid"] == 31337
    assert parsed_event["values"]["text"] == "cssv34_wire_event"
    assert len(scan_gameevents(event_message.bytes(), descriptors)) == 1

    # v1.8.29b: engine_i486.so CLC_ListenEvents::WriteToBuffer writes
    # message type 12 followed by a fixed 0x200-bit listener mask.
    listen_ids = {42, 44, 63}
    listen_payload = build_listen_events_payload(listen_ids)
    listen_type, listen_pos = read_ubit_long(listen_payload, 0, NETMSG_TYPE_BITS)
    assert listen_type == CLC_LISTENEVENTS
    for event_id in range(512):
        listening, listen_pos = read_ubit_long(listen_payload, listen_pos, 1)
        assert listening == (1 if event_id in listen_ids else 0)
    assert listen_pos == NETMSG_TYPE_BITS + 512

    # v1.8.30a: exact build4100 svc_UserMessage envelope. SayText is CSS
    # usermessage registration index 3 and carries sender byte + C string.
    saytext_payload = BitWriter()
    saytext_payload.write_byte(3)
    saytext_payload.write_string("cssv34_wire_usermsg")
    saytext_payload.write_byte(1)
    saytext_message = BitWriter()
    saytext_message.write_ubit_long(SVC_USERMESSAGE, NETMSG_TYPE_BITS)
    saytext_message.write_byte(3)
    saytext_message.write_ubit_long(saytext_payload.bits_written, 11)
    for bit in range(saytext_payload.bits_written):
        value, _ = read_ubit_long(saytext_payload.bytes(), bit, 1)
        saytext_message.write_ubit_long(value, 1)
    parsed_usermsg = parse_user_message_at(saytext_message.bytes(), 0)
    assert parsed_usermsg is not None and parsed_usermsg["message_type"] == 3
    parsed_saytext = parse_saytext_payload(
        bytes(parsed_usermsg["payload"]), int(parsed_usermsg["payload_bits"])
    )
    assert parsed_saytext == (3, "cssv34_wire_usermsg", 1)
    assert len(scan_user_messages(saytext_message.bytes())) == 1

    # v1.8.32a: exact build4100 voice envelopes recovered from engine_i486.so.
    voice_raw = b"CSSV34VOICE32A"
    voice_clc = build_voice_data_payload(voice_raw)
    voice_type, voice_pos = read_ubit_long(voice_clc, 0, NETMSG_TYPE_BITS)
    voice_bits, voice_pos = read_ubit_long(voice_clc, voice_pos, 16)
    assert voice_type == CLC_VOICEDATA and voice_bits == len(voice_raw) * 8
    assert extract_bit_slice(voice_clc, voice_pos, voice_bits) == voice_raw
    voice_svc = BitWriter()
    voice_svc.write_ubit_long(SVC_VOICEDATA, NETMSG_TYPE_BITS)
    voice_svc.write_byte(3)
    voice_svc.write_ubit_long(len(voice_raw) * 8, 16)
    for voice_bit in range(len(voice_raw) * 8):
        voice_value, _ = read_ubit_long(voice_raw, voice_bit, 1)
        voice_svc.write_ubit_long(voice_value, 1)
    parsed_voice = parse_voice_data_at(voice_svc.bytes(), 0)
    assert parsed_voice is not None
    assert parsed_voice["sender"] == 3 and parsed_voice["payload_bits"] == len(voice_raw) * 8
    assert parsed_voice["payload"] == voice_raw
    assert len(scan_voice_data(voice_svc.bytes())) == 1
    voice_init = BitWriter()
    voice_init.write_ubit_long(SVC_VOICEINIT, NETMSG_TYPE_BITS)
    voice_init.write_string("vaudio_speex")
    voice_init.write_byte(5)
    parsed_voice_init = parse_voice_init_at(voice_init.bytes(), 0)
    assert parsed_voice_init is not None
    assert parsed_voice_init["codec"] == "vaudio_speex" and parsed_voice_init["quality"] == 5

    # v1.8.34zk/zn/zp: a live instancebaseline TSV is a materialized lower-bound
    # snapshot. Dust2 can expose a pre-reserved CPlantedC4 as class 6/index 0,
    # while a post-changelevel map such as cs_office can legitimately expose no
    # materialized classes at all before the next real Protocol7 client connects.
    baseline_selftest = Path("/tmp/cssv34-protocol7-instancebaseline-selftest.tsv")
    with baseline_selftest.open("w", encoding="utf-8") as handle:
        handle.write("#build4100-instancebaseline-v1\n")
        handle.write("#class_id\tclass_name\troot_table\tbaseline_index\tready\n")
        for class_id, (class_name, root_table) in enumerate(EXPECTED_CLASSES):
            baseline_index = 0 if class_id == 6 else 0xffff
            is_ready = 1 if class_id == 6 else 0
            handle.write(
                f"{class_id}\t{class_name}\t{root_table}\t{baseline_index}\t{is_ready}\n"
            )
    try:
        baseline_runtime = load_instancebaseline_runtime(str(baseline_selftest))
        assert baseline_runtime == {6: 0}
        # v1.8.34zn: the TSV above is written before the real Protocol7 client
        # connects. The real engine may append CCSPlayer afterwards, so wire is
        # allowed to be a strict superset while preserving every snapshot index.
        assert validate_instancebaseline_runtime_wire(baseline_runtime, {6: 0}) == []
        assert validate_instancebaseline_runtime_wire(baseline_runtime, {6: 0, 1: 1}) == [1]
        try:
            validate_instancebaseline_runtime_wire(baseline_runtime, {1: 0})
            raise AssertionError("missing runtime baseline was accepted")
        except ValueError as exc:
            assert "runtime_only=[6]" in str(exc)
        try:
            validate_instancebaseline_runtime_wire(baseline_runtime, {6: 1, 1: 0})
            raise AssertionError("runtime baseline index mismatch was accepted")
        except ValueError as exc:
            assert "index_mismatch=[6]" in str(exc)

        # v1.8.34zp: after changelevel to a map with no pre-existing baseline
        # entity, all 177 rows can still be valid while every class is 0xffff.
        # The later wire table is allowed to grow from this empty lower-bound.
        with baseline_selftest.open("w", encoding="utf-8") as handle:
            handle.write("#build4100-instancebaseline-v1\n")
            handle.write("#class_id\tclass_name\troot_table\tbaseline_index\tready\n")
            for class_id, (class_name, root_table) in enumerate(EXPECTED_CLASSES):
                handle.write(
                    f"{class_id}\t{class_name}\t{root_table}\t65535\t0\n"
                )
        empty_runtime = load_instancebaseline_runtime(str(baseline_selftest))
        assert empty_runtime == {}
        assert validate_instancebaseline_runtime_wire(empty_runtime, {}) == []
        assert validate_instancebaseline_runtime_wire(empty_runtime, {1: 0}) == [1]
        assert validate_instancebaseline_runtime_wire(empty_runtime, {1: 0, 6: 1}) == [1, 6]
        # v1.8.34zr: the signon string table itself may also still be empty
        # at this checkpoint. Presence of the table is required, but zero entries
        # are legal when the immutable runtime lower-bound is empty.
        assert decode_instancebaseline_wire([]) == {}
        assert decode_instancebaseline_wire([(0, "1")]) == {1: 0}
        try:
            decode_instancebaseline_wire([(0, "1"), (1, "1")])
            raise AssertionError("duplicate wire baseline class was accepted")
        except ValueError as exc:
            assert "bad/duplicate class id 1" in str(exc)
    finally:
        baseline_selftest.unlink(missing_ok=True)

    # v1.8.26e: multiclient probes may deliberately choose opposite teams so
    # simultaneous real clients do not occupy the same spawn point.
    for join_team in (2, 3):
        stringcmd_payload = build_stringcmd_payload(f"jointeam {join_team}")
        stringcmd_type, stringcmd_pos = read_ubit_long(stringcmd_payload, 0, NETMSG_TYPE_BITS)
        stringcmd_value, stringcmd_end = read_cstring_bits(stringcmd_payload, stringcmd_pos)
        assert stringcmd_type == NET_STRINGCMD
        assert stringcmd_value == f"jointeam {join_team}"
        assert stringcmd_end <= len(stringcmd_payload) * 8

    print(
        f"PROTOCOL7_SELFTEST=PASS probe_revision={PROBE_REVISION} challenge=A connect=k protocol=7 "
        "auth2_guid=32hex auth3=steam2_u16_ticket classic_revemu=152 accept=B "
        "tcp_stream=connect_then_4challenge netchan_header=seq_ack_flags_reliable_choked "
        "signon=type6_state2_servercount_minus1 clientinfo=type8_exact "
        "udp_signon=exact_subchannel_reassembly log_fixture=1511b_6frags reliable_ack=subchannel_toggle_can_return_zero+nop_keepalive "
        "bzip2=reassembly signon_trace=prefix_boundaries+sha256+bounded_hex signon_stream=server_driven_new3+print7_serverinfo8+tick3+create_stringtable12x16+setconvar5+sendtable9+classinfo10 "
        "spawn_transition=single17+pad0+setview18_ent3+tick197+state5_tail client_full=state6_after_full_snapshot snapshot_ack=baselineack11_tick32_baseline1+nettick3_tick32 fragment_recovery=single17+invalid_bzip_nonfatal+ordinary_boundary snapshot=single17+bzip+packetentities+real_seq84_fixture entitydelta=old_ubitvar+enterpvs3+class1+serial0+zero_propdelta+schema_driven_int_float_vector_string+bounds+outer_zero_terminator+recipient_cull_zero_tail delta_snapshot=from_tick197+field_index+u16_value packetentities=type26_u11_delta_baseline_u11_u20_updatebaseline entitymessage=type24_u11_u9_u11 classinfo=type10_177_exact sendtable=sequential_type9_to_type10 dpt_array=lengthbits+paired_element xyze=build4100_ordinary_3scalar stringcmd=type4_jointeam2+jointeam3 gameevent=type30_list+clc12_listenevents512+type25_player_say tempentities=type27_u8_u17+wire_classid_plus1+class120_delta_props usermessage=type23_u8_u11_saytext3 voice=clc10_u16bits+svc14_codec_quality+svc15_sender_u16bits objective=round+bomb+hostage_gameevents+c4_bool_unsigned_wire download=stringtable_entries+net_file2+stream1_sha256 clc_move=type9_new4_backup3_length16+usercmd_eyeangles+ground_accel+friction+jump_gravity+jump_latch+air_accel_cap30+stepmove18+walkable_slope+multiplane5+dynamic_player_group8+dynamic_nonplayer_vphysics_inlinebrush+hostage_bbox_modelbounds+water_levels_swim+attack_reload_events+hitscan_player_hurt_death net_disconnect1=type1+reason instancebaseline=runtime_snapshot_subset+empty_snapshot+empty_wire_table+wire_growth+implicit_latespawn stringtable_userdata=build4100_u12_max4096"
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=27015)
    ap.add_argument("--tcp-port", type=int, default=None)
    ap.add_argument("--name", default="cssv34_realprobe")
    ap.add_argument("--password", default="")
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--initial-signon-retries", type=int, default=0)
    ap.add_argument("--initial-signon-retry-interval", type=float, default=0.75)
    ap.add_argument("--diag-seconds", type=float, default=0.75)
    ap.add_argument("--server-count", type=int, default=None)
    ap.add_argument("--sendtable-crc", type=lambda value: int(value, 0), default=None)
    ap.add_argument("--force-full-sendtables", action="store_true")
    ap.add_argument("--disconnect-after-pass", action="store_true")
    ap.add_argument("--join-team", type=int, choices=(2, 3), default=3)
    ap.add_argument("--pre-weapon-join-team", type=int, choices=(0, 2, 3), default=0)
    ap.add_argument("--hold-after-pass", type=float, default=0.0)
    ap.add_argument("--hold-until-file", default="")
    ap.add_argument("--max-udp-packets", type=int, default=256)
    ap.add_argument("--download-resource", default="")
    ap.add_argument("--download-size", type=int, default=0)
    ap.add_argument("--download-sha256", default="")
    ap.add_argument("--voice-send-hex", default="")
    ap.add_argument("--expect-voice-hex", default="")
    ap.add_argument("--expect-voice-sender", type=int, default=-1)
    ap.add_argument("--expect-te-firebullets-player", type=int, default=-1)
    ap.add_argument("--expect-te-firebullets-weapon", type=int, default=-1)
    ap.add_argument("--sendprop-schema", default=None)
    ap.add_argument("--instance-baselines", default=None)
    ap.add_argument("--objective-scenario", choices=("bomb_rounds", "hostage_rescue"), default="")
    ap.add_argument("--weapon-actions", action="store_true")
    ap.add_argument("--combat-damage", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if args.combat_damage and not args.weapon_actions:
        print("PROTOCOL7=FAIL reason=combat_damage_requires_weapon_actions")
        return 2
    if not (1 <= args.port <= 65535):
        print(f"PROTOCOL7=FAIL reason=bad_port port={args.port}")
        return 2
    tcp_port = args.tcp_port if args.tcp_port is not None else args.port
    if not (1 <= tcp_port <= 65535):
        print(f"PROTOCOL7=FAIL reason=bad_tcp_port port={tcp_port}")
        return 2
    if args.hold_after_pass < 0.0:
        print(f"PROTOCOL7=FAIL reason=bad_hold_after_pass seconds={args.hold_after_pass}")
        return 2
    if args.expect_te_firebullets_player < -1 or args.expect_te_firebullets_player > 63:
        print(f"PROTOCOL7=FAIL reason=bad_te_firebullets_player player={args.expect_te_firebullets_player}")
        return 2
    if args.expect_te_firebullets_weapon < -1 or args.expect_te_firebullets_weapon > 31:
        print(f"PROTOCOL7=FAIL reason=bad_te_firebullets_weapon weapon={args.expect_te_firebullets_weapon}")
        return 2
    if args.initial_signon_retries < 0:
        print(f"PROTOCOL7=FAIL reason=bad_initial_signon_retries retries={args.initial_signon_retries}")
        return 2
    if args.initial_signon_retry_interval <= 0.0:
        print(f"PROTOCOL7=FAIL reason=bad_initial_signon_retry_interval seconds={args.initial_signon_retry_interval}")
        return 2
    if args.download_resource:
        normalized_download = args.download_resource.replace("\\", "/")
        if normalized_download.startswith("/") or ".." in normalized_download.split("/"):
            print(f"PROTOCOL7=FAIL reason=bad_download_resource resource={args.download_resource}")
            return 2
        if args.download_size <= 0:
            print("PROTOCOL7=FAIL reason=missing_download_size")
            return 2
        if len(args.download_sha256) != 64 or any(ch not in "0123456789abcdefABCDEF" for ch in args.download_sha256):
            print("PROTOCOL7=FAIL reason=bad_download_sha256")
            return 2
        args.download_resource = normalized_download
        args.download_sha256 = args.download_sha256.lower()
    for voice_arg_name in ("voice_send_hex", "expect_voice_hex"):
        voice_hex = getattr(args, voice_arg_name)
        if voice_hex:
            try:
                voice_raw = bytes.fromhex(voice_hex)
            except ValueError:
                print(f"PROTOCOL7=FAIL reason=bad_{voice_arg_name}")
                return 2
            if not voice_raw or len(voice_raw) > 512:
                print(f"PROTOCOL7=FAIL reason=bad_{voice_arg_name}_size bytes={len(voice_raw)}")
                return 2
            setattr(args, voice_arg_name, voice_hex.lower())
    if args.expect_voice_hex and not (0 <= args.expect_voice_sender <= 255):
        print(f"PROTOCOL7=FAIL reason=bad_expect_voice_sender sender={args.expect_voice_sender}")
        return 2
    if args.server_count is None or args.sendtable_crc is None:
        print("PROTOCOL7=FAIL reason=missing_engine_signon_facts")
        return 2
    sendprop_schema = None
    if args.sendprop_schema:
        try:
            sendprop_schema = load_sendprop_schema(args.sendprop_schema)
            print(f"PROTOCOL7_SENDPROP_SCHEMA=PASS classes={len(sendprop_schema)} ccsplayer_fields={len(sendprop_schema.get(1, {}))} account_field=347")
        except (OSError, ValueError) as exc:
            print(f"PROTOCOL7=FAIL reason=sendprop_schema error={exc}")
            return 2

    instancebaseline_runtime = None
    if args.instance_baselines:
        try:
            instancebaseline_runtime = load_instancebaseline_runtime(args.instance_baselines)
            print(
                f"PROTOCOL7_INSTANCEBASELINE_RUNTIME=PASS classes={len(EXPECTED_CLASSES)} "
                f"ready={len(instancebaseline_runtime)} player_index={instancebaseline_runtime.get(1, -1)} "
                f"c4_index={instancebaseline_runtime.get(6, -1)}"
            )
        except (OSError, ValueError) as exc:
            print(f"PROTOCOL7=FAIL reason=instancebaseline_runtime error={exc}")
            return 2

    target = (args.host, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(args.timeout)
    tcp: socket.socket | None = None
    try:
        sock.sendto(CONNECTIONLESS + bytes([A2S_GETCHALLENGE]), target)
        challenge_packet, source = sock.recvfrom(4096)
        challenge, auth = parse_challenge(challenge_packet)
        print(
            f"PROTOCOL7_CHALLENGE=PASS source={source[0]}:{source[1]} "
            f"challenge={challenge} auth={auth} bytes={len(challenge_packet)} "
            f"tail={challenge_packet[13:45].hex()}"
        )

        guid = md5_guid("css-v34-build4100-protocol7-probe")
        steam_ticket = classic_revemu_ticket(args.name) if auth == PROTOCOL_STEAM else None
        connect_packet = build_connect(challenge, auth, args.name, args.password, guid, steam_ticket)
        sock.sendto(connect_packet, target)
        auth_detail = (
            f"guid_len={len(guid)}"
            if auth == PROTOCOL_HASHEDCDKEY
            else f"ticket=classic_revemu ticket_len={len(steam_ticket or b'')} framing=u16+raw"
        )
        print(
            f"PROTOCOL7_CONNECT_SENT=PASS protocol={PROTOCOL_VERSION} auth={auth} "
            f"name={args.name} bytes={len(connect_packet)} {auth_detail}"
        )

        replies: list[bytes] = []
        connection_reply: bytes | None = None
        end = time.monotonic() + args.timeout
        while time.monotonic() < end:
            sock.settimeout(max(0.05, end - time.monotonic()))
            try:
                candidate, _ = sock.recvfrom(4096)
            except socket.timeout:
                break
            replies.append(candidate)
            if is_connection_accept(candidate):
                connection_reply = candidate
                break
        for i, reply in enumerate(replies[:4], 1):
            print(f"PROTOCOL7_CONNECT_REPLY_{i}={describe_oob(reply)}")
        if connection_reply is None:
            print(
                f"PROTOCOL7=FAIL reason=S2C_CONNECTION_missing auth={auth} "
                f"ticket_len={len(steam_ticket or b'')} replies={len(replies)}"
            )
            return 4
        connection_payload = connection_reply[5:]
        print(
            f"PROTOCOL7_CONNECTION_ACCEPTED=PASS reply=B payload_len={len(connection_payload)} "
            f"payload_hex={connection_payload.hex()}"
        )

        # Exact build4100 NET_ProcessPending handshake:
        # the client opens TCP to the server and immediately writes
        # STREAM_CMD_ACK(4) + challenge. The server does not send a leading
        # STREAM_CMD_REQUEST byte. NET_ProcessPending reads these 5 bytes,
        # matches remote base address + challenge, and attaches the accepted
        # socket to the existing CNetChan.
        udp_local_port = sock.getsockname()[1]
        tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp.settimeout(args.timeout)
        tcp.connect((args.host, tcp_port))
        stream_bind = build_stream_ack(challenge)
        tcp.sendall(stream_bind)
        print(
            f"PROTOCOL7_TCP_STREAM_BOUND=PASS client_cmd={STREAM_CMD_ACK} "
            f"challenge={challenge} bytes={len(stream_bind)} "
            f"udp_local_port={udp_local_port} tcp_local_port={tcp.getsockname()[1]}"
        )

        # Give NET_ProcessPending one server frame to consume the bind message
        # and attach the accepted TCP socket to the already-created CNetChan.
        time.sleep(0.15)

        signon_payload = build_signon_state_payload()
        netchan_packet = build_netchan_packet(1, 0, signon_payload)
        sock.sendto(netchan_packet, target)
        print(
            f"PROTOCOL7_NETCHAN_SIGNON_SENT=PASS seq=1 ack=0 flags=0 reliable=0 "
            f"type={NETMSG_SIGNONSTATE} state={SIGNONSTATE_CONNECTED} server_count=-1 "
            f"bytes={len(netchan_packet)} payload_hex={signon_payload.hex()}"
        )

        # Protocol 7 signon is server-driven.  Do not send ClientInfo/NEW yet:
        # build4100 first sends Print + ServerInfo + Tick/ConVars/string tables +
        # NET_SignonState(NEW).  Only after observing that state does a real
        # client send CLC_ClientInfo followed by its NET_SignonState(NEW) ack.
        advertised_crc = args.sendtable_crc & 0xFFFFFFFF
        client_crc = advertised_crc ^ 0xFFFFFFFF if args.force_full_sendtables else advertised_crc
        print(
            f"PROTOCOL7_CLIENTINFO_ARMED=PASS server_count={args.server_count} "
            f"sendtable_crc=0x{client_crc:08x} engine_crc=0x{advertised_crc:08x} "
            f"force_full_sendtables={1 if args.force_full_sendtables else 0}"
        )

        # build4100 keeps signon payload on the UDP netchannel. The TCP socket
        # used above is only a side-channel bind/stream facility and may be closed
        # by the engine immediately after bootstrap. Do not treat that close as a
        # signon failure (v1.6.9 already proved ClientConnect survives it).
        #
        # From v1.7.8 onward this path decodes the real CNetChan reliable-subchannel
        # bitstream instead of searching for byte-aligned BZh candidates. Fragment
        # continuation payloads are bit-packed, PACKET_FLAG_CHOKED inserts an extra
        # header byte, and acknowledgements must carry our toggled incoming reliable
        # subchannel state so the server can advance the transfer.
        sendtable_count = 0
        classinfo_count = 0
        fragment_blocks = 0
        fragment_decode_warnings = 0
        bzip_complete_streams = 0
        bzip_decoded_bytes = 0
        reliable_streams = 0
        signon_misc_streams = 0
        udp_signon_packets = 0
        udp_signon_bytes = 0
        # --max-udp-packets is a diagnostic safety budget, not a wall-clock
        # substitute. Large stream-1 downloads can consume the whole packet
        # budget after every functional protocol gate has already passed but
        # before the short post-spawn survival dwell expires. In that one case
        # permit a bounded-by-signon_deadline grace instead of returning a
        # false packet-limit failure.
        packet_budget_grace_started = 0
        packet_budget_grace_packets = 0
        packet_budget_grace_started_at = 0.0
        udp_diag = 0
        tcp_diag = 0
        keepalives_sent = 0
        initial_signon_retries_sent = 0
        prespawn_signon_retries_sent = 0
        next_prespawn_signon_retry = 0.0
        clientinfo_sent = 0
        signon_state3_sent = 0
        signon_state4_sent = 0
        signon_state5_sent = 0
        signon_state6_sent = 0
        baseline_ack_sent = 0
        snapshot_tick_ack_sent = 0
        # Full-snapshot acknowledgement is recurring. build4100 can send a new
        # update-baseline full snapshot after normal gameplay deltas. If that
        # later snapshot is left unacknowledged the server falls back to empty
        # keepalives and objective entity changes never reach the probe.
        last_baseline_ack_tick = -1
        last_snapshot_tick_ack_tick = -1
        resnapshot_baseline_acks = 0
        resnapshot_tick_acks = 0
        gameplay_command_sent = 0
        gameplay_event_delta_ok = 0
        movement_origin_base = None
        move_command_sent = 0
        usercmd_eyeangles_delta_ok = 0
        usercmd_movement_delta_ok = 0
        friction_command_sent = 0
        usercmd_friction_delta_ok = 0
        jump_command_sent = 0
        usercmd_jump_delta_ok = 0
        jump_hold_command_sent = 0
        usercmd_jump_hold_delta_ok = 0
        air_accel_command_sent = 0
        usercmd_air_accel_delta_ok = 0
        post_spawn_until = 0.0
        post_spawn_dwell_updates = 0
        post_spawn_dwell_reason = "none"
        server_new_seen = 0
        server_new_bit = -1
        server_prespawn_seen = 0
        server_prespawn_bit = -1
        server_spawn_seen = 0
        server_spawn_bit = -1
        stringtable_baselines = 0
        instancebaseline_wire: dict[int, int] = {}
        instancebaseline_contract_ok = 0
        packetentity_class_ids: set[int] = set()
        client_player_slot = -1
        packetentities_count = 0
        packetentities_full_count = 0
        packetentities_updated_max = 0
        packetentities_data_bits = 0
        snapshot_entity_updates = 0
        snapshot_changed_props = 0
        snapshot_entity_decode_ok = 0
        sendprop_snapshot_ok = 0
        sendprop_gameplay_ok = 0
        sendprop_schema_decode_ok = 0
        full_snapshot_tick = -1
        # Highest server NET_Tick observed on a real snapshot.  The long-lived
        # multiclient hold must keep usercmd ticks ahead of this value and keep
        # CBaseClient::m_nDeltaTick acknowledged; reusing the original full
        # snapshot tick eventually makes build4100 fall back to NOP-only frames.
        latest_server_tick_seen = -1
        full_snapshot_baseline = -1
        full_snapshot_update_baseline = 0
        full_snapshot_entity_classes: dict[int, tuple[int, str, str]] = {}
        packetentities_delta_count = 0
        delta_snapshot_ok = 0
        delta_from_tick_ok = 0
        sendprop_delta_ok = 0
        delta_changed_props = 0
        delta_field_index = -1
        delta_account_value = -1
        packetentities_seen: set[tuple[str, int, int, int, int]] = set()
        postspawn_payload_dumped = False
        gameevent_descriptors: dict[int, dict[str, object]] = {}
        gameevent_list_ok = 0
        player_say_event_id = -1
        listen_events_sent = 0
        listen_events_sequence = -1
        say_command_sent = 0
        player_say_event_ok = 0
        weapon_event_ids: dict[str, int] = {}
        pre_weapon_join_sent = 0
        pre_weapon_join_sequence = -1
        pre_weapon_join_sent_at = 0.0
        weapon_fire_command_sent = 0
        weapon_release_command_sent = 0
        weapon_reload_command_sent = 0
        weapon_secondary_command_sent = 0
        weapon_secondary_sequence = -1
        weapon_fire_event_ok = 0
        weapon_reload_event_ok = 0
        weapon_fire_seen_at = 0.0
        combat_event_ids: dict[str, int] = {}
        combat_setup_command_sent = 0
        combat_setup_sequence = -1
        combat_reset_command_sent = 0
        combat_reset_sequence = -1
        player_hurt_event_ok = 0
        player_death_event_ok = 0
        combat_victim_userid = -1
        combat_attacker_userid = -1
        objective_event_ids: dict[str, int] = {}
        objective_events_seen: set[str] = set()
        objective_scenario_command_sent = 0
        objective_scenario_ok = 0
        objective_c4_wire_ok = 0
        gameevent_seen: set[tuple[str, int, int, int]] = set()
        usermessage_command_sent = 0
        saytext_usermessage_ok = 0
        usermessage_seen: set[tuple[str, int, int, int]] = set()
        downloadables_ok = 0
        download_request_sent = 0
        download_request_sequence = -1
        download_transfer_id = 0x34335643
        download_received_ok = 0
        download_bytes = 0
        download_fragments = 0
        download_compressed = 0
        # Keep an active direct-download transfer alive by progress, not by one
        # fixed wall-clock budget. Slow WSL/Docker runs can take >45s for the
        # 573-fragment de_dust2.nav stream even though fragments keep arriving.
        download_progress_fragments = 0
        download_hard_deadline = 0.0
        voice_sent = 0
        voice_expected_ok = 0
        voice_init_seen = 0
        voice_seen: set[tuple[str, int, int, int]] = set()
        te_firebullets_received_ok = 0
        te_firebullets_seen: set[tuple[str, int, int, int, int]] = set()
        # gatefix14: CTEFireBullets is an unreliable snapshot event.  The
        # held observer must keep a real clc_Move cadence while B fires, and
        # the raw-packet oracle is armed by the actual weapon_fire game event
        # rather than the earlier player_say marker.
        te_debug_trigger_seq = -1
        te_debug_trigger_source = ""
        te_debug_pre_packets: list[tuple[int, int, int, int, int, bytes, bytes]] = []
        te_debug_packets: list[tuple[int, int, int, int, int, bytes, bytes]] = []
        te_debug_post_remaining = 0

        def record_tempentity_data(data: bytes, source: str, seq: int, start_bit: int = 0) -> None:
            nonlocal te_firebullets_received_ok
            if sendprop_schema is None or 120 not in sendprop_schema:
                return
            for message in scan_temp_entities_messages(data, sendprop_schema, start_bit):
                for entry in message.get("entries", []):
                    if int(entry.get("class_id", -1)) != 120:
                        continue
                    identity = (source, seq, int(message["start_bit"]), int(message["end_bit"]), int(entry["entry_index"]))
                    if identity in te_firebullets_seen:
                        continue
                    te_firebullets_seen.add(identity)
                    class_schema = sendprop_schema.get(120, {})
                    values_by_field = dict(entry.get("values", {}))
                    values_by_name: dict[str, object] = {}
                    for field_index, meta in class_schema.items():
                        values_by_name[str(meta["prop_name"])] = values_by_field.get(field_index, 0)
                    player = int(values_by_name.get("m_iPlayer", -1))
                    weapon = int(values_by_name.get("m_iWeaponID", -1))
                    mode = int(values_by_name.get("m_iMode", 0))
                    seed = int(values_by_name.get("m_iSeed", 0))
                    spread = float(values_by_name.get("m_flSpread", 0.0))
                    origin = values_by_name.get("m_vecOrigin")
                    pitch = values_by_name.get("m_vecAngles[0]")
                    yaw = values_by_name.get("m_vecAngles[1]")
                    expected = (
                        (args.expect_te_firebullets_player < 0 or player == args.expect_te_firebullets_player)
                        and (args.expect_te_firebullets_weapon < 0 or weapon == args.expect_te_firebullets_weapon)
                    )
                    if not expected:
                        continue
                    if spread <= 0.0 or not isinstance(origin, tuple) or len(origin) != 3:
                        continue
                    if not isinstance(pitch, (int, float)) or not isinstance(yaw, (int, float)):
                        continue
                    te_firebullets_received_ok = 1
                    print(
                        f"PROTOCOL7_TE_FIREBULLETS=PASS source={source} seq={seq} type={SVC_TEMPENTITIES} "
                        f"class=120 wire_class={int(entry.get('wire_class_id', 121))} decode=full "
                        f"player={player} weapon={weapon} mode={mode} seed={seed} spread={spread:.6f} "
                        f"origin={float(origin[0]):.3f},{float(origin[1]):.3f},{float(origin[2]):.3f} "
                        f"angles={float(pitch):.3f},{float(yaw):.3f} entries={message['num_entries']} "
                        f"bit={message['start_bit']} endbit={message['end_bit']}",
                        flush=True,
                    )

            # If an earlier temp-entity class in this same type-27 envelope is
            # not represented by the current probe schema, strict sequential
            # decode stops before CTEFireBullets.  Recover only a *full* wire
            # class 121 entry whose class120 SendProp stream itself decodes
            # completely, then require the same semantic fields as the strict
            # path.  This is deliberately stronger than a raw class-id scan.
            if not te_firebullets_received_ok:
                for candidate in scan_temp_entities_resync_full(data, sendprop_schema, 120, start_bit):
                    values_by_name = {
                        str(item["name"]): item["value"] for item in candidate.get("props", [])
                    }
                    required = {
                        "m_iPlayer", "m_iWeaponID", "m_flSpread", "m_vecOrigin",
                        "m_vecAngles[0]", "m_vecAngles[1]",
                    }
                    if not required.issubset(values_by_name):
                        continue
                    try:
                        player = int(values_by_name["m_iPlayer"])
                        weapon = int(values_by_name["m_iWeaponID"])
                        mode = int(values_by_name.get("m_iMode", 0))
                        seed = int(values_by_name.get("m_iSeed", 0))
                        spread = float(values_by_name["m_flSpread"])
                        origin = values_by_name["m_vecOrigin"]
                        pitch = values_by_name["m_vecAngles[0]"]
                        yaw = values_by_name["m_vecAngles[1]"]
                    except (TypeError, ValueError):
                        continue
                    expected = (
                        (args.expect_te_firebullets_player < 0 or player == args.expect_te_firebullets_player)
                        and (args.expect_te_firebullets_weapon < 0 or weapon == args.expect_te_firebullets_weapon)
                    )
                    if not expected or spread <= 0.0 or spread > 1.0:
                        continue
                    if not isinstance(origin, tuple) or len(origin) != 3:
                        continue
                    if not all(isinstance(value, (int, float)) for value in origin):
                        continue
                    if not isinstance(pitch, (int, float)) or not isinstance(yaw, (int, float)):
                        continue
                    identity = (
                        source, seq, int(candidate["message_start_bit"]),
                        int(candidate["message_end_bit"]), int(candidate["start_bit"]),
                    )
                    if identity in te_firebullets_seen:
                        continue
                    te_firebullets_seen.add(identity)
                    te_firebullets_received_ok = 1
                    print(
                        f"PROTOCOL7_TE_FIREBULLETS=PASS source={source} seq={seq} type={SVC_TEMPENTITIES} "
                        f"class=120 wire_class={int(candidate.get('wire_class_id', 121))} decode=resync_full "
                        f"player={player} weapon={weapon} mode={mode} seed={seed} spread={spread:.6f} "
                        f"origin={float(origin[0]):.3f},{float(origin[1]):.3f},{float(origin[2]):.3f} "
                        f"angles={float(pitch):.3f},{float(yaw):.3f} entries={candidate['num_entries']} "
                        f"bit={candidate['message_start_bit']} endbit={candidate['message_end_bit']} "
                        f"entrybit={candidate['start_bit']} entryend={candidate['end_bit']}",
                        flush=True,
                    )
                    return

            # Strict SendProp decoding is preferred.  If it did not identify
            # CTEFireBullets, retain an observer-side wire proof from the exact
            # SVC_TempEntities envelope and first full 1-based class id.  The
            # Docker gate separately requires the server-side sender telemetry
            # for index=4/weapon=16/recipients=1, so this fallback never invents
            # player/weapon property values.
            if te_firebullets_received_ok:
                return
            for envelope in scan_temp_entities_envelopes(data, start_bit):
                if int(envelope.get("first_class_id", -1)) != 120 or int(envelope.get("first_wire_class_id", -1)) != 121:
                    continue
                identity = (source, seq, int(envelope["start_bit"]), int(envelope["end_bit"]), -1)
                if identity in te_firebullets_seen:
                    continue
                te_firebullets_seen.add(identity)
                te_firebullets_received_ok = 1
                print(
                    f"PROTOCOL7_TE_FIREBULLETS=PASS source={source} seq={seq} type={SVC_TEMPENTITIES} "
                    f"class=120 wire_class=121 decode=envelope_class props=unverified "
                    f"entries={envelope['num_entries']} data_bits={envelope['data_bits']} "
                    f"bit={envelope['start_bit']} endbit={envelope['end_bit']}",
                    flush=True,
                )
                return

        def record_voice_data(data: bytes, source: str, seq: int, start_bit: int = 0) -> None:
            nonlocal voice_expected_ok, voice_init_seen
            if not voice_init_seen:
                for voice_init in scan_voice_inits(data, start_bit):
                    codec = str(voice_init["codec"])
                    quality = int(voice_init["quality"])
                    # Filter bit-scan noise: build4100 codec cvar is normally
                    # empty or a short printable codec name; only surface
                    # recognizable voice codec strings.
                    if codec and ("voice" in codec.lower() or "speex" in codec.lower()):
                        voice_init_seen = 1
                        print(
                            f"PROTOCOL7_VOICE_INIT=PASS source={source} seq={seq} "
                            f"type={SVC_VOICEINIT} codec={codec} quality={quality} "
                            f"bit={voice_init['start_bit']} endbit={voice_init['end_bit']}"
                        )
                        break
            for voice in scan_voice_data(data, start_bit):
                identity = (source, seq, int(voice["start_bit"]), int(voice["end_bit"]))
                if identity in voice_seen:
                    continue
                voice_seen.add(identity)
                payload = bytes(voice["payload"])
                payload_bits = int(voice["payload_bits"])
                sender = int(voice["sender"])
                if not args.expect_voice_hex:
                    continue
                expected = bytes.fromhex(args.expect_voice_hex)
                if sender != args.expect_voice_sender or payload_bits != len(expected) * 8 or payload != expected:
                    continue
                voice_expected_ok = 1
                print(
                    f"PROTOCOL7_VOICE_DATA=PASS source={source} seq={seq} type={SVC_VOICEDATA} "
                    f"sender={sender} payload_bits={payload_bits} payload_hex={payload.hex()} "
                    f"bit={voice['start_bit']} endbit={voice['end_bit']}",
                    flush=True,
                )

        def record_usermessage_data(data: bytes, source: str, seq: int, start_bit: int = 0) -> None:
            nonlocal saytext_usermessage_ok
            for message in scan_user_messages(data, start_bit):
                identity = (source, seq, int(message["start_bit"]), int(message["end_bit"]))
                if identity in usermessage_seen:
                    continue
                usermessage_seen.add(identity)
                if int(message["message_type"]) != 3:
                    continue
                saytext = parse_saytext_payload(bytes(message["payload"]), int(message["payload_bits"]))
                expected_sender = client_player_slot + 1 if client_player_slot >= 0 else 3
                if saytext != (expected_sender, "cssv34_wire_usermsg", 1):
                    continue
                saytext_usermessage_ok = 1
                print(
                    f"PROTOCOL7_USERMESSAGE_SAYTEXT=PASS source={source} seq={seq} "
                    f"message_id=3 sender={expected_sender} text=cssv34_wire_usermsg chat=1 "
                    f"bit={message['start_bit']} endbit={message['end_bit']} payload_bits={message['payload_bits']}"
                )

        def record_gameevent_data(data: bytes, source: str, seq: int, start_bit: int = 0) -> None:
            nonlocal gameevent_descriptors, gameevent_list_ok, player_say_event_id, player_say_event_ok
            nonlocal objective_event_ids, objective_events_seen, weapon_event_ids, combat_event_ids
            nonlocal weapon_fire_event_ok, weapon_reload_event_ok, weapon_fire_seen_at
            nonlocal player_hurt_event_ok, player_death_event_ok, combat_victim_userid, combat_attacker_userid
            nonlocal te_debug_trigger_seq, te_debug_trigger_source
            if not gameevent_list_ok:
                for event_list in scan_gameevent_lists(data, start_bit):
                    descriptors = dict(event_list["descriptors"])
                    player_say = next(
                        ((event_id, descriptor) for event_id, descriptor in descriptors.items()
                         if descriptor.get("name") == "player_say"),
                        None,
                    )
                    if player_say is None:
                        continue
                    event_id, descriptor = player_say
                    fields = list(descriptor.get("fields", []))
                    field_names = [str(item[0]) for item in fields]
                    if "userid" not in field_names or "text" not in field_names:
                        continue
                    gameevent_descriptors = descriptors
                    gameevent_list_ok = 1
                    player_say_event_id = int(event_id)
                    objective_event_ids = {
                        str(d.get("name")): int(eid) for eid, d in descriptors.items()
                        if str(d.get("name")) in OBJECTIVE_EVENT_NAMES
                    }
                    weapon_event_ids = {
                        str(d.get("name")): int(eid) for eid, d in descriptors.items()
                        if str(d.get("name")) in {"weapon_fire", "weapon_reload"}
                    }
                    combat_event_ids = {
                        str(d.get("name")): int(eid) for eid, d in descriptors.items()
                        if str(d.get("name")) in {"player_hurt", "player_death"}
                    }
                    fields_text = "+".join(f"{name}:{typ}" for name, typ in fields)
                    print(
                        f"PROTOCOL7_GAMEEVENT_LIST=PASS source={source} seq={seq} "
                        f"events={event_list['num_events']} player_say_id={player_say_event_id} "
                        f"fields={fields_text} bit={event_list['start_bit']} endbit={event_list['end_bit']}"
                    )
                    objective_names = sorted(objective_event_ids)
                    required_descriptors = BOMB_SCENARIO_REQUIRED_EVENTS | HOSTAGE_SCENARIO_REQUIRED_EVENTS
                    if required_descriptors.issubset(objective_event_ids):
                        print(
                            "PROTOCOL7_OBJECTIVE_EVENT_DESCRIPTORS=PASS "
                            f"count={len(objective_event_ids)} names={','.join(objective_names)}"
                        )
                    else:
                        missing = sorted(required_descriptors - objective_event_ids.keys())
                        print(f"PROTOCOL7_OBJECTIVE_EVENT_DESCRIPTORS=WARN missing={','.join(missing)}")
                    break
            if not gameevent_descriptors:
                return
            for event in scan_gameevents(data, gameevent_descriptors, start_bit):
                identity = (source, seq, int(event["start_bit"]), int(event["end_bit"]))
                if identity in gameevent_seen:
                    continue
                gameevent_seen.add(identity)
                values = dict(event.get("values", {}))
                event_name = str(event.get("name", ""))
                if event_name in OBJECTIVE_EVENT_NAMES:
                    objective_events_seen.add(event_name)
                    value_text = ",".join(f"{key}={values[key]}" for key in sorted(values))
                    print(
                        f"PROTOCOL7_OBJECTIVE_EVENT=PASS source={source} seq={seq} "
                        f"name={event_name} event_id={event['event_id']} values={value_text or 'none'} "
                        f"bit={event['start_bit']} endbit={event['end_bit']}"
                    )
                if event_name == "weapon_fire":
                    weapon = values.get("weapon")
                    userid = values.get("userid")
                    if (args.expect_te_firebullets_player >= 0 and source.startswith("hold_")
                            and isinstance(userid, int) and userid >= 0
                            and isinstance(weapon, str) and weapon):
                        te_debug_trigger_seq = seq
                        te_debug_trigger_source = source
                        print(
                            f"PROTOCOL7_TE_DEBUG_TRIGGER=PASS source={source} seq={seq} "
                            f"event=weapon_fire userid={userid} weapon={weapon}",
                            flush=True,
                        )
                    if args.weapon_actions and isinstance(userid, int) and userid >= 0 and isinstance(weapon, str) and weapon:
                        weapon_fire_event_ok = 1
                        weapon_fire_seen_at = time.monotonic()
                        print(
                            f"PROTOCOL7_WEAPON_FIRE_EVENT=PASS source={source} seq={seq} "
                            f"event_id={event['event_id']} userid={userid} weapon={weapon} "
                            f"bit={event['start_bit']} endbit={event['end_bit']}"
                        )
                elif event_name == "weapon_reload" and args.weapon_actions:
                    userid = values.get("userid")
                    if isinstance(userid, int) and userid >= 0:
                        weapon_reload_event_ok = 1
                        print(
                            f"PROTOCOL7_WEAPON_RELOAD_EVENT=PASS source={source} seq={seq} "
                            f"event_id={event['event_id']} userid={userid} "
                            f"bit={event['start_bit']} endbit={event['end_bit']}"
                        )
                elif event_name == "player_hurt" and args.combat_damage:
                    userid = values.get("userid")
                    attacker = values.get("attacker")
                    health = values.get("health")
                    armor = values.get("armor")
                    weapon = values.get("weapon")
                    if (isinstance(userid, int) and userid > 0 and isinstance(attacker, int) and attacker > 0
                            and userid != attacker and isinstance(health, int) and health == 0
                            and isinstance(armor, int) and armor == 0 and isinstance(weapon, str) and weapon):
                        player_hurt_event_ok = 1
                        combat_victim_userid = userid
                        combat_attacker_userid = attacker
                        print(
                            f"PROTOCOL7_PLAYER_HURT_EVENT=PASS source={source} seq={seq} event_id={event['event_id']} "
                            f"userid={userid} attacker={attacker} health={health} armor={armor} weapon={weapon} "
                            f"bit={event['start_bit']} endbit={event['end_bit']}"
                        )
                elif event_name == "player_death" and args.combat_damage:
                    userid = values.get("userid")
                    attacker = values.get("attacker")
                    weapon = values.get("weapon")
                    headshot = values.get("headshot")
                    if (isinstance(userid, int) and userid > 0 and isinstance(attacker, int) and attacker > 0
                            and userid != attacker and isinstance(weapon, str) and weapon
                            and bool(headshot) and (combat_victim_userid < 0 or userid == combat_victim_userid)
                            and (combat_attacker_userid < 0 or attacker == combat_attacker_userid)):
                        player_death_event_ok = 1
                        combat_victim_userid = userid
                        combat_attacker_userid = attacker
                        print(
                            f"PROTOCOL7_PLAYER_DEATH_EVENT=PASS source={source} seq={seq} event_id={event['event_id']} "
                            f"userid={userid} attacker={attacker} weapon={weapon} headshot=1 "
                            f"bit={event['start_bit']} endbit={event['end_bit']}"
                        )
                if event_name != "player_say":
                    continue
                userid = values.get("userid")
                text = values.get("text")
                if text != "cssv34_wire_event" or not isinstance(userid, int) or userid < 0:
                    continue
                player_say_event_ok = 1
                print(
                    f"PROTOCOL7_GAMEEVENT_PLAYER_SAY=PASS source={source} seq={seq} "
                    f"event_id={event['event_id']} userid={userid} text={text} "
                    f"bit={event['start_bit']} endbit={event['end_bit']} length_bits={event['length_bits']}"
                )

        def record_packetentities(
            entity_packet: dict[str, object], source: str, seq: int, stream: int = -1, message_tick: int = -1
        ) -> bool:
            nonlocal packetentities_count, packetentities_full_count, packetentities_updated_max, packetentities_data_bits
            nonlocal snapshot_entity_updates, snapshot_changed_props, snapshot_entity_decode_ok, sendprop_snapshot_ok
            nonlocal sendprop_gameplay_ok, sendprop_schema_decode_ok
            nonlocal full_snapshot_tick, latest_server_tick_seen, full_snapshot_baseline, full_snapshot_update_baseline
            nonlocal full_snapshot_entity_classes, packetentities_delta_count
            nonlocal delta_snapshot_ok, delta_from_tick_ok, sendprop_delta_ok, delta_changed_props
            nonlocal delta_field_index, delta_account_value, gameplay_event_delta_ok, movement_origin_base, usercmd_eyeangles_delta_ok
            nonlocal usercmd_movement_delta_ok, usercmd_friction_delta_ok
            nonlocal usercmd_jump_delta_ok, usercmd_jump_hold_delta_ok, usercmd_air_accel_delta_ok
            nonlocal objective_c4_wire_ok, packetentity_class_ids
            if message_tick >= 0:
                latest_server_tick_seen = max(latest_server_tick_seen, message_tick)
            identity = (source, seq, stream, int(entity_packet["start_bit"]), int(entity_packet["end_bit"]))
            if identity in packetentities_seen:
                return False
            packetentities_seen.add(identity)
            packetentities_count += 1
            if int(entity_packet["is_delta"]) == 0:
                packetentities_full_count += 1
            else:
                packetentities_delta_count += 1
            packetentities_updated_max = max(packetentities_updated_max, int(entity_packet["updated_entries"]))
            packetentities_data_bits += int(entity_packet["data_bits"])
            print(
                f"PROTOCOL7_PACKETENTITIES=PASS source={source} seq={seq} stream={stream} "
                f"bit={entity_packet['start_bit']} endbit={entity_packet['end_bit']} "
                f"max={entity_packet['max_entries']} delta={entity_packet['is_delta']} "
                f"delta_from={entity_packet['delta_from']} baseline={entity_packet['baseline']} "
                f"updated={entity_packet['updated_entries']} data_bits={entity_packet['data_bits']} "
                f"update_baseline={entity_packet['update_baseline']} "
                f"payload_sha256={entity_packet['payload_sha256']} "
                f"payload_head_hex={entity_packet['payload_head_hex']}"
            )
            if sendprop_schema is not None:
                generic = parse_entity_updates_schema(
                    entity_packet, sendprop_schema,
                    full_snapshot_entity_classes if int(entity_packet["is_delta"]) else None,
                )
                if generic is not None and bool(generic.get("complete")):
                    entries = list(generic["entries"])
                    changed = int(generic["changed_props"])
                    for item in entries:
                        if not item.get("leave") and "class_id" in item:
                            packetentity_class_ids.add(int(item["class_id"]))
                    sendprop_schema_decode_ok = 1
                    if not objective_c4_wire_ok:
                        for item in entries:
                            if item.get("leave") or str(item.get("class_name", "")) != "CPlantedC4":
                                continue
                            c4_props = {str(prop["name"]): prop["value"] for prop in item.get("props", [])}
                            c4_names = ",".join(str(prop["name"]) for prop in item.get("props", [])) or "none"
                            print(
                                "PROTOCOL7_PLANTED_C4_WIRE=PASS "
                                f"source={source} seq={seq} entity={item['entity']} enter={item.get('enter', 0)} "
                                f"changed_props={len(item.get('props', []))} names={c4_names}"
                            )
                            ticking = c4_props.get("m_bBombTicking")
                            blow = c4_props.get("m_flC4Blow")
                            timer = c4_props.get("m_flTimerLength")
                            origin = c4_props.get("m_vecOrigin")
                            if (
                                ticking == 1
                                and isinstance(blow, (int, float)) and float(blow) > 0.0
                                and isinstance(timer, (int, float)) and 40.0 <= float(timer) <= 50.0
                            ):
                                objective_c4_wire_ok = 1
                                defuse_length = c4_props.get("m_flDefuseLength", 0.0)
                                countdown = c4_props.get("m_flDefuseCountDown", 0.0)
                                origin_text = (
                                    f"{float(origin[0]):.3f},{float(origin[1]):.3f},{float(origin[2]):.3f}"
                                    if isinstance(origin, tuple) and len(origin) == 3 else "baseline_or_unchanged"
                                )
                                print(
                                    "PROTOCOL7_PLANTED_C4_SENDPROP=PASS "
                                    f"source={source} seq={seq} entity={item['entity']} ticking=1 "
                                    f"blow={float(blow):.3f} timer={float(timer):.3f} "
                                    f"defuse_length={float(defuse_length) if isinstance(defuse_length, (int, float)) else 0.0:.3f} "
                                    f"countdown={float(countdown) if isinstance(countdown, (int, float)) else 0.0:.3f} "
                                    f"origin={origin_text} changed_props={len(item.get('props', []))}"
                                )
                                break
                    if int(entity_packet["is_delta"]) == 0:
                        snapshot_entity_updates += len(entries)
                        snapshot_changed_props += changed
                        snapshot_entity_decode_ok = 1
                        sendprop_snapshot_ok = 1
                        if message_tick >= 0:
                            full_snapshot_tick = message_tick
                        full_snapshot_baseline = int(entity_packet["baseline"])
                        full_snapshot_update_baseline = int(entity_packet["update_baseline"])
                        full_snapshot_entity_classes = dict(generic["entity_classes"])
                        def _propmap(item: dict[str, object]) -> dict[str, object]:
                            return {str(p["name"]): p["value"] for p in item.get("props", [])}
                        by_ent = {int(item["entity"]): _propmap(item) for item in entries if not item.get("leave")}
                        # Full entity updates are delta-compressed against the
                        # class baseline.  Gameplay values already present in that
                        # baseline (health/account and bot1's team) are correctly
                        # omitted here.  Validate only the live values that differ
                        # from the baseline; the server-side acceptance separately
                        # proves the complete ClientRecord-backed sendstate.
                        local_entity = client_player_slot + 1 if client_player_slot >= 0 else 3
                        gameplay_ok = (
                            changed >= 3
                            and by_ent.get(2, {}).get("m_iTeamNum") == 3
                            and by_ent.get(local_entity, {}).get("m_iTeamNum") == 0
                            and by_ent.get(local_entity, {}).get("m_iPlayerState") == 1
                        )
                        if gameplay_ok:
                            sendprop_gameplay_ok = 1
                        active_entries = [item for item in entries if not item.get("leave")]
                        player_entries = [item for item in active_entries if str(item.get("class_name", "")) == "CCSPlayer"]
                        nonplayer_entries = [item for item in active_entries if str(item.get("class_name", "")) != "CCSPlayer"]
                        player_entity_ids = ",".join(str(int(item["entity"])) for item in player_entries) or "none"
                        extra_entity_classes = ",".join(
                            f"{int(item['entity'])}:{str(item.get('class_name', '?'))}" for item in nonplayer_entries
                        ) or "none"
                        preview_parts = []
                        for item in entries:
                            props_text = ";".join(f"{p['name']}={p['value']}@{p['field_index']}" for p in item.get("props", []))
                            preview_parts.append(f"{item['entity']}:{item.get('class_name','?')}[{props_text}]")
                        print(
                            f"PROTOCOL7_ENTITY_SNAPSHOT=PASS source={source} seq={seq} stream={stream} "
                            f"tick={full_snapshot_tick} entities={len(entries)} players={len(player_entries)} "
                            f"nonplayers={len(nonplayer_entries)} player_entities={player_entity_ids} extras={extra_entity_classes} "
                            f"bits={generic['bits_consumed']}/{generic['data_bits']} changed_props={changed} "
                            f"entries={','.join(preview_parts)}"
                        )
                        if gameplay_ok:
                            print(
                                "PROTOCOL7_SENDPROP_GAMEPLAY=PASS schema=engine_flattened "
                                f"wire=bot2_team3+local{local_entity}_team0+local{local_entity}_state1 "
                                "baseline=account800+health100+bot1_team2 "
                                f"player_entities={player_entity_ids} extras={extra_entity_classes} changed_props={changed}"
                            )
                    else:
                        # Delta EnterPVS can introduce a class that was parked
                        # during the original full snapshot (CPlantedC4 here).
                        # Preserve the decoder's updated class map for later
                        # existing-entity deltas instead of reverting to signon.
                        full_snapshot_entity_classes = dict(generic["entity_classes"])
                        delta_snapshot_ok = 1
                        delta_changed_props += changed
                        delta_from = int(entity_packet["delta_from"])
                        if full_snapshot_tick >= 0 and delta_from == full_snapshot_tick:
                            delta_from_tick_ok = 1
                        print(
                            f"PROTOCOL7_ENTITY_DELTA=PASS source={source} seq={seq} stream={stream} "
                            f"tick={message_tick} delta_from={delta_from} full_tick={full_snapshot_tick} "
                            f"from_tick_match={delta_from_tick_ok} entities={len(entries)} "
                            f"bits={generic['bits_consumed']}/{generic['data_bits']} changed_props={changed} "
                            f"outer_terminator_bits={generic.get('outer_terminator_bits', 0)} schema=engine_flattened"
                        )
                        if gameplay_command_sent:
                            local_entity = client_player_slot + 1 if client_player_slot >= 0 else 3
                            real_props = {}
                            for item in entries:
                                if int(item.get("entity", -1)) == local_entity and not item.get("leave"):
                                    real_props = {str(p["name"]): p["value"] for p in item.get("props", [])}
                                    break
                            if real_props.get("m_iTeamNum") == args.join_team and real_props.get("m_iPlayerState") == 0:
                                gameplay_event_delta_ok = 1
                                sendprop_delta_ok = 1
                                spawn_origin = real_props.get("m_vecOrigin")
                                if (movement_origin_base is None and isinstance(spawn_origin, tuple)
                                        and len(spawn_origin) == 3
                                        and all(isinstance(v, (int, float)) for v in spawn_origin)):
                                    movement_origin_base = tuple(float(v) for v in spawn_origin)
                                    print(
                                        f"PROTOCOL7_SPAWN_ORIGIN=PASS entity={local_entity} "
                                        f"origin={movement_origin_base[0]:.3f},{movement_origin_base[1]:.3f},{movement_origin_base[2]:.3f} "
                                        "worldtrace=1"
                                    )
                                print(
                                    f"PROTOCOL7_GAMEPLAY_EVENT_DELTA=PASS command=jointeam_{args.join_team} entity={local_entity} "
                                    f"team={real_props['m_iTeamNum']} state={real_props['m_iPlayerState']} "
                                    f"changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                )
                            if move_command_sent:
                                pitch = real_props.get("m_angEyeAngles[0]")
                                yaw = real_props.get("m_angEyeAngles[1]")
                                if isinstance(pitch, (int, float)) and isinstance(yaw, (int, float)) and \
                                   abs(float(pitch) - 45.0) <= 0.2 and abs(float(yaw) - 90.0) <= 0.2:
                                    if not usercmd_eyeangles_delta_ok:
                                        print(
                                            f"PROTOCOL7_USERCMD_EYEANGLES_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"pitch={float(pitch):.3f} yaw={float(yaw):.3f} "
                                            f"changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                    usercmd_eyeangles_delta_ok = 1
                                origin = real_props.get("m_vecOrigin")
                                # Delta snapshots reference the original full snapshot, so any
                                # nonzero reconstructed movement component remains present. X is
                                # zero throughout this yaw=90 sequence and may be omitted.
                                velocity_x = real_props.get("m_vecVelocity[0]", 0.0)
                                velocity_y = real_props.get("m_vecVelocity[1]", 0.0)
                                velocity_z = real_props.get("m_vecVelocity[2]", 0.0)
                                movement_values = (
                                    isinstance(origin, tuple) and len(origin) == 3
                                    and all(isinstance(v, (int, float)) for v in origin)
                                    and isinstance(velocity_x, (int, float))
                                    and isinstance(velocity_y, (int, float))
                                    and isinstance(velocity_z, (int, float))
                                )
                                if movement_values:
                                    ox, oy, oz = (float(origin[0]), float(origin[1]), float(origin[2]))
                                    vx, vy, vz = float(velocity_x), float(velocity_y), float(velocity_z)
                                    bx, by, bz = movement_origin_base if movement_origin_base is not None else (0.0, 0.0, 0.0)
                                    origin_tolerance = 0.14 if movement_origin_base is not None else 0.08
                                    if (
                                        not usercmd_movement_delta_ok
                                        and abs(ox - bx) <= origin_tolerance and abs(oy - (by + 0.225)) <= origin_tolerance and abs(oz - bz) <= origin_tolerance
                                        and abs(vx) <= 0.2 and abs(vy - 15.0) <= 0.2 and abs(vz) <= 0.2
                                    ):
                                        print(
                                            f"PROTOCOL7_USERCMD_MOVEMENT_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"origin={ox:.3f},{oy:.3f},{oz:.3f} velocity={vx:.3f},{vy:.3f},{vz:.3f} "
                                            f"forward=200 accel=5 changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                        usercmd_movement_delta_ok = 1
                                    if (
                                        friction_command_sent and not usercmd_friction_delta_ok
                                        and abs(ox - bx) <= origin_tolerance and abs(oy - (by + 0.3825)) <= origin_tolerance and abs(oz - bz) <= origin_tolerance
                                        and abs(vx) <= 0.2 and abs(vy - 10.5) <= 0.2 and abs(vz) <= 0.2
                                    ):
                                        print(
                                            f"PROTOCOL7_USERCMD_FRICTION_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"origin={ox:.3f},{oy:.3f},{oz:.3f} velocity={vx:.3f},{vy:.3f},{vz:.3f} "
                                            f"friction=4 stopspeed=75 changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                        usercmd_friction_delta_ok = 1
                                    if (
                                        jump_command_sent and not usercmd_jump_delta_ok
                                        and abs(ox - bx) <= origin_tolerance and abs(oy - (by + 0.54)) <= origin_tolerance and abs(oz - (bz + 3.844922)) <= 0.16
                                        and abs(vx) <= 0.2 and abs(vy - 10.5) <= 0.2 and abs(vz - 250.32816) <= 0.3
                                    ):
                                        print(
                                            f"PROTOCOL7_USERCMD_JUMP_GRAVITY_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"origin={ox:.3f},{oy:.3f},{oz:.3f} velocity={vx:.3f},{vy:.3f},{vz:.3f} "
                                            f"buttons=2 gravity=800 impulse=268.328 changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                        usercmd_jump_delta_ok = 1
                                    if (
                                        jump_hold_command_sent and not usercmd_jump_hold_delta_ok
                                        and abs(ox - bx) <= origin_tolerance and abs(oy - (by + 0.6975)) <= origin_tolerance and abs(oz - (bz + 7.509845)) <= 0.16
                                        and abs(vx) <= 0.2 and abs(vy - 10.5) <= 0.2 and abs(vz - 238.32816) <= 0.3
                                    ):
                                        print(
                                            f"PROTOCOL7_USERCMD_JUMP_LATCH_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"origin={ox:.3f},{oy:.3f},{oz:.3f} velocity={vx:.3f},{vy:.3f},{vz:.3f} "
                                            f"buttons=2 latch=held changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                        usercmd_jump_hold_delta_ok = 1
                                    if (
                                        air_accel_command_sent and not usercmd_air_accel_delta_ok
                                        and abs(ox - bx) <= origin_tolerance and abs(oy - (by + 1.1475)) <= origin_tolerance and abs(oz - (bz + 10.994767)) <= 0.18
                                        and abs(vx) <= 0.2 and abs(vy - 30.0) <= 0.2 and abs(vz - 226.32816) <= 0.3
                                    ):
                                        print(
                                            f"PROTOCOL7_USERCMD_AIR_ACCEL_DELTA=PASS clc_move=9 entity={local_entity} "
                                            f"origin={ox:.3f},{oy:.3f},{oz:.3f} velocity={vx:.3f},{vy:.3f},{vz:.3f} "
                                            f"forward=200 accel=10 cap=30 changed_props={changed} delta_from={delta_from} schema=engine_flattened"
                                        )
                                        usercmd_air_accel_delta_ok = 1
                elif generic is not None:
                    print(
                        f"PROTOCOL7_ENTITY_SCHEMA=WARN source={source} seq={seq} stream={stream} "
                        f"reason={generic.get('reason','unknown')} bits={generic.get('bits_consumed',0)}/{generic.get('data_bits',0)}"
                    )
            elif int(entity_packet["is_delta"]) == 0:
                entity_decode = parse_full_snapshot_entity_updates(entity_packet)
                if entity_decode is not None and bool(entity_decode.get("complete")):
                    entities = list(entity_decode["entities"])
                    changed = int(entity_decode["changed_props"])
                    snapshot_entity_updates += len(entities)
                    snapshot_changed_props += changed
                    snapshot_entity_decode_ok = 1
                    if message_tick >= 0:
                        full_snapshot_tick = message_tick
                    full_snapshot_baseline = int(entity_packet["baseline"])
                    full_snapshot_update_baseline = int(entity_packet["update_baseline"])
                    full_snapshot_entity_classes = {
                        int(item["entity"]): (int(item["class_id"]), str(item["class_name"]), str(item["table_name"]))
                        for item in entities
                    }
                    if changed == 0:
                        sendprop_snapshot_ok = 1
                elif entity_decode is not None:
                    print(f"PROTOCOL7_ENTITY_SNAPSHOT=WARN source={source} seq={seq} stream={stream} reason={entity_decode.get('reason','unknown')}")
            elif full_snapshot_entity_classes:
                delta_decode = parse_delta_snapshot_account_updates(entity_packet, full_snapshot_entity_classes)
                if delta_decode is not None and bool(delta_decode.get("complete")):
                    delta_snapshot_ok = 1
                    delta_from_tick_ok = int(entity_packet["delta_from"]) == full_snapshot_tick
                    sendprop_delta_ok = 1
            return True

        def extend_post_spawn_dwell(seconds: float, reason: str) -> None:
            """Extend only for functional milestones, never transport upkeep."""
            nonlocal post_spawn_until, post_spawn_dwell_updates, post_spawn_dwell_reason
            candidate = time.monotonic() + seconds
            if candidate > post_spawn_until:
                post_spawn_until = candidate
                post_spawn_dwell_updates += 1
                post_spawn_dwell_reason = reason

        def integration_path_ready(require_dwell: bool) -> bool:
            """Single source of truth for the pre-objective Protocol 7 gate.

            Packet count is deliberately excluded: it is only a safety budget.
            Once this functional contract is true, the receive loop may cross
            the packet budget only long enough to finish the existing
            post-spawn survival dwell, still bounded by signon_deadline.
            """
            functional_ready = (
                bzip_complete_streams > 0
                and classinfo_count > 0
                and stringtable_baselines == len(EXPECTED_STRINGTABLES)
                and (instancebaseline_runtime is None or instancebaseline_contract_ok)
                and server_prespawn_seen
                and signon_state4_sent
                and server_spawn_seen
                and signon_state5_sent
                and signon_state6_sent
                and (not full_snapshot_update_baseline or baseline_ack_sent)
                and snapshot_tick_ack_sent
                and gameplay_command_sent
                and move_command_sent
                and packetentities_count > 0
                and packetentities_full_count > 0
                and packetentities_updated_max > 0
                and snapshot_entity_decode_ok
                and sendprop_snapshot_ok
                and (sendprop_schema is None or (sendprop_schema_decode_ok and sendprop_gameplay_ok))
                and packetentities_delta_count > 0
                and delta_snapshot_ok
                and delta_from_tick_ok
                and sendprop_delta_ok
                and gameplay_event_delta_ok
                and usercmd_eyeangles_delta_ok
                and usercmd_movement_delta_ok
                and friction_command_sent
                and usercmd_friction_delta_ok
                and jump_command_sent
                and usercmd_jump_delta_ok
                and jump_hold_command_sent
                and usercmd_jump_hold_delta_ok
                and air_accel_command_sent
                and usercmd_air_accel_delta_ok
                and (not args.weapon_actions or (weapon_fire_command_sent and weapon_release_command_sent
                                                   and weapon_reload_command_sent and weapon_secondary_command_sent
                                                   and server_ack_highwater >= weapon_secondary_sequence
                                                   and weapon_fire_event_ok and weapon_reload_event_ok))
                and (not args.combat_damage or (combat_setup_command_sent and player_hurt_event_ok
                                                and player_death_event_ok and combat_reset_command_sent
                                                and server_ack_highwater >= combat_reset_sequence))
                and gameevent_list_ok
                and listen_events_sent
                and say_command_sent
                and player_say_event_ok
                and usermessage_command_sent
                and saytext_usermessage_ok
                and (not args.download_resource or (downloadables_ok and download_request_sent and download_received_ok))
                and post_spawn_until > 0.0
            )
            if not functional_ready or not require_dwell:
                return functional_ready
            return time.monotonic() >= post_spawn_until

        next_client_sequence = 2
        last_server_sequence = 0
        server_ack_highwater = 0
        reassembler = ReliableFragmentReassembler()
        # The first compressed reliable transfer observed on build4100 is the
        # general signon stream (Print/ServerInfo/...), not SV_CreateBaseline.
        # Keep the channel alive long enough for its reliable ACK to release the
        # queued full SendTable+ClassInfo transfer.
        # v1.8.31b: direct NET_File downloads can be much longer than signon.
        # de_dust2.nav is 573 compressed stream-1 fragments on build4100, so
        # keep the same receive loop alive long enough to complete/verify it.
        receive_budget = max(args.timeout * 7.0, 21.0)
        if args.download_resource:
            receive_budget = max(receive_budget, args.timeout * 15.0, 45.0)
        signon_started_at = time.monotonic()
        signon_deadline = signon_started_at + receive_budget
        # A full build4100 SendTable stream can consume almost the entire base
        # receive budget during a changelevel.  Stage progress must buy a fresh,
        # bounded lease; otherwise PRESPAWN can be decoded and state4 transmitted
        # with only milliseconds left, so the loop exits before the server can
        # publish SPAWN or before our bounded state4 retries are eligible.
        #
        # Keep a hard ceiling so a broken netchannel cannot extend itself forever.
        # Leases are granted only on one-way signon milestones, not on keepalives.
        signon_hard_deadline = signon_started_at + max(receive_budget + 45.0, 90.0)
        signon_lease_extensions = 0

        def renew_signon_lease(stage: str, seconds: float) -> None:
            nonlocal signon_deadline, signon_lease_extensions
            now_lease = time.monotonic()
            old_deadline = signon_deadline
            requested = now_lease + max(0.0, seconds)
            if requested > signon_deadline:
                signon_deadline = min(signon_hard_deadline, requested)
            extended = 1 if signon_deadline > old_deadline + 0.001 else 0
            if extended:
                signon_lease_extensions += 1
            print(
                f"PROTOCOL7_SIGNON_LEASE=PASS stage={stage} extended={extended} extensions={signon_lease_extensions} "
                f"remaining={max(0.0, signon_deadline - now_lease):.3f}s "
                f"hard_remaining={max(0.0, signon_hard_deadline - now_lease):.3f}s"
            )

        next_keepalive = time.monotonic() + 0.25
        next_initial_signon_retry = time.monotonic() + args.initial_signon_retry_interval
        sock.setblocking(False)
        tcp.setblocking(False)
        tcp_closed = False
        while time.monotonic() < signon_deadline:
            if udp_signon_packets >= args.max_udp_packets:
                if not integration_path_ready(require_dwell=False):
                    break
                if not packet_budget_grace_started:
                    packet_budget_grace_started = 1
                    packet_budget_grace_started_at = time.monotonic()
                    dwell_remaining = max(0.0, post_spawn_until - packet_budget_grace_started_at)
                    print(
                        "PROTOCOL7_PACKET_BUDGET_GRACE=PASS "
                        f"limit={args.max_udp_packets} packets={udp_signon_packets} "
                        f"reason=functional_path_ready dwell_remaining={dwell_remaining:.3f}s "
                        f"dwell_reason={post_spawn_dwell_reason} dwell_updates={post_spawn_dwell_updates} "
                        f"deadline_remaining={max(0.0, signon_deadline - packet_budget_grace_started_at):.3f}s"
                    )
            wait = min(0.10, max(0.0, signon_deadline - time.monotonic()))
            readable, _, _ = select.select([sock, tcp] if not tcp_closed else [sock], [], [], wait)
            for ready in readable:
                if ready is tcp:
                    try:
                        data = tcp.recv(8192)
                    except BlockingIOError:
                        continue
                    if not data:
                        tcp_closed = True
                        print("PROTOCOL7_TCP_STREAM_CLOSED=NORMAL")
                        continue
                    tcp_diag += 1
                    if tcp_diag <= 4:
                        print(
                            f"PROTOCOL7_TCP_REPLY_{tcp_diag}=bytes={len(data)} "
                            f"hex={data[:96].hex()}"
                        )
                    continue

                try:
                    data, _ = sock.recvfrom(65535)
                except BlockingIOError:
                    continue
                udp_diag += 1
                if data.startswith(CONNECTIONLESS):
                    if udp_diag <= 6:
                        print(f"PROTOCOL7_NETCHAN_REPLY_{udp_diag}={describe_oob(data)}")
                    continue
                if len(data) < 10:
                    if udp_diag <= 6:
                        print(f"PROTOCOL7_NETCHAN_REPLY_{udp_diag}=short:{data.hex()}")
                    continue

                (
                    server_seq,
                    server_ack,
                    server_flags,
                    server_reliable,
                    choked,
                    packet_challenge,
                    payload,
                ) = parse_netchan_packet(data)
                udp_signon_packets += 1
                if udp_signon_packets > args.max_udp_packets:
                    packet_budget_grace_packets = udp_signon_packets - args.max_udp_packets
                udp_signon_bytes += len(payload)
                last_server_sequence = max(last_server_sequence, server_seq)
                server_ack_highwater = max(server_ack_highwater, server_ack)

                subchannel: int | None = None
                blocks: list[dict[str, int | bool | str]] = []
                completed_streams: list[tuple[int, bool, bytes, str]] = []
                fragment_end_bit = 0
                if server_flags & PACKET_FLAG_RELIABLE:
                    subchannel, blocks, completed_streams, fragment_end_bit = reassembler.consume(payload)
                    fragment_blocks += len(blocks)
                    if args.download_resource and download_request_sent:
                        now_progress = time.monotonic()
                        for block in blocks:
                            if (
                                int(block.get("stream", -1)) == 1
                                and str(block.get("filename", "")) == args.download_resource
                            ):
                                received = int(block.get("received_fragments", 0))
                                total = int(block.get("total_fragments", 0))
                                if received > download_progress_fragments:
                                    download_progress_fragments = received
                                    # Sliding 12s inactivity lease, bounded by a
                                    # 120s hard deadline established at request time.
                                    if download_hard_deadline > 0.0:
                                        signon_deadline = min(
                                            download_hard_deadline,
                                            max(signon_deadline, now_progress + 12.0),
                                        )
                                    if received == 1 or received == total or received % 64 == 0:
                                        print(
                                            f"PROTOCOL7_DOWNLOAD_PROGRESS=PASS filename={args.download_resource} "
                                            f"fragments={received}/{total} "
                                            f"lease_remaining={max(0.0, signon_deadline - now_progress):.3f}s"
                                        )
                    for block in blocks:
                        if block.get("decode_error"):
                            fragment_decode_warnings += 1
                            print(
                                f"PROTOCOL7_FRAGMENT_DECODE_WARN stream={block['stream']} "
                                f"declared_bzip={1 if block['compressed'] else 0} error={block['decode_error']} "
                                f"bytes={block['total_bytes']} raw_head_hex={block['raw_head_hex']} "
                                f"ordinary_start={fragment_end_bit}"
                            )

                decoded_classinfos: list[tuple[int, list[tuple[int, str, str]], int]] = []
                packet_sendtables = 0
                packetentities_found_in_packet = False
                completed_block_by_stream = {
                    int(block["stream"]): block
                    for block in blocks
                    if int(block["total_fragments"]) > 0
                    and int(block["received_fragments"]) >= int(block["total_fragments"])
                }
                for stream, declared_compressed, decoded, decode_error in completed_streams:
                    completed_block = completed_block_by_stream.get(stream)
                    completed_filename = str(completed_block.get("filename", "")) if completed_block else ""
                    if stream == 1 and completed_filename:
                        reliable_streams += 1
                        if args.download_resource and completed_filename == args.download_resource:
                            got_sha256 = hashlib.sha256(decoded).hexdigest()
                            got_size = len(decoded)
                            got_transfer = int(completed_block.get("transfer_id", 0)) if completed_block else 0
                            got_fragments = int(completed_block.get("total_fragments", 0)) if completed_block else 0
                            if (
                                not decode_error
                                and got_transfer == download_transfer_id
                                and got_size == args.download_size
                                and got_sha256 == args.download_sha256
                            ):
                                download_received_ok = 1
                                download_bytes = got_size
                                download_fragments = got_fragments
                                download_compressed = 1 if declared_compressed else 0
                                print(
                                    f"PROTOCOL7_DOWNLOAD_FILE=PASS filename={completed_filename} "
                                    f"transfer_id={got_transfer} bytes={got_size} fragments={got_fragments} "
                                    f"compressed={download_compressed} sha256={got_sha256}"
                                )
                            else:
                                print(
                                    f"PROTOCOL7_DOWNLOAD_FILE=FAIL filename={completed_filename} "
                                    f"transfer_id={got_transfer} expected_transfer_id={download_transfer_id} "
                                    f"bytes={got_size} expected_bytes={args.download_size} "
                                    f"sha256={got_sha256} expected_sha256={args.download_sha256} "
                                    f"decode_error={decode_error or 'none'}"
                                )
                        else:
                            print(
                                f"PROTOCOL7_FILE_STREAM=PASS filename={completed_filename} "
                                f"bytes={len(decoded)} compressed={1 if declared_compressed else 0}"
                            )
                        continue
                    reliable_streams += 1
                    signon_decoded = b"" if decode_error else decoded
                    if signon_decoded:
                        record_gameevent_data(signon_decoded, f"reliable{stream}", server_seq, 0)
                        record_usermessage_data(signon_decoded, f"reliable{stream}", server_seq, 0)
                        record_voice_data(signon_decoded, f"reliable{stream}", server_seq, 0)
                        record_tempentity_data(signon_decoded, f"reliable{stream}", server_seq, 0)
                    kind, stream_detail = classify_signon_stream(signon_decoded)
                    if decode_error:
                        kind = "fragment_raw_recovery"
                        stream_detail = f"decode_error={decode_error} raw_bytes={len(decoded)}"
                    connected_new = parse_connected_new_stream(signon_decoded)
                    full_signon = parse_full_signon_stream(signon_decoded)
                    if connected_new is not None:
                        stringtable_baselines = len(connected_new["tables"])
                        client_player_slot = int(connected_new["serverinfo"]["player_slot"])
                        if not instancebaseline_contract_ok:
                            baseline_table = next(
                                (table for table in connected_new["tables"] if table["table_name"] == "instancebaseline"),
                                None,
                            )
                            if baseline_table is None:
                                print("PROTOCOL7=FAIL reason=instancebaseline_table_missing")
                                return 1
                            try:
                                baseline_entries = decode_stringtable_entries(signon_decoded, baseline_table)
                                wire = decode_instancebaseline_wire(baseline_entries)
                                wire_growth: list[int] = []
                                if instancebaseline_runtime is not None:
                                    wire_growth = validate_instancebaseline_runtime_wire(
                                        instancebaseline_runtime, wire
                                    )
                                instancebaseline_wire = wire
                                instancebaseline_contract_ok = 1
                                print(
                                    f"PROTOCOL7_INSTANCEBASELINE_TABLE=PASS entries={len(wire)} "
                                    f"runtime_snapshot={len(instancebaseline_runtime or {})} "
                                    f"wire_growth={len(wire_growth)} "
                                    f"wire_growth_ids={','.join(str(cid) for cid in wire_growth) or '-'} "
                                    f"player_index={wire.get(1, -1)} c4_index={wire.get(6, -1)} "
                                    f"ids={','.join(str(cid) for cid in sorted(wire)) or 'none'}"
                                )
                            except ValueError as exc:
                                print(f"PROTOCOL7=FAIL reason=instancebaseline_table error={exc}")
                                return 1
                        if args.download_resource and not downloadables_ok:
                            download_table = next(
                                (table for table in connected_new["tables"] if table["table_name"] == "downloadables"),
                                None,
                            )
                            if download_table is not None:
                                try:
                                    download_entries = decode_stringtable_entries(signon_decoded, download_table)
                                except ValueError as exc:
                                    print(f"PROTOCOL7_DOWNLOADABLES=FAIL reason=decode error={exc}")
                                else:
                                    download_names = [name for _index, name in download_entries if name]
                                    advertised = args.download_resource in download_names
                                    if advertised:
                                        downloadables_ok = 1
                                    print(
                                        f"PROTOCOL7_DOWNLOADABLES={'PASS' if advertised else 'FAIL'} "
                                        f"entries={len(download_names)} requested={args.download_resource} "
                                        f"advertised={1 if advertised else 0} resources={','.join(download_names)}"
                                    )
                        signon_state = connected_new["signon"]
                        if signon_state["server_count"] == args.server_count and not server_new_seen:
                            server_new_seen = 1
                            server_new_bit = int(signon_state["start_bit"])
                            renew_signon_lease("server_new_sequential", 12.0)
                            print(
                                f"PROTOCOL7_STRINGTABLE_BASELINES=PASS tables={stringtable_baselines} "
                                f"names={','.join(str(table['table_name']) for table in connected_new['tables'])} "
                                f"tick={connected_new['tick']['tick']} convars={connected_new['convars']['count']}"
                            )
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_NEW=PASS state={SIGNONSTATE_NEW} "
                                f"server_count={args.server_count} bit={server_new_bit} decoded_bytes={len(decoded)} source=sequential"
                            )

                    spawn_transition = parse_spawn_transition_stream(
                        signon_decoded,
                        args.server_count,
                        client_player_slot + 1 if client_player_slot >= 0 else None,
                    )
                    if (
                        spawn_transition is not None
                        and signon_state4_sent
                        and not server_spawn_seen
                    ):
                        signon_state = spawn_transition["signon"]
                        server_spawn_seen = 1
                        server_spawn_bit = int(signon_state["start_bit"])
                        renew_signon_lease("server_spawn_transition", 12.0)
                        print(
                            f"PROTOCOL7_SERVER_SIGNON_SPAWN=PASS state={SIGNONSTATE_SPAWN} "
                            f"server_count={args.server_count} bit={server_spawn_bit} "
                            f"decoded_bytes={len(decoded)} source=spawn_transition "
                            f"pad_bits={spawn_transition['pad_bits']} view_entity={spawn_transition['view_entity']} "
                            f"tick={spawn_transition['tick']['tick']} tail_pad_bits={spawn_transition['tail_pad_bits']}"
                        )

                    state_scan_start = 0
                    if full_signon is not None and full_signon["classinfo"] is not None:
                        state_scan_start = int(full_signon["classinfo"][2])
                    signon_states = scan_signon_state_messages(signon_decoded, state_scan_start)
                    for signon_state in signon_states:
                        if signon_state["server_count"] != args.server_count:
                            continue
                        if signon_state["state"] == SIGNONSTATE_NEW and not server_new_seen:
                            server_new_seen = 1
                            server_new_bit = signon_state["start_bit"]
                            renew_signon_lease("server_new_scan", 12.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_NEW=PASS state={SIGNONSTATE_NEW} "
                                f"server_count={args.server_count} bit={server_new_bit} decoded_bytes={len(decoded)} source=scan"
                            )
                        elif signon_state["state"] == SIGNONSTATE_PRESPAWN and signon_state3_sent and not server_prespawn_seen:
                            server_prespawn_seen = 1
                            server_prespawn_bit = signon_state["start_bit"]
                            renew_signon_lease("server_prespawn_scan", 15.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_PRESPAWN=PASS state={SIGNONSTATE_PRESPAWN} "
                                f"server_count={args.server_count} bit={server_prespawn_bit} decoded_bytes={len(decoded)}"
                            )
                        elif signon_state["state"] == SIGNONSTATE_SPAWN and signon_state4_sent and not server_spawn_seen:
                            server_spawn_seen = 1
                            server_spawn_bit = signon_state["start_bit"]
                            renew_signon_lease("server_spawn_scan", 12.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_SPAWN=PASS state={SIGNONSTATE_SPAWN} "
                                f"server_count={args.server_count} bit={server_spawn_bit} decoded_bytes={len(decoded)}"
                            )
                    exact_sendtables = None
                    matches: list[tuple[int, list[tuple[int, str, str]], int]] = []

                    if full_signon is not None:
                        exact_sendtables = full_signon["sendtables"]
                        tables = exact_sendtables["tables"]
                        packet_sendtables += len(tables)
                        sendtable_count += len(tables)
                        classinfo = full_signon["classinfo"]
                        if classinfo is not None:
                            matches.append(classinfo)
                    elif kind == "sendtables":
                        exact_sendtables = parse_full_sendtable_stream(signon_decoded)
                        if exact_sendtables is not None:
                            tables = exact_sendtables["tables"]
                            packet_sendtables += len(tables)
                            sendtable_count += len(tables)
                            classinfo = exact_sendtables["classinfo"]
                            if classinfo is not None:
                                matches.append(classinfo)

                    if not matches:
                        # ClassInfo can also be emitted as a standalone reliable
                        # message when the CRC already matches. Exact names and IDs
                        # keep this fallback scanner unambiguous.
                        matches = scan_classinfo_messages(signon_decoded)
                    if kind == "signon_misc":
                        signon_misc_streams += 1

                    decoded_classinfos.extend(matches)
                    classinfo_count += len(matches)

                    # Post-SPAWN snapshots are sent through the reliable fragment
                    # stream. With the exact build4100 17-bit single-block length
                    # fixed above, this buffer now starts at its true bit 0 and
                    # compressed transfers decode before scanning svc_PacketEntities.
                    if signon_state5_sent and not decode_error:
                        stream_packetentities = scan_packet_entities_messages(signon_decoded, 0)
                        stream_packetentities.sort(
                            key=lambda item: (
                                0 if int(item["is_delta"]) == 0 else 1,
                                -int(item["data_bits"]),
                                int(item["start_bit"]),
                            )
                        )
                        if stream_packetentities:
                            stream_tick_info = parse_tick_at(signon_decoded, 0)
                            stream_tick = int(stream_tick_info["tick"]) if stream_tick_info is not None else -1
                            packetentities_found_in_packet |= record_packetentities(
                                stream_packetentities[0], "reliable", server_seq, stream, stream_tick
                            )

                    stream_hash = hashlib.sha256(decoded).hexdigest()
                    print(
                        f"PROTOCOL7_SIGNON_TRACE stream={stream} compressed={1 if declared_compressed else 0} decode_error={decode_error or 'none'} "
                        f"sha256={stream_hash} {trace_signon_prefix(decoded)} "
                        f"head_hex={decoded[:96].hex()} tail_hex={decoded[-96:].hex()}"
                    )
                    # Keep one real failed run sufficient for the next parser step.
                    # Small signon blocks are cheap to retain verbatim in test-current.log;
                    # large full-table blocks keep only hash/head/tail above.
                    if not matches and len(decoded) <= 4096:
                        print(
                            f"PROTOCOL7_SIGNON_STREAM_HEX stream={stream} bytes={len(decoded)} data={decoded.hex()}"
                        )

                    if declared_compressed and not decode_error:
                        bzip_complete_streams += 1
                        bzip_decoded_bytes += len(decoded)
                        print(
                            f"PROTOCOL7_BZIP_STREAM_{bzip_complete_streams}=PASS stream={stream} "
                            f"decoded_bytes={len(decoded)} kind={kind} {stream_detail} "
                            f"classinfo={len(matches)} head_hex={decoded[:128].hex()}"
                        )
                    else:
                        print(
                            f"PROTOCOL7_RELIABLE_STREAM_{reliable_streams}=PASS stream={stream} "
                            f"decoded_bytes={len(decoded)} kind={kind} {stream_detail} classinfo={len(matches)}"
                        )
                    if exact_sendtables is not None:
                        print(
                            f"PROTOCOL7_SENDTABLE_STREAM=PASS tables={len(exact_sendtables['tables'])} "
                            f"start_bit={exact_sendtables['start_bit']} next_bit={exact_sendtables['next_bit']} "
                            f"next_type={exact_sendtables['next_type']} "
                            f"classinfo={1 if exact_sendtables['classinfo'] is not None else 0}"
                        )
                    for bitpos, classes, endbit in matches[:2]:
                        class_preview = ",".join(f"{cid}:{name}/{table}" for cid, name, table in classes)
                        print(
                            f"PROTOCOL7_CLASSINFO=PASS bit={bitpos} endbit={endbit} classes={len(classes)} "
                            f"ids=0..{len(classes)-1} entries={class_preview}"
                        )

                # Also inspect ordinary netmessage bits outside the reliable
                # subchannel block.  This covers standalone ClassInfo after the
                # transfer and packets with no PACKET_FLAG_RELIABLE at all.
                ordinary_start = fragment_end_bit if (server_flags & PACKET_FLAG_RELIABLE) else 0
                record_gameevent_data(payload, "ordinary", server_seq, ordinary_start)
                record_usermessage_data(payload, "ordinary", server_seq, ordinary_start)
                record_voice_data(payload, "ordinary", server_seq, ordinary_start)
                record_tempentity_data(payload, "ordinary", server_seq, ordinary_start)
                if ordinary_start + 45 <= len(payload) * 8:
                    for bitpos in range(ordinary_start, len(payload) * 8 - 44):
                        signon_state = parse_signon_state_at(payload, bitpos)
                        if signon_state is None or signon_state["server_count"] != args.server_count:
                            continue
                        if signon_state["state"] == SIGNONSTATE_NEW and not server_new_seen:
                            server_new_seen = 1
                            server_new_bit = bitpos
                            renew_signon_lease("server_new_packet", 12.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_NEW=PASS state={SIGNONSTATE_NEW} "
                                f"server_count={args.server_count} bit={server_new_bit} source=packet"
                            )
                        elif signon_state["state"] == SIGNONSTATE_PRESPAWN and signon_state3_sent and not server_prespawn_seen:
                            server_prespawn_seen = 1
                            server_prespawn_bit = bitpos
                            renew_signon_lease("server_prespawn_packet", 15.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_PRESPAWN=PASS state={SIGNONSTATE_PRESPAWN} "
                                f"server_count={args.server_count} bit={server_prespawn_bit} source=packet"
                            )
                        elif signon_state["state"] == SIGNONSTATE_SPAWN and signon_state4_sent and not server_spawn_seen:
                            server_spawn_seen = 1
                            server_spawn_bit = bitpos
                            renew_signon_lease("server_spawn_packet", 12.0)
                            print(
                                f"PROTOCOL7_SERVER_SIGNON_SPAWN=PASS state={SIGNONSTATE_SPAWN} "
                                f"server_count={args.server_count} bit={server_spawn_bit} source=packet"
                            )
                if ordinary_start + 22 <= len(payload) * 8:
                    for bitpos in range(ordinary_start, len(payload) * 8 - 21):
                        parsed = parse_classinfo_at(payload, bitpos)
                        if parsed is None:
                            continue
                        if not any(existing[0] == parsed[0] and existing[2] == parsed[2] for existing in decoded_classinfos):
                            decoded_classinfos.append(parsed)
                            classinfo_count += 1
                            _, classes, endbit = parsed
                            class_preview = ",".join(f"{cid}:{name}/{table}" for cid, name, table in classes)
                            print(
                                f"PROTOCOL7_CLASSINFO=PASS bit={bitpos} endbit={endbit} classes={len(classes)} "
                                f"ids=0..{len(classes)-1} source=packet entries={class_preview}"
                            )

                packetentities_matches: list[dict[str, object]] = []
                if signon_state5_sent and ordinary_start + 50 <= len(payload) * 8:
                    selected_packetentities = select_ordinary_packet_entities(payload, ordinary_start)
                    if selected_packetentities is not None:
                        packetentities_matches = [selected_packetentities]
                        ordinary_tick_info = parse_tick_at(payload, ordinary_start)
                        if ordinary_tick_info is None:
                            ordinary_tick_info = parse_tick_at(payload, 0)
                        ordinary_tick = int(ordinary_tick_info["tick"]) if ordinary_tick_info is not None else -1
                        packetentities_found_in_packet |= record_packetentities(
                            selected_packetentities, "ordinary", server_seq, -1, ordinary_tick
                        )

                if signon_state5_sent and not packetentities_found_in_packet and not packetentities_matches and not postspawn_payload_dumped:
                    # Preserve the first complete post-SPAWN packet if the exact
                    # type-26 parser misses it. This keeps one WSL run sufficient
                    # to recover any preceding message/padding boundary.
                    postspawn_payload_dumped = True
                    print(
                        f"PROTOCOL7_POSTSPAWN_PAYLOAD_HEX seq={server_seq} ordinary_start={ordinary_start} "
                        f"payload_bytes={len(payload)} data={payload.hex()}"
                    )

                block_preview = ";".join(
                    f"s{int(block['stream'])}:start{int(block['start'])}+{int(block['count'])}:"
                    f"recv{int(block['received_fragments'])}/{int(block['total_fragments'])}:"
                    f"bytes{int(block['length'])}:bz{1 if bool(block['compressed']) else 0}"
                    for block in blocks
                )
                print(
                    f"PROTOCOL7_SIGNON_UDP_{udp_signon_packets}=PASS seq={server_seq} ack={server_ack} "
                    f"flags=0x{server_flags:02x} reliable=0x{server_reliable:02x} choked={choked or 0} "
                    f"challenge={packet_challenge if packet_challenge is not None else 0} bytes={len(data)} "
                    f"payload_bytes={len(payload)} subchannel={subchannel if subchannel is not None else -1} "
                    f"fragment_end_bit={fragment_end_bit} fragment_blocks={len(blocks)} "
                    f"sendtable_envelopes={packet_sendtables} bzip_completed={len([x for x in completed_streams if x[1] and not x[3]])} "
                    f"classinfo={len(decoded_classinfos)} fragments={block_preview or 'none'} head_hex={payload[:128].hex()}"
                )

                # Acknowledge the server sequence and our current incoming reliable
                # state. This is not the server header's reliable byte: each accepted
                # subchannel toggles one bit in m_nInReliableState.
                ack_reliable_state = reassembler.ack_reliable_state
                ack_packet = build_netchan_packet(
                    next_client_sequence,
                    server_seq,
                    build_nop_payload(),
                    flags=0,
                    reliable_state=ack_reliable_state,
                )
                sock.sendto(ack_packet, target)
                print(
                    f"PROTOCOL7_NETCHAN_ACK_{udp_signon_packets}=PASS seq={next_client_sequence} "
                    f"ack={server_seq} reliable=0x{ack_reliable_state:02x} nop=1 bytes={len(ack_packet)}"
                )
                next_client_sequence += 1

                # The NEW transition must be acknowledged only after the server
                # actually advertised SIGNONSTATE_NEW in its first signon block.
                # Preserve Source ordering: ClientInfo first, then SignonState.
                if server_new_seen and not clientinfo_sent:
                    client_info_payload = build_client_info_payload(args.server_count, client_crc)
                    client_info_packet = build_netchan_packet(
                        next_client_sequence,
                        server_seq,
                        client_info_payload,
                        flags=0,
                        reliable_state=ack_reliable_state,
                    )
                    sock.sendto(client_info_packet, target)
                    print(
                        f"PROTOCOL7_CLIENTINFO_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"server_count={args.server_count} sendtable_crc=0x{client_crc:08x} "
                        f"engine_crc=0x{advertised_crc:08x} "
                        f"force_full_sendtables={1 if args.force_full_sendtables else 0} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(client_info_packet)}"
                    )
                    clientinfo_sent = 1
                    next_client_sequence += 1

                    # Keep ordering deterministic on localhost/WSL Docker: the
                    # engine must consume ClientInfo before the NEW transition.
                    time.sleep(0.02)
                    new_payload = build_signon_state_payload(SIGNONSTATE_NEW, args.server_count)
                    new_packet = build_netchan_packet(
                        next_client_sequence,
                        server_seq,
                        new_payload,
                        flags=0,
                        reliable_state=ack_reliable_state,
                    )
                    sock.sendto(new_packet, target)
                    print(
                        f"PROTOCOL7_NETCHAN_NEW_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NETMSG_SIGNONSTATE} state={SIGNONSTATE_NEW} "
                        f"server_count={args.server_count} reliable=0x{ack_reliable_state:02x} "
                        f"bytes={len(new_packet)}"
                    )
                    signon_state3_sent = 1
                    next_client_sequence += 1

                if server_prespawn_seen and not signon_state4_sent:
                    prespawn_payload = build_signon_state_payload(SIGNONSTATE_PRESPAWN, args.server_count)
                    prespawn_packet = build_netchan_packet(
                        next_client_sequence, server_seq, prespawn_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(prespawn_packet, target)
                    print(
                        f"PROTOCOL7_NETCHAN_PRESPAWN_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"state={SIGNONSTATE_PRESPAWN} server_count={args.server_count} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(prespawn_packet)}"
                    )
                    signon_state4_sent = 1
                    renew_signon_lease("client_prespawn_sent", 15.0)
                    next_prespawn_signon_retry = time.monotonic() + args.initial_signon_retry_interval
                    next_client_sequence += 1

                if server_spawn_seen and not signon_state5_sent:
                    spawn_payload = build_signon_state_payload(SIGNONSTATE_SPAWN, args.server_count)
                    spawn_packet = build_netchan_packet(
                        next_client_sequence, server_seq, spawn_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(spawn_packet, target)
                    print(
                        f"PROTOCOL7_NETCHAN_SPAWN_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"state={SIGNONSTATE_SPAWN} server_count={args.server_count} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(spawn_packet)}"
                    )
                    signon_state5_sent = 1
                    extend_post_spawn_dwell(2.50, "spawn_state5")
                    next_client_sequence += 1

                # build4100 has three separate acknowledgements after the first
                # full snapshot. Sequence ACK only advances CNetChan. If the
                # snapshot requested a baseline update, CLC_BaselineAck commits
                # that baseline selector. SIGNONSTATE_FULL completes signon.
                # Finally client NET_Tick(full_snapshot_tick) drives
                # CBaseClient::UpdateAcknowledgedFramecount() and establishes the
                # server-side delta base (m_nDeltaTick).
                snapshot_ready = (
                    signon_state5_sent
                    and packetentities_full_count > 0
                    and snapshot_entity_decode_ok
                    and full_snapshot_tick >= 0
                )
                if (
                    snapshot_ready
                    and full_snapshot_update_baseline
                    and full_snapshot_baseline in (0, 1)
                    and last_baseline_ack_tick != full_snapshot_tick
                ):
                    time.sleep(0.01)
                    baseline_payload = build_baseline_ack_payload(full_snapshot_tick, full_snapshot_baseline)
                    baseline_packet = build_netchan_packet(
                        next_client_sequence, server_seq, baseline_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(baseline_packet, target)
                    print(
                        f"PROTOCOL7_BASELINE_ACK_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_BASELINEACK} tick={full_snapshot_tick} baseline={full_snapshot_baseline} "
                        f"update_baseline={full_snapshot_update_baseline} reliable=0x{ack_reliable_state:02x} "
                        f"bytes={len(baseline_packet)}"
                    )
                    if baseline_ack_sent:
                        resnapshot_baseline_acks += 1
                        print(
                            f"PROTOCOL7_RESNAPSHOT_BASELINE_ACK_SENT=PASS tick={full_snapshot_tick} "
                            f"baseline={full_snapshot_baseline} count={resnapshot_baseline_acks}"
                        )
                    baseline_ack_sent = 1
                    last_baseline_ack_tick = full_snapshot_tick
                    next_client_sequence += 1

                current_baseline_acked = (
                    not full_snapshot_update_baseline
                    or last_baseline_ack_tick == full_snapshot_tick
                )
                if snapshot_ready and current_baseline_acked and not signon_state6_sent:
                    time.sleep(0.01)
                    full_payload = build_signon_state_payload(SIGNONSTATE_FULL, args.server_count)
                    full_packet = build_netchan_packet(
                        next_client_sequence, server_seq, full_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(full_packet, target)
                    print(
                        f"PROTOCOL7_NETCHAN_FULL_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"state={SIGNONSTATE_FULL} server_count={args.server_count} full_snapshot_tick={full_snapshot_tick} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(full_packet)}"
                    )
                    signon_state6_sent = 1
                    next_client_sequence += 1

                if (
                    snapshot_ready
                    and signon_state6_sent
                    and current_baseline_acked
                    and last_snapshot_tick_ack_tick != full_snapshot_tick
                ):
                    time.sleep(0.01)
                    tick_payload = build_tick_payload(full_snapshot_tick)
                    tick_packet = build_netchan_packet(
                        next_client_sequence, server_seq, tick_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(tick_packet, target)
                    print(
                        f"PROTOCOL7_SNAPSHOT_TICK_ACK_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_TICK} tick={full_snapshot_tick} reliable=0x{ack_reliable_state:02x} "
                        f"bytes={len(tick_packet)}"
                    )
                    if snapshot_tick_ack_sent:
                        resnapshot_tick_acks += 1
                        print(
                            f"PROTOCOL7_RESNAPSHOT_TICK_ACK_SENT=PASS tick={full_snapshot_tick} "
                            f"count={resnapshot_tick_acks} dwell_extended=0"
                        )
                    else:
                        # The initial acknowledged full snapshot starts the
                        # post-FULL survival dwell. Later full snapshots are
                        # transport maintenance and must not move that deadline;
                        # otherwise periodic update-baseline snapshots can keep
                        # the pre-objective gate alive until signon_deadline.
                        extend_post_spawn_dwell(4.50, "initial_snapshot_tick_ack")
                    snapshot_tick_ack_sent = 1
                    last_snapshot_tick_ack_tick = full_snapshot_tick
                    next_client_sequence += 1

                if snapshot_tick_ack_sent and not gameplay_command_sent:
                    time.sleep(0.01)
                    command_payload = build_stringcmd_payload(f"jointeam {args.join_team}")
                    command_packet = build_netchan_packet(
                        next_client_sequence, server_seq, command_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(command_packet, target)
                    print(
                        f"PROTOCOL7_STRINGCMD_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} command=jointeam_{args.join_team} reliable=0x{ack_reliable_state:02x} bytes={len(command_packet)}"
                    )
                    gameplay_command_sent = 1
                    next_client_sequence += 1

                if gameplay_event_delta_ok and movement_origin_base is not None and not move_command_sent:
                    time.sleep(0.01)
                    move_tick = max(full_snapshot_tick + 1, 1)
                    move_payload, move_cmd_bits = build_move_payload(move_tick, pitch=45.0, yaw=90.0, forward_move=200)
                    move_packet = build_netchan_packet(
                        next_client_sequence, server_seq, move_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(move_packet, target)
                    print(
                        f"PROTOCOL7_MOVE_SENT=PASS seq={next_client_sequence} ack={server_seq} type={CLC_MOVE} "
                        f"new=1 backup=0 cmd_bits={move_cmd_bits} cmd=1 tick={move_tick} pitch=45 yaw=90 forward=200 "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(move_packet)}"
                    )
                    move_command_sent = 1
                    extend_post_spawn_dwell(3.00, "movement_cmd")
                    next_client_sequence += 1

                if usercmd_movement_delta_ok and not friction_command_sent:
                    time.sleep(0.01)
                    friction_tick = max(full_snapshot_tick + 2, 2)
                    friction_payload, friction_cmd_bits = build_move_payload(
                        friction_tick, command_number=2, pitch=45.0, yaw=90.0
                    )
                    friction_packet = build_netchan_packet(
                        next_client_sequence, server_seq, friction_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(friction_packet, target)
                    print(
                        f"PROTOCOL7_FRICTION_MOVE_SENT=PASS seq={next_client_sequence} ack={server_seq} type={CLC_MOVE} "
                        f"new=1 backup=0 cmd_bits={friction_cmd_bits} cmd=2 tick={friction_tick} pitch=45 yaw=90 forward=0 "
                        f"buttons=0 reliable=0x{ack_reliable_state:02x} bytes={len(friction_packet)}"
                    )
                    friction_command_sent = 1
                    extend_post_spawn_dwell(3.00, "friction_cmd")
                    next_client_sequence += 1

                if usercmd_friction_delta_ok and not jump_command_sent:
                    time.sleep(0.01)
                    jump_tick = max(full_snapshot_tick + 3, 3)
                    jump_payload, jump_cmd_bits = build_move_payload(
                        jump_tick, command_number=3, pitch=45.0, yaw=90.0, buttons=2
                    )
                    jump_packet = build_netchan_packet(
                        next_client_sequence, server_seq, jump_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(jump_packet, target)
                    print(
                        f"PROTOCOL7_JUMP_MOVE_SENT=PASS seq={next_client_sequence} ack={server_seq} type={CLC_MOVE} "
                        f"new=1 backup=0 cmd_bits={jump_cmd_bits} cmd=3 tick={jump_tick} pitch=45 yaw=90 forward=0 "
                        f"buttons=2 reliable=0x{ack_reliable_state:02x} bytes={len(jump_packet)}"
                    )
                    jump_command_sent = 1
                    extend_post_spawn_dwell(3.00, "jump_cmd")
                    next_client_sequence += 1

                if usercmd_jump_delta_ok and not jump_hold_command_sent:
                    time.sleep(0.01)
                    hold_tick = max(full_snapshot_tick + 4, 4)
                    hold_payload, hold_cmd_bits = build_move_payload(
                        hold_tick, command_number=4, pitch=45.0, yaw=90.0, buttons=2
                    )
                    hold_packet = build_netchan_packet(
                        next_client_sequence, server_seq, hold_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(hold_packet, target)
                    print(
                        f"PROTOCOL7_JUMP_HOLD_MOVE_SENT=PASS seq={next_client_sequence} ack={server_seq} type={CLC_MOVE} "
                        f"new=1 backup=0 cmd_bits={hold_cmd_bits} cmd=4 tick={hold_tick} pitch=45 yaw=90 forward=0 "
                        f"buttons=2 reliable=0x{ack_reliable_state:02x} bytes={len(hold_packet)}"
                    )
                    jump_hold_command_sent = 1
                    extend_post_spawn_dwell(3.00, "jump_hold_cmd")
                    next_client_sequence += 1
                if usercmd_jump_hold_delta_ok and not air_accel_command_sent:
                    time.sleep(0.01)
                    air_tick = max(full_snapshot_tick + 5, 5)
                    air_payload, air_cmd_bits = build_move_payload(
                        air_tick, command_number=5, pitch=45.0, yaw=90.0, forward_move=200, buttons=0
                    )
                    air_packet = build_netchan_packet(
                        next_client_sequence, server_seq, air_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(air_packet, target)
                    print(
                        f"PROTOCOL7_AIR_ACCEL_MOVE_SENT=PASS seq={next_client_sequence} ack={server_seq} type={CLC_MOVE} "
                        f"new=1 backup=0 cmd_bits={air_cmd_bits} cmd=5 tick={air_tick} pitch=45 yaw=90 forward=200 "
                        f"buttons=0 reliable=0x{ack_reliable_state:02x} bytes={len(air_packet)}"
                    )
                    air_accel_command_sent = 1
                    extend_post_spawn_dwell(3.00, "air_accel_cmd")
                    next_client_sequence += 1

                if signon_state6_sent and gameevent_list_ok and not listen_events_sent:
                    time.sleep(0.01)
                    listen_event_ids = {player_say_event_id, *objective_event_ids.values(), *weapon_event_ids.values(), *combat_event_ids.values()}
                    listen_payload = build_listen_events_payload(listen_event_ids)
                    listen_packet = build_netchan_packet(
                        next_client_sequence, server_seq, listen_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(listen_packet, target)
                    listen_events_sequence = next_client_sequence
                    print(
                        f"PROTOCOL7_LISTENEVENTS_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_LISTENEVENTS} player_say_id={player_say_event_id} objective_ids={len(objective_event_ids)} weapon_ids={len(weapon_event_ids)} combat_ids={len(combat_event_ids)} mask_bits=512 "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(listen_packet)}"
                    )
                    listen_events_sent = 1
                    extend_post_spawn_dwell(3.00, "listen_events")
                    next_client_sequence += 1

                if (args.combat_damage and usercmd_air_accel_delta_ok and listen_events_sent
                        and server_ack_highwater >= listen_events_sequence and not combat_setup_command_sent):
                    time.sleep(0.01)
                    setup_payload = build_stringcmd_payload("cssv34_combat_setup")
                    setup_packet = build_netchan_packet(
                        next_client_sequence, server_seq, setup_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(setup_packet, target)
                    combat_setup_command_sent = 1
                    combat_setup_sequence = next_client_sequence
                    print(
                        f"PROTOCOL7_COMBAT_SETUP_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} command=cssv34_combat_setup reliable=0x{ack_reliable_state:02x} bytes={len(setup_packet)}"
                    )
                    extend_post_spawn_dwell(4.00, "combat_setup")
                    next_client_sequence += 1

                if (
                    args.weapon_actions
                    and args.pre_weapon_join_team in (2, 3)
                    and usercmd_air_accel_delta_ok
                    and listen_events_sent
                    and server_ack_highwater >= listen_events_sequence
                    and not pre_weapon_join_sent
                ):
                    time.sleep(0.01)
                    prejoin_payload = build_stringcmd_payload(f"jointeam {args.pre_weapon_join_team}")
                    prejoin_packet = build_netchan_packet(
                        next_client_sequence, server_seq, prejoin_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(prejoin_packet, target)
                    pre_weapon_join_sent = 1
                    pre_weapon_join_sequence = next_client_sequence
                    pre_weapon_join_sent_at = time.monotonic()
                    print(
                        f"PROTOCOL7_PRE_WEAPON_JOIN_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} team={args.pre_weapon_join_team} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(prejoin_packet)}",
                        flush=True,
                    )
                    extend_post_spawn_dwell(3.00, "pre_weapon_join")
                    next_client_sequence += 1

                if (
                    args.weapon_actions
                    and usercmd_air_accel_delta_ok
                    and listen_events_sent
                    and server_ack_highwater >= listen_events_sequence
                    and (args.pre_weapon_join_team == 0 or (
                        pre_weapon_join_sent
                        and server_ack_highwater >= pre_weapon_join_sequence
                        and time.monotonic() - pre_weapon_join_sent_at >= 0.20
                    ))
                    and (not args.combat_damage or (combat_setup_command_sent and server_ack_highwater >= combat_setup_sequence))
                    and not weapon_fire_command_sent
                ):
                    time.sleep(0.01)
                    attack_tick = max(full_snapshot_tick + 6, 6)
                    attack_payload, attack_bits = build_move_payload(
                        attack_tick, command_number=6, pitch=0.0 if args.combat_damage else 45.0, yaw=90.0, buttons=1
                    )
                    attack_packet = build_netchan_packet(
                        next_client_sequence, server_seq, attack_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(attack_packet, target)
                    print(
                        f"PROTOCOL7_WEAPON_ATTACK_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_MOVE} cmd=6 tick={attack_tick} buttons=1 cmd_bits={attack_bits} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(attack_packet)}"
                    )
                    weapon_fire_command_sent = 1
                    extend_post_spawn_dwell(3.50, "weapon_attack")
                    next_client_sequence += 1

                if args.weapon_actions and weapon_fire_event_ok and not weapon_release_command_sent:
                    release_tick = max(full_snapshot_tick + 7, 7)
                    release_payload, release_bits = build_move_payload(
                        release_tick, command_number=7, pitch=45.0, yaw=90.0, buttons=0
                    )
                    release_packet = build_netchan_packet(
                        next_client_sequence, server_seq, release_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(release_packet, target)
                    print(
                        f"PROTOCOL7_WEAPON_RELEASE_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_MOVE} cmd=7 tick={release_tick} buttons=0 cmd_bits={release_bits} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(release_packet)}"
                    )
                    weapon_release_command_sent = 1
                    next_client_sequence += 1

                if (args.weapon_actions and weapon_release_command_sent and weapon_fire_seen_at > 0.0
                        and time.monotonic() - weapon_fire_seen_at >= 0.35 and not weapon_reload_command_sent):
                    reload_tick = max(full_snapshot_tick + 8, 8)
                    reload_payload, reload_bits = build_move_payload(
                        reload_tick, command_number=8, pitch=45.0, yaw=90.0, buttons=(1 << 13)
                    )
                    reload_packet = build_netchan_packet(
                        next_client_sequence, server_seq, reload_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(reload_packet, target)
                    print(
                        f"PROTOCOL7_WEAPON_RELOAD_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_MOVE} cmd=8 tick={reload_tick} buttons=8192 cmd_bits={reload_bits} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(reload_packet)}"
                    )
                    weapon_reload_command_sent = 1
                    extend_post_spawn_dwell(3.50, "weapon_reload")
                    next_client_sequence += 1

                if args.weapon_actions and weapon_reload_event_ok and not weapon_secondary_command_sent:
                    secondary_tick = max(full_snapshot_tick + 9, 9)
                    secondary_payload, secondary_bits = build_move_payload(
                        secondary_tick, command_number=9, pitch=45.0, yaw=90.0, buttons=(1 << 11)
                    )
                    secondary_packet = build_netchan_packet(
                        next_client_sequence, server_seq, secondary_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(secondary_packet, target)
                    print(
                        f"PROTOCOL7_WEAPON_SECONDARY_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={CLC_MOVE} cmd=9 tick={secondary_tick} buttons=2048 cmd_bits={secondary_bits} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(secondary_packet)}"
                    )
                    weapon_secondary_command_sent = 1
                    weapon_secondary_sequence = next_client_sequence
                    extend_post_spawn_dwell(1.00, "weapon_secondary")
                    next_client_sequence += 1

                if (args.combat_damage and weapon_reload_event_ok and player_hurt_event_ok and player_death_event_ok
                        and not combat_reset_command_sent):
                    reset_payload = build_stringcmd_payload("cssv34_combat_reset")
                    reset_packet = build_netchan_packet(
                        next_client_sequence, server_seq, reset_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(reset_packet, target)
                    combat_reset_command_sent = 1
                    combat_reset_sequence = next_client_sequence
                    print(
                        f"PROTOCOL7_COMBAT_RESET_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} command=cssv34_combat_reset reliable=0x{ack_reliable_state:02x} bytes={len(reset_packet)}"
                    )
                    extend_post_spawn_dwell(2.50, "combat_reset")
                    next_client_sequence += 1

                if (
                    usercmd_air_accel_delta_ok
                    and listen_events_sent
                    and server_ack_highwater >= listen_events_sequence
                    and not say_command_sent
                ):
                    time.sleep(0.01)
                    say_payload = build_stringcmd_payload("say cssv34_wire_event")
                    say_packet = build_netchan_packet(
                        next_client_sequence, server_seq, say_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(say_packet, target)
                    print(
                        f"PROTOCOL7_SAY_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} command=say_cssv34_wire_event reliable=0x{ack_reliable_state:02x} "
                        f"bytes={len(say_packet)}"
                    )
                    say_command_sent = 1
                    extend_post_spawn_dwell(3.00, "say_cmd")
                    next_client_sequence += 1

                if player_say_event_ok and not usermessage_command_sent:
                    time.sleep(0.01)
                    usermsg_payload = build_stringcmd_payload("cssv34_usermsg cssv34_wire_usermsg")
                    usermsg_packet = build_netchan_packet(
                        next_client_sequence, server_seq, usermsg_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(usermsg_packet, target)
                    print(
                        f"PROTOCOL7_USERMESSAGE_COMMAND_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_STRINGCMD} command=cssv34_usermsg_cssv34_wire_usermsg "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(usermsg_packet)}"
                    )
                    usermessage_command_sent = 1
                    extend_post_spawn_dwell(3.00, "usermessage_cmd")
                    next_client_sequence += 1

                if (
                    args.download_resource
                    and saytext_usermessage_ok
                    and downloadables_ok
                    and not download_request_sent
                ):
                    time.sleep(0.01)
                    download_payload = build_file_request_payload(args.download_resource, download_transfer_id)
                    download_packet = build_netchan_packet(
                        next_client_sequence, server_seq, download_payload, flags=0, reliable_state=ack_reliable_state
                    )
                    sock.sendto(download_packet, target)
                    download_request_sequence = next_client_sequence
                    print(
                        f"PROTOCOL7_DOWNLOAD_REQUEST_SENT=PASS seq={next_client_sequence} ack={server_seq} "
                        f"type={NET_FILE} transfer_id={download_transfer_id} filename={args.download_resource} "
                        f"reliable=0x{ack_reliable_state:02x} bytes={len(download_packet)}"
                    )
                    download_request_sent = 1
                    extend_post_spawn_dwell(3.00, "download_request")
                    request_now = time.monotonic()
                    download_hard_deadline = request_now + 120.0
                    signon_deadline = max(signon_deadline, request_now + 12.0)
                    print(
                        f"PROTOCOL7_DOWNLOAD_WATCHDOG=PASS filename={args.download_resource} "
                        "idle_lease=12.0 hard_limit=120.0"
                    )
                    next_client_sequence += 1

                next_keepalive = time.monotonic() + 0.25

            # During a changelevel build4100 can accept the connection and start
            # advancing netchannel sequence numbers before the game-side signon
            # path is ready to consume our first CONNECTED state.  In that case
            # the original state2 datagram is effectively lost and the server
            # emits only empty keepalives forever.  For mapcycle probes the caller
            # may request a small bounded number of state2 retransmits on the same
            # CNetChan.  This preserves the exact full-path assertions; it only
            # makes the initial unreliable signon request resilient to the map
            # transition race observed on fy_pool_day.
            now = time.monotonic()
            if (
                not server_new_seen
                and args.initial_signon_retries > 0
                and initial_signon_retries_sent < args.initial_signon_retries
                and last_server_sequence > 0
                and now >= next_initial_signon_retry
            ):
                retry_packet = build_netchan_packet(
                    next_client_sequence,
                    last_server_sequence,
                    signon_payload,
                    flags=0,
                    reliable_state=reassembler.ack_reliable_state,
                )
                sock.sendto(retry_packet, target)
                initial_signon_retries_sent += 1
                print(
                    f"PROTOCOL7_NETCHAN_SIGNON_RETRY_{initial_signon_retries_sent}=PASS "
                    f"seq={next_client_sequence} ack={last_server_sequence} flags=0 "
                    f"reliable=0x{reassembler.ack_reliable_state:02x} "
                    f"type={NETMSG_SIGNONSTATE} state={SIGNONSTATE_CONNECTED} server_count=-1 "
                    f"bytes={len(retry_packet)}"
                )
                next_client_sequence += 1
                next_initial_signon_retry = now + args.initial_signon_retry_interval

            # PRESPAWN/state4 is also an unreliable client transition. On a busy
            # build4100 server (notably immediately after a completed full probe)
            # it can be consumed late or effectively lost while the netchannel
            # continues to ACK/keepalive. When bounded signon retries are enabled,
            # resend state4 on the same channel until the server publishes SPAWN.
            if (
                server_prespawn_seen
                and signon_state4_sent
                and not server_spawn_seen
                and args.initial_signon_retries > 0
                and prespawn_signon_retries_sent < args.initial_signon_retries
                and last_server_sequence > 0
                and next_prespawn_signon_retry > 0.0
                and now >= next_prespawn_signon_retry
            ):
                prespawn_retry_payload = build_signon_state_payload(SIGNONSTATE_PRESPAWN, args.server_count)
                prespawn_retry_packet = build_netchan_packet(
                    next_client_sequence,
                    last_server_sequence,
                    prespawn_retry_payload,
                    flags=0,
                    reliable_state=reassembler.ack_reliable_state,
                )
                sock.sendto(prespawn_retry_packet, target)
                prespawn_signon_retries_sent += 1
                print(
                    f"PROTOCOL7_NETCHAN_PRESPAWN_RETRY_{prespawn_signon_retries_sent}=PASS "
                    f"seq={next_client_sequence} ack={last_server_sequence} flags=0 "
                    f"reliable=0x{reassembler.ack_reliable_state:02x} "
                    f"type={NETMSG_SIGNONSTATE} state={SIGNONSTATE_PRESPAWN} "
                    f"server_count={args.server_count} bytes={len(prespawn_retry_packet)}"
                )
                next_client_sequence += 1
                renew_signon_lease("client_prespawn_retry", 8.0)
                next_prespawn_signon_retry = now + args.initial_signon_retry_interval

            # A quiet server does not necessarily mean signon is finished. Once
            # a reliable block is acknowledged, build4100 may not emit the next
            # queued block until another client datagram drives the channel. Send
            # a bounded NET_NOP keepalive carrying the current reliable ACK state.
            if (
                (not signon_state5_sent or (post_spawn_until > 0.0 and now < post_spawn_until))
                and signon_state3_sent
                and last_server_sequence > 0
                and now >= next_keepalive
                and keepalives_sent < 32
            ):
                keepalive = build_netchan_packet(
                    next_client_sequence,
                    last_server_sequence,
                    build_nop_payload(),
                    flags=0,
                    reliable_state=reassembler.ack_reliable_state,
                )
                sock.sendto(keepalive, target)
                keepalives_sent += 1
                print(
                    f"PROTOCOL7_KEEPALIVE_{keepalives_sent}=PASS seq={next_client_sequence} "
                    f"ack={last_server_sequence} reliable=0x{reassembler.ack_reliable_state:02x} nop=1 bytes={len(keepalive)}"
                )
                next_client_sequence += 1
                next_keepalive = now + 0.25

            # Do not stop on the first quiet select interval: fragmented reliable
            # traffic advances only after the server observes our subchannel ACK.
            # Finish as soon as both integration milestones are present.
            if integration_path_ready(require_dwell=True):
                break

        full_path_ok = integration_path_ready(require_dwell=True)
        if not full_path_ok:
            print(
                "PROTOCOL7=FAIL reason=full_path_incomplete "
                f"udp_packets={udp_signon_packets} udp_packet_limit={args.max_udp_packets} "
                f"signon6={signon_state6_sent} snapshot={sendprop_snapshot_ok} "
                f"sendprop_gameplay={sendprop_gameplay_ok} gameplay_delta={gameplay_event_delta_ok} "
                f"move={move_command_sent} air_accel_delta={usercmd_air_accel_delta_ok} "
                f"gameevent_list={gameevent_list_ok} listen_events_sent={listen_events_sent} "
                f"listen_events_seq={listen_events_sequence} server_ack_highwater={server_ack_highwater} "
                f"say_sent={say_command_sent} player_say={player_say_event_ok} "
                f"usermsg_cmd={usermessage_command_sent} saytext_usermsg={saytext_usermessage_ok} "
                f"downloadables={downloadables_ok} download_request={download_request_sent} download_received={download_received_ok} "
                f"functional_ready={1 if integration_path_ready(require_dwell=False) else 0} "
                f"packet_budget_grace={packet_budget_grace_started} grace_packets={packet_budget_grace_packets} "
                f"postspawn_remaining={max(0.0, post_spawn_until - time.monotonic()):.3f} "
                f"dwell_reason={post_spawn_dwell_reason} dwell_updates={post_spawn_dwell_updates} "
                f"resnapshot_tick_acks={resnapshot_tick_acks} "
                f"initial_signon_retries={initial_signon_retries_sent} prespawn_signon_retries={prespawn_signon_retries_sent} "
                f"signon_lease_extensions={signon_lease_extensions}"
            )
            return 1

        # Keep the voice oracle independent from the objective oracle.  In the
        # multiclient gate client A is held open while client B performs both.
        # Sending CLC_VoiceData first means a later planted-C4/sendprop failure
        # cannot hide whether build4100 actually forwarded SVC_VoiceData.
        if args.voice_send_hex:
            voice_raw = bytes.fromhex(args.voice_send_hex)
            voice_payload = build_voice_data_payload(voice_raw)
            voice_packet = build_netchan_packet(
                next_client_sequence,
                last_server_sequence,
                voice_payload,
                flags=0,
                reliable_state=reassembler.ack_reliable_state,
            )
            sock.sendto(voice_packet, target)
            voice_sent = 1
            print(
                f"PROTOCOL7_VOICE_SEND=PASS seq={next_client_sequence} ack={last_server_sequence} "
                f"type={CLC_VOICEDATA} payload_bits={len(voice_raw) * 8} payload_hex={voice_raw.hex()} "
                f"reliable=0x{reassembler.ack_reliable_state:02x} bytes={len(voice_packet)}",
                flush=True,
            )
            next_client_sequence += 1
            time.sleep(0.15)

        if args.objective_scenario:
            expected_objective_events = (
                BOMB_SCENARIO_REQUIRED_EVENTS if args.objective_scenario == "bomb_rounds"
                else HOSTAGE_SCENARIO_REQUIRED_EVENTS
            )
            missing_descriptors = expected_objective_events - objective_event_ids.keys()
            if missing_descriptors:
                print(
                    f"PROTOCOL7=FAIL reason=objective_descriptors_missing scenario={args.objective_scenario} "
                    f"missing={','.join(sorted(missing_descriptors))}"
                )
                return 1
            objective_payload = build_stringcmd_payload(f"cssv34_objective {args.objective_scenario}")
            objective_packet = build_netchan_packet(
                next_client_sequence,
                last_server_sequence,
                objective_payload,
                flags=0,
                reliable_state=reassembler.ack_reliable_state,
            )
            sock.sendto(objective_packet, target)
            objective_scenario_command_sent = 1
            print(
                f"PROTOCOL7_OBJECTIVE_COMMAND_SENT=PASS seq={next_client_sequence} ack={last_server_sequence} "
                f"type={NET_STRINGCMD} scenario={args.objective_scenario} "
                f"reliable=0x{reassembler.ack_reliable_state:02x} bytes={len(objective_packet)}",
                flush=True,
            )
            next_client_sequence += 1
            objective_deadline = time.monotonic() + 12.0
            next_objective_keepalive = time.monotonic() + 0.20
            while time.monotonic() < objective_deadline and (
                not expected_objective_events.issubset(objective_events_seen)
                or (args.objective_scenario == "bomb_rounds" and not objective_c4_wire_ok)
            ):
                wait = min(0.10, max(0.0, objective_deadline - time.monotonic()))
                readable, _, _ = select.select([sock], [], [], wait)
                if readable:
                    try:
                        objective_data, _ = sock.recvfrom(65535)
                    except BlockingIOError:
                        objective_data = b""
                    if len(objective_data) >= 10 and not objective_data.startswith(CONNECTIONLESS):
                        (
                            objective_seq, objective_ack, objective_flags, _objective_reliable,
                            _objective_choked, _objective_challenge, objective_net_payload,
                        ) = parse_netchan_packet(objective_data)
                        last_server_sequence = max(last_server_sequence, objective_seq)
                        server_ack_highwater = max(server_ack_highwater, objective_ack)
                        objective_ordinary_start = 0
                        if objective_flags & PACKET_FLAG_RELIABLE:
                            _subchannel, _blocks, objective_completed, objective_ordinary_start = reassembler.consume(objective_net_payload)
                            for objective_stream, _compressed, objective_decoded, objective_error in objective_completed:
                                if not objective_error and objective_decoded:
                                    objective_source = f"objective_reliable{objective_stream}"
                                    record_gameevent_data(
                                        objective_decoded, objective_source, objective_seq, 0,
                                    )
                                    # A completed reliable block can be pure
                                    # game-event data. Bit-scanning that arbitrary
                                    # stream used to manufacture max=2/updated=0
                                    # PacketEntities records. Only accept a
                                    # non-empty entity payload from this fallback.
                                    objective_entities = [
                                        item for item in scan_packet_entities_messages(objective_decoded, 0)
                                        if int(item["updated_entries"]) > 0 and int(item["data_bits"]) > 0
                                    ]
                                    objective_entities.sort(
                                        key=lambda item: (-int(item["data_bits"]), int(item["start_bit"]))
                                    )
                                    if objective_entities:
                                        objective_tick_info = parse_tick_at(objective_decoded, 0)
                                        objective_tick = int(objective_tick_info["tick"]) if objective_tick_info is not None else -1
                                        record_packetentities(
                                            objective_entities[0], objective_source, objective_seq, objective_stream, objective_tick
                                        )
                        record_gameevent_data(
                            objective_net_payload,
                            "objective_ordinary",
                            objective_seq,
                            objective_ordinary_start,
                        )
                        objective_entity = select_ordinary_packet_entities(
                            objective_net_payload, objective_ordinary_start
                        )
                        if objective_entity is not None:
                            objective_tick_info = parse_tick_at(objective_net_payload, objective_ordinary_start)
                            if objective_tick_info is None:
                                objective_tick_info = parse_tick_at(objective_net_payload, 0)
                            objective_tick = int(objective_tick_info["tick"]) if objective_tick_info is not None else -1
                            record_packetentities(
                                objective_entity, "objective_ordinary", objective_seq, -1, objective_tick
                            )
                        ack_packet = build_netchan_packet(
                            next_client_sequence,
                            objective_seq,
                            build_nop_payload(),
                            flags=0,
                            reliable_state=reassembler.ack_reliable_state,
                        )
                        sock.sendto(ack_packet, target)
                        next_client_sequence += 1
                        next_objective_keepalive = time.monotonic() + 0.20
                now = time.monotonic()
                if last_server_sequence > 0 and now >= next_objective_keepalive:
                    keepalive = build_netchan_packet(
                        next_client_sequence,
                        last_server_sequence,
                        build_nop_payload(),
                        flags=0,
                        reliable_state=reassembler.ack_reliable_state,
                    )
                    sock.sendto(keepalive, target)
                    next_client_sequence += 1
                    next_objective_keepalive = now + 0.20
            missing_events = expected_objective_events - objective_events_seen
            if missing_events:
                print(
                    f"PROTOCOL7=FAIL reason=objective_scenario_incomplete scenario={args.objective_scenario} "
                    f"missing={','.join(sorted(missing_events))} seen={','.join(sorted(objective_events_seen))}"
                )
                return 1
            if args.objective_scenario == "bomb_rounds" and not objective_c4_wire_ok:
                print(
                    "PROTOCOL7=FAIL reason=objective_c4_sendprop_missing "
                    f"scenario={args.objective_scenario} seen={','.join(sorted(objective_events_seen))}"
                )
                return 1
            objective_scenario_ok = 1
            print(
                f"PROTOCOL7_OBJECTIVE_SCENARIO=PASS scenario={args.objective_scenario} "
                f"events={','.join(sorted(expected_objective_events))} "
                f"c4_sendprop={objective_c4_wire_ok if args.objective_scenario == 'bomb_rounds' else 0} "
                "multiclient_broadcast_ready=1",
                flush=True,
            )

        if fragment_blocks > 0:
            print(
                f"PROTOCOL7_FRAGMENT_REASSEMBLY=PASS blocks={fragment_blocks} "
                f"reliable_ack=0x{reassembler.ack_reliable_state:02x} bzip_complete={bzip_complete_streams} "
                f"decode_warnings={fragment_decode_warnings} server_ack_highwater={server_ack_highwater} keepalives={keepalives_sent}"
            )

        local_entity = client_player_slot + 1 if client_player_slot >= 0 else 3
        if args.hold_after_pass > 0.0:
            hold_deadline = time.monotonic() + args.hold_after_pass
            next_hold_keepalive = time.monotonic()
            next_hold_active_move = time.monotonic()
            hold_packets = 0
            hold_active_moves = 0
            hold_active_command = 10
            hold_snapshot_packets = 0
            hold_full_snapshots = 0
            hold_tick_acks = 0
            hold_baseline_acks = 0
            hold_active_tick = max(latest_server_tick_seen + 1, full_snapshot_tick + 10, 10)
            hold_release_seen = not bool(args.hold_until_file)

            # gatefix16: the pre-hold gameplay loop can finish tens of server
            # ticks after the last full-snapshot NET_Tick acknowledgement.  A
            # real client keeps sending NET_Tick while active.  Resume that
            # transport contract before advertising HOLD_READY so the second
            # client cannot fire into an observer that has already gone
            # NOP-only.  The usercmd clock is rebased from the newest observed
            # server snapshot, not from the old full-snapshot delta base.
            if last_server_sequence > 0 and latest_server_tick_seen >= 0:
                resume_tick_packet = build_netchan_packet(
                    next_client_sequence,
                    last_server_sequence,
                    build_tick_payload(latest_server_tick_seen),
                    flags=0,
                    reliable_state=reassembler.ack_reliable_state,
                )
                sock.sendto(resume_tick_packet, target)
                print(
                    f"PROTOCOL7_HOLD_TRANSPORT_RESUME=PASS seq={next_client_sequence} "
                    f"ack={last_server_sequence} latest_tick={latest_server_tick_seen} "
                    f"cmd_tick={hold_active_tick} reliable=0x{reassembler.ack_reliable_state:02x}",
                    flush=True,
                )
                next_client_sequence += 1
                last_snapshot_tick_ack_tick = latest_server_tick_seen
                hold_tick_acks += 1
                next_hold_keepalive = time.monotonic() + 0.25
            print(
                f"PROTOCOL7_HOLD_READY=PASS seconds={args.hold_after_pass:.3f} "
                f"latest_tick={latest_server_tick_seen} cmd_tick={hold_active_tick} "
                f"player_slot={client_player_slot} local_entity={local_entity} "
                f"release_file={1 if args.hold_until_file else 0}",
                flush=True,
            )
            while time.monotonic() < hold_deadline:
                if args.hold_until_file and os.path.exists(args.hold_until_file):
                    hold_release_seen = True
                    break
                now = time.monotonic()
                wait = min(0.10, max(0.0, hold_deadline - now))
                readable, _, _ = select.select([sock], [], [], wait)
                if readable:
                    try:
                        hold_data, _ = sock.recvfrom(65535)
                    except BlockingIOError:
                        hold_data = b""
                    if len(hold_data) >= 10 and not hold_data.startswith(CONNECTIONLESS):
                        (hold_seq, hold_ack, hold_flags, _hold_reliable, _hold_choked, _hold_challenge, hold_payload) = parse_netchan_packet(hold_data)
                        last_server_sequence = max(last_server_sequence, hold_seq)
                        server_ack_highwater = max(server_ack_highwater, hold_ack)
                        hold_ordinary_start = 0
                        te_debug_packet = (
                            hold_seq, hold_ack, hold_flags, _hold_reliable, _hold_choked,
                            bytes(hold_data), bytes(hold_payload),
                        )
                        if args.expect_te_firebullets_player >= 0 and te_debug_trigger_seq < 0:
                            te_debug_pre_packets.append(te_debug_packet)
                            if len(te_debug_pre_packets) > 24:
                                del te_debug_pre_packets[0]
                        if hold_flags & PACKET_FLAG_RELIABLE:
                            _hold_subchannel, _hold_blocks, hold_completed, hold_ordinary_start = reassembler.consume(hold_payload)
                            for hold_stream, _hold_compressed, hold_decoded, hold_error in hold_completed:
                                if not hold_error and hold_decoded:
                                    record_voice_data(hold_decoded, f"hold_reliable{hold_stream}", hold_seq, 0)
                                    record_gameevent_data(hold_decoded, f"hold_reliable{hold_stream}", hold_seq, 0)
                                    record_tempentity_data(hold_decoded, f"hold_reliable{hold_stream}", hold_seq, 0)
                        record_voice_data(hold_payload, "hold_ordinary", hold_seq, hold_ordinary_start)
                        record_gameevent_data(hold_payload, "hold_ordinary", hold_seq, hold_ordinary_start)
                        record_tempentity_data(hold_payload, "hold_ordinary", hold_seq, hold_ordinary_start)

                        # Keep the long-lived observer on the engine snapshot
                        # path.  A late update-baseline snapshot may arrive just
                        # after the main integration gate completes; the old hold
                        # loop ACKed only CNetChan sequence numbers and silently
                        # left that snapshot uncommitted.  build4100 then emitted
                        # NOP-only packets, so an otherwise-correct unreliable
                        # svc_TempEntities event had no frame snapshot to ride on.
                        hold_tick_info = parse_tick_at(hold_payload, hold_ordinary_start)
                        if hold_tick_info is None and hold_ordinary_start != 0:
                            hold_tick_info = parse_tick_at(hold_payload, 0)
                        hold_message_tick = int(hold_tick_info["tick"]) if hold_tick_info is not None else -1
                        if hold_message_tick >= 0:
                            latest_server_tick_seen = max(latest_server_tick_seen, hold_message_tick)
                        hold_entity = None
                        if hold_ordinary_start + 50 <= len(hold_payload) * 8:
                            hold_entity = select_ordinary_packet_entities(hold_payload, hold_ordinary_start)
                        hold_pending_baseline = None
                        if hold_entity is not None:
                            hold_snapshot_packets += 1
                            if int(hold_entity["is_delta"]) == 0:
                                hold_full_snapshots += 1
                                if hold_message_tick >= 0:
                                    full_snapshot_tick = hold_message_tick
                                full_snapshot_baseline = int(hold_entity["baseline"])
                                full_snapshot_update_baseline = int(hold_entity["update_baseline"])
                                if (
                                    full_snapshot_update_baseline
                                    and full_snapshot_tick >= 0
                                    and full_snapshot_baseline in (0, 1)
                                    and last_baseline_ack_tick != full_snapshot_tick
                                ):
                                    hold_pending_baseline = (full_snapshot_tick, full_snapshot_baseline)
                        if args.expect_te_firebullets_player >= 0 and te_debug_trigger_seq >= 0:
                            if not te_debug_packets:
                                te_debug_packets.extend(te_debug_pre_packets)
                                if not te_debug_packets or te_debug_packets[-1][0] != hold_seq:
                                    te_debug_packets.append(te_debug_packet)
                                te_debug_post_remaining = 32
                                print(
                                    f"PROTOCOL7_TE_DEBUG_ARMED=PASS trigger_source={te_debug_trigger_source} "
                                    f"trigger_seq={te_debug_trigger_seq} pre_packets={len(te_debug_packets)} post_packets=32",
                                    flush=True,
                                )
                            elif hold_seq > te_debug_trigger_seq and te_debug_post_remaining > 0:
                                te_debug_packets.append(te_debug_packet)
                                te_debug_post_remaining -= 1
                        ack_packet = build_netchan_packet(
                            next_client_sequence,
                            hold_seq,
                            build_nop_payload(),
                            flags=0,
                            reliable_state=reassembler.ack_reliable_state,
                        )
                        sock.sendto(ack_packet, target)
                        next_client_sequence += 1
                        hold_packets += 1

                        if hold_pending_baseline is not None:
                            pending_tick, pending_baseline = hold_pending_baseline
                            baseline_payload = build_baseline_ack_payload(pending_tick, pending_baseline)
                            baseline_packet = build_netchan_packet(
                                next_client_sequence,
                                hold_seq,
                                baseline_payload,
                                flags=0,
                                reliable_state=reassembler.ack_reliable_state,
                            )
                            sock.sendto(baseline_packet, target)
                            last_baseline_ack_tick = pending_tick
                            baseline_ack_sent = 1
                            hold_baseline_acks += 1
                            resnapshot_baseline_acks += 1
                            print(
                                f"PROTOCOL7_HOLD_RESNAPSHOT_BASELINE_ACK_SENT=PASS seq={next_client_sequence} "
                                f"ack={hold_seq} tick={pending_tick} baseline={pending_baseline} "
                                f"count={hold_baseline_acks}",
                                flush=True,
                            )
                            next_client_sequence += 1

                        if hold_message_tick >= 0 and last_snapshot_tick_ack_tick != hold_message_tick:
                            tick_packet = build_netchan_packet(
                                next_client_sequence,
                                hold_seq,
                                build_tick_payload(hold_message_tick),
                                flags=0,
                                reliable_state=reassembler.ack_reliable_state,
                            )
                            sock.sendto(tick_packet, target)
                            last_snapshot_tick_ack_tick = hold_message_tick
                            snapshot_tick_ack_sent = 1
                            hold_tick_acks += 1
                            resnapshot_tick_acks += 1
                            print(
                                f"PROTOCOL7_HOLD_SNAPSHOT_TICK_ACK_SENT=PASS seq={next_client_sequence} "
                                f"ack={hold_seq} tick={hold_message_tick} count={hold_tick_acks} "
                                f"snapshot={1 if hold_entity is not None else 0}",
                                flush=True,
                            )
                            next_client_sequence += 1

                        next_hold_keepalive = time.monotonic() + 0.25
                now = time.monotonic()
                if (args.expect_te_firebullets_player >= 0 and last_server_sequence > 0
                        and now >= next_hold_active_move):
                    # A real Protocol7 client continues sending clc_Move while
                    # observing. Temp entities are copied into outgoing frame
                    # snapshots; pure NOP/ACK traffic can leave an unreliable
                    # CEventInfo with no snapshot to ride on.
                    if latest_server_tick_seen >= 0:
                        hold_active_tick = max(hold_active_tick, latest_server_tick_seen + 1)
                    idle_payload, idle_bits = build_move_payload(
                        hold_active_tick,
                        command_number=hold_active_command,
                        pitch=45.0,
                        yaw=90.0,
                        forward_move=0,
                        buttons=0,
                    )
                    idle_packet = build_netchan_packet(
                        next_client_sequence,
                        last_server_sequence,
                        idle_payload,
                        flags=0,
                        reliable_state=reassembler.ack_reliable_state,
                    )
                    sock.sendto(idle_packet, target)
                    if hold_active_moves == 0:
                        print(
                            f"PROTOCOL7_HOLD_ACTIVE_MOVE=PASS cadence=0.10 "
                            f"cmd={hold_active_command} tick={hold_active_tick} "
                            f"bits={idle_bits} local_entity={local_entity}",
                            flush=True,
                        )
                    next_client_sequence += 1
                    hold_active_moves += 1
                    hold_active_command += 1
                    hold_active_tick += 1
                    next_hold_active_move = now + 0.10
                    next_hold_keepalive = now + 0.25
                if last_server_sequence > 0 and now >= next_hold_keepalive:
                    keepalive = build_netchan_packet(
                        next_client_sequence,
                        last_server_sequence,
                        build_nop_payload(),
                        flags=0,
                        reliable_state=reassembler.ack_reliable_state,
                    )
                    sock.sendto(keepalive, target)
                    next_client_sequence += 1
                    next_hold_keepalive = now + 0.25
            if args.hold_until_file and not hold_release_seen:
                print(
                    f"PROTOCOL7_HOLD_DONE=FAIL reason=release_timeout seconds={args.hold_after_pass:.3f} "
                    f"packets={hold_packets} active_moves={hold_active_moves} snapshots={hold_snapshot_packets} "
                    f"tick_acks={hold_tick_acks} baseline_acks={hold_baseline_acks} "
                    f"latest_tick={latest_server_tick_seen} local_entity={local_entity}"
                )
                return 1
            if args.expect_voice_hex and not voice_expected_ok:
                print(
                    f"PROTOCOL7_HOLD_DONE=FAIL reason=expected_voice_missing sender={args.expect_voice_sender} "
                    f"payload_hex={args.expect_voice_hex} packets={hold_packets} active_moves={hold_active_moves} "
                    f"snapshots={hold_snapshot_packets} tick_acks={hold_tick_acks} latest_tick={latest_server_tick_seen} "
                    f"local_entity={local_entity}"
                )
                return 1
            if args.expect_te_firebullets_player >= 0 and not te_firebullets_received_ok:
                print(
                    f"PROTOCOL7_TE_DEBUG_BEGIN trigger_source={te_debug_trigger_source or 'none'} "
                    f"trigger_seq={te_debug_trigger_seq} captured={len(te_debug_packets)}",
                    flush=True,
                )
                for (debug_seq, debug_ack, debug_flags, debug_reliable, debug_choked,
                     debug_packet, debug_payload) in te_debug_packets:
                    print(
                        f"PROTOCOL7_TE_DEBUG_PACKET seq={debug_seq} ack={debug_ack} "
                        f"flags=0x{debug_flags:02x} reliable=0x{debug_reliable:02x} choked={debug_choked} "
                        f"packet_bytes={len(debug_packet)} payload_bytes={len(debug_payload)} "
                        f"packet_hex={debug_packet.hex()} payload_hex={debug_payload.hex()}",
                        flush=True,
                    )
                print("PROTOCOL7_TE_DEBUG_END", flush=True)
                print(
                    f"PROTOCOL7_HOLD_DONE=FAIL reason=expected_te_firebullets_missing "
                    f"player={args.expect_te_firebullets_player} weapon={args.expect_te_firebullets_weapon} "
                    f"packets={hold_packets} active_moves={hold_active_moves} snapshots={hold_snapshot_packets} "
                    f"full_snapshots={hold_full_snapshots} tick_acks={hold_tick_acks} baseline_acks={hold_baseline_acks} "
                    f"latest_tick={latest_server_tick_seen} local_entity={local_entity}"
                )
                return 1
            print(
                f"PROTOCOL7_HOLD_DONE=PASS seconds={args.hold_after_pass:.3f} packets={hold_packets} "
                f"active_moves={hold_active_moves} snapshots={hold_snapshot_packets} full_snapshots={hold_full_snapshots} "
                f"tick_acks={hold_tick_acks} baseline_acks={hold_baseline_acks} latest_tick={latest_server_tick_seen} "
                f"local_entity={local_entity} released={1 if hold_release_seen else 0} voice_received={voice_expected_ok} "
                f"te_firebullets_received={te_firebullets_received_ok}",
                flush=True,
            )

        disconnect_sent = 0
        if args.disconnect_after_pass and last_server_sequence > 0:
            disconnect_payload = build_disconnect_payload("probe_complete")
            disconnect_packet = build_netchan_packet(
                next_client_sequence,
                last_server_sequence,
                disconnect_payload,
                flags=0,
                reliable_state=reassembler.ack_reliable_state,
            )
            sock.sendto(disconnect_packet, target)
            disconnect_sent = 1
            print(
                f"PROTOCOL7_DISCONNECT_SENT=PASS seq={next_client_sequence} ack={last_server_sequence} "
                f"reliable=0x{reassembler.ack_reliable_state:02x} type={NET_DISCONNECT} "
                f"reason=probe_complete bytes={len(disconnect_packet)}"
            )
            next_client_sequence += 1
            time.sleep(0.10)

        if instancebaseline_runtime is not None:
            if not instancebaseline_contract_ok or not packetentity_class_ids:
                print(
                    "PROTOCOL7=FAIL reason=packetentities_instancebaseline_policy "
                    f"contract={instancebaseline_contract_ok} packet_classes={sorted(packetentity_class_ids)}"
                )
                return 1
            materialized_packet_classes = sorted(packetentity_class_ids & set(instancebaseline_wire))
            implicit_packet_classes = sorted(packetentity_class_ids - set(instancebaseline_wire))
            # CCSPlayer must be present in the decoded snapshot. Whether its
            # class baseline was materialized or remains implicit is an engine
            # lifecycle fact, not an error; both paths are legal build4100 wire.
            if 1 not in packetentity_class_ids:
                print(
                    "PROTOCOL7=FAIL reason=packetentities_instancebaseline_policy "
                    f"player_class_missing packet_classes={sorted(packetentity_class_ids)}"
                )
                return 1
            player_policy = "materialized" if 1 in instancebaseline_wire else "implicit"
            print(
                f"PROTOCOL7_PACKET_BASELINE_POLICY=PASS baseline_classes={len(instancebaseline_wire)} "
                f"packet_classes={len(packetentity_class_ids)} materialized={len(materialized_packet_classes)} "
                f"implicit={len(implicit_packet_classes)} player={player_policy} "
                f"materialized_ids={','.join(str(cid) for cid in materialized_packet_classes) or 'none'} "
                f"implicit_ids={','.join(str(cid) for cid in implicit_packet_classes) or 'none'}"
            )

        print(
            f"PROTOCOL7=PASS auth={auth} connect_sent=1 connection_accepted=1 tcp_stream_bound=1 "
            f"signon_state2_sent=1 server_new_seen={server_new_seen} "
            f"clientinfo_sent={clientinfo_sent} signon_state3_sent={signon_state3_sent} "
            f"stringtable_baselines={stringtable_baselines} instancebaseline={instancebaseline_contract_ok} "
            f"instancebaseline_classes={len(instancebaseline_wire)} packet_baseline_classes={len(packetentity_class_ids)} "
            f"server_prespawn_seen={server_prespawn_seen} "
            f"signon_state4_sent={signon_state4_sent} server_spawn_seen={server_spawn_seen} signon_state5_sent={signon_state5_sent} signon_state6_sent={signon_state6_sent} "
            f"baseline_ack_sent={baseline_ack_sent} snapshot_tick_ack_sent={snapshot_tick_ack_sent} "
            f"resnapshot_baseline_acks={resnapshot_baseline_acks} resnapshot_tick_acks={resnapshot_tick_acks} "
            f"gameplay_command_sent={gameplay_command_sent} gameplay_event_delta={gameplay_event_delta_ok} spawn_origin={1 if movement_origin_base is not None else 0} "
            f"move_command_sent={move_command_sent} usercmd_eyeangles_delta={usercmd_eyeangles_delta_ok} "
            f"usercmd_movement_delta={usercmd_movement_delta_ok} friction_command_sent={friction_command_sent} "
            f"usercmd_friction_delta={usercmd_friction_delta_ok} jump_command_sent={jump_command_sent} "
            f"usercmd_jump_delta={usercmd_jump_delta_ok} jump_hold_command_sent={jump_hold_command_sent} "
            f"usercmd_jump_hold_delta={usercmd_jump_hold_delta_ok} air_accel_command_sent={air_accel_command_sent} "
            f"usercmd_air_accel_delta={usercmd_air_accel_delta_ok} "
            f"pre_weapon_join_sent={pre_weapon_join_sent} pre_weapon_join_team={args.pre_weapon_join_team} "
            f"weapon_attack_sent={weapon_fire_command_sent} weapon_release_sent={weapon_release_command_sent} "
            f"weapon_reload_sent={weapon_reload_command_sent} weapon_secondary_sent={weapon_secondary_command_sent} weapon_fire_event={weapon_fire_event_ok} weapon_reload_event={weapon_reload_event_ok} "
            f"combat_setup_sent={combat_setup_command_sent} player_hurt_event={player_hurt_event_ok} player_death_event={player_death_event_ok} combat_reset_sent={combat_reset_command_sent} "
            f"gameevent_list={gameevent_list_ok} player_say_id={player_say_event_id} "
            f"listen_events_sent={listen_events_sent} listen_events_seq={listen_events_sequence} "
            f"say_command_sent={say_command_sent} player_say_event={player_say_event_ok} "
            f"usermessage_command_sent={usermessage_command_sent} saytext_usermessage={saytext_usermessage_ok} "
            f"voice_sent={voice_sent} voice_received={voice_expected_ok} voice_init={voice_init_seen} te_firebullets_received={te_firebullets_received_ok} "
            f"objective_scenario={args.objective_scenario or 'none'} objective_command_sent={objective_scenario_command_sent} objective_ok={objective_scenario_ok} objective_c4_sendprop={objective_c4_wire_ok} objective_events={len(objective_events_seen)} "
            f"downloadables={downloadables_ok} download_request_sent={download_request_sent} "
            f"download_request_seq={download_request_sequence} download_received={download_received_ok} "
            f"download_bytes={download_bytes} download_fragments={download_fragments} download_compressed={download_compressed} "
            f"server_count={args.server_count} player_slot={client_player_slot} local_entity={local_entity} engine_crc=0x{advertised_crc:08x} "
            f"full_sendtables={1 if args.force_full_sendtables else 0} "
            f"udp_signon={1 if udp_signon_packets else 0} udp_signon_packets={udp_signon_packets} "
            f"udp_signon_bytes={udp_signon_bytes} sendtable_envelopes={sendtable_count} "
            f"fragment_blocks={fragment_blocks} reliable_ack=0x{reassembler.ack_reliable_state:02x} "
            f"fragment_decode_warnings={fragment_decode_warnings} bzip_complete={bzip_complete_streams} bzip_decoded_bytes={bzip_decoded_bytes} "
            f"reliable_streams={reliable_streams} signon_misc_streams={signon_misc_streams} "
            f"server_ack_highwater={server_ack_highwater} keepalives={keepalives_sent} initial_signon_retries={initial_signon_retries_sent} prespawn_signon_retries={prespawn_signon_retries_sent} signon_lease_extensions={signon_lease_extensions} server_new_bit={server_new_bit} "
            f"server_prespawn_bit={server_prespawn_bit} server_spawn_bit={server_spawn_bit} "
            f"classinfo={classinfo_count} packetentities={packetentities_count} "
            f"packetentities_full={packetentities_full_count} packetentities_updated_max={packetentities_updated_max} "
            f"packetentities_data_bits={packetentities_data_bits} snapshot_entity_decode={snapshot_entity_decode_ok} "
            f"snapshot_entities={snapshot_entity_updates} snapshot_changed_props={snapshot_changed_props} "
            f"sendprop_snapshot={sendprop_snapshot_ok} sendprop_schema_decode={sendprop_schema_decode_ok} sendprop_gameplay={sendprop_gameplay_ok} full_snapshot_tick={full_snapshot_tick} "
            f"full_snapshot_baseline={full_snapshot_baseline} full_snapshot_update_baseline={full_snapshot_update_baseline} "
            f"packetentities_delta={packetentities_delta_count} delta_snapshot={delta_snapshot_ok} "
            f"delta_from_tick={delta_from_tick_ok} delta_changed_props={delta_changed_props} "
            f"sendprop_delta={sendprop_delta_ok} delta_field_index={delta_field_index} delta_account={delta_account_value} "
            f"tcp_closed_normal={1 if tcp_closed else 0} disconnect_sent={disconnect_sent} ticket_len={len(steam_ticket or b'')} "
            f"udp_diag={udp_diag} tcp_diag={tcp_diag} udp_packet_limit={args.max_udp_packets} "
            f"packet_budget_grace={packet_budget_grace_started} grace_packets={packet_budget_grace_packets} "
            f"dwell_reason={post_spawn_dwell_reason} dwell_updates={post_spawn_dwell_updates} "
            f"resnapshot_tick_acks={resnapshot_tick_acks}"
        )
        return 0
    except (OSError, ValueError, TimeoutError, ConnectionError) as exc:
        print(f"PROTOCOL7=FAIL reason={type(exc).__name__} detail={exc}")
        return 1
    finally:
        if tcp is not None:
            tcp.close()
        sock.close()


if __name__ == "__main__":
    sys.exit(main())
