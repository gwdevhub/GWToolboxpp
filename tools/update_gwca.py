#!/usr/bin/env python3
"""Check for and vendor a new gwdevhub/GWCA release into Dependencies/GWCA/.

Standalone and stdlib-only (no `pip install`), so it runs the same on the
Windows and Linux setups this repo targets.

This DOWNLOADS only when you ask it to. It is deliberately NOT wired into the
build: Dependencies/GWCA/ is a *pinned, committed* dependency, and bumping it
should be an explicit, reviewable change -- new binaries, new headers, new pin
-- rather than a silent per-build side effect that makes builds non-reproducible
and needs network access to compile. Same arrangement as kamadanwasm's
tools/update_gwca.py, for the same reasons; this is that script adapted to
GWToolboxpp's existing Dependencies/GWCA/ layout instead of vendor/gwca/.

All four assets come from one release, which is the point: gwca.dll/gwca.lib
(native), gwca.wasm (wasm), and the headers everything compiles against are
matched by construction. Taking them from different places leaves nothing but
GWCA_ABI_VERSION between you and struct offsets read at the wrong addresses,
and that only moves when the version does.

Auth: gwdevhub/GWCA is a PRIVATE repo, so a credential is required. This prefers
the GitHub CLI (`gh`) when installed and authenticated -- it handles private
release-asset downloads, which redirect to a signed CDN URL. Failing that it
falls back to the stdlib with GITHUB_TOKEN / GH_TOKEN (needs 'repo' scope),
stripping the bearer token on the CDN redirect so the download is not rejected.

What it vendors, from one release:
  * gwca.dll          -> Dependencies/GWCA/bin/gwca.dll     (native build)
  * gwca.lib           -> Dependencies/GWCA/lib/gwca.lib     (native build)
  * gwca.wasm         -> Dependencies/GWCA/wasm/gwca.wasm   (wasm build)
  * GWCA-headers.zip  -> Dependencies/GWCA/include/ and Dependencies/GWCA/source/
  * the pin           -> Dependencies/GWCA/gwca.json (tag, ABI, sha256 of each asset)

The zip's Include/ is flattened to include/ (matching the directory this repo
has always vendored headers into) and Source/ -- the Win32 type shim a wasm
consumer's stdafx.h reaches under GWCA_WASM=1 -- goes to source/. Only Source/
is needed for the wasm side of the port; the native Win32 build never touches it.

Headers are normalized CRLF -> LF on the way out: the release zip is built on
Windows and its entries are CRLF, but this repo has always committed GWCA's
headers as LF -- extracting raw would turn every line of every header into a
diff on every single update, forever, for no actual content change.

Usage:
  python3 tools/update_gwca.py            # vendor the latest release, if newer
  python3 tools/update_gwca.py --check    # report only; exit 10 if an update exists
  python3 tools/update_gwca.py --force    # re-download even if already current
  python3 tools/update_gwca.py --tag v4.7.2.9   # pin to a specific release
  python3 tools/update_gwca.py --dry-run  # show what would change, write nothing

Exit codes:
  0   updated, already current, or --check found nothing new
  2   usage, or the release is missing an asset this needs
  3   auth / network / GitHub API error
  10  --check only: a newer release exists (nothing was downloaded)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

DEFAULT_REPO = "gwdevhub/GWCA"
API_TIMEOUT = 20        # short: the build-time --check must not hang a build
DOWNLOAD_TIMEOUT = 240

DLL_ASSET = "gwca.dll"
LIB_ASSET = "gwca.lib"
WASM_ASSET = "gwca.wasm"
HEADERS_ASSET = "GWCA-headers.zip"
REQUIRED_ASSETS = (DLL_ASSET, LIB_ASSET, WASM_ASSET, HEADERS_ASSET)

# What to take out of the headers zip. GWCA's own build leaves artefacts in
# Include/ -- a UIMessages.h.bak from its enum-comment updater -- which the
# release zip can carry and which must not be vendored.
HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".inl", ".ipp", ".in"}
# Top-level zip dirs, and where each is vendored to.
ROOT_MAP = {"Include": "include", "Source": "source"}

REPO_ROOT = Path(__file__).resolve().parent.parent
GWCA_DIR = REPO_ROOT / "Dependencies" / "GWCA"
PIN = GWCA_DIR / "gwca.json"


class TransportError(Exception):
    """Any failure reaching GitHub (auth, network, API)."""


def info(msg=""):
    print(msg)


def die(msg, code=2):
    sys.stderr.write("update_gwca: %s\n" % msg)
    raise SystemExit(code)


# ---------------------------------------------------------------------------
# Transport: prefer the gh CLI (private repos + signed asset URLs), else stdlib.
# ---------------------------------------------------------------------------
def resolve_token():
    return os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")


def find_gh():
    gh = shutil.which("gh")
    if not gh:
        return None
    try:
        r = subprocess.run([gh, "auth", "status"], capture_output=True, text=True, timeout=15)
        return gh if r.returncode == 0 else None
    except (OSError, subprocess.SubprocessError):
        return None


def _gh(gh, args, timeout):
    try:
        r = subprocess.run([gh, *args], capture_output=True, text=True, timeout=timeout)
    except (OSError, subprocess.SubprocessError) as e:
        raise TransportError("gh %s: %s" % (" ".join(args), e))
    if r.returncode != 0:
        raise TransportError((r.stderr or r.stdout).strip() or "gh %s failed" % " ".join(args))
    return r.stdout


def _request(url, accept, token):
    req = urllib.request.Request(url)
    req.add_header("User-Agent", "gwtoolboxpp-gwca-updater")
    req.add_header("Accept", accept)
    if token:
        req.add_header("Authorization", "Bearer %s" % token)
    return req


class _StripAuthOnRedirect(urllib.request.HTTPRedirectHandler):
    """Drop the bearer token when GitHub redirects an asset to the signed CDN
    URL -- the CDN rejects a request carrying two auth mechanisms."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        newreq = super().redirect_request(req, fp, code, msg, headers, newurl)
        if newreq is not None:
            for h in [k for k in newreq.headers if k.lower() == "authorization"]:
                del newreq.headers[h]
        return newreq


