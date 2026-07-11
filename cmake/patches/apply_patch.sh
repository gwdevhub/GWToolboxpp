#!/usr/bin/env sh
# POSIX equivalent of apply_patch.bat, used when configuring from a non-Windows host
# (e.g. cross-compiling via scripts/build-wine-prefix.sh), where .bat scripts can't run.
set -e
dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
patch_file="$dir/$1"

if git apply --reverse --check --ignore-whitespace "$patch_file" 2>/dev/null; then
    echo "Already applied, nothing to do"
    exit 0
fi
echo "Applying patch"
git apply --ignore-whitespace "$patch_file"
