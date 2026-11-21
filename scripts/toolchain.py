"""Provisions the build's toolchain into .toolchain/ (git-ignored), each step skipped when current:
  llvm-mingw-<host>  the pinned release in llvm-mingw.version, checked against its SHA-256
  case-include       Linux: links for the mixed-case Windows header names the sources use
  vkd3d-<version>    Linux: vkd3d-compiler, the fxc stand-in for the SM3 shaders (tools/hlsl_compile.py)

    python3 scripts/toolchain.py
"""

import require_python  # noqa: F401  (first: stops an old Python before anything else runs)

import hashlib
import os
import re
import shutil
import sys
import urllib.request

from common import (IS_WINDOWS, capture, HOST, LLVM_MINGW_DIR, LOG_DIR, REPO_ROOT, TOOLCHAIN_DIR, Failure, Log, main_guard,
                          read_pin, run, timestamp)

CASE_INCLUDE_DIR = os.path.join(TOOLCHAIN_DIR, "case-include")
SCANNED_FOLDERS = ("Core", "GWToolbox", "GWToolboxdll", "RestClient", "plugins", "Dependencies")
SCANNED_EXTENSIONS = (".cpp", ".h", ".hpp", ".inl", ".c")
# 1.6 (what most distros package) miscompiles these shaders; keep in step with VKD3D_MINIMUM_VERSION in
# GWToolboxdll/CMakeLists.txt.
VKD3D_VERSION = os.environ.get("VKD3D_VERSION", "1.19")


def download(url, path):
    with urllib.request.urlopen(url) as response, open(path, "wb") as file:
        shutil.copyfileobj(response, file, 1 << 20)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def read_stamp(folder):
    try:
        with open(os.path.join(folder, ".gwtoolbox-version"), encoding="utf-8") as file:
            return file.read().strip()
    except OSError:
        return None


def adopt_legacy_toolchain(log):
    # Checkouts set up while other pipelines existed keep the toolchain in .mingw-toolchain/; move it rather than
    # download it again.
    legacy = os.path.join(REPO_ROOT, ".mingw-toolchain")
    if os.path.isdir(legacy) and not os.path.exists(TOOLCHAIN_DIR):
        log.step("moving .mingw-toolchain/ to .toolchain/")
        os.rename(legacy, TOOLCHAIN_DIR)


def install_llvm_mingw(log):
    pin = read_pin()
    asset, expected = pin[HOST + "_asset"], pin[HOST + "_sha256"]
    stamp = "%s %s" % (pin["version"], expected)
    # Checkouts set up before the folder was per host share llvm-mingw/; take it over if it is ours.
    shared = os.path.join(TOOLCHAIN_DIR, "llvm-mingw")
    if not os.path.exists(LLVM_MINGW_DIR) and read_stamp(shared) == stamp:
        log.step("moving .toolchain/llvm-mingw/ to .toolchain/%s/" % os.path.basename(LLVM_MINGW_DIR))
        os.rename(shared, LLVM_MINGW_DIR)
    if read_stamp(LLVM_MINGW_DIR) == stamp:
        log.step("llvm-mingw %s already installed" % pin["version"])
        return
    log.step("installing llvm-mingw %s into .toolchain/%s/" % (pin["version"], os.path.basename(LLVM_MINGW_DIR)))
    os.makedirs(TOOLCHAIN_DIR, exist_ok=True)
    archive = os.path.join(TOOLCHAIN_DIR, asset)
    download("https://github.com/mstorsjo/llvm-mingw/releases/download/%s/%s" % (pin["version"], asset), archive)
    actual = sha256(archive)
    if actual != expected:
        os.remove(archive)
        raise Failure("llvm-mingw download has SHA-256 %s, expected %s" % (actual, expected))
    extracted = os.path.join(TOOLCHAIN_DIR, re.sub(r"\.(zip|tar\.xz)$", "", asset))
    for old in (LLVM_MINGW_DIR, extracted):
        shutil.rmtree(old, ignore_errors=True)
    # tar keeps the Linux release's symlinks; Windows' own tar.exe (not Git's) unpacks the zip far faster than zipfile.
    tar = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "tar.exe") if IS_WINDOWS else "tar"
    run(log, [tar, "-xf", archive, "-C", TOOLCHAIN_DIR], "unpacking llvm-mingw")
    os.remove(archive)
    os.rename(extracted, LLVM_MINGW_DIR)
    with open(os.path.join(LLVM_MINGW_DIR, ".gwtoolbox-version"), "w", encoding="utf-8") as file:
        file.write(stamp + "\n")


