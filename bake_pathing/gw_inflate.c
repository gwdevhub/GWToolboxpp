#include "gw_inflate.h"
#include <stdlib.h>

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t bitpos;
} BitStream;

typedef struct {
    uint8_t length;
    uint16_t symbol;
} ShortCode;

typedef struct {
    uint32_t first;
    int32_t last;
    uint8_t length;
} LongCode;

typedef struct {
    ShortCode short_codes[256];
    LongCode long_codes[24];
    uint16_t* long_symbols;
    uint32_t long_count;
} Huffman;

static uint32_t word_at(const BitStream* stream, size_t index)
{
    size_t offset = index * 4;
    if (offset >= stream->size || stream->size - offset < 4) return 0;
    const uint8_t* p = stream->data + offset;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t peek_bits(const BitStream* stream, uint32_t count)
{
    if (!count) return 0;
    size_t index = stream->bitpos / 32;
    uint64_t bits = ((uint64_t)word_at(stream, index) << 32) | word_at(stream, index + 1);
    return (uint32_t)((bits << (stream->bitpos % 32)) >> (64 - count));
}

static int read_bits(BitStream* stream, uint32_t count, uint32_t* value)
{
    if (count > 32 || stream->bitpos > stream->size * 8 ||
        count > stream->size * 8 - stream->bitpos) return 0;
    *value = peek_bits(stream, count);
    stream->bitpos += count;
    return 1;
}

static int build_huffman(BitStream* stream, const GwInflateTables* tables, Huffman* huffman)
{
    uint32_t symbol_count;
    if (!read_bits(stream, 16, &symbol_count) || !symbol_count) return 0;
    int32_t* follow = (int32_t*)malloc(symbol_count * sizeof(*follow));
    huffman->long_symbols = (uint16_t*)malloc(symbol_count * sizeof(*huffman->long_symbols));
    if (!follow || !huffman->long_symbols) {
        free(follow);
        free(huffman->long_symbols);
        huffman->long_symbols = NULL;
        return 0;
    }
    int32_t roots[32];
    for (int i = 0; i < 32; ++i) roots[i] = -1;
    for (int i = 0; i < 256; ++i) {
        huffman->short_codes[i].length = 0;
        huffman->short_codes[i].symbol = 0;
    }
    for (int i = 0; i < 24; ++i) {
        huffman->long_codes[i].first = 0;
        huffman->long_codes[i].last = -1;
        huffman->long_codes[i].length = 0;
    }
    huffman->long_count = 0;

    int32_t symbol_index = (int32_t)symbol_count - 1;
    uint32_t last_number = 0;
    uint32_t total_symbols = 0;
    while (symbol_index >= 0) {
        uint32_t encoded = peek_bits(stream, 32);
        int group = 0;
        while (group < 14 && tables->table1_first[group] > encoded) ++group;
        if (group == 14) goto invalid;
        uint32_t bit_count = (uint32_t)group + 3;
        uint32_t offset = (encoded - tables->table1_first[group]) >> (32 - bit_count);
        if (offset > tables->table1_index[group]) goto invalid;
        uint32_t table_index = tables->table1_index[group] - offset;
        if (table_index >= 256) goto invalid;
        uint8_t entry = tables->table2[table_index];
        uint32_t number = entry >> 5;
        uint32_t length = entry & 31;
        uint32_t unused;
        if (!read_bits(stream, bit_count, &unused)) goto invalid;
        last_number = number;
        if (length || symbol_count < 2) {
            ++number;
            total_symbols += number;
            if (number > (uint32_t)(symbol_index + 1)) goto invalid;
            for (uint32_t i = 0; i < number; ++i) {
                follow[symbol_index] = roots[length];
                roots[length] = symbol_index--;
            }
        }
        else {
            symbol_index -= (int32_t)(number + 1);
            if (symbol_index < -1) goto invalid;
        }
    }
    if (last_number && !total_symbols) {
        if (last_number > symbol_count) goto invalid;
        follow[last_number - 1] = roots[0];
        roots[0] = (int32_t)last_number - 1;
        total_symbols = 1;
    }

    int64_t next_encoding = 0;
    uint32_t short_symbols = 0;
    for (int length = 0; length <= 8; ++length) {
        for (int32_t symbol = roots[length]; symbol != -1; symbol = follow[symbol]) {
            if (next_encoding < 0 || next_encoding >= (INT64_C(1) << length)) goto invalid;
            uint32_t first = (uint32_t)next_encoding << (8 - length);
            uint32_t count = 1u << (8 - length);
            if (first + count > 256) goto invalid;
            for (uint32_t i = first; i < first + count; ++i) {
                huffman->short_codes[i].length = (uint8_t)length;
                huffman->short_codes[i].symbol = (uint16_t)symbol;
            }
            ++short_symbols;
            --next_encoding;
        }
        next_encoding = next_encoding * 2 + 1;
    }
    if (short_symbols > total_symbols) goto invalid;
    if (short_symbols != total_symbols) {
        for (int length = 9; length < 32; ++length) {
            for (int32_t symbol = roots[length]; symbol != -1; symbol = follow[symbol]) {
                if (next_encoding < 0 || next_encoding >= (INT64_C(1) << length) ||
                    huffman->long_count >= symbol_count) goto invalid;
                uint32_t partial = (uint32_t)(next_encoding >> (length - 8));
                if (partial >= 256) goto invalid;
                huffman->short_codes[partial].length = 255;
                huffman->long_symbols[huffman->long_count++] = (uint16_t)symbol;
                --next_encoding;
            }
            int64_t first = (next_encoding + 1) << (32 - length);
            if (first < 0 || first > UINT32_MAX) goto invalid;
            huffman->long_codes[length - 9].first = (uint32_t)first;
            huffman->long_codes[length - 9].last = (int32_t)huffman->long_count - 1;
            huffman->long_codes[length - 9].length = (uint8_t)length;
            next_encoding = next_encoding * 2 + 1;
        }
    }
    free(follow);
    return 1;
invalid:
    free(follow);
    free(huffman->long_symbols);
    huffman->long_symbols = NULL;
    return 0;
}

static int next_code(BitStream* stream, const Huffman* huffman, uint32_t* symbol)
{
    ShortCode code = huffman->short_codes[peek_bits(stream, 8)];
    uint32_t length = code.length;
    uint32_t value = code.symbol;
    if (length == 255) {
        uint32_t bits = peek_bits(stream, 32);
        int index = 0;
        while (index < 24 && huffman->long_codes[index].first > bits) ++index;
        if (index == 24 || !huffman->long_codes[index].length) return 0;
        LongCode large = huffman->long_codes[index];
        uint32_t shift = 32 - large.length;
        uint32_t group_index = (bits - large.first) >> shift;
        int64_t symbol_index = (int64_t)large.last - group_index;
        if (symbol_index < 0 || (uint64_t)symbol_index >= huffman->long_count) return 0;
        value = huffman->long_symbols[symbol_index];
        length = large.length;
    }
    uint32_t unused;
    if (!read_bits(stream, length, &unused)) return 0;
    *symbol = value;
    return 1;
}

int gw_inflate_all(const uint8_t* source, size_t source_size, uint8_t* output,
                   size_t capacity, size_t* output_size, const GwInflateTables* tables)
{
    if (!source || !output || !output_size || !tables || source_size < 8 || source_size > SIZE_MAX / 8) return -1;
    *output_size = 0;
    BitStream stream = {source, source_size, 0};
    uint32_t unused, first_four;
    if (!read_bits(&stream, 4, &unused) || !read_bits(&stream, 4, &first_four)) return -1;
    for (;;) {
        Huffman literal = {0};
        Huffman distance = {0};
        uint32_t block_size = 0;
        int ok = build_huffman(&stream, tables, &literal);
        if (ok) ok = build_huffman(&stream, tables, &distance);
        if (ok) ok = read_bits(&stream, 4, &block_size);
        if (!ok) {
            free(literal.long_symbols);
            free(distance.long_symbols);
            return *output_size && stream.bitpos >= source_size * 8 - 32 ? 0 : -1;
        }
        block_size = (block_size + 1) * 4096;
        for (uint32_t i = 0; i < block_size; ++i) {
            uint32_t code;
            if (!next_code(&stream, &literal, &code)) goto end_block;
            if (code < 256) {
                if (*output_size == capacity) {
                    free(literal.long_symbols);
                    free(distance.long_symbols);
                    return 1;
                }
                output[(*output_size)++] = (uint8_t)code;
            }
            else {
                uint32_t index = code - 256;
                if (index >= 29) goto end_block;
                uint32_t extra;
                if (!read_bits(&stream, tables->length_bits[index], &extra)) goto end_block;
                uint32_t amount = first_four + (tables->table3[index] | extra) + 1;
                if (!next_code(&stream, &distance, &code) || code >= 32) goto end_block;
                if (!read_bits(&stream, tables->distance_bits[code], &extra)) goto end_block;
                uint32_t backtrack = tables->distance_base[code] | extra;
                if (backtrack >= *output_size) goto end_block;
                if (amount > capacity - *output_size) {
                    free(literal.long_symbols);
                    free(distance.long_symbols);
                    return 1;
                }
                size_t previous = *output_size - (backtrack + 1);
                for (uint32_t n = 0; n < amount; ++n) {
                    output[(*output_size)++] = output[previous + n];
                }
            }
        }
        free(literal.long_symbols);
        free(distance.long_symbols);
        if (stream.bitpos >= source_size * 8) return 0;
        continue;
end_block:
        free(literal.long_symbols);
        free(distance.long_symbols);
        return *output_size && stream.bitpos >= source_size * 8 - 32 ? 0 : -1;
    }
}
