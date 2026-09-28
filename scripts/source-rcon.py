#!/usr/bin/env python3
import argparse
import socket
import struct
import sys

REVISION = "v1.8.27d"
SERVERDATA_RESPONSE_VALUE = 0
SERVERDATA_EXECCOMMAND = 2
SERVERDATA_AUTH_RESPONSE = 2
SERVERDATA_AUTH = 3
MAX_PACKET_SIZE = 1024 * 1024


def build_packet(request_id: int, packet_type: int, body: str) -> bytes:
    if "\x00" in body or any(ch in body for ch in "\r\n"):
        raise ValueError("invalid packet body")
    payload = struct.pack("<ii", request_id, packet_type) + body.encode("latin1") + b"\x00\x00"
    return struct.pack("<i", len(payload)) + payload


def parse_packet(data: bytes):
    if len(data) < 14:
        raise ValueError("short packet")
    size = struct.unpack_from("<i", data, 0)[0]
    if size < 10 or size > MAX_PACKET_SIZE:
        raise ValueError(f"invalid packet size {size}")
    if len(data) != size + 4:
        raise ValueError(f"packet length mismatch size={size} bytes={len(data)}")
    request_id, packet_type = struct.unpack_from("<ii", data, 4)
    payload = data[12:]
    if len(payload) < 2 or payload[-2:] != b"\x00\x00":
        raise ValueError("packet terminator missing")
    body = payload[:-2].decode("latin1", "replace")
    return request_id, packet_type, body


def recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = []
    remaining = size
    while remaining:
        chunk = sock.recv(remaining)
        if not chunk:
            raise ConnectionError("connection closed")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def recv_packet(sock: socket.socket):
    header = recv_exact(sock, 4)
    size = struct.unpack("<i", header)[0]
    if size < 10 or size > MAX_PACKET_SIZE:
        raise ValueError(f"invalid packet size {size}")
    raw = header + recv_exact(sock, size)
    return parse_packet(raw)


def self_test() -> int:
    auth = build_packet(41, SERVERDATA_AUTH, "pw")
    assert parse_packet(auth) == (41, SERVERDATA_AUTH, "pw")
    command = build_packet(42, SERVERDATA_EXECCOMMAND, "changelevel cs_office")
    assert parse_packet(command) == (42, SERVERDATA_EXECCOMMAND, "changelevel cs_office")
    auth_reply = struct.pack("<i", 10) + struct.pack("<ii", 41, SERVERDATA_AUTH_RESPONSE) + b"\x00\x00"
    assert parse_packet(auth_reply) == (41, SERVERDATA_AUTH_RESPONSE, "")
    print(f"SOURCE_RCON_SELFTEST=PASS revision={REVISION} transport=tcp auth=packet command=packet")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Minimal Source build4100 TCP RCON client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=27015)
    ap.add_argument("--password")
    ap.add_argument("--command")
    ap.add_argument("--timeout", type=float, default=2.0)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.password or not args.command:
        ap.error("--password and --command are required")
    if args.port <= 0 or args.port > 65535 or args.timeout <= 0:
        ap.error("invalid --port/--timeout")
    if any(ch in args.password for ch in "\x00\r\n"):
        ap.error("invalid password")
    command = args.command.strip()
    if not command or any(ch in command for ch in "\x00\r\n"):
        ap.error("invalid command")

    auth_id = 0x4100
    command_id = auth_id + 1
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(args.timeout)
    try:
        sock.connect((args.host, args.port))
        sock.sendall(build_packet(auth_id, SERVERDATA_AUTH, args.password))

        auth_ok = False
        # Source RCON may emit an empty RESPONSE_VALUE before AUTH_RESPONSE.
        for _ in range(3):
            reply_id, reply_type, _ = recv_packet(sock)
            if reply_type == SERVERDATA_AUTH_RESPONSE:
                if reply_id == -1:
                    raise PermissionError("authentication rejected")
                if reply_id != auth_id:
                    raise ValueError(f"unexpected auth id {reply_id}")
                auth_ok = True
                break
        if not auth_ok:
            raise ValueError("auth response missing")

        sock.sendall(build_packet(command_id, SERVERDATA_EXECCOMMAND, command))
        reply_text = "none"
        try:
            reply_id, reply_type, reply_body = recv_packet(sock)
            if reply_id not in (command_id, -1):
                raise ValueError(f"unexpected command reply id {reply_id}")
            if reply_id == -1:
                raise PermissionError("command rejected")
            text = reply_body.replace("\r", " ").replace("\n", " ").strip()
            reply_text = (text[:160] if text else f"type{reply_type}_empty").replace(" ", "_")
        except (socket.timeout, ConnectionError):
            # changelevel can close/recycle the connection before a command reply.
            # The LevelShutdown/LevelInit/ServerActivate callbacks are authoritative.
            reply_text = "disconnect_or_timeout_allowed"

        command_tag = command.replace(" ", "_")
        print(
            f"SOURCE_RCON=PASS revision={REVISION} transport=tcp source={args.host}:{args.port} "
            f"auth=PASS command={command_tag} reply={reply_text}"
        )
        return 0
    except (OSError, ValueError, PermissionError) as exc:
        print(f"SOURCE_RCON=FAIL revision={REVISION} reason={type(exc).__name__}:{exc}", file=sys.stderr)
        return 1
    finally:
        sock.close()


if __name__ == "__main__":
    raise SystemExit(main())
