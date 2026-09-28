# Runtime baseline

`original/` contains the user-supplied CSS v34 Linux build4100 runtime and is the immutable behavioral/ABI baseline.

Do not edit it in place. Experiments must use copies, bind-mount replacement, or loader/interposition tooling.

Canonical core files:

```text
original/srcds_i486
original/bin/engine_i486.so
original/bin/vphysics_i486.so
original/cstrike/bin/server_i486.so
```

Verify baseline integrity with:

```bash
./scripts/verify-baseline.sh
```

## Current use at reconstruction v1.7.3

The runtime is now used by both static differential oracles and the real-engine acceptance test.

`test-real-engine-docker.sh` first proves the known-working Docker/ReviveEmu/BufferFix image with the original game DLL, then launches the same runtime and bind-mounts only the reconstructed `server_i486.so` over `cstrike/bin/server_i486.so`.

The current reconstructed path reaches map startup, fake-client lifecycle, a real Protocol 7 `ClientConnect`, reliable UDP signon, BZip2 SendTable reassembly and exact 15-class `svc_ClassInfo`.

Current live blocker is string-table baseline/sign-on completion.

AutoProbe `v51.36.0` remains historical ABI/runtime evidence for unresolved contracts; generated `reports/` are not required for normal continuation.
