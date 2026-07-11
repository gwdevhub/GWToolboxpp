#!/usr/bin/env python3
"""Read the Guild Wars client BUILD ID from an on-disk Gw.exe without running the game.

This is a standard-library-only port of GWLauncher's
``GuildWarsExecutableParser.GetFileId()``
(``gwlauncher/GW Launcher/Guildwars/GuildWarsExecutableParser.cs``). The client's PE
version resource is static (ArenaNet never bumps it), so the only reliable on-disk
signal of "which build is this" is an integer file/build id baked into a tiny
``mov eax, imm32 ; ret`` accessor next to the EULA-assertion string reference.

Algorithm (mirrors the C# byte-for-byte):

  1. PE-parse the exe; locate ``.rdata`` and ``.text`` sections and the ImageBase.
  2. Scan ``.rdata`` for the ASCII assertion string ``index < arrsize(s_eula)`` and
     take its RVA.
  3. Compute the absolute VA (ImageBase + RVA) and scan ``.text`` for the instruction
     that references it: ``mov ecx, imm32`` (0xB9) + <VA little-endian> + ``call`` (0xE8).
  4. Scan backwards up to 0xFF bytes from that reference for ``mov eax, imm32 ; ret``
     (bytes ``B8 ?? ?? ?? ?? C3``); the imm32 is the build id (~6-digit integer).
  5. Also compute SHA-256, size and mtime, and diff build/sha against a state file.

If the anchor string is absent or any scan fails, build_id is reported as null (with a
precise reason on stderr) and sha/size/mtime are still emitted.

Exit code is 0 on success (whether or not a build id was found); non-zero only on
usage / I/O errors (e.g. the exe does not exist).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
from datetime import datetime, timezone

DEFAULT_EXE = r"C:\Program Files (x86)\Guild Wars\Gw.exe"
DEFAULT_STATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "state.json")

EULA_ANCHOR = b"index < arrsize(s_eula)"


class PeError(Exception):
    """Raised when the file is not a parseable PE image."""


class Section:
    __slots__ = ("name", "virtual_size", "virtual_address", "raw_size", "raw_ptr")

    def __init__(self, name, virtual_size, virtual_address, raw_size, raw_ptr):
        self.name = name
        self.virtual_size = virtual_size
        self.virtual_address = virtual_address
        self.raw_size = raw_size
        self.raw_ptr = raw_ptr


class PeFile:
    """Minimal PE32 reader: sections, ImageBase, RVA<->offset, pattern scan."""

    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise PeError("not an MZ/PE image (bad DOS signature)")
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if data[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
            raise PeError("bad PE signature")
        coff = e_lfanew + 4
        (num_sections,) = struct.unpack_from("<H", data, coff + 2)
        (size_opt_hdr,) = struct.unpack_from("<H", data, coff + 16)
        opt = coff + 20
        (magic,) = struct.unpack_from("<H", data, opt)
        if magic == 0x10B:  # PE32
            (self.image_base,) = struct.unpack_from("<I", data, opt + 28)
        elif magic == 0x20B:  # PE32+
            (self.image_base,) = struct.unpack_from("<Q", data, opt + 24)
        else:
            raise PeError(f"unknown optional-header magic 0x{magic:04X}")

        sec_start = opt + size_opt_hdr
        self.sections: list[Section] = []
        for i in range(num_sections):
            base = sec_start + i * 40
            raw_name = data[base : base + 8]
            name = raw_name.rstrip(b"\x00").decode("latin-1", "replace")
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, base + 8)
            self.sections.append(Section(name, vsize, vaddr, rsize, rptr))

    def section(self, name: str) -> Section:
        for s in self.sections:
            if s.name == name:
                return s
        raise PeError(f"unable to find {name} section")

    def rva_to_offset(self, rva: int) -> int:
        for s in self.sections:
            if s.virtual_address <= rva <= s.virtual_address + s.virtual_size:
                return rva - s.virtual_address + s.raw_ptr
        raise PeError(f"could not find section for RVA 0x{rva:08X}")

    def read_u32(self, rva: int) -> int:
        off = self.rva_to_offset(rva)
        return struct.unpack_from("<I", self.data, off)[0]

    def find_in_range(self, pattern: bytes, mask: str, offset: int, start: int, end: int) -> int:
        """Scan for pattern/mask between two RVAs; scans backwards if end < start.

        Returns the matched RVA plus ``offset``, or 0 if not found. Faithful port of
        the C# ``FindInRange`` (inclusive-start / exclusive-end semantics and the
        distinct backward branch).
        """
        if len(pattern) != len(mask):
            raise ValueError("pattern and mask must be the same length")
        data = self.data
        plen = len(pattern)

        def matches(pos: int) -> bool:
            for j in range(plen):
                if mask[j] == "x" and data[pos + j] != pattern[j]:
                    return False
            return True

        if end >= start:
            frm = self.rva_to_offset(start)
            to = self.rva_to_offset(end) - plen
            i = frm
            while i <= to:
                if matches(i):
                    return (start + (i - frm) + offset) & 0xFFFFFFFF
                i += 1
        else:
            frm = self.rva_to_offset(start) - plen
            to = self.rva_to_offset(end)
            i = frm
            while i >= to:
                if matches(i):
                    return (end + (i - to) + offset) & 0xFFFFFFFF
                i -= 1
        return 0

    def find(self, pattern: bytes, mask: str, offset: int = 0, section: Section | None = None) -> int:
        if section is None:
            section = self.section(".text")
        start = section.virtual_address
        end = start + section.virtual_size
        return self.find_in_range(pattern, mask, offset, start, end)


def get_file_id(pe: PeFile) -> tuple[int | None, str]:
    """Return (build_id, reason). build_id is None on failure, reason explains why."""
    try:
        rdata = pe.section(".rdata")
    except PeError as e:
        return None, str(e)

    eula_rva = pe.find(EULA_ANCHOR, "x" * len(EULA_ANCHOR), 0, rdata)
    if eula_rva == 0:
        return None, "anchor string 'index < arrsize(s_eula)' not found in .rdata"

    va = (pe.image_base + eula_rva) & 0xFFFFFFFF
    b = struct.pack("<I", va)
    # mov ecx, imm32 (0xB9) <VA-le> ; call (0xE8)
    use_pattern = bytes([0xB9, b[0], b[1], b[2], b[3], 0xE8])
    use_rva = pe.find(use_pattern, "xxxxxx")
    if use_rva == 0:
        return None, (
            f"no 'mov ecx, 0x{va:08X}; call' reference to the EULA string found in .text"
        )

    # backward scan for mov eax, imm32 ; ret  ->  B8 ?? ?? ?? ?? C3
    fn_rva = pe.find_in_range(bytes([0xB8, 0, 0, 0, 0, 0xC3]), "x????x", 0, use_rva, use_rva - 0xFF)
    if fn_rva == 0:
        return None, "no adjacent 'mov eax, imm32; ret' (B8 ?? ?? ?? ?? C3) within 0xFF bytes"

    build_id = pe.read_u32(fn_rva + 1)
    return build_id, ""


def sha256_and_size(path: str) -> tuple[str, int]:
    h = hashlib.sha256()
    total = 0
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
            total += len(chunk)
    return h.hexdigest(), total


def load_state(path: str) -> dict:
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        prog="detect_build.py",
        description="Read the Guild Wars client build id (+ sha256/size/mtime) from an "
        "on-disk Gw.exe and diff it against a state file.",
    )
    ap.add_argument("--exe", default=DEFAULT_EXE, help="path to Gw.exe (default: live client)")
    ap.add_argument("--state", default=DEFAULT_STATE, help="path to state.json (default: alongside this script)")
    ap.add_argument("--update-state", action="store_true", help="write build/sha back to the state file")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.exe):
        print(f"error: exe not found: {args.exe}", file=sys.stderr)
        return 2

    try:
        with open(args.exe, "rb") as f:
            data = f.read()
    except OSError as e:
        print(f"error: cannot read exe: {e}", file=sys.stderr)
        return 2

    build_id: int | None = None
    try:
        pe = PeFile(data)
        build_id, reason = get_file_id(pe)
        if build_id is None:
            print(f"warning: build id unavailable: {reason}; reporting sha/size only", file=sys.stderr)
    except PeError as e:
        print(f"warning: PE parse failed: {e}; reporting sha/size only", file=sys.stderr)

    sha256 = hashlib.sha256(data).hexdigest()
    size = len(data)
    mtime = datetime.fromtimestamp(os.path.getmtime(args.exe), tz=timezone.utc).isoformat()

    state = load_state(args.state)
    last_build = state.get("last_build")
    last_sha = state.get("last_sha")
    if last_build is None and last_sha is None:
        changed = True
    else:
        changed = (build_id != last_build) or (sha256.lower() != str(last_sha).lower())

    result = {
        "build_id": build_id,
        "sha256": sha256,
        "size": size,
        "mtime": mtime,
        "changed": changed,
    }
    print(json.dumps(result, indent=2))

    if args.update_state:
        new_state = dict(state)
        new_state["last_build"] = build_id
        new_state["last_sha"] = sha256
        with open(args.state, "w", encoding="utf-8") as f:
            json.dump(new_state, f, indent=2)
            f.write("\n")
        print(f"state written to {args.state}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
