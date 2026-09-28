#!/usr/bin/env python3
"""Pre-publication leak audit for the cleaned CS:S v34 project tree."""
from __future__ import annotations

import argparse
import fnmatch
import ipaddress
import os
from pathlib import Path
import re
import subprocess
import sys

SKIP_DIRS = {
    ".git", ".idea", ".vscode", "__pycache__", "build", "out", "reports",
    "sources", "runtime", "dist",
}

SENSITIVE_BASENAMES = {
    ".env", ".netrc", ".npmrc", ".pypirc", "id_rsa", "id_ed25519",
    "credentials", "credentials.json", "secrets.json",
}
SENSITIVE_GLOBS = (
    "*.pem", "*.key", "*.p12", "*.pfx", "*.kdbx", "*.keystore",
)

# 1.0.0.34 is the public CS:S PatchVersion, not an IP literal in this project.
SAFE_VERSION_LITERALS = {"1.0.0.34"}

IPV4_RE = re.compile(r"(?<![0-9.])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![0-9.])")
MAC_RE = re.compile(r"(?i)(?<![0-9a-f])(?:[0-9a-f]{2}[:-]){5}[0-9a-f]{2}(?![0-9a-f])")
EMAIL_RE = re.compile(r"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b")
HOME_PATH_RE = re.compile(
    r"(?:/home/[A-Za-z0-9._-]+(?:/[^\s'\"<>`]*)?|"
    r"/mnt/[A-Za-z]/Users/[A-Za-z0-9._-]+(?:/[^\s'\"<>`]*)?|"
    r"[A-Za-z]:\\Users\\[A-Za-z0-9._-]+(?:\\[^\s'\"<>`]*)?)"
)
PROMPT_RE = re.compile(r"(?m)^[A-Za-z0-9._-]+@[A-Za-z0-9._-]+:[^\n]*[$#]\s")
URL_CREDS_RE = re.compile(r"(?i)https?://[^\s/@:]+:[^\s/@]+@[^\s/]+")

SECRET_PATTERNS = (
    ("private_key", re.compile(r"-----BEGIN (?:RSA |EC |DSA |OPENSSH )?PRIVATE KEY-----")),
    ("aws_access_key", re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("github_token", re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("slack_token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b")),
    ("openai_key", re.compile(r"\bsk-[A-Za-z0-9_-]{20,}\b")),
    ("google_api_key", re.compile(r"\bAIza[0-9A-Za-z_-]{30,}\b")),
)

# Flag only literal credential assignments; variable expansion and empty values are safe.
CRED_ASSIGN_RE = re.compile(
    r"(?i)^\s*(?:export\s+)?"
    r"([A-Z0-9_]*(?:PASSWORD|PASSWD|TOKEN|SECRET|API_KEY|ACCESS_KEY|RCON)[A-Z0-9_]*)"
    r"\s*=\s*(['\"]?)([^'\"\s#]+)\2\s*(?:#.*)?$"
)
RCON_LITERAL_RE = re.compile(r"(?i)(?:\+rcon_password|--password)\s+(['\"]?)([^\s'\"]+)\1")


def candidate_files(root: Path):
    """Prefer Git-visible files; otherwise walk the cleaned source tree."""
    try:
        out = subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "-co", "--exclude-standard", "-z"],
            stderr=subprocess.DEVNULL,
        )
        if out:
            for raw in out.split(b"\0"):
                if raw:
                    p = root / os.fsdecode(raw)
                    if p.is_file():
                        yield p
            return
    except (OSError, subprocess.CalledProcessError):
        pass

    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in files:
            yield Path(base) / name


def suspicious_filename(path: Path) -> bool:
    name = path.name
    if name in SENSITIVE_BASENAMES:
        return True
    if name.startswith(".env.") and name not in {".env.example", ".env.sample"}:
        return True
    return any(fnmatch.fnmatch(name, pat) for pat in SENSITIVE_GLOBS)


def is_public_ip_literal(value: str) -> bool:
    if value in SAFE_VERSION_LITERALS:
        return False
    try:
        addr = ipaddress.ip_address(value)
    except ValueError:
        return False
    # Only globally routable addresses are publication leaks here. Loopback,
    # wildcard, RFC1918, link-local and documentation ranges are allowed.
    return addr.is_global


