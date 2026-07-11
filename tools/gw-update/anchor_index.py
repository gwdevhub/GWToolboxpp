#!/usr/bin/env python3
"""Build the master anchor index of every memory-scan site across GWCA + toolbox.

Guild Wars ships without debug symbols, so GWCA and GWToolbox locate game code/data
at runtime by *scanning* the client's memory for byte signatures, referenced string
literals, or ArenaNet's shipped assertion strings. Every one of those scan call sites
is a maintenance anchor that a client update can break (see
docs/gw-auto-update/01-failure-surface.md and 02-detection-and-feedback.md).

This tool walks the GWCA and toolbox source trees, finds every occurrence of a
``GW::Scanner::*`` scan call, classifies it by update-resilience, and records the
literal pattern / string / assertion text it depends on. It also parses every
``GWCA_INFO("[SCAN] <name> = %p", <var>)`` self-report line and links each to the
nearest preceding scan site, so a broken ``[SCAN] ... = 00000000`` log entry maps
straight back to the exact source line that must be re-derived.

Scan calls captured (and their ``kind`` classification):

  * ``Scanner::Find`` / ``Scanner::FindInRange``               -> "raw"      (fragile: raw instruction bytes, break first)
  * ``Scanner::FindUseOfString`` / ``FindNthUseOfString``      -> "string"   (semi-resilient: re-find a referenced literal)
  * ``Scanner::FindAssertion``                                 -> "assertion"(self-healing: re-find a shipped assert string)
  * ``Scanner::FunctionFromNearCall`` / ``ToFunctionStart``    -> "wrapper"  (address transforms around one of the above)

Output: ``anchor-index.json`` next to this script, plus a summary breakdown to stdout.

Notes on correctness:
  * The C++ lexer strips ``//`` and ``/* */`` comments (so commented-out scan calls
    are ignored) while preserving string literals and byte offsets, then paren-matches
    across newlines so multi-line calls are captured whole.
  * ``Scanner::*`` *definitions* in GWCA/Source/Scanner.cpp (e.g.
    ``uintptr_t GW::Scanner::Find(const char* pattern, ...)``) are filtered out by
    detecting a parameter declaration as the first argument -- they are infrastructure,
    not game anchors. The internal calls inside Scanner.cpp use bare names (no
    ``Scanner::`` qualifier) and are excluded automatically.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

# --- default repo roots (overridable on the command line) --------------------
DEFAULT_GWCA = r"C:\Users\m\source\repos\GWCA"
DEFAULT_TOOLBOX = r"C:\Users\m\source\repos\gwtoolboxpp"

# Directory names never worth scanning (third-party / generated).
SKIP_DIR_NAMES = {"_deps", "build", "build-clang", "imgui", ".git", "out", "cmake-build"}

# Scan-call names, longest-first so the alternation never matches a prefix
# (e.g. "Find" must come after "FindInRange").
_CALL_NAMES = [
    "FindNthUseOfString",
    "FindUseOfString",
    "FindInRange",
    "FindAssertion",
    "FunctionFromNearCall",
    "ToFunctionStart",
    "Find",
]
# Negative lookbehind keeps "FileScanner::Find" (a different class) from matching
# the "Scanner::Find" substring inside it.
CALL_RE = re.compile(
    r"(?<![A-Za-z0-9_])(?:GW::)?Scanner::(" + "|".join(_CALL_NAMES) + r")\s*\("
)

KIND_BY_NAME = {
    "Find": "raw",
    "FindInRange": "raw",
    "FindUseOfString": "string",
    "FindNthUseOfString": "string",
    "FindAssertion": "assertion",
    "FunctionFromNearCall": "wrapper",
    "ToFunctionStart": "wrapper",
}

# A first argument that is a parameter declaration => this is a function
# *definition* (only happens in Scanner.cpp), not a real scan site.
_TYPE_KW = (
    r"char|wchar_t|int|unsigned|uint8_t|uint16_t|uint32_t|uint64_t|size_t|"
    r"bool|DWORD|uintptr_t|void|HMODULE|float|double|ScannerSection"
)
DEFINITION_ARG_RE = re.compile(
    r"^\s*(?:const\s+)?(?:unsigned\s+)?(?:" + _TYPE_KW + r")\b[\s*&]*\w+\s*$"
)

# GWCA_INFO("[SCAN] <name> = <fmt>", <var>, ...) self-report lines.
# <fmt> may be %p, %08X, "%p, %d", "%p\n" etc.; <name> holds no '=' or '"'.
SCAN_REPORT_RE = re.compile(
    r'GWCA_INFO\s*\(\s*"\[SCAN\]\s*(?P<name>[^"=]+?)\s*=\s*[^"]*"\s*,\s*'
)

# Casts to peel off the front of a [SCAN] report variable.
_CAST_PREFIX_RE = re.compile(
    r"^\s*(?:reinterpret_cast\s*<[^>]*>\s*\(|static_cast\s*<[^>]*>\s*\(|"
    r"\((?:void|uintptr_t|DWORD|void\s*\*|std::uintptr_t)\s*\*?\)\s*)"
)


# ============================================================================
# C++ lexing helpers
# ============================================================================
def strip_comments(text: str) -> str:
    """Return a copy with // and /* */ comments blanked to spaces.

    Length, newlines and byte offsets are preserved so positions map 1:1 back
    to the original. String and char literals are left intact (their contents
    are load-bearing for pattern/assertion extraction)."""
    out = list(text)
    i, n = 0, len(text)
    state = "code"  # code | line_comment | block_comment | string | char
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "line_comment"
                continue
            if c == "/" and nxt == "*":
                out[i] = out[i + 1] = " "
                i += 2
                state = "block_comment"
                continue
            if c == '"':
                state = "string"
            elif c == "'":
                state = "char"
        elif state == "line_comment":
            if c == "\n":
                state = "code"
            else:
                out[i] = " "
        elif state == "block_comment":
            if c == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "code"
                continue
            if c != "\n":
                out[i] = " "
        elif state in ("string", "char"):
            if c == "\\":  # escape: skip next char
                i += 2
                continue
            if (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "code"
        i += 1
    return "".join(out)


def match_paren(text: str, open_idx: int) -> int:
    """Given the index of a '(', return the index of its matching ')'.

    String/char literals are skipped so parens inside them do not count.
    Returns -1 if unbalanced (truncated file / malformed)."""
    depth = 0
    i, n = open_idx, len(text)
    state = "code"
    while i < n:
        c = text[i]
        if state == "code":
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return i
            elif c == '"':
                state = "string"
            elif c == "'":
                state = "char"
        elif state == "string":
            if c == "\\":
                i += 2
                continue
            if c == '"':
                state = "code"
        elif state == "char":
            if c == "\\":
                i += 2
                continue
            if c == "'":
                state = "code"
        i += 1
    return -1


def split_top_level_args(arg_text: str) -> list[str]:
    """Split a call's argument text on top-level commas (paren/string aware)."""
    args, depth, buf = [], 0, []
    i, n = 0, len(arg_text)
    state = "code"
    while i < n:
        c = arg_text[i]
        if state == "code":
            if c in "([{":
                depth += 1
            elif c in ")]}":
                depth -= 1
            elif c == '"':
                state = "string"
            elif c == "'":
                state = "char"
            elif c == "," and depth == 0:
                args.append("".join(buf))
                buf = []
                i += 1
                continue
        elif state == "string":
            if c == "\\":
                buf.append(c)
                if i + 1 < n:
                    buf.append(arg_text[i + 1])
                i += 2
                continue
            if c == '"':
                state = "code"
        elif state == "char":
            if c == "\\":
                buf.append(c)
                if i + 1 < n:
                    buf.append(arg_text[i + 1])
                i += 2
                continue
            if c == "'":
                state = "code"
        buf.append(c)
        i += 1
    if buf or args:
        args.append("".join(buf))
    return [a.strip() for a in args]


