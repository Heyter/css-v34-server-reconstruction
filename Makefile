.PHONY: help verify audit runtime-work bootstrap-main bootstrap-refs lock verify-main toolchain tools dev

help:
	@echo "verify         Verify immutable build4100 baseline"
	@echo "audit          Generate ELF/symbol/interface baseline reports"
	@echo "runtime-work   Copy immutable runtime into runtime/work"
	@echo "bootstrap-main Clone MAIN SourceEngine2007 donor"
	@echo "bootstrap-refs Clone reference source trees"
	@echo "lock           Record exact source Git SHAs"
	@echo "verify-main    Validate MAIN donor layout"
	@echo "toolchain      Check host ELF32 compiler/linker"
	@echo "tools          Build ELF32 helper tools"
	@echo "dev            Open Docker i386 dev shell"

verify:
	./scripts/verify-baseline.sh

audit:
	./scripts/audit-baseline.sh

runtime-work:
	./scripts/create-runtime-work.sh

bootstrap-main:
	./scripts/bootstrap-sources.sh main

bootstrap-refs:
	./scripts/bootstrap-sources.sh refs

lock:
	./scripts/lock-sources.sh

verify-main:
	./scripts/verify-main-source.sh

toolchain:
	./scripts/check-toolchain.sh

tools:
	./scripts/build-tools.sh

dev:
	docker compose -f docker/compose.dev.yml run --rm dev