def replace_link(target, link):
    os.makedirs(os.path.dirname(link), exist_ok=True)
    if os.path.islink(link):
        if os.readlink(link) == target:
            return
        os.remove(link)
    os.symlink(target, link)


def link_headers():
    mingw_include = os.environ.get("MINGW_INCLUDE_DIR", os.path.join(LLVM_MINGW_DIR, "generic-w64-mingw32", "include"))
    pattern = re.compile(rb"#include <([A-Za-z0-9_./-]+\.h)>")
    included = set()
    for folder in SCANNED_FOLDERS:
        for directory, _, names in os.walk(os.path.join(REPO_ROOT, folder)):
            for name in names:
                if name.endswith(SCANNED_EXTENSIONS):
                    with open(os.path.join(directory, name), "rb") as file:
                        included.update(match.decode() for match in pattern.findall(file.read()))
    for header in sorted(included):
        lowercase = header.lower()
        if os.path.exists(os.path.join(mingw_include, header)) or not os.path.exists(os.path.join(mingw_include, lowercase)):
            continue
        replace_link(os.path.join(mingw_include, lowercase), os.path.join(CASE_INCLUDE_DIR, header))
    # DirectXTex includes <wrl\client.h>; clang on Linux takes the backslash literally, so link that exact name. The
    # dependency file records it with a slash, and a missing dependency rebuilds the file every time, so link that too.
    for windows_path in ("wrl\\client.h",):
        slash_path = windows_path.replace("\\", "/")
        replace_link(os.path.join(mingw_include, slash_path), os.path.join(CASE_INCLUDE_DIR, windows_path))
        replace_link(os.path.join(mingw_include, slash_path), os.path.join(CASE_INCLUDE_DIR, slash_path))


def install_vkd3d(log, target_dir):
    source = os.path.join(target_dir, "vkd3d-" + VKD3D_VERSION)
    compiler = os.path.join(source, "vkd3d-compiler")
    # libtool's wrapper keeps the absolute build path, so a moved toolchain folder needs vkd3d built again.
    if os.access(compiler, os.X_OK) and capture([compiler, "--version"]) is not None:
        return
    shutil.rmtree(source, ignore_errors=True)
    log.step("building vkd3d-compiler " + VKD3D_VERSION)
    os.makedirs(target_dir, exist_ok=True)
    # The release tarball, not git: the git tree generates headers with widl, which ships with wine and would drag
    # the whole thing back in as a build dependency.
    if not os.path.isdir(source):
        archive = source + ".tar.xz"
        download("https://dl.winehq.org/vkd3d/source/vkd3d-%s.tar.xz" % VKD3D_VERSION, archive)
        run(log, ["tar", "-xJf", archive, "-C", target_dir], "unpacking vkd3d")
        os.remove(archive)
    spirv = os.path.join(target_dir, "SPIRV-Headers")
    if not os.path.isdir(spirv):
        run(log, ["git", "clone", "-q", "--depth", "1", "https://github.com/KhronosGroup/SPIRV-Headers.git", spirv])
    # Generated by vkd3d's release script and absent from the tarball; only its --version uses it.
    version_header = os.path.join(source, "include", "private", "vkd3d_version.h")
    if not os.path.isfile(version_header):
        with open(version_header, "w", encoding="utf-8") as file:
            file.write('#define VKD3D_VCS_ID " (%s)"\n' % VKD3D_VERSION)
    if not os.path.isfile(os.path.join(source, "Makefile")):
        run(log, ["./configure", "--prefix=" + os.path.join(source, "inst"),
                  "CPPFLAGS=-I" + os.path.join(spirv, "include")], "configuring vkd3d", cwd=source)
    # Only the shader compiler; libvkd3d proper is the D3D12 runtime and irrelevant here.
    run(log, ["make", "-j%d" % (os.cpu_count() or 1), "vkd3d-compiler"], "building vkd3d", cwd=source)


def provision(log):
    adopt_legacy_toolchain(log)
    install_llvm_mingw(log)
    if not IS_WINDOWS:
        link_headers()
        install_vkd3d(log, TOOLCHAIN_DIR)


def main():
    if len(sys.argv) != 1:
        raise Failure(__doc__)
    provision(Log(os.path.join(LOG_DIR, "toolchain-%s.log" % timestamp())))


if __name__ == "__main__":
    main_guard(main)