def get_release(repo, tag, gh, token):
    endpoint = ("repos/%s/releases/tags/%s" % (repo, tag)) if tag \
        else ("repos/%s/releases/latest" % repo)
    if gh:
        return json.loads(_gh(gh, ["api", endpoint], API_TIMEOUT))
    if not token:
        raise TransportError(
            "no authenticated gh CLI and no GITHUB_TOKEN/GH_TOKEN -- %s is private" % repo)
    try:
        with urllib.request.urlopen(
                _request("https://api.github.com/%s" % endpoint,
                         "application/vnd.github+json", token), timeout=API_TIMEOUT) as resp:
            return json.load(resp)
    except urllib.error.HTTPError as e:
        raise TransportError("HTTP %s %s" % (e.code, e.reason))
    except (urllib.error.URLError, TimeoutError, OSError) as e:
        raise TransportError(str(e))


def download_assets(repo, tag, gh, token, assets, tmp):
    """Fetch the required assets into `tmp`."""
    if gh:
        args = ["release", "download", tag, "--repo", repo, "--dir", str(tmp), "--clobber"]
        for name in REQUIRED_ASSETS:
            args += ["--pattern", name]
        _gh(gh, args, DOWNLOAD_TIMEOUT)
        return
    if not token:
        raise TransportError("no gh CLI and no GITHUB_TOKEN -- cannot read a private repo")
    opener = urllib.request.build_opener(_StripAuthOnRedirect)
    for name in REQUIRED_ASSETS:
        # The API asset endpoint with an octet-stream Accept, not
        # browser_download_url: that is what resolves a private asset to a
        # signed download.
        req = _request(assets[name]["url"], "application/octet-stream", token)
        with opener.open(req, timeout=DOWNLOAD_TIMEOUT) as resp, open(tmp / name, "wb") as f:
            shutil.copyfileobj(resp, f)


