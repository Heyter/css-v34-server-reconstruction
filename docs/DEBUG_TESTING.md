# DEBUG testing — preserved after project closure

The release project is closed at `v1.8.35g / RC1`. Long soak and broad regression were intentionally deferred, but the active DEBUG path is preserved so testing can be resumed later without restoring historical AutoProbe/tool generations.

## Required local runtime

Project root used in WSL:

```text
~/projects/css-v34-server-start
```

Keep:

```text
runtime/original/              immutable original build4100 runtime
runtime/test-maps/             de_dust2/cs_office/de_aztec/fy_pool_day fixtures
```

The cleanup script never deletes `runtime/original/` or `runtime/test-maps/`.

## Build DEBUG

```bash
cd ~/projects/css-v34-server-start
./scripts/build-debug.sh 2>&1 | tee build-debug.log
```

Expected final marker:

```text
BUILD_PROFILE=PASS profile=DEBUG ... elf32=1 exports=CreateInterface textrel=none
```

DEBUG keeps `EngineProbe`; RELEASE compiles it out.

## Full preserved gate

```bash
./scripts/test-current.sh 2>&1 | tee test-current.log
```

The gate retains the useful current checks:

- ELF32 / export-surface / TEXTREL validation;
- 25 source-level smoke tests;
- ABI, ServerClass, SendTable and entity-factory oracles;
- Protocol 7 self-test and signon/gameplay path;
- real build4100 Docker runtime on the known map fixtures;
- reconnect, simultaneous clients, changelevel/mapcycle;
- game events, user messages, voice and NET_File;
- movement/collision/water/ladder and objective gameplay;
- weapon scripts, fire/reload/hitscan/ballistics and TE_FireBullets.

Historical Markdown-shape gates are disabled by default because the old versioned documentation is removed. They are not runtime evidence.

## Deferred stage 1: soak

If the project is reopened, repeat the existing real-engine/client flows rather than restoring old AutoProbe tooling. Suggested checks:

```text
repeated mapcycle/changelevel
connect/disconnect/reconnect loops
2-4 Protocol7 clients
long-running gameplay
RSS / FD growth
crashes / asserts / protocol stalls
```

## Deferred stage 2: broad regression

The accepted core fixtures are:

```text
de_dust2
cs_office
de_aztec
fy_pool_day
```

Additional old v34 maps can be added to the same real-engine harness if required.

## Focused parity

To re-run the final external behavior comparison:

```bash
./scripts/build-release.sh 2>&1 | tee build-release.log
./scripts/compare-original-release.sh 2>&1 | tee compare-original-release.log
```

The final accepted closure result was:

```text
ORIGINAL_REBUILT_PARITY=PASS
protocol=7
app_id=240
map=de_dust2
a2s=match
status=match
cvars=match
```

Player/bot counts can differ by startup timing and are not treated as semantic mismatches by the final parity gate.
