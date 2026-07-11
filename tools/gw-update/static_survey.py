#!/usr/bin/env python3
"""static_survey.py -- certify a source tree's raw byte-pattern anchors against a
given Gw.exe WITHOUT running the game.

Reads an anchor-index.json (produced by anchor_index.py), then re-scans every raw
``Scanner::Find`` / ``FindInRange`` byte pattern against the on-disk client using the
exact masked matcher GWCA uses (imported from verify_pattern.py), and classifies each:

  absent  (0 matches across .text/.rdata/.data)  -> BROKEN
            High confidence: the byte sequence is gone, so the anchor cannot resolve.
  unique  (exactly 1 match in .text)             -> OK
  multi   (>1 matches)                           -> AMBIGUOUS
            NOT necessarily broken. Short patterns, FindInRange (bounded to a runtime
            address range) and first-match patterns are intentionally non-unique.
            Confirm via the runtime [SCAN] survey (scan_survey.py) or Ghidra.

This is a fast NEGATIVE pre-check run before ever launching the game: it reliably
surfaces patterns that *definitely* broke on a client update and narrows the ambiguous
set. It does NOT replace the runtime [SCAN] survey -- enum shifts, struct-offset drift,
and first-match breakage are invisible to a static scan. See
docs/gw-auto-update/02-detection-and-feedback.md.

Only inspects files; writes only its --out JSON. Never runs or modifies the client.
"""
import argparse
import json
import os
import sys

# Reuse the proven masked PE scanner from verify_pattern.py (same directory).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verify_pattern as vp  # noqa: E402

DEFAULT_EXE = r"C:\Program Files (x86)\Guild Wars\Gw.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_INDEX = os.path.join(HERE, "anchor-index.json")
DEFAULT_OUT = os.path.join(HERE, "static-survey.json")
SEARCH_SECTIONS = (".text", ".rdata", ".data")


def scan_site(pe, pattern_text, mask):
    """Return (classification, detail) for one raw anchor.

    classification in {"ok", "ambiguous", "broken", "skipped"}.
    """
    try:
        pattern = vp.parse_pattern(pattern_text)
    except ValueError as exc:
        return "skipped", {"reason": f"unparseable pattern: {exc}"}
    if len(pattern) < len(mask):
        return "skipped", {"reason": f"pattern ({len(pattern)}) shorter than mask ({len(mask)})"}
    if len(mask) == 0:
        return "skipped", {"reason": "empty mask"}

    counts = {}
    for sec_name in SEARCH_SECTIONS:
        sec = pe.sections.get(sec_name)
        if sec is None:
            continue
        counts[sec_name] = len(vp.find_all(sec, pattern, mask, 0))

    text_n = counts.get(".text", 0)
    total = sum(counts.values())
    if total == 0:
        return "broken", {"counts": counts}
    if text_n == 1 and total == 1:
        return "ok", {"counts": counts}
    if text_n == 1:
        # unique in .text but also present elsewhere -> treat as ok (GWCA scans .text)
        return "ok", {"counts": counts}
    return "ambiguous", {"counts": counts}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE, help="Gw.exe to scan (default: live client)")
    ap.add_argument("--index", default=DEFAULT_INDEX, help="anchor-index.json path")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output static-survey.json path")
    ap.add_argument("--repo", choices=("gwca", "toolbox"), help="limit to one repo")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.exe):
        print(f"error: exe not found: {args.exe}", file=sys.stderr)
        return 2
    if not os.path.isfile(args.index):
        print(f"error: index not found: {args.index} (run anchor_index.py first)", file=sys.stderr)
        return 2

    try:
        pe = vp.Pe(args.exe)
    except vp.PeError as exc:
        print(f"error: PE parse failed: {exc}", file=sys.stderr)
        return 2

    with open(args.index, encoding="utf-8") as fh:
        index = json.load(fh)

    raw_sites = [s for s in index.get("sites", [])
                 if s.get("kind") == "raw" and s.get("pattern") and s.get("mask")]
    if args.repo:
        raw_sites = [s for s in raw_sites if s.get("repo") == args.repo]

    buckets = {"ok": [], "ambiguous": [], "broken": [], "skipped": []}
    for s in raw_sites:
        cls, detail = scan_site(pe, s["pattern"], s["mask"])
        rec = {
            "repo": s.get("repo"),
            "file": s.get("file"),
            "line": s.get("line"),
            "call": s.get("call"),
            "pattern": s.get("pattern"),
            "mask": s.get("mask"),
            "is_range": "FindInRange" in (s.get("raw_expr") or ""),
            **detail,
        }
        buckets[cls].append(rec)

    result = {
        "exe": args.exe,
        "image_base": hex(pe.image_base),
        "raw_sites_scanned": len(raw_sites),
        "summary": {k: len(v) for k, v in buckets.items()},
        "broken": buckets["broken"],
        "ambiguous": buckets["ambiguous"],
        "skipped": buckets["skipped"],
        "ok": buckets["ok"],
    }
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)

    # ---- human summary ----
    su = result["summary"]
    print(f"static breakage survey  exe={args.exe}")
    print(f"  raw anchors scanned : {result['raw_sites_scanned']}")
    print(f"  OK (unique)         : {su['ok']}")
    print(f"  ambiguous (>1 match): {su['ambiguous']}  (short/FindInRange/first-match; not necessarily broken)")
    print(f"  BROKEN (0 matches)  : {su['broken']}")
    print(f"  skipped             : {su['skipped']}")
    if buckets["broken"]:
        print("  --- BROKEN anchors (byte sequence absent from the client) ---")
        for r in buckets["broken"]:
            print(f"    {r['repo']:7} {r['file']}:{r['line']}  {r['call']}  {r['pattern']}")
    banner = "NEEDS-FIX (definite breakage)" if buckets["broken"] else "no definite breakage (ambiguous set still needs runtime [SCAN] confirmation)"
    print(f"  => {banner}")
    print(f"  wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