# ---------------------------------------------------------------------------
# Versions and the pin
# ---------------------------------------------------------------------------
def parse_version(tag):
    """'v4.7.2.9' -> (4, 7, 2, 9); tolerant of a missing 'v' and extra text."""
    return tuple(int(n) for n in re.findall(r"\d+", tag or ""))


def read_pin():
    try:
        return json.loads(PIN.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def current_tag():
    return read_pin().get("tag")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def abi_from_headers(include_dir):
    """The ABI constant the vendored headers declare. Consumers can compare this
    against gwca_abi_version() at runtime to guard against a headers/binary
    mismatch."""
    v = Path(include_dir) / "GWCA" / "Utilities" / "Version.h"
    try:
        m = re.search(r"GWCA_ABI_VERSION\s+(0x[0-9a-fA-F]+)",
                      v.read_text(encoding="utf-8", errors="replace"))
    except OSError:
        return None
    return m.group(1) if m else None


# ---------------------------------------------------------------------------
# Extraction
# ---------------------------------------------------------------------------
def headers_members(zip_path):
    """(member, relative dest path) for everything worth vendoring out of the
    zip. Include/ -> include/, Source/ -> source/ (see ROOT_MAP); anything
    outside those roots, and any non-header file, is skipped -- see
    HEADER_SUFFIXES.
    """
    out = []
    with zipfile.ZipFile(zip_path) as z:
        for m in z.namelist():
            if m.endswith("/") or m.endswith("\\"):
                continue
            parts = m.replace("\\", "/").split("/")
            if not parts or parts[0] not in ROOT_MAP:
                continue
            if ".." in parts:
                continue                       # zip-slip: never write outside the tree
            if Path(parts[-1]).suffix.lower() not in HEADER_SUFFIXES:
                continue
            rel = "/".join([ROOT_MAP[parts[0]], *parts[1:]])
            out.append((m, rel))
    return out


def extract_headers(zip_path, members, dest):
    """Replace the vendored headers wholesale, so one upstream deletion does not
    leave a stale header behind to be found by an include.

    Normalized to LF on the way out: the release zip is built on Windows and
    its entries are CRLF, but this repo has always committed GWCA's headers as
    LF -- extracting raw would turn every line of every header into a diff on
    every single update, forever, for no actual content change.
    """
    for root in ROOT_MAP.values():
        if (dest / root).exists():
            shutil.rmtree(dest / root)
    base = dest.resolve()
    with zipfile.ZipFile(zip_path) as z:
        for member, rel in members:
            target = (dest / rel).resolve()
            if not str(target).startswith(str(base)):
                die("refusing to extract %s outside Dependencies/GWCA/" % member)
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(member) as src:
                data = src.read().replace(b"\r\n", b"\n")
            with open(target, "wb") as f:
                f.write(data)


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="report only; exit 10 if a newer release exists")
    ap.add_argument("--force", action="store_true", help="re-vendor even if already current")
    ap.add_argument("--tag", default=None, help="pin to a specific release (e.g. v4.7.2.9)")
    ap.add_argument("--dry-run", action="store_true", help="show what would change, write nothing")
    ap.add_argument("--repo", default=DEFAULT_REPO)
    ap.add_argument("-q", "--quiet", action="store_true",
                    help="say nothing unless there is something to say (for --check in a build)")
    args = ap.parse_args()

    gh = find_gh()
    token = resolve_token()

    try:
        release = get_release(args.repo, args.tag, gh, token)
    except TransportError as e:
        if args.check:
            # A build-time check must never fail the build over a network blip.
            if not args.quiet:
                info("gwca: could not check for updates (%s)" % e)
            return 0
        die("could not reach %s: %s" % (args.repo, e), 3)

    tag = release.get("tag_name") or args.tag or "?"
    have = current_tag()
    assets = {a["name"]: a for a in release.get("assets", [])}
    missing = [n for n in REQUIRED_ASSETS if n not in assets]

    if args.check:
        if missing:
            if not args.quiet:
                info("gwca: %s has no %s -- nothing to update to"
                     % (tag, ", ".join(missing)))
            return 0
        if have and parse_version(tag) <= parse_version(have):
            if not args.quiet:
                info("gwca: %s is current" % have)
            return 0
        info("gwca: %s is available (vendored: %s). Update with:" % (tag, have or "none"))
        info("    python3 tools/update_gwca.py")
        return 10

    if missing:
        die("release %s has no %s. gwca.wasm is published by GWCA's build-wasm "
            "CI job; a release cut before that job existed will not have it -- "
            "pass --tag to pick one that does." % (tag, ", ".join(missing)))

    if have and not args.force and parse_version(tag) <= parse_version(have) \
            and GWCA_DIR.is_dir():
        info("gwca: %s already vendored (--force to re-download)" % have)
        return 0

    info("gwca: vendoring %s (have: %s)" % (tag, have or "none"))
    if args.dry_run:
        for name in REQUIRED_ASSETS:
            info("    would download %s (%d bytes)" % (name, assets[name].get("size", 0)))
        info("    would write %s and update %s" % (GWCA_DIR, PIN.name))
        return 0

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        try:
            download_assets(args.repo, tag, gh, token, assets, tmp)
        except TransportError as e:
            die("download failed: %s" % e, 3)
        for name in REQUIRED_ASSETS:
            if not (tmp / name).is_file():
                die("release %s did not yield %s" % (tag, name), 3)

        members = headers_members(tmp / HEADERS_ASSET)
        if not any(rel.startswith("include/") for _, rel in members):
            die("%s has no Include/ -- not the expected layout" % HEADERS_ASSET)
        if not any(rel.startswith("source/") for _, rel in members):
            die("%s has no Source/ -- it predates the wasm shim being staged into "
                "the zip, so it cannot build a wasm consumer" % HEADERS_ASSET)

        GWCA_DIR.mkdir(parents=True, exist_ok=True)
        extract_headers(tmp / HEADERS_ASSET, members, GWCA_DIR)

        (GWCA_DIR / "bin").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(tmp / DLL_ASSET, GWCA_DIR / "bin" / DLL_ASSET)
        (GWCA_DIR / "lib").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(tmp / LIB_ASSET, GWCA_DIR / "lib" / LIB_ASSET)
        (GWCA_DIR / "wasm").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(tmp / WASM_ASSET, GWCA_DIR / "wasm" / WASM_ASSET)

        abi = abi_from_headers(GWCA_DIR / "include")
        if not abi:
            die("vendored headers declare no GWCA_ABI_VERSION")

        pin = {
            "_": "Written by tools/update_gwca.py. The pinned GWCA release this "
                 "repo builds against: gwca.dll/gwca.lib (native), gwca.wasm "
                 "(wasm) and the headers, from one release so they cannot "
                 "disagree. Read `abi` to guard against a headers/binary mismatch.",
            "repo": args.repo,
            "tag": tag,
            "abi": abi,
            "assets": {name: {"size": (tmp / name).stat().st_size,
                              "sha256": sha256(tmp / name)}
                       for name in REQUIRED_ASSETS},
        }
        PIN.write_text(json.dumps(pin, indent=2) + "\n", encoding="utf-8")

    info("    %-20s %d bytes" % (DLL_ASSET, (GWCA_DIR / "bin" / DLL_ASSET).stat().st_size))
    info("    %-20s %d bytes" % (LIB_ASSET, (GWCA_DIR / "lib" / LIB_ASSET).stat().st_size))
    info("    %-20s %d bytes" % (WASM_ASSET, (GWCA_DIR / "wasm" / WASM_ASSET).stat().st_size))
    info("    %-20s %d files" % ("headers", len(members)))
    info("    ABI %s, pinned in %s" % (abi, PIN.name))
    info("")
    info("Rebuild, and commit Dependencies/GWCA/ -- it is the pinned dependency, not a cache.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
