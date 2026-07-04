#!/usr/bin/env python3
"""verify_pattern.py -- masked byte-signature scanner over an on-disk PE.

Re-implements the semantics of GWCA's FileScanner / Scanner (see
GWCA/Source/FileScanner.cpp and GWCA/Source/Scanner.cpp) against a Gw.exe on
disk, WITHOUT loading or running the game. The purpose is to check the
*uniqueness* of a byte signature before it is trusted: a signature that GWCA's
Scanner::Find would resolve at runtime must match EXACTLY ONCE in its section.
This tool is the core re-derivation safety primitive -- after a client update
you re-derive a broken signature, then run it through here to confirm
`unique == true` before committing it.

Matcher semantics reproduced faithfully from GWCA:

  * The PE is treated as an image mapping (SEC_IMAGE): each section's raw bytes
    live at its VirtualAddress and are scanned over [VirtualAddress,
    VirtualAddress + Misc.VirtualSize). Bytes past SizeOfRawData are zero-filled
    (as a real image mapping would). (FileScanner::FileScanner ctor)

  * FindInRange (forward): pattern length is taken from the MASK length
    (strlen(mask)), the scan end is pulled in by that length, and for every
    candidate position i there is a quick-reject on pattern[0] followed by a
    masked compare where 'x' bytes must be equal and '?' bytes are ignored.
    The returned address is `i + offset`. We reproduce the pattern[0]
    quick-reject exactly (it is observable when mask[0]=='?'), and we collect
    ALL matches instead of returning only the first, so uniqueness can be
    judged. (FileScanner::FindInRange)

  * Reported addresses:
      rva         = section.VirtualAddress + position + offset
      va          = OptionalHeader.ImageBase + rva   (runtime VA at preferred
                    base; GWCA returns dllImageBase + rva)
      file_offset = section.PointerToRawData + position + offset (raw on-disk
                    byte offset; null if the offset-adjusted position lands in
                    the section's zero-fill tail beyond SizeOfRawData)

  * --to-func-start reproduces Scanner::ToFunctionStart: for each match, walk
    BACKWARD (up to --func-start-range bytes) within .text looking for the
    function prologue bytes 55 8B EC (push ebp; mov ebp, esp), matching
    FindInRange's backward branch.

Pattern input accepts either form:
  * python-escaped byte string:  '\\x8b\\x45\\xe8'
  * space-separated hex:         '8b 45 e8'  (also '8B,45,E8', '0x8b ...')

Examples (current live GWCA raw patterns; each should be unique on a green tree):
  python verify_pattern.py --section .text \\
      --pattern '\\x8b\\x45\\xe8\\x8b\\x4d\\xec\\x8b\\x55\\xf0' --mask 'xxxxxxxxx'
  python verify_pattern.py --section .text \\
      --pattern '\\x81\\xf9\\x26\\x0b\\x00\\x00' --mask 'xxxxxx'
"""

import argparse
import json
import re
import struct
import sys

# GWCA scans exactly these three named sections.
KNOWN_SECTIONS = (".text", ".rdata", ".data")


class PeError(Exception):
    pass


class Section:
    __slots__ = ("name", "virtual_size", "virtual_address",
                 "size_of_raw_data", "pointer_to_raw_data", "data")

    def __init__(self, name, virtual_size, virtual_address,
                 size_of_raw_data, pointer_to_raw_data, data):
        self.name = name
        self.virtual_size = virtual_size
        self.virtual_address = virtual_address
        self.size_of_raw_data = size_of_raw_data
        self.pointer_to_raw_data = pointer_to_raw_data
        # Image-mapped view: raw bytes at index 0, zero-filled out to
        # virtual_size (an SEC_IMAGE mapping zero-fills the tail). The scan
        # region length is virtual_size, matching GWCA's section end.
        self.data = data


