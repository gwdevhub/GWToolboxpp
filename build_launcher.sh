#!/usr/bin/env bash
# build_launcher.sh: builds GWToolbox_Launcher_Windows, GWToolbox_Launcher_Linux for the relwithdebinfo preset. Extra arguments go to scripts/build.py.
set -euo pipefail
cd "$(dirname "$0")"
exec python3 scripts/build.py --preset relwithdebinfo --target GWToolbox_Launcher_Windows --target GWToolbox_Launcher_Linux "$@"
