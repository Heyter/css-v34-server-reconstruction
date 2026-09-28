#!/usr/bin/env python3
import argparse
import json
import re
import socket
import struct
import sys
import time
from pathlib import Path

CONNECTIONLESS = b"\xff\xff\xff\xff"
A2S_INFO = b"TSource Engine Query\x00"
S2A_INFO = 0x49
S2C_CHALLENGE = 0x41
SERVERDATA_RESPONSE_VALUE = 0
SERVERDATA_EXECCOMMAND = 2
SERVERDATA_AUTH_RESPONSE = 2
SERVERDATA_AUTH = 3
MAX_PACKET = 1024 * 1024


class StageError(RuntimeError):
    def __init__(self, stage: str, exc: Exception):
        self.stage = stage
        self.exc = exc
        super().__init__(f"{stage}:{type(exc).__name__}:{exc}")


def stage_call(stage: str, fn, *args, **kwargs):
    try:
        return fn(*args, **kwargs)
    except (OSError, ValueError, TimeoutError, ConnectionError, PermissionError) as exc:
        raise StageError(stage, exc) from exc


def cstring(data: bytes, off: int):
    end = data.find(b"\x00", off)
    if end < 0:
        raise ValueError("unterminated cstring")
    return data[off:end].decode("latin1", "replace"), end + 1


def a2s_info(host: str, port: int, timeout: float):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        request = CONNECTIONLESS + A2S_INFO
        sock.sendto(request, (host, port))
        data, _ = sock.recvfrom(65535)
        if len(data) >= 9 and data[:4] == CONNECTIONLESS and data[4] == S2C_CHALLENGE:
            challenge = data[5:9]
            sock.sendto(request + challenge, (host, port))
            data, _ = sock.recvfrom(65535)
        if len(data) < 6 or data[:4] != CONNECTIONLESS or data[4] != S2A_INFO:
            raise ValueError(f"unexpected A2S_INFO response head={data[:16].hex()}")
        off = 5
        protocol = data[off]
        off += 1
        name, off = cstring(data, off)
        map_name, off = cstring(data, off)
        folder, off = cstring(data, off)
        game, off = cstring(data, off)
        if off + 8 > len(data):
            raise ValueError("short A2S_INFO fixed fields")
        app_id = struct.unpack_from("<H", data, off)[0]
        off += 2
        players, max_players, bots = data[off], data[off + 1], data[off + 2]
        off += 3
        server_type = chr(data[off]); off += 1
        environment = chr(data[off]); off += 1
        visibility = data[off]; off += 1
        vac = data[off]; off += 1
        version, off = cstring(data, off)
        return {
            "protocol": protocol,
            "name": name,
            "map": map_name,
            "folder": folder,
            "game": game,
            "app_id": app_id,
            "players": players,
            "max_players": max_players,
            "bots": bots,
            "server_type": server_type,
            "environment": environment,
            "visibility": visibility,
            "vac": vac,
            "version": version,
        }
    finally:
        sock.close()


def rcon_packet(request_id: int, packet_type: int, body: str) -> bytes:
    payload = struct.pack("<ii", request_id, packet_type) + body.encode("latin1") + b"\x00\x00"
    return struct.pack("<i", len(payload)) + payload


def recv_exact(sock: socket.socket, size: int) -> bytes:
    out = bytearray()
    while len(out) < size:
        chunk = sock.recv(size - len(out))
        if not chunk:
            raise ConnectionError("rcon closed")
        out.extend(chunk)
    return bytes(out)


def recv_rcon(sock: socket.socket):
    size = struct.unpack("<i", recv_exact(sock, 4))[0]
    if size < 10 or size > MAX_PACKET:
        raise ValueError(f"bad rcon size={size}")
    payload = recv_exact(sock, size)
    request_id, packet_type = struct.unpack_from("<ii", payload, 0)
    if payload[-2:] != b"\x00\x00":
        raise ValueError("bad rcon terminator")
    return request_id, packet_type, payload[8:-2].decode("latin1", "replace")


def rcon_auth(host: str, port: int, password: str, timeout: float):
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    auth_id = 0x4100
    sock.sendall(rcon_packet(auth_id, SERVERDATA_AUTH, password))
    for _ in range(4):
        rid, typ, _ = recv_rcon(sock)
        if typ == SERVERDATA_AUTH_RESPONSE:
            if rid != auth_id:
                sock.close()
                raise PermissionError("rcon auth rejected")
            return sock
    sock.close()
    raise ValueError("rcon auth response missing")


