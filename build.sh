#!/usr/bin/env bash
# Builds GWToolboxdll in Docker into bin/: build.sh [--config C] [--target T] [--cmake-arg A] [--rebuild-image]
set -euo pipefail

config=RelWithDebInfo
target=GWToolboxdll
cmake_args=""
rebuild_image=false

usage() {
    cat <<'EOF'
Usage: build.sh [options]

Options:
  --config <Debug|RelWithDebInfo|Release>   CMake config to build (default: RelWithDebInfo)
  --target <name>                           CMake target to build (default: GWToolboxdll; use "all" for everything)
  --cmake-arg <value>                       Extra argument passed through to CMake configure
  --rebuild-image                           Rebuild the toolchain image without the Docker cache
  -h, --help                                Show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --config) config="$2"; shift 2 ;;
        --target) target="$2"; shift 2 ;;
        --cmake-arg) cmake_args="$cmake_args $2"; shift 2 ;;
        --rebuild-image) rebuild_image=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "$0")" && pwd)"
toolchain_image=gwtoolbox-builder:latest

if ! docker info >/dev/null 2>&1; then
    echo "Docker isn't running: start the Docker daemon and run this again." >&2
    exit 1
fi

# Layer caching makes this a no-op unless Dockerfile.toolchain or setup-toolchain.sh changed.
no_cache=()
$rebuild_image && no_cache=(--no-cache)
docker build "${no_cache[@]}" -t "$toolchain_image" -f "$root/Dockerfile.toolchain" "$root"

# The sources are copied into the image and the outputs come back with --output, so no volumes are mounted.
docker build --target export --output "type=local,dest=$root/bin" \
    --build-arg BUILDER_IMAGE="$toolchain_image" \
    --build-arg CONFIG="$config" \
    --build-arg TARGET="$target" \
    --build-arg CMAKE_ARGS="$cmake_args" \
    -f "$root/Dockerfile" "$root"

echo "Built $target ($config) into $root/bin"
