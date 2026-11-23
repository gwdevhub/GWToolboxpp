#!/usr/bin/env bash
# build_dll.sh: builds GWToolbox_DLL for the relwithdebinfo preset. Extra arguments go to scripts/build.py.
set -euo pipefail
cd "$(dirname "$0")"
exec python3 scripts/build.py --preset relwithdebinfo --target GWToolbox_DLL "$@"
