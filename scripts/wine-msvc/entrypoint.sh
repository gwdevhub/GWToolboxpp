#!/usr/bin/env bash
# Container entrypoint: starts a persistent wineserver (cl/link/lib/rc under wine are much
# faster with a warm server instead of spawning+killing one per invocation) then execs the
# requested command against the mounted repo.
set -euo pipefail

WINE=$(command -v wine64 || command -v wine)

wineserver -k >/dev/null 2>&1 || true
wineserver -p
"${WINE}" wineboot >/dev/null 2>&1

exec "$@"