def maybe_ipv6_tokens(line: str):
    # Extract colon-heavy tokens, then let ipaddress validate them. This avoids
    # treating C++ scope operators and timestamps as addresses.
    for token in re.findall(r"(?i)(?<![0-9a-f:])(?:[0-9a-f]{0,4}:){2,7}[0-9a-f]{0,4}(?![0-9a-f:])", line):
        token = token.strip(":")
        colon_count = token.count(":")
        # Avoid C++ scope fragments such as Foo::Bar or v34::build while still
        # accepting normal/compressed IPv6 spellings.
        if colon_count < 3 and not re.match(r"(?i)^[0-9a-f]{4}::", token):
            continue
        try:
            ipaddress.ip_address(token)
        except ValueError:
            continue
        yield token


def safe_credential_literal(value: str) -> bool:
    if value in {"", "''", '""'}:
        return True
    if "$" in value or "${" in value or "$(" in value:
        return True
    lowered = value.lower()
    placeholders = ("example", "placeholder", "changeme", "<", "your_", "dummy")
    return any(x in lowered for x in placeholders)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    args = ap.parse_args()
    root = Path(args.root).resolve()

    findings: list[tuple[str, str, int, str]] = []
    scanned = 0

    for path in candidate_files(root):
        try:
            rel = path.relative_to(root).as_posix()
        except ValueError:
            continue
        scanned += 1

        if suspicious_filename(path):
            findings.append(("sensitive_filename", rel, 0, path.name))

        try:
            raw = path.read_bytes()
        except OSError:
            continue
        if b"\0" in raw[:8192]:
            # Binary artifacts are not text-scanned. Release binaries belong in
            # dist/ and are skipped by the fallback walker.
            continue
        text = raw.decode("utf-8", errors="replace")

        for lineno, line in enumerate(text.splitlines(), 1):
            for m in IPV4_RE.finditer(line):
                value = m.group(0)
                if is_public_ip_literal(value):
                    findings.append(("public_ipv4", rel, lineno, value))

            for value in maybe_ipv6_tokens(line):
                try:
                    addr = ipaddress.ip_address(value)
                except ValueError:
                    continue
                if addr.is_global:
                    findings.append(("public_ipv6", rel, lineno, value))

            for m in MAC_RE.finditer(line):
                findings.append(("mac_address", rel, lineno, m.group(0)))

            for m in EMAIL_RE.finditer(line):
                findings.append(("email", rel, lineno, m.group(0)))

            for m in HOME_PATH_RE.finditer(line):
                findings.append(("absolute_user_path", rel, lineno, m.group(0)))

            if PROMPT_RE.search(line):
                findings.append(("shell_prompt_identity", rel, lineno, line.strip()))

            if URL_CREDS_RE.search(line):
                findings.append(("url_credentials", rel, lineno, line.strip()))

            for kind, pattern in SECRET_PATTERNS:
                m = pattern.search(line)
                if m:
                    findings.append((kind, rel, lineno, m.group(0)))

            if path.suffix in {".sh", ".env", ".yml", ".yaml"}:
                m = CRED_ASSIGN_RE.match(line)
                if m and not safe_credential_literal(m.group(3)):
                    findings.append(("literal_credential", rel, lineno, f"{m.group(1)}=<literal>"))

                m = RCON_LITERAL_RE.search(line)
                if m and not safe_credential_literal(m.group(2)):
                    findings.append(("literal_rcon_password", rel, lineno, "<literal>"))

    if findings:
        print(f"PUBLIC_AUDIT=FAIL scanned_files={scanned} findings={len(findings)}")
        for kind, rel, lineno, value in findings:
            where = f"{rel}:{lineno}" if lineno else rel
            print(f"PUBLIC_AUDIT_FINDING kind={kind} file={where} value={value}")
        return 1

    print(
        "PUBLIC_AUDIT=PASS "
        f"scanned_files={scanned} public_ip=0 mac=0 email=0 secrets=0 user_paths=0"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