def rcon_command(sock: socket.socket, command: str, request_id: int):
    sock.sendall(rcon_packet(request_id, SERVERDATA_EXECCOMMAND, command))
    chunks = []
    base_timeout = sock.gettimeout() or 2.0
    first_deadline = time.monotonic() + max(2.0, float(base_timeout))
    while time.monotonic() < first_deadline:
        try:
            rid, typ, body = recv_rcon(sock)
        except socket.timeout:
            if chunks:
                break
            continue
        if rid not in (request_id, -1):
            continue
        if rid == -1:
            raise PermissionError(f"rcon command rejected: {command}")
        if typ in (SERVERDATA_RESPONSE_VALUE, SERVERDATA_AUTH_RESPONSE):
            chunks.append(body)
            sock.settimeout(0.15)
            try:
                while True:
                    rid2, typ2, body2 = recv_rcon(sock)
                    if rid2 == request_id and typ2 in (SERVERDATA_RESPONSE_VALUE, SERVERDATA_AUTH_RESPONSE):
                        chunks.append(body2)
            except socket.timeout:
                pass
            finally:
                sock.settimeout(base_timeout)
            break
    if not chunks:
        raise TimeoutError(f"rcon no reply: {command}")
    return "".join(chunks)


def parse_status(text: str):
    def line_value(key: str):
        m = re.search(rf"(?mi)^\s*{re.escape(key)}\s*:\s*(.+?)\s*$", text)
        return m.group(1).strip() if m else ""

    map_value = line_value("map")
    map_name = map_value.split()[0] if map_value else ""
    players_value = line_value("players")
    players = -1
    max_players = -1
    m = re.search(r"(\d+)\s*\(\s*(\d+)\s+max", players_value)
    if m:
        players, max_players = int(m.group(1)), int(m.group(2))
    return {
        "hostname": line_value("hostname"),
        "version": line_value("version"),
        "map": map_name,
        "players": players,
        "max_players": max_players,
    }


def parse_cvar(name: str, text: str):
    patterns = [
        rf'(?mi)^\s*"?{re.escape(name)}"?\s*=\s*"([^"]*)"',
        rf'(?mi)^\s*{re.escape(name)}\s+"([^"]*)"',
    ]
    for pattern in patterns:
        m = re.search(pattern, text)
        if m:
            return m.group(1)
    compact = " ".join(text.replace("\r", " ").replace("\n", " ").split())
    return compact[:240]


def probe(host: str, port: int, password: str, timeout: float):
    info = stage_call("a2s", a2s_info, host, port, timeout)
    sock = stage_call("rcon_auth", rcon_auth, host, port, password, timeout)
    try:
        status_text = stage_call("rcon_status", rcon_command, sock, "status", 0x4200)
        cvars = {}
        for index, name in enumerate(("sv_lan", "sv_sendtables", "sv_voiceenable", "bot_quota", "mp_autoteambalance", "mp_limitteams"), 1):
            text = stage_call(f"rcon_cvar_{name}", rcon_command, sock, name, 0x4200 + index)
            cvars[name] = parse_cvar(name, text)
    finally:
        sock.close()
    return {"a2s": info, "status": parse_status(status_text), "cvars": cvars}


def stable_view(snapshot: dict):
    a = snapshot["a2s"]
    s = snapshot["status"]
    # Player totals can move by one while the two servers finish bot creation.
    # We still validate their range separately, but parity is based on stable
    # externally visible protocol/game/runtime properties.
    return {
        "a2s": {k: a[k] for k in (
            "protocol", "name", "map", "folder", "game", "app_id", "max_players",
            "server_type", "environment", "visibility", "vac", "version"
        )},
        "status": {k: s[k] for k in ("hostname", "version", "map", "max_players")},
        "cvars": snapshot["cvars"],
    }


