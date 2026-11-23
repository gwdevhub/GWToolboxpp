#!/usr/bin/env python3
"""One-shot release script for GWToolbox (build -> tag -> GitHub release).

Usage:
  python deploy.py                 - bump CMakeLists.txt to the next minor version, open history.md, then release
  python deploy.py 8.20            - bump CMakeLists.txt to 8.20, open history.md, then release
  python deploy.py --publish       - publish the GitHub release straight away (default: a draft you review first)
  python deploy.py --yes           - skip the interactive "Build and release?" confirmation
  python deploy.py --dry-run       - build and check everything, but change no git state and create no release

Flags can appear in any order. A release is refused unless the site changelog
(site/src/content/docs/history.md) has a non-empty "## Version <ver>" block - we
never ship a version without release notes. A dry run doesn't need the block yet:
it uses a placeholder and says so.

Branch flow: the version bump, build (code checks) and release commit all happen
on the dev branch. dev is then merged into master and the release tag is created
on master. HEAD is returned to dev when the release finishes.

Release assets: the DLL and its PDB, the launcher exe and its PDB, and the Linux
AppImage with its debug file. The PDBs and the debug file are gzipped first. A
binary and its gzipped file are uploaded only when the newest GitHub release that
has the binary has a different hash, so an unchanged binary isn't uploaded again.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(REPO_ROOT / "scripts"))
from common import gzip_file  # noqa: E402

CMAKE = REPO_ROOT / "CMakeLists.txt"
# The live site is the Astro app under site/ (deployed by site-deploy.yml). It
# renders release notes from this file - the old docs/history.md (Jekyll) is no
# longer deployed, so the changelog must be written here.
HISTORY = REPO_ROOT / "site" / "src" / "content" / "docs" / "history.md"
# Download-button fallback version (the page prefers the live Releases API).
SITE_CONFIG = REPO_ROOT / "site" / "site.config.json"
BIN = REPO_ROOT / "bin" / "RelWithDebInfo"

# Files touched by a version bump, as repo-relative paths for git add/diff.
RELEASE_FILES = [
    "CMakeLists.txt",
    "site/src/content/docs/history.md",
    "site/site.config.json",
]

# Code checks + the release commit happen on DEV_BRANCH; it is then merged into
# RELEASE_BRANCH, which is what gets tagged.
DEV_BRANCH = "dev"
RELEASE_BRANCH = "master"

# Content that is technically present but does not count as real release notes.
PLACEHOLDER_NOTES = {"", "tbd", "tba", "todo", "...", "notes", "release notes", "changelog"}

DRY_RUN = False


def log(msg: str) -> None:
    print(f"[deploy] {msg}")


def fail(msg: str) -> "NoReturn":  # type: ignore[name-defined]
    log(msg)
    log("FAILED.")
    sys.exit(1)


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    """Run a command, echoing it. Raises on non-zero exit unless check=False."""
    check = kwargs.pop("check", True)
    log("$ " + " ".join(cmd))
    proc = subprocess.run(cmd, cwd=REPO_ROOT, **kwargs)
    if check and proc.returncode != 0:
        fail(f"command failed ({proc.returncode}): {' '.join(cmd)}")
    return proc


def change(cmd: list[str]) -> None:
    """Run a command that changes git or GitHub state, or only print it in a dry run."""
    if DRY_RUN:
        log("[dry-run] would run: " + " ".join(cmd))
        return
    run(cmd)


def git_quiet(*args: str) -> bool:
    """Return True if the git command exits 0, suppressing all output."""
    return subprocess.run(
        ["git", *args], cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    ).returncode == 0


def origin_repo_slug() -> str | None:
    """Return 'owner/name' parsed from the origin remote URL, or None.

    Passed to gh as --repo so release creation never depends on a default
    repository being configured (`gh repo set-default`) - a fresh clone, or one
    with multiple remotes, otherwise fails with "no default remote repository".
    """
    url = subprocess.run(
        ["git", "config", "--get", "remote.origin.url"], cwd=REPO_ROOT,
        capture_output=True, text=True).stdout.strip()
    if not url:
        return None
    # Handles https://github.com/owner/name(.git) and git@github.com:owner/name(.git)
    m = re.search(r"[:/]([^/:]+/[^/]+?)(?:\.git)?/?$", url)
    return m.group(1) if m else None


def current_branch() -> str:
    """Return the short name of the currently checked-out branch."""
    return subprocess.run(
        ["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=REPO_ROOT,
        capture_output=True, text=True,
    ).stdout.strip()


# --- version handling -------------------------------------------------------

def read_current_version() -> str:
    text = CMAKE.read_text(encoding="utf-8")
    m = re.search(r'(?m)^set\(GWTOOLBOXDLL_VERSION\s+"([^"]*)"\)', text)
    if not m:
        fail("Could not parse current GWTOOLBOXDLL_VERSION from CMakeLists.txt")
    return m.group(1)


def next_version(version: str) -> str:
    """Return the next MAJOR.MINOR version, incrementing the minor component."""
    m = re.fullmatch(r"(\d+)\.(\d+)", version)
    if not m:
        fail(f'Cannot compute next version from "{version}" - expected MAJOR.MINOR')
    return f"{m.group(1)}.{int(m.group(2)) + 1}"


def bump_cmake_version(new_ver: str) -> None:
    maj, minor = new_ver.split(".")
    module_ver = f"{maj},{minor},0,0"
    text = CMAKE.read_text(encoding="utf-8")
    text, n1 = re.subn(
        r'(?m)^set\(GWTOOLBOXDLL_VERSION "[^"]*"\)',
        f'set(GWTOOLBOXDLL_VERSION "{new_ver}")', text)
    # The module version is stored quoted: set(GWTOOLBOX_MODULE_VERSION "8,19,0,0")
    # Keep the trailing comment (if any) intact by only replacing the quoted value.
    text, n2 = re.subn(
        r'(?m)^(set\(GWTOOLBOX_MODULE_VERSION )"[0-9,]+"\)',
        rf'\g<1>"{module_ver}")', text)
    if n1 != 1 or n2 != 1:
        fail("Failed to rewrite version lines in CMakeLists.txt "
             f"(version matches={n1}, module matches={n2})")
    CMAKE.write_text(text, encoding="utf-8")
    log(f"Bumped CMakeLists.txt to {new_ver} (module {module_ver})")


def bump_site_version(new_ver: str) -> None:
    """Keep the download-button fallback (site.config.json) in sync.

    The landing page resolves the displayed version from the GitHub Releases API
    at build time; toolboxVersion is only the offline/rate-limited fallback, but
    it must still match so the button never advertises a stale version.
    """
    text = SITE_CONFIG.read_text(encoding="utf-8")
    text, n = re.subn(
        r'("toolboxVersion"\s*:\s*)"[^"]*"',
        rf'\g<1>"{new_ver}"', text)
    if n != 1:
        fail(f'Failed to rewrite "toolboxVersion" in {SITE_CONFIG.name} (matches={n})')
    SITE_CONFIG.write_text(text, encoding="utf-8")
    log(f"Bumped {SITE_CONFIG.name} toolboxVersion to {new_ver}")


# --- release notes ----------------------------------------------------------

def extract_notes(version: str) -> str | None:
    """Return the body of the '## Version <version>' block, or None if absent."""
    if not HISTORY.exists():
        return None
    text = HISTORY.read_text(encoding="utf-8")
    pat = (r'(?ms)^##\s*Version\s+' + re.escape(version)
           + r'\s*\r?\n(.*?)(?=^##\s*Version\s|\Z)')
    m = re.search(pat, text)
    return m.group(1).strip() if m else None


def notes_are_real(notes: str | None) -> bool:
    """A block counts as real notes only if it has at least one substantive bullet."""
    if not notes:
        return False
    for line in notes.splitlines():
        # strip bullet markers / numbering / whitespace
        content = re.sub(r'^\s*(?:[-*+]|\d+\.)\s*', "", line).strip()
        if content and content.lower() not in PLACEHOLDER_NOTES:
            return True
    return False


def require_notes(version: str) -> str:
    """Guard: refuse to continue without real release notes for this version."""
    notes = extract_notes(version)
    if notes is None:
        fail(f'No "## Version {version}" section found in {HISTORY.name} - '
             "add release notes before deploying.")
    if not notes_are_real(notes):
        fail(f'The "## Version {version}" section in {HISTORY.name} is empty or '
             "only a placeholder - real release notes are required before deploying.")
    return notes


def open_editor(path: Path) -> None:
    editor = os.environ.get("EDITOR")
    if editor:
        cmd = [*editor.split(), str(path)]
    elif os.name == "nt":
        cmd = ["notepad", str(path)]
    else:
        cmd = ["nano", str(path)]
    log(f'Opening {path} - add the changelog entry, save, then close the editor.')
    subprocess.run(cmd, cwd=REPO_ROOT)


# --- release files ----------------------------------------------------------

def require_files(paths: list[Path]) -> None:
    for path in paths:
        if not path.exists():
            fail(f"Missing {path.relative_to(REPO_ROOT)} - the build didn't produce it.")


def compress_for_release(path: Path) -> Path:
    target = path.with_name(path.name + ".gz")
    if target.exists() and target.stat().st_mtime >= path.stat().st_mtime:
        log(f"{target.name} is up to date with {path.name} - not gzipping it again")
        return target
    log(f"Gzipping {path.name} -> {target.name}")
    gzip_file(path, target)
    return target


# --- comparing with earlier releases ----------------------------------------

def parse_json_documents(text: str) -> list:
    """Parse the concatenated JSON arrays that `gh api --paginate` prints, one per page."""
    decoder = json.JSONDecoder()
    items = []
    position = 0
    while position < len(text):
        document, position = decoder.raw_decode(text, position)
        items.extend(document)
        while position < len(text) and text[position].isspace():
            position += 1
    return items


def fetch_releases(slug: str) -> list[dict]:
    """The repository's published releases, newest first. Drafts are left out."""
    listing = run(["gh", "api", "--paginate", f"repos/{slug}/releases?per_page=100"],
                  capture_output=True, text=True)
    releases = parse_json_documents(listing.stdout)
    return [release for release in releases if not release.get("draft")]


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def latest_digest(releases: list[dict], name: str) -> str | None:
    """The sha256 of the asset called `name` in the newest release that has one.

    None when no release has an asset of that name; "" when GitHub gives no digest for it.
    """
    for release in releases:
        for asset in release.get("assets", []):
            if asset.get("name") == name:
                return (asset.get("digest") or "").removeprefix("sha256:")
    return None