def extract_string_literals(arg: str) -> str | None:
    """Concatenate the inner text of every C/C++ string literal in ``arg``.

    Preserves escape sequences as written (e.g. ``\\x8b``); drops L/u8/u/U
    prefixes. Returns None when the argument holds no string literal."""
    parts = []
    i, n = 0, len(arg)
    while i < n:
        c = arg[i]
        if c == '"':
            j = i + 1
            inner = []
            while j < n:
                cj = arg[j]
                if cj == "\\":
                    inner.append(arg[j : j + 2])
                    j += 2
                    continue
                if cj == '"':
                    break
                inner.append(cj)
                j += 1
            parts.append("".join(inner))
            i = j + 1
        else:
            i += 1
    return "".join(parts) if parts else None


def clean_report_var(raw: str) -> str:
    """Strip leading casts from a [SCAN] report variable expression."""
    var = raw.strip()
    changed = True
    while changed:
        changed = False
        m = _CAST_PREFIX_RE.match(var)
        if m:
            var = var[m.end():].strip()
            if var.endswith(")"):
                var = var[:-1].strip()
            changed = True
    return var.rstrip(")").strip()


def line_of(text: str, idx: int) -> int:
    return text.count("\n", 0, idx) + 1


# ============================================================================
# Per-file scanning
# ============================================================================
def scan_file(repo: str, root: str, path: str, sites: list, reports: list) -> None:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as exc:
        print(f"warning: cannot read {path}: {exc}", file=sys.stderr)
        return

    rel = os.path.relpath(path, root).replace("\\", "/")
    stripped = strip_comments(text)

    file_sites: list[dict] = []

    # --- scan-call sites -----------------------------------------------------
    for m in CALL_RE.finditer(stripped):
        name = m.group(1)
        open_idx = m.end() - 1  # position of '('
        close_idx = match_paren(stripped, open_idx)
        if close_idx < 0:
            continue
        arg_region = text[open_idx + 1 : close_idx]
        args = split_top_level_args(arg_region)

        # Skip Scanner::* function *definitions* (first arg is a param decl).
        if args and DEFINITION_ARG_RE.match(args[0]):
            continue

        kind = KIND_BY_NAME[name]
        raw_expr = re.sub(r"\s+", " ", text[m.start() : close_idx + 1]).strip()
        rec: dict = {
            "repo": repo,
            "file": rel,
            "line": line_of(text, m.start()),
            "kind": kind,
            "call": name,
            "raw_expr": raw_expr,
        }

        if kind == "raw":
            rec["pattern"] = extract_string_literals(args[0]) if args else None
            rec["mask"] = (
                extract_string_literals(args[1])
                if len(args) > 1 and '"' in args[1]
                else None
            )
        elif kind == "string":
            rec["string_literal"] = extract_string_literals(args[0]) if args else None
        elif kind == "assertion":
            rec["assertion_file"] = extract_string_literals(args[0]) if args else None
            rec["assertion_msg"] = (
                extract_string_literals(args[1]) if len(args) > 1 else None
            )
        # wrappers carry no literal of their own.

        sites.append(rec)
        file_sites.append(rec)

    # --- [SCAN] self-report lines -------------------------------------------
    for m in SCAN_REPORT_RE.finditer(stripped):
        # Recover the GWCA_INFO(...) call bounds to read its argument list.
        open_idx = stripped.rfind("(", m.start(), m.end())
        close_idx = match_paren(stripped, open_idx)
        var = None
        if close_idx >= 0:
            args = split_top_level_args(text[open_idx + 1 : close_idx])
            if len(args) >= 2:
                var = clean_report_var(args[1])
        line = line_of(text, m.start())
        # Associate with nearest preceding scan site in the same file.
        owning = None
        for s in file_sites:
            if s["line"] <= line and (owning is None or s["line"] > owning["line"]):
                owning = s
        reports.append(
            {
                "repo": repo,
                "name": m.group("name").strip(),
                "var": var,
                "file": rel,
                "line": line,
                "owning_site_line": owning["line"] if owning else None,
                "owning_site_kind": owning["kind"] if owning else None,
            }
        )


