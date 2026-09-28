<p align="center">
  🌐 <strong>Language</strong> · <strong>English</strong> · <a href="./README_RU.md">Русский</a>
</p>

---

# Counter-Strike: Source v34 Server Rebuild

Reconstructed `server_i486.so` for **CS:S v34 / SRCDS build 4100 / Protocol 7 / Linux i386**.

## Build

Requirements: **Linux/WSL2** and **Docker**.

```bash
cd css-v34-server-start
chmod +x scripts/*.sh
./scripts/build-release.sh
```

Release binary:

```text
dist/release/server_i486.so
```

Debug build if needed:

```bash
./scripts/build-debug.sh
```

Debug binary:

```text
build/reconstruction-debug/server_i486.so
```

## Install

Back up the original DLL, then copy the release binary to your CS:S v34 server:

```text
cstrike/bin/server_i486.so
```

Example:

```bash
cp dist/release/server_i486.so /path/to/css-server/cstrike/bin/server_i486.so
```