def select_uploads(releases: list[dict], binaries: list[tuple[Path, Path]]) -> list[Path]:
    """The binaries and gzipped files to upload: a pair only when its binary differs from the newest release that has it."""
    uploads: list[Path] = []
    for binary, gz in binaries:
        local = sha256_of(binary)
        released = latest_digest(releases, binary.name)
        if released is None:
            log(f"{binary.name}: no release has it yet - uploading it and {gz.name}")
        elif released == local:
            log(f"{binary.name}: same as the newest release that has it - not uploading it or {gz.name}")
            continue
        elif released == "":
            log(f"{binary.name}: the newest release that has it gives no hash - uploading it and {gz.name}")
        else:
            log(f"{binary.name}: differs from the newest release that has it - uploading it and {gz.name}")
        uploads += [binary, gz]
    return uploads


# --- main -------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Build, tag, and publish a GWToolbox release.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    p.add_argument("version", nargs="?",
                   help="new MAJOR.MINOR version to bump to (optional)")
    p.add_argument("--publish", action="store_true",
                   help="publish the GitHub release straight away (by default it's created as a draft)")
    p.add_argument("--yes", "-y", action="store_true",
                   help="skip the interactive confirmation prompt")
    p.add_argument("--dry-run", action="store_true",
                   help="build and check everything, but change no git state and create no release")
    return p.parse_args()


