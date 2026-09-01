#!/usr/bin/env bash
# Build a Docker image with a clang-cl + xwin (x86 Windows) + vcpkg toolchain, then
# configure and build GWToolboxdll inside it. clang targets i686-pc-windows-msvc directly,
# so nothing Microsoft-built ever executes.
#
# You do not need Docker for this toolchain - scripts/xwin/setup-toolchain.sh provisions it
# straight onto the host. Docker just keeps the ~800MB SDK and the vkd3d build disposable.
#
# See scripts/xwin/Dockerfile for how the toolchain is assembled.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "${SCRIPT_DIR}/.." && pwd)
IMAGE_NAME="gwtoolboxpp-xwin"

CONFIG="RelWithDebInfo"
TARGET="GWToolboxdll"
CMAKE_ARGS=""
JOBS=""
REBUILD_IMAGE=0
SHELL_ONLY=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --config <Debug|RelWithDebInfo|Release>   CMake config to build (default: ${CONFIG})
  --target <name>                           CMake build target (default: ${TARGET}; use "all" for everything)
  --cmake-arg <value>                       Extra argument to pass through to CMake configure
  --jobs <n>                                Parallel build jobs (default: all cores)
  --rebuild-image                           Force a clean rebuild of the docker image
  --shell                                   Drop into an interactive shell in the container instead of building
  -h, --help                                Show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --config) CONFIG="$2"; shift 2 ;;
        --target) TARGET="$2"; shift 2 ;;
        --cmake-arg) CMAKE_ARGS="${CMAKE_ARGS} $2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
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
    echo "[build-xwin] building docker image '${IMAGE_NAME}' (this downloads the MSVC + Windows SDK headers/libs and builds vkd3d; expect it to take a while the first time)..."
    docker build "${REBUILD_IMAGE:+--no-cache}" -t "${IMAGE_NAME}" -f "${SCRIPT_DIR}/xwin/Dockerfile" "${SCRIPT_DIR}/xwin"
fi

TTY_FLAGS=""
if [ -t 0 ] && [ -t 1 ]; then
    TTY_FLAGS="-it"
fi

RUN_ARGS=(--rm ${TTY_FLAGS} -v "${REPO_ROOT}:/src" -w /src -e CONFIG="${CONFIG}" -e TARGET="${TARGET}" -e CMAKE_ARGS="${CMAKE_ARGS}" -e JOBS="${JOBS}" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" "${IMAGE_NAME}")

if [ "${SHELL_ONLY}" -eq 1 ]; then
    exec docker run "${RUN_ARGS[@]}" bash
fi

# The container builds as root; hand back ownership of generated files in the bind-mounted
# repo once we're done so the host stays editable.
fix_ownership() {
    docker run --rm -v "${REPO_ROOT}:/src" "${IMAGE_NAME}" chown -R "$(id -u):$(id -g)" /src >/dev/null 2>&1 || true
}
trap fix_ownership EXIT

echo "[build-xwin] configuring (preset: xwin, config: ${CONFIG})..."
exec docker run "${RUN_ARGS[@]}" /src/scripts/xwin/build-in-container.sh
