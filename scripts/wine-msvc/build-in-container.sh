#!/usr/bin/env bash
# Configure + build GWToolboxdll inside the wine-msvc container (see Dockerfile).
#
# Root cause of the ninja hangs this works around: wine LAZILY (re)spawns its session shell
# (explorer.exe) whenever a Windows app is launched and no desktop is alive. That explorer.exe
# is forked from the current job and inherits the write end of ninja's per-job capture pipe,
# then never exits -- so ninja's read end never sees EOF and it blocks forever in ppoll() with
# the job's `sh` left as a zombie. (mspdbsrv.exe has the same problem via /FS.)
#
# Two-part deterministic fix:
#   1. Keep ONE persistent wine desktop (explorer.exe) alive for the whole build, started with
#      /dev/null std handles. wine then reuses it instead of forking a fresh explorer per job,
#      so nothing inherits a job pipe.
#   2. Pre-start one persistent mspdbsrv at a fixed endpoint (the /FS PDB server).
# Compile/link jobs are additionally assigned to ninja's `console` pool so they get ninja's own
# stdio with no capture pipe at all.
set -u

CONFIG=${CONFIG:-RelWithDebInfo}
TARGET=${TARGET:-GWToolboxdll}
CMAKE_ARGS=${CMAKE_ARGS:-}

. /opt/msvc/bin/x86/msvcenv.sh   # WINEPATH / INCLUDE / LIB / WINEDLLOVERRIDES / BINDIR

WINE=$(command -v wine64 || command -v wine)
export WINEDEBUG=-all
export _MSPDBSRV_ENDPOINT_=gwtoolboxpp_build

# Bring up all wine services once.
"$WINE" wineboot -i >/dev/null 2>&1 </dev/null

# Persistent desktop so wine never forks a fresh explorer.exe that inherits a job pipe.
# Detached with /dev/null std handles; killed on exit.
"$WINE" explorer /desktop=gwbuild,1x1 >/dev/null 2>&1 </dev/null &
desktop_pid=$!
sleep 2

# Persistent PDB server for /FS.
"$WINE" "$BINDIR/mspdbsrv.exe" -start -shutdowntime -1 >/dev/null 2>&1 </dev/null &
sleep 1

rc=0
if [ ! -f build-wine/CMakeCache.txt ]; then
    # shellcheck disable=SC2086 # CMAKE_ARGS is intentionally word-split
    cmake --preset wine \
        -DCMAKE_BUILD_TYPE="$CONFIG" \
        -DCMAKE_JOB_POOL_COMPILE=console \
        -DCMAKE_JOB_POOL_LINK=console \
        $CMAKE_ARGS
    rc=$?
fi

if [ $rc -eq 0 ]; then
    if [ "$TARGET" = "all" ]; then
        cmake --build build-wine --config "$CONFIG" -j 1
    else
        cmake --build build-wine --config "$CONFIG" --target "$TARGET" -j 1
    fi
    rc=$?
fi

"$WINE" "$BINDIR/mspdbsrv.exe" -stop >/dev/null 2>&1 || true
kill "$desktop_pid" >/dev/null 2>&1 || true

# Everything above ran as root inside the container; give the artefacts back to the invoking user
# so build-wine/ and bin/ stay editable from the host.
if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown -R "$HOST_UID:$HOST_GID" build-wine bin 2>/dev/null || true
fi

exit $rc