def main() -> None:
    global DRY_RUN
    args = parse_args()
    DRY_RUN = args.dry_run
    if DRY_RUN:
        log("DRY RUN: nothing is committed, pushed, tagged or released.")

    if not (REPO_ROOT / ".git").exists():
        fail("deploy.py must be run from within the GWToolbox git repository.")

    # All code checks and the release commit happen on dev. Switch to it (and
    # fast-forward to origin) before reading the version so we bump dev's value.
    if current_branch() != DEV_BRANCH:
        log(f"Switching to {DEV_BRANCH}...")
        change(["git", "checkout", DEV_BRANCH])
    log(f"Updating {DEV_BRANCH} from origin...")
    if DRY_RUN:
        log(f"[dry-run] would fast-forward {DEV_BRANCH} from origin")
    elif not git_quiet("pull", "--ff-only", "origin", DEV_BRANCH):
        fail(f"Could not fast-forward {DEV_BRANCH} from origin - resolve manually and retry.")

    cur_ver = read_current_version()
    log(f"Current version in CMakeLists.txt: {cur_ver}")

    # Without an explicit version, always roll forward to the next minor so we
    # never re-release the version already shipped in CMakeLists.txt.
    target_ver = args.version if args.version else next_version(cur_ver)
    if not re.fullmatch(r"\d+\.\d+", target_ver):
        fail(f'Invalid version "{target_ver}" - expected MAJOR.MINOR')

    ver = target_ver
    tag = f"{ver}_Release"

    # --- ALL origin checks happen BEFORE we mutate any files ---
    # A fetch/existence failure must never leave a half-applied version bump in
    # the working tree, so fetch tags and verify the tag is free up front.
    # Refresh tags from origin first so the local check isn't fooled by a stale
    # tag list (e.g. someone else already released this version).
    if DRY_RUN:
        log("[dry-run] would fetch tags from origin (skipped; the remote tag check below still runs)")
    else:
        log("Fetching tags from origin...")
        # --force: origin is canonical for release tags, so realign any local tag
        # that has drifted from origin instead of failing on "would clobber existing
        # tag". --prune-tags drops local tags that no longer exist on origin. Together
        # the local tag set becomes an exact mirror of origin, which is what the
        # existence check below relies on.
        fetch = subprocess.run(
            ["git", "fetch", "--tags", "--force", "--prune-tags", "origin"],
            cwd=REPO_ROOT, capture_output=True, text=True)
        if fetch.returncode != 0:
            # git_quiet would swallow the reason, so echo git's own output verbatim
            # and point at the usual culprits before bailing (nothing has changed yet).
            detail = (fetch.stderr or fetch.stdout or "").strip()
            log("Could not fetch tags from origin - aborting before any files are changed.")
            if detail:
                log(f"git fetch exited {fetch.returncode}. Output was:")
                for line in detail.splitlines():
                    log(f"    {line}")
            else:
                log(f"git fetch exited {fetch.returncode} with no output.")
            log("Common causes:")
            log("  - A stale per-branch refspec in 'git config --get-all remote.origin.fetch' "
                "(a tracked branch was deleted on origin); remove the dead entry.")
            log("  - No network / DNS, or the origin host is unreachable.")
            log("  - Expired or missing credentials for the origin remote.")
            fail("Fix the fetch and re-run deploy.py.")
    if git_quiet("rev-parse", "-q", "--verify", f"refs/tags/{tag}"):
        fail(f"Tag {tag} already exists locally - aborting.")
    if git_quiet("ls-remote", "--exit-code", "--tags", "origin", f"refs/tags/{tag}"):
        fail(f"Tag {tag} already exists on origin - aborting.")

    # --- now safe to mutate: version bump + changelog editing (skip if unchanged) ---
    if target_ver != cur_ver:
        if DRY_RUN:
            log(f"[dry-run] would bump CMakeLists.txt and {SITE_CONFIG.name} to {target_ver}")
        else:
            bump_cmake_version(target_ver)
            bump_site_version(target_ver)
            # New version => there is no existing block; force the author to write one.
            if not notes_are_real(extract_notes(ver)):
                open_editor(HISTORY)

    # --- GUARD: never release without real notes (checked before any push/build) ---
    if DRY_RUN:
        notes = extract_notes(ver)
        if not notes_are_real(notes):
            log(f'[dry-run] no real notes for {ver} yet in {HISTORY.name}; using a placeholder')
            notes = f"(dry run: no release notes written yet for {ver})"
    else:
        notes = require_notes(ver)

    print("[deploy] Release notes:")
    print("-" * 40)
    print(notes)
    print("-" * 40)

    # --- confirm before anything irreversible ---
    if not args.yes and not DRY_RUN:
        if input(f"[deploy] Build and release {tag}? [y/N] ").strip().lower() != "y":
            fail("Aborted by user.")

    # --- build: the DLL and both launchers ---
    log("Building RelWithDebInfo: the DLL and both launchers...")
    run([sys.executable, str(REPO_ROOT / "scripts" / "build.py"), "--preset", "relwithdebinfo",
         "--target", "GWToolbox_DLL",
         "--target", "GWToolbox_Launcher_Windows",
         "--target", "GWToolbox_Launcher_Linux"])

    dll = BIN / "GWToolboxdll.dll"
    dll_pdb = BIN / "GWToolboxdll.pdb"
    exe = BIN / "GWToolbox.exe"
    exe_pdb = BIN / "GWToolbox.pdb"
    appimage = BIN / "GWToolbox.AppImage"
    debug = BIN / "GWToolbox.debug"
    require_files([dll, dll_pdb, exe, exe_pdb, appimage, debug])

    # --- compress the debug files for distribution ---
    dll_pdb_gz = compress_for_release(dll_pdb)
    exe_pdb_gz = compress_for_release(exe_pdb)
    debug_gz = compress_for_release(debug)

    # --- upload only the binaries that changed since the newest release that has them ---
    slug = origin_repo_slug()
    if not slug:
        fail("Can't tell which GitHub repository origin points at, so the release assets can't be compared.")
    log(f"Comparing the built binaries with the releases of {slug}...")
    releases = fetch_releases(slug)
    upload_paths = select_uploads(releases, [(dll, dll_pdb_gz), (exe, exe_pdb_gz), (appimage, debug_gz)])

    # --- commit version bump + history on dev (only the release files, if changed) ---
    # Diff against HEAD (not the index) so an already-`git add`ed-but-uncommitted
    # bump is still picked up - a plain `git diff` would miss staged changes.
    if not git_quiet("diff", "--quiet", "HEAD", "--", *RELEASE_FILES):
        log("Committing version bump...")
        change(["git", "add", *RELEASE_FILES])
        change(["git", "commit", "-m", f"release: {ver}"])
    log(f"Pushing {DEV_BRANCH}...")
    change(["git", "push", "origin", DEV_BRANCH])

    # --- merge dev into master; the resulting master commit is what we tag ---
    log(f"Merging {DEV_BRANCH} into {RELEASE_BRANCH}...")
    change(["git", "checkout", RELEASE_BRANCH])
    if DRY_RUN:
        log(f"[dry-run] would fast-forward {RELEASE_BRANCH} from origin")
    elif not git_quiet("pull", "--ff-only", "origin", RELEASE_BRANCH):
        fail(f"Could not fast-forward {RELEASE_BRANCH} from origin - resolve manually and retry.")
    change(["git", "merge", "--no-edit", DEV_BRANCH])
    change(["git", "push", "origin", RELEASE_BRANCH])

    # --- tag master, push ---
    change(["git", "tag", tag])
    change(["git", "push", "origin", tag])

    # gh wants the notes in a file
    with tempfile.NamedTemporaryFile(
            "w", suffix=".md", prefix=f"gwtb_release_notes_{ver}_",
            delete=False, encoding="utf-8") as f:
        f.write(notes + "\n")
        notes_path = f.name

    try:
        log(f"Creating GitHub release {tag} as a {'published release' if args.publish else 'draft'}")
        gh = ["gh", "release", "create"]
        slug = origin_repo_slug()
        if slug:
            gh += ["--repo", slug]
        if not args.publish:
            gh.append("--draft")
        gh += [tag, "--title", tag, "--notes-file", notes_path, *[str(path) for path in upload_paths]]
        if DRY_RUN:
            change(gh)
        else:
            created = run(gh, capture_output=True, text=True)
            urls = [line.strip() for line in created.stdout.splitlines() if line.strip().startswith("https://")]
            url = urls[-1] if urls else "(gh printed no URL - find it on the repository's Releases page)"
            print("=" * 60)
            print(f"[deploy] {'Release published' if args.publish else 'DRAFT release created - not public yet'}: {url}")
            if not args.publish:
                print("[deploy] Open the link above, review the release, and publish it on GitHub when you're ready.")
            print("=" * 60)
    finally:
        try:
            os.unlink(notes_path)
        except OSError:
            pass

    # --- back to dev so the working tree is left where the next release starts ---
    log(f"Returning to {DEV_BRANCH}...")
    change(["git", "checkout", DEV_BRANCH])

    log("Dry run finished." if DRY_RUN else "Done.")


if __name__ == "__main__":
    main()