def validate(snapshot: dict, label: str):
    a = snapshot["a2s"]
    s = snapshot["status"]
    errors = []
    if a["protocol"] != 7:
        errors.append(f"{label}: protocol={a['protocol']} expected=7")
    if a["app_id"] != 240:
        errors.append(f"{label}: app_id={a['app_id']} expected=240")
    if a["map"] != "de_dust2":
        errors.append(f"{label}: a2s_map={a['map']} expected=de_dust2")
    if a["max_players"] != 4:
        errors.append(f"{label}: a2s_max={a['max_players']} expected=4")
    if not (0 <= a["players"] <= a["max_players"]):
        errors.append(f"{label}: a2s_players={a['players']} invalid")
    if not (0 <= a["bots"] <= a["players"]):
        errors.append(f"{label}: a2s_bots={a['bots']} invalid players={a['players']}")
    if s["map"] != "de_dust2":
        errors.append(f"{label}: status_map={s['map']} expected=de_dust2")
    if s["max_players"] not in (-1, 4):
        errors.append(f"{label}: status_max={s['max_players']} expected=4")
    # These three are parity-harness invariants: they are deliberately forced
    # for both sides and are required by the protocol/runtime comparison.
    expected_cvars = {
        "sv_lan": "1",
        "sv_sendtables": "1",
        "sv_voiceenable": "1",
    }
    for name, expected in expected_cvars.items():
        if snapshot["cvars"].get(name) != expected:
            errors.append(f"{label}: {name}={snapshot['cvars'].get(name)!r} expected={expected!r}")

    # Gameplay cvars can be overwritten by the image/server.cfg after the
    # command line is parsed.  The original server is the parity baseline, so
    # do not impose guessed values here.  Require readable values and let the
    # final stable_view comparison demand exact original==rebuilt parity.
    for name in ("bot_quota", "mp_autoteambalance", "mp_limitteams"):
        value = snapshot["cvars"].get(name)
        if value is None or value == "":
            errors.append(f"{label}: {name}=missing")
    return errors


