#!/usr/bin/env bash
# Build a Docker image with a headless Wine + real MSVC (x86 v143+) + vcpkg toolchain,
# then configure and build GWToolboxdll inside it.
#
# Why Docker: MSVC-under-wine needs a fair pile of host state (a wine prefix, the
# unpacked MSVC/WinSDK trees, a bootstrapped vcpkg) - keeping all of that inside an
# image/container makes the whole thing disposable: `docker image rm` wipes it cleanly,
# and it never touches the host's actual Wine setup (if the machine has one for gaming).
#
# See scripts/wine-msvc/Dockerfile for how the toolchain itself is assembled
# (mstorsjo/msvc-wine downloads the real MSVC + Windows SDK under the VS Build Tools
# EULA, and wraps cl/link/lib/rc to run under wine).
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "${SCRIPT_DIR}/.." && pwd)
IMAGE_NAME="gwtoolboxpp-wine-msvc"

CONFIG="RelWithDebInfo"
TARGET="GWToolboxdll"
REBUILD_IMAGE=0
SHELL_ONLY=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --config <Debug|RelWithDebInfo|Release>   CMake config to build (default: ${CONFIG})
  --target <name>                           CMake build target (default: ${TARGET}; use "all" for everything)
  --rebuild-image                           Force a clean rebuild of the docker image
  --shell                                   Drop into an interactive shell in the container instead of building
  -h, --help                                Show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --config) CONFIG="$2"; shift 2 ;;
        --target) TARGET="$2"; shift 2 ;;
        --rebuild-image) REBUILD_IMAGE=1; shift ;;
        --shell) SHELL_ONLY=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if ! command -v docker >/dev/null 2>&1; then
    echo "docker is required but not found in PATH." >&2
    exit 1
fi

if [ "${REBUILD_IMAGE}" -eq 1 ] || ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
    echo "[build-wine-prefix] building docker image '${IMAGE_NAME}' (this downloads MSVC + Windows SDK; expect it to take a while the first time)..."
    docker build "${REBUILD_IMAGE:+--no-cache}" -t "${IMAGE_NAME}" -f "${SCRIPT_DIR}/wine-msvc/Dockerfile" "${SCRIPT_DIR}/wine-msvc"
fi

RUN_ARGS=(--rm -v "${REPO_ROOT}:/src" -w /src "${IMAGE_NAME}")

if [ "${SHELL_ONLY}" -eq 1 ]; then
    exec docker run -it "${RUN_ARGS[@]}" bash
fi

# The container runs as root, so anything it writes into the bind-mounted repo (build-wine/,
# bin/, vcpkg_installed/, generated shader headers, ...) ends up root-owned on the host.
# Hand it back to the invoking user once we're done so the repo stays normally editable.
fix_ownership() {
    docker run --rm -v "${REPO_ROOT}:/src" "${IMAGE_NAME}" chown -R "$(id -u):$(id -g)" /src >/dev/null 2>&1 || true
}
trap fix_ownership EXIT

echo "[build-wine-prefix] configuring (preset: wine, config: ${CONFIG})..."
docker run "${RUN_ARGS[@]}" bash -lc "cmake --preset wine -DCMAKE_BUILD_TYPE='${CONFIG}'"

echo "[build-wine-prefix] building target '${TARGET}'..."
if [ "${TARGET}" = "all" ]; then
    docker run "${RUN_ARGS[@]}" bash -lc "cmake --build build-wine --config '${CONFIG}'"
else
    docker run "${RUN_ARGS[@]}" bash -lc "cmake --build build-wine --config '${CONFIG}' --target '${TARGET}'"
fi

echo "[build-wine-prefix] done. Output under ${REPO_ROOT}/bin/"
