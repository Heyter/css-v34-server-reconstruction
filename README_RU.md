<p align="center">
  🌐 <strong>Язык</strong> · <a href="./README.md">English</a> · <strong>Русский</strong>
</p>

---

# Counter-Strike: Source v34 Server Rebuild

Реконструированный `server_i486.so` для **CS:S v34 / SRCDS build 4100 / Protocol 7 / Linux i386**.

## Сборка

Нужно: **Linux/WSL2** и **Docker**.

```bash
cd css-v34-server-start
chmod +x scripts/*.sh
./scripts/build-release.sh
```

Готовый RELEASE-файл:

```text
dist/release/server_i486.so
```

DEBUG-сборка при необходимости:

```bash
./scripts/build-debug.sh
```

DEBUG-файл:

```text
build/reconstruction-debug/server_i486.so
```

## Установка

Сделайте резервную копию старого DLL и скопируйте RELEASE-файл в сервер CS:S v34:

```text
cstrike/bin/server_i486.so
```

Пример:

```bash
cp dist/release/server_i486.so /path/to/css-server/cstrike/bin/server_i486.so
```
