"""Compresses a file in the LZ4 block format, after a 4-byte little-endian decompressed size, for embedding as a resource.
GWToolboxdll/Utils/Lz4.cpp decompresses it. Format: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md

    python tools/lz4_pack.py <input> <output>
"""
import struct
import sys

MIN_MATCH = 4
MAX_OFFSET = 65535
# The block format requires the last 5 bytes to be literals and the last match to start 12 or more bytes before the end.
LAST_LITERALS = 5
MATCH_LIMIT = 12


def write_length(out, length):
    while length >= 255:
        out.append(255)
        length -= 255
    out.append(length)


def write_sequence(out, literals, offset=0, match_length=0):
    token_literals = min(len(literals), 15)
    token_match = min(match_length - MIN_MATCH, 15) if match_length else 0
    out.append(token_literals << 4 | token_match)
    if len(literals) >= 15:
        write_length(out, len(literals) - 15)
    out += literals
    if match_length:
        out += struct.pack("<H", offset)
        if match_length - MIN_MATCH >= 15:
            write_length(out, match_length - MIN_MATCH - 15)


def compress(data):
    out = bytearray(struct.pack("<I", len(data)))
    last_position = {}
    anchor = 0
    position = 0
    match_end_limit = len(data) - LAST_LITERALS
    while position < len(data) - MATCH_LIMIT:
        key = data[position:position + MIN_MATCH]
        candidate = last_position.get(key)
        last_position[key] = position
        if candidate is None or position - candidate > MAX_OFFSET:
            position += 1
            continue
        length = MIN_MATCH
        while position + length < match_end_limit and data[candidate + length] == data[position + length]:
            length += 1
        write_sequence(out, data[anchor:position], position - candidate, length)
        position += length
        anchor = position
    write_sequence(out, data[anchor:])
    return bytes(out)


if __name__ == "__main__":
    with open(sys.argv[1], "rb") as source:
        packed = compress(source.read())
    with open(sys.argv[2], "wb") as destination:
        destination.write(packed)
