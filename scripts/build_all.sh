#!/usr/bin/env bash
# Builds the DLL, the Windows launcher and the Linux launcher (packed into GWToolbox.AppImage) for the relwithdebinfo
# preset. Run bootstrap.sh first. Extra arguments go to scripts/build.py.
set -euo pipefail

cd "$(dirname "$0")/.."
exec python3 scripts/build.py --preset relwithdebinfo \
    --target GWToolbox_DLL --target GWToolbox_Launcher_Windows --target GWToolbox_Launcher_Linux "$@"
