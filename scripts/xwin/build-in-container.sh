#!/usr/bin/env bash
# Configure + build inside the clang/xwin container (see Dockerfile).
set -euo pipefail

CONFIG=${CONFIG:-RelWithDebInfo}
TARGET=${TARGET:-GWToolboxdll}
JOBS=${JOBS:-$(nproc)}
CMAKE_ARGS=${CMAKE_ARGS:-}

# The SDK header-casing links point into the mounted repo's include list, so refresh them
# for whatever source tree is actually mounted. Everything already downloaded is reused.
TOOLCHAIN_ROOT=${TOOLCHAIN_ROOT:-/opt/xwin-toolchain} ./scripts/xwin/setup-toolchain.sh >/dev/null

if [ ! -f build-xwin/CMakeCache.txt ]; then
    # shellcheck disable=SC2086 # CMAKE_ARGS is intentionally word-split
    cmake --preset xwin -DCMAKE_BUILD_TYPE="$CONFIG" $CMAKE_ARGS
fi

if [ "$TARGET" = "all" ]; then
    cmake --build build-xwin --config "$CONFIG" -j "$JOBS"
else
    cmake --build build-xwin --config "$CONFIG" --target "$TARGET" -j "$JOBS"
fi

# Everything above ran as root inside the container; hand the artefacts back so build-xwin/
# and bin/ stay editable from the host.
if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown -R "$HOST_UID:$HOST_GID" build-xwin bin 2>/dev/null || true
fi
