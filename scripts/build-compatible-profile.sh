#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PROFILE="${1:-}"
case "$PROFILE" in
  DEBUG|debug)
    PROFILE=DEBUG
    BUILD_DIR="$ROOT/build/reconstruction-debug"
    ;;
  RELEASE|release)
    PROFILE=RELEASE
    BUILD_DIR="$ROOT/build/reconstruction-release"
    ;;
  *)
    echo "usage: $0 DEBUG|RELEASE" >&2
    exit 2
    ;;
esac

# Emergency/developer escape hatch. RC/release builds intentionally default to
# the Debian 12 compatibility container so the resulting i386 DSO does not pick
# up symbol versions from a newer WSL host (for example GLIBC_2.38/2.43,
# GLIBC_ABI_GNU_TLS or CXXABI_1.3.15 from GCC 15/glibc 2.4x).
if [[ "${V34_BUILD_NATIVE:-0}" == "1" ]]; then
  echo "COMPAT_BUILD_BYPASS=1 profile=$PROFILE mode=native_host"
  exec "$ROOT/scripts/build-profile.sh" "$PROFILE"
fi

command -v docker >/dev/null 2>&1 || {
  echo "COMPAT_BUILD=FAIL profile=$PROFILE reason=docker_not_found hint=V34_BUILD_NATIVE=1_for_developer_only" >&2
  exit 1
}

IMAGE="${V34_COMPAT_BUILD_IMAGE:-css-v34-rebuild-debian12-i386:compat-v1}"
DOCKERFILE="$ROOT/docker/Dockerfile.dev"
[[ -f "$DOCKERFILE" ]] || {
  echo "COMPAT_BUILD=FAIL profile=$PROFILE reason=dockerfile_missing path=$DOCKERFILE" >&2
  exit 1
}

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "COMPAT_BUILD_IMAGE_BEGIN image=$IMAGE base=debian:12-slim"
  # Dockerfile.dev has no COPY instructions, so use docker/ as the tiny build
  # context instead of shipping the whole source/build tree to the daemon.
  docker build -f "$DOCKERFILE" -t "$IMAGE" "$ROOT/docker"
  echo "COMPAT_BUILD_IMAGE_END image=$IMAGE"
else
  echo "COMPAT_BUILD_IMAGE_CACHED image=$IMAGE"
fi

uid="$(id -u)"
gid="$(id -g)"
rm -rf "$BUILD_DIR"

echo "COMPAT_BUILD_BEGIN profile=$PROFILE image=$IMAGE uid=$uid gid=$gid"
docker run --rm \
  --user "$uid:$gid" \
  -e HOME=/tmp \
  -e V34_COMPAT_BUILD_ACTIVE=1 \
  -v "$ROOT:/work" \
  -w /work \
  "$IMAGE" \
  /bin/bash -lc '
    set -euo pipefail
    libc_line="$(ldd --version 2>&1 | head -n 1)"
    cxx_line="$(c++ --version 2>&1 | head -n 1)"
    echo "COMPAT_TOOLCHAIN=PASS libc=${libc_line// /_} cxx=${cxx_line// /_}"
    ./scripts/build-profile.sh "'"$PROFILE"'"
  '
echo "COMPAT_BUILD_END profile=$PROFILE image=$IMAGE"