class Pe:
    """Minimal PE32 parser -- just enough to mirror GWCA's FileScanner."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.raw = f.read()
        self.image_base = 0
        self.sections = {}
        self._parse()

    def _parse(self):
        raw = self.raw
        if len(raw) < 0x40 or raw[0:2] != b"MZ":
            raise PeError("not a DOS/PE image (missing MZ header)")
        e_lfanew = struct.unpack_from("<I", raw, 0x3C)[0]
        if e_lfanew + 4 + 20 > len(raw):
            raise PeError("truncated PE header")
        if raw[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
            raise PeError("missing PE signature")

        # IMAGE_FILE_HEADER
        fh_off = e_lfanew + 4
        (machine, num_sections, _time, _psym, _nsym,
         size_opt_hdr, _chars) = struct.unpack_from("<HHIIIHH", raw, fh_off)
        if machine != 0x14C:  # IMAGE_FILE_MACHINE_I386
            raise PeError(f"not a 32-bit x86 image (machine=0x{machine:x})")

        # IMAGE_OPTIONAL_HEADER32
        opt_off = fh_off + 20
        magic = struct.unpack_from("<H", raw, opt_off)[0]
        if magic != 0x10B:
            raise PeError(f"not PE32 (optional header magic=0x{magic:x})")
        # ImageBase sits at offset 28 within the PE32 optional header.
        self.image_base = struct.unpack_from("<I", raw, opt_off + 28)[0]

        # Section headers follow the optional header.
        sec_off = opt_off + size_opt_hdr
        for i in range(num_sections):
            base = sec_off + i * 40
            if base + 40 > len(raw):
                raise PeError("truncated section header table")
            name_bytes = raw[base:base + 8]
            name = name_bytes.split(b"\x00", 1)[0].decode("latin-1")
            (virtual_size, virtual_address, size_of_raw_data,
             pointer_to_raw_data) = struct.unpack_from("<IIII", raw, base + 8)

            if name not in KNOWN_SECTIONS or name in self.sections:
                continue

            raw_take = min(size_of_raw_data, virtual_size)
            body = raw[pointer_to_raw_data:pointer_to_raw_data + raw_take]
            if len(body) < virtual_size:
                body = body + b"\x00" * (virtual_size - len(body))
            self.sections[name] = Section(
                name, virtual_size, virtual_address,
                size_of_raw_data, pointer_to_raw_data, body)


def parse_pattern(text):
    """Parse a pattern into a list of byte values (0-255).

    Accepts a python-escaped '\\xNN' string or space/comma-separated hex.
    """
    text = text.strip()
    esc = re.findall(r"\\x([0-9a-fA-F]{2})", text)
    if esc:
        # If it looked like an escaped string, every meaningful token must be a
        # \xNN escape; reject stray trailing junk so escaping bugs are visible.
        stripped = re.sub(r"\\x[0-9a-fA-F]{2}", "", text)
        if stripped.strip():
            raise ValueError(
                f"pattern mixes \\xNN escapes with unrecognised text: "
                f"{stripped.strip()!r}")
        return [int(h, 16) for h in esc]

    tokens = re.split(r"[\s,]+", text)
    out = []
    for tok in tokens:
        if not tok:
            continue
        t = tok[2:] if tok.lower().startswith("0x") else tok
        if not re.fullmatch(r"[0-9a-fA-F]{1,2}", t):
            raise ValueError(f"unrecognised pattern token: {tok!r}")
        out.append(int(t, 16))
    if not out:
        raise ValueError("empty pattern")
    return out


def find_all(section, pattern, mask, offset):
    """Forward masked scan mirroring FileScanner::FindInRange, collecting every
    match position. Returns list of raw positions (offset applied)."""
    data = section.data
    plen = len(mask)
    if plen == 0:
        return []
    first = pattern[0]
    # GWCA: end -= patternLength; loop i in [start, end).
    scan_end = section.virtual_size - plen
    matches = []
    idx_range = range(plen)
    for i in range(0, scan_end + 1):
        # pattern[0] quick-reject (applied regardless of mask, as in GWCA).
        if data[i] != first:
            continue
        ok = True
        for idx in idx_range:
            if mask[idx] == "x" and pattern[idx] != data[i + idx]:
                ok = False
                break
        if ok:
            matches.append(i + offset)
    return matches


def walk_to_function_start(text_section, match_rva, scan_range):
    """Backward scan for the 55 8B EC prologue, mirroring
    Scanner::ToFunctionStart (which drives FileScanner::FindInRange's backward
    branch). match_rva is relative to the .text section start. Returns the
    position (relative to section start) of the prologue, or None."""
    if text_section is None:
        return None
    data = text_section.data
    proto = (0x55, 0x8B, 0xEC)
    plen = len(proto)
    start = match_rva
    # GWCA: end -= patternLength; then scans i from start down to >= end.
    end = (match_rva - scan_range) - plen
    if end < 0:
        end = 0
    if start > len(data) - plen:
        start = len(data) - plen
    for i in range(start, end - 1, -1):
        if i < 0:
            break
        if data[i] == proto[0] and data[i + 1] == proto[1] and data[i + 2] == proto[2]:
            return i
    return None


def build_match_record(pe, section, raw_pos, want_func_start, text_section,
                        func_start_range):
    """Turn a raw offset-adjusted position into rva/va/file_offset fields."""
    rva = section.virtual_address + raw_pos
    va = pe.image_base + rva
    # file_offset is only meaningful if it lands within the section's raw data.
    if 0 <= raw_pos < section.size_of_raw_data:
        file_offset = section.pointer_to_raw_data + raw_pos
    else:
        file_offset = None

    rec = {
        "va": f"0x{va:08x}",
        "rva": f"0x{rva:08x}",
        "file_offset": (f"0x{file_offset:08x}"
                        if file_offset is not None else None),
    }

    if want_func_start:
        fs = None
        # ToFunctionStart operates on .text. Map this match into .text if it
        # lands there; otherwise it cannot be resolved to a prologue.
        if text_section is not None:
            match_rel = rva - text_section.virtual_address
            if 0 <= match_rel < text_section.virtual_size:
                fs = walk_to_function_start(text_section, match_rel,
                                            func_start_range)
        if fs is not None:
            fs_rva = text_section.virtual_address + fs
            fs_va = pe.image_base + fs_rva
            fs_file = (text_section.pointer_to_raw_data + fs
                       if fs < text_section.size_of_raw_data else None)
            rec["func_start"] = {
                "va": f"0x{fs_va:08x}",
                "rva": f"0x{fs_rva:08x}",
                "file_offset": (f"0x{fs_file:08x}"
                                if fs_file is not None else None),
            }
        else:
            rec["func_start"] = None

    return rec


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="verify_pattern.py",
        description="Masked byte-signature scanner over an on-disk PE, "
                    "reproducing GWCA Scanner::Find semantics to check a "
                    "signature's uniqueness before it is used.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Prints JSON: {count, unique, matches:[{va,rva,file_offset}], ...}. "
               "unique == (count == 1) is the green result.")
    parser.add_argument(
        "--exe",
        default=r"C:\Program Files (x86)\Guild Wars\Gw.exe",
        help="Path to the PE to scan (default: live Guild Wars client).")
    parser.add_argument(
        "--pattern", required=True,
        help=r"Byte pattern: '\xNN\xNN...' or space/comma-separated hex.")
    parser.add_argument(
        "--mask", required=True,
        help="Mask string: 'x' = must match, '?' = wildcard. "
             "Length must equal the pattern's byte count.")
    parser.add_argument(
        "--section", default=".text", choices=list(KNOWN_SECTIONS),
        help="Section to scan (default: .text).")
    parser.add_argument(
        "--offset", default="0",
        help="Signed offset added to each match address (int; accepts 0x..). "
             "Default 0.")
    parser.add_argument(
        "--to-func-start", action="store_true",
        help="Also resolve each match to its function entry (walk back to "
             "55 8B EC in .text), like Scanner::ToFunctionStart.")
    parser.add_argument(
        "--func-start-range", default="0xff",
        help="Max bytes to walk back for --to-func-start (default 0xff, "
             "GWCA's ToFunctionStart default).")

    args = parser.parse_args(argv)

    try:
        offset = int(args.offset, 0)
        func_start_range = int(args.func_start_range, 0)
    except ValueError as e:
        parser.error(f"bad numeric argument: {e}")

    try:
        pattern = parse_pattern(args.pattern)
    except ValueError as e:
        parser.error(str(e))

    mask = args.mask
    if len(mask) != len(pattern):
        parser.error(
            f"mask length ({len(mask)}) != pattern byte count ({len(pattern)}). "
            f"This usually means the --pattern was mis-escaped by the shell. "
            f"pattern bytes = {' '.join(f'{b:02x}' for b in pattern)}")
    bad = set(mask) - {"x", "?"}
    if bad:
        parser.error(f"mask must contain only 'x' and '?'; got {sorted(bad)}")

    try:
        pe = Pe(args.exe)
    except (OSError, PeError) as e:
        print(json.dumps({"error": str(e), "exe": args.exe}), file=sys.stderr)
        return 2

    section = pe.sections.get(args.section)
    text_section = pe.sections.get(".text")
    if section is None:
        print(json.dumps({
            "error": f"section {args.section} not found in image",
            "sections_present": sorted(pe.sections),
        }), file=sys.stderr)
        return 2

    positions = find_all(section, pattern, mask, offset)
    matches = [
        build_match_record(pe, section, pos, args.to_func_start,
                            text_section, func_start_range)
        for pos in positions
    ]

    result = {
        "exe": args.exe,
        "section": args.section,
        "image_base": f"0x{pe.image_base:08x}",
        "pattern": " ".join(f"{b:02x}" for b in pattern),
        "mask": mask,
        "offset": offset,
        "count": len(matches),
        "unique": len(matches) == 1,
        "matches": matches,
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
