#!/usr/bin/env bash
# Builds the Linux launcher and packs it into bin/RelWithDebInfo/GWToolbox.AppImage. scripts/build.py does this too; this
# is just the shortcut for building only the AppImage.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

exec python3 scripts/build.py --preset relwithdebinfo --target GWToolbox_Launcher_Linux