def collect_files(root: str, subdirs_exts: list[tuple[str, tuple[str, ...]]]) -> list[str]:
    """Walk ``root``/<subdir> collecting files with the given extensions."""
    found: list[str] = []
    for subdir, exts in subdirs_exts:
        base = os.path.join(root, subdir)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d.lower() not in SKIP_DIR_NAMES]
            for fn in filenames:
                if fn.lower().endswith(exts):
                    found.append(os.path.join(dirpath, fn))
    return sorted(found)


# ============================================================================
# main
# ============================================================================
def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        description="Build anchor-index.json: every GW::Scanner scan site across "
        "GWCA + GWToolbox, classified by update-resilience, plus the "
        "[SCAN] self-report channel mapped back to its owning source line.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--gwca", default=DEFAULT_GWCA, help="GWCA repo root")
    ap.add_argument("--toolbox", default=DEFAULT_TOOLBOX, help="gwtoolboxpp repo root")
    ap.add_argument(
        "--out",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "anchor-index.json"),
        help="output JSON path (default: alongside this script)",
    )
    args = ap.parse_args(argv)

    sites: list[dict] = []
    reports: list[dict] = []

    # GWCA: Source/*.cpp plus Include/GWCA/** headers (rarely scanned, but checked).
    gwca_files = collect_files(
        args.gwca, [("Source", (".cpp",)), ("Include", (".h", ".hpp", ".cpp"))]
    )
    for p in gwca_files:
        scan_file("gwca", args.gwca, p, sites, reports)

    # Toolbox: GWToolboxdll/**/*.cpp (imgui/_deps/build excluded by SKIP_DIR_NAMES).
    tb_files = collect_files(args.toolbox, [("GWToolboxdll", (".cpp",))])
    for p in tb_files:
        scan_file("toolbox", args.toolbox, p, sites, reports)

    # ---- counts -------------------------------------------------------------
    def tally(recs, repo=None):
        out = {"raw": 0, "string": 0, "assertion": 0, "wrapper": 0}
        for r in recs:
            if repo is None or r["repo"] == repo:
                out[r["kind"]] += 1
        return out

    gwca_kinds = tally(sites, "gwca")
    tb_kinds = tally(sites, "toolbox")
    all_kinds = tally(sites)

    def tally_calls(repo):
        out = {n: 0 for n in _CALL_NAMES}
        for s in sites:
            if s["repo"] == repo:
                out[s["call"]] += 1
        return out

    gwca_calls = tally_calls("gwca")
    tb_calls = tally_calls("toolbox")

    def anchors(k):  # non-wrapper anchor sites (comparable to doc's 155/64)
        return k["raw"] + k["string"] + k["assertion"]

    counts = {
        "total_sites": len(sites),
        "by_repo": {
            "gwca": sum(1 for s in sites if s["repo"] == "gwca"),
            "toolbox": sum(1 for s in sites if s["repo"] == "toolbox"),
        },
        "by_kind": all_kinds,
        "by_repo_kind": {"gwca": gwca_kinds, "toolbox": tb_kinds},
        "by_repo_call": {"gwca": gwca_calls, "toolbox": tb_calls},
        "anchor_sites_excluding_wrappers": {
            "gwca": anchors(gwca_kinds),
            "toolbox": anchors(tb_kinds),
            "total": anchors(all_kinds),
        },
        "scan_reports": {
            "total": len(reports),
            "gwca": sum(1 for r in reports if r["repo"] == "gwca"),
            "toolbox": sum(1 for r in reports if r["repo"] == "toolbox"),
            "linked_to_site": sum(1 for r in reports if r["owning_site_line"] is not None),
        },
        "files_scanned": {"gwca": len(gwca_files), "toolbox": len(tb_files)},
    }

    doc = {
        "generated_from": {"gwca": args.gwca, "toolbox": args.toolbox},
        "counts": counts,
        "sites": sites,
        "reports": reports,
    }
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=2, ensure_ascii=False)

    # ---- stdout summary -----------------------------------------------------
    p = print
    p(f"anchor-index written to {args.out}")
    p("")
    p(f"total scan sites: {counts['total_sites']}")
    p(f"  by repo:  gwca={counts['by_repo']['gwca']}  toolbox={counts['by_repo']['toolbox']}")
    p(f"  by kind:  raw={all_kinds['raw']}  string={all_kinds['string']}  "
      f"assertion={all_kinds['assertion']}  wrapper={all_kinds['wrapper']}")
    p("")
    p("  GWCA    "
      f"raw={gwca_kinds['raw']}  string={gwca_kinds['string']}  "
      f"assertion={gwca_kinds['assertion']}  wrapper={gwca_kinds['wrapper']}  "
      f"(anchors excl. wrappers = {anchors(gwca_kinds)})")
    p("  Toolbox "
      f"raw={tb_kinds['raw']}  string={tb_kinds['string']}  "
      f"assertion={tb_kinds['assertion']}  wrapper={tb_kinds['wrapper']}  "
      f"(anchors excl. wrappers = {anchors(tb_kinds)})")
    p("")
    p(f"[SCAN] report lines: {counts['scan_reports']['total']} "
      f"(gwca={counts['scan_reports']['gwca']}, "
      f"toolbox={counts['scan_reports']['toolbox']}, "
      f"linked_to_site={counts['scan_reports']['linked_to_site']})")
    p("")
    p("  GWCA per-call: " + "  ".join(f"{n}={gwca_calls[n]}" for n in _CALL_NAMES))
    p("  TB   per-call: " + "  ".join(f"{n}={tb_calls[n]}" for n in _CALL_NAMES))
    p("")
    p("ground-truth check (docs/01-failure-surface.md):")
    p(f"  GWCA anchors excl. wrappers = {anchors(gwca_kinds)}  (doc ~155)")
    p(f"    Find(-only) = {gwca_calls['Find']}  (doc ~54 raw Find)")
    p(f"    string = {gwca_kinds['string']}  (doc ~32);  "
      f"assertion = {gwca_kinds['assertion']}  (doc ~50)")
    p(f"  GWCA [SCAN] lines = {counts['scan_reports']['gwca']}  (doc ~120)")
    p(f"  Toolbox anchors excl. wrappers = {anchors(tb_kinds)}  (doc ~64)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
