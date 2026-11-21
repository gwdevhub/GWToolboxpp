#!/usr/bin/env bash
# Checks and installs what Linux needs before scripts/bootstrap.py can run, then runs it. bootstrap.py (shared with
# bootstrap.bat) installs llvm-mingw, vkd3d-compiler and vcpkg into the checkout, configures the build and
# sets CLion, Rider (Flatpak) and VS Code up. It opens none of them; the last lines say how.
#
#   1. Refuses to run while CLion or Rider, or a CMake run one of them started, is still running.
#   2. Installs the missing host packages with apt: git, curl, python3, ninja, vcpkg's zip/unzip/pkg-config, and
#      what vkd3d-compiler (the shader compiler) needs to build. CMake 3.29+ is only checked.
#   3. Runs scripts/bootstrap.py.
#
# Run it from anywhere:  bash bootstrap.sh
set -euo pipefail

if [ "$(uname -s)" != "Linux" ]; then
    echo "bootstrap.sh is for Linux. On Windows run bootstrap.bat instead." >&2
    exit 1
fi

REPO_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$REPO_ROOT"

# Everything this script prints also goes to a timestamped log, which bootstrap.py then appends to.
mkdir -p logs
BOOTSTRAP_LOG="${REPO_ROOT}/logs/bootstrap-$(date +%Y%m%d-%H%M%S).log"
exec 3>&1 4>&2
exec > >(tee -a "$BOOTSTRAP_LOG") 2>&1
echo "bootstrap log: ${BOOTSTRAP_LOG}"

log() { echo "==> $*"; }
die() { echo "bootstrap: $*" >&2; exit 1; }

# A running CLion or Rider rewrites its .idea files on exit, which would undo what the bootstrap writes there.
running_ides=$(flatpak ps --columns=application 2>/dev/null | grep -xE 'com\.jetbrains\.(CLion|Rider)' | sort -u | tr '\n' ' ' || true)
if pgrep -x clion >/dev/null 2>&1 || pgrep -x rider >/dev/null 2>&1 || [ -n "$running_ides" ]; then
    die "CLion or Rider is running (${running_ides:-native}). Close it and re-run, so it does not overwrite the .idea files."
fi
# The IDEs run CMake on the host through flatpak-spawn, so closing or killing the IDE does not stop a reload that is
# under way; it keeps writing into build/ and would race the bootstrap's configure.
if pgrep -f -- "${REPO_ROOT}/build/" >/dev/null 2>&1; then
    die "A CMake run started by an IDE is still using this checkout's build/. Wait for it to finish, then re-run."
fi

# git/curl/tar/xz fetch llvm-mingw and vcpkg; zip/unzip/pkg-config are what vcpkg needs; python3 runs the bootstrap
# and the build's scripts; build-essential, bison, flex, libjson-perl and libvulkan-dev build vkd3d-compiler (its
# configure requires Vulkan even though only the shader compiler is built).
HOST_PACKAGES=(git curl tar xz-utils python3 ninja-build zip unzip pkg-config
               build-essential bison flex libjson-perl libvulkan-dev)

command -v dpkg-query >/dev/null 2>&1 || die "this script installs packages with apt; on another distro install the equivalents of: ${HOST_PACKAGES[*]}, then run python3 scripts/bootstrap.py"
missing=()
for pkg in "${HOST_PACKAGES[@]}"; do
    dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed" || missing+=("$pkg")
done
if [ "${#missing[@]}" -eq 0 ]; then
    log "host packages already present"
else
    log "installing missing packages with apt: ${missing[*]}"
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends "${missing[@]}"
fi

# cmake is not installed by apt here (the project needs >= 3.29, newer than some distros ship), so only check it.
command -v cmake >/dev/null 2>&1 || die "cmake not found; install cmake >= 3.29 (e.g. pip install --user 'cmake>=3.29')"
version=$(cmake --version | head -1 | awk '{print $3}')
if [ "$(printf '%s\n' 3.29 "$version" | sort -V | head -1)" != "3.29" ]; then
    die "cmake ${version} is too old; the project needs >= 3.29"
fi
python3 "${REPO_ROOT}/scripts/require_python.py" >/dev/null || die "install a newer python3, then re-run"

exec 1>&3 2>&4 3>&- 4>&-
exec python3 "${REPO_ROOT}/scripts/bootstrap.py" --log "$BOOTSTRAP_LOG"
