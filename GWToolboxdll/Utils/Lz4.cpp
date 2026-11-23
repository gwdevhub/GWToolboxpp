#include "stdafx.h"

#include <Utils/Lz4.h>

// Format: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
bool Lz4Decompress(const void* compressed, const size_t compressed_size, std::string& out)
{
    out.clear();
    if (compressed_size < 4) return false;
    const auto* in = static_cast<const uint8_t*>(compressed);
    const auto* const end = in + compressed_size;
    uint32_t decompressed_size;
    memcpy(&decompressed_size, in, sizeof(decompressed_size));
    in += sizeof(decompressed_size);
    out.reserve(decompressed_size);

    // A length nibble of 15 continues in the following bytes, each added on, until one is less than 255.
    bool truncated = false;
    const auto read_length = [&](size_t length) {
        if (length != 15) return length;
        uint8_t byte;
        do {
            if (in == end) {
                truncated = true;
                return length;
            }
            byte = *in++;
            length += byte;
        } while (byte == 255);
        return length;
    };

    while (in < end) {
        const uint8_t token = *in++;
        const size_t literals = read_length(token >> 4);
        if (truncated || literals > static_cast<size_t>(end - in)) return false;
        out.append(reinterpret_cast<const char*>(in), literals);
        in += literals;
        if (in == end) break; // the last sequence is literals only
        if (end - in < 2) return false;
        const size_t offset = in[0] | in[1] << 8;
        in += 2;
        const size_t match = read_length(token & 15) + 4;
        if (truncated || offset == 0 || offset > out.size() || out.size() + match > decompressed_size) return false;
        // Byte by byte: a match may overlap the bytes it is copying, which repeats them.
        const size_t start = out.size();
        out.resize(start + match);
        for (size_t i = 0; i < match; i++) {
            out[start + i] = out[start - offset + i];
        }
    }
    return out.size() == decompressed_size;
}
