#pragma once

#include <string>

// Decompresses what tools/lz4_pack.py writes: a 4-byte little-endian decompressed size, then an LZ4 block. False, and
// out incomplete, if the data is malformed.
bool Lz4Decompress(const void* compressed, size_t compressed_size, std::string& out);
