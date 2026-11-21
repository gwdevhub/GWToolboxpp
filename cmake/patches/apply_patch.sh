#!/usr/bin/env sh
# POSIX equivalent of apply_patch.bat, used when configuring on Linux, where .bat scripts cannot run.
set -e
dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
patch_file="$dir/$1"
# The sources come from an archive, not a clone, so stop git finding the repository the build folder sits in;
# outside a repository git apply patches the current folder.
GIT_CEILING_DIRECTORIES=$(dirname -- "$PWD")
export GIT_CEILING_DIRECTORIES

if git apply --reverse --check --ignore-whitespace "$patch_file" 2>/dev/null; then
    echo "Already applied, nothing to do"
    exit 0
fi
echo "Applying patch"
git apply --ignore-whitespace "$patch_file"