def self_test():
    sample = (
        CONNECTIONLESS + bytes([S2A_INFO, 7]) +
        b"CSS Test\x00de_dust2\x00cstrike\x00Counter-Strike: Source\x00" +
        struct.pack("<H", 240) + bytes([2, 4, 2]) + b"dl" + bytes([0, 0]) + b"1.0.0.34\x00"
    )
    # Exercise parsers without network by reproducing the fixed-field walk.
    data = sample
    off = 6
    name, off = cstring(data, off)
    map_name, off = cstring(data, off)
    folder, off = cstring(data, off)
    game, off = cstring(data, off)
    assert (name, map_name, folder, game) == ("CSS Test", "de_dust2", "cstrike", "Counter-Strike: Source")
    status = parse_status("hostname: CSS Test\nversion : 1.0.0.34/7 4100 insecure\nmap : de_dust2 at: 0 x, 0 y, 0 z\nplayers : 2 (4 max)\n")
    assert status["map"] == "de_dust2" and status["players"] == 2 and status["max_players"] == 4
    assert parse_cvar("sv_lan", '"sv_lan" = "1" ( def. "0" )') == "1"

    # parity3 observed the stock image applying server.cfg values after the
    # launch command (bot_quota=4, mp_limitteams=2).  Those values are valid
    # when both original and rebuilt expose them identically; they are not
    # per-side constants.
    runtime_cfg_snapshot = {
        "a2s": {
            "protocol": 7, "name": "CSS Test", "map": "de_dust2",
            "folder": "cstrike", "game": "Counter-Strike: Source",
            "app_id": 240, "players": 2, "max_players": 4, "bots": 2,
            "server_type": "d", "environment": "l", "visibility": 0,
            "vac": 0, "version": "1.0.0.34",
        },
        "status": {
            "hostname": "CSS Test", "version": "1.0.0.34/7 4100 insecure",
            "map": "de_dust2", "players": 2, "max_players": 4,
        },
        "cvars": {
            "sv_lan": "1", "sv_sendtables": "1", "sv_voiceenable": "1",
            "bot_quota": "4", "mp_autoteambalance": "0", "mp_limitteams": "2",
        },
    }
    assert validate(runtime_cfg_snapshot, "selftest") == []
    altered = json.loads(json.dumps(runtime_cfg_snapshot))
    altered["cvars"]["bot_quota"] = "3"
    assert stable_view(runtime_cfg_snapshot) != stable_view(altered)
    print(
        "EXTERNAL_PARITY_PROBE_SELFTEST=PASS "
        "a2s=info rcon=status+cvars normalize=stable runtime_cfg=compare_not_assume"
    )
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("a2s-ready")
    a.add_argument("--host", default="127.0.0.1")
    a.add_argument("--port", type=int, required=True)
    a.add_argument("--timeout", type=float, default=1.0)
    a.add_argument("--expected-app-id", type=int, default=240)
    a.add_argument("--expected-map", default="de_dust2")
    p = sub.add_parser("probe")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, required=True)
    p.add_argument("--password", required=True)
    p.add_argument("--timeout", type=float, default=2.0)
    p.add_argument("--out", required=True)
    c = sub.add_parser("compare")
    c.add_argument("--original", required=True)
    c.add_argument("--rebuilt", required=True)
    sub.add_parser("self-test")
    args = ap.parse_args()

    try:
        if args.cmd == "self-test":
            return self_test()
        if args.cmd == "a2s-ready":
            info = a2s_info(args.host, args.port, args.timeout)
            if info["protocol"] != 7:
                raise ValueError(f"protocol={info['protocol']} expected=7")
            if info["app_id"] != args.expected_app_id:
                raise ValueError(f"app_id={info['app_id']} expected={args.expected_app_id}")
            if info["map"] != args.expected_map:
                raise ValueError(f"map={info['map']} expected={args.expected_map}")
            print(
                "EXTERNAL_A2S_READY=PASS "
                f"host={args.host} port={args.port} protocol={info['protocol']} "
                f"app_id={info['app_id']} map={info['map']} players={info['players']} "
                f"max={info['max_players']} bots={info['bots']}"
            )
            return 0
        if args.cmd == "probe":
            snapshot = probe(args.host, args.port, args.password, args.timeout)
            errors = validate(snapshot, "probe")
            Path(args.out).write_text(json.dumps(snapshot, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            if errors:
                print("EXTERNAL_PARITY_PROBE=FAIL " + " ; ".join(errors))
                return 1
            a = snapshot["a2s"]
            cvars = snapshot["cvars"]
            print(
                "EXTERNAL_PARITY_PROBE=PASS "
                f"protocol={a['protocol']} app_id={a['app_id']} map={a['map']} players={a['players']} "
                f"max={a['max_players']} bots={a['bots']} version={a['version']} "
                f"bot_quota={cvars['bot_quota']} mp_autoteambalance={cvars['mp_autoteambalance']} "
                f"mp_limitteams={cvars['mp_limitteams']} out={args.out}"
            )
            return 0
        original = json.loads(Path(args.original).read_text(encoding="utf-8"))
        rebuilt = json.loads(Path(args.rebuilt).read_text(encoding="utf-8"))
        errors = validate(original, "original") + validate(rebuilt, "rebuilt")
        left = stable_view(original)
        right = stable_view(rebuilt)
        if left != right:
            for section in ("a2s", "status", "cvars"):
                keys = sorted(set(left[section]) | set(right[section]))
                for key in keys:
                    if left[section].get(key) != right[section].get(key):
                        errors.append(
                            f"parity:{section}.{key} original={left[section].get(key)!r} rebuilt={right[section].get(key)!r}"
                        )
        if errors:
            print("ORIGINAL_REBUILT_PARITY=FAIL")
            for item in errors:
                print("PARITY_DIFF " + item)
            return 1
        oa, ra = original["a2s"], rebuilt["a2s"]
        print(
            "ORIGINAL_REBUILT_PARITY=PASS "
            f"protocol={oa['protocol']} app_id={oa['app_id']} map={oa['map']} max={oa['max_players']} "
            f"original_players={oa['players']} rebuilt_players={ra['players']} "
            f"original_bots={oa['bots']} rebuilt_bots={ra['bots']} "
            "a2s=match status=match cvars=match"
        )
        return 0
    except StageError as exc:
        print(
            f"EXTERNAL_PARITY_PROBE=FAIL command={args.cmd} stage={exc.stage} "
            f"reason={type(exc.exc).__name__}:{exc.exc}",
            file=sys.stderr,
        )
        return 2
    except (OSError, ValueError, TimeoutError, ConnectionError, PermissionError, json.JSONDecodeError) as exc:
        print(
            f"EXTERNAL_PARITY_PROBE=FAIL command={args.cmd} stage=command "
            f"reason={type(exc).__name__}:{exc}",
            file=sys.stderr,
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
