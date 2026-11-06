#include "stdafx.h"

#include "GuildWarsFileServer.h"

#include <stdexcept>

// Adapted from gwdevhub/gwlauncher backend/src/GwHuffman.cpp.
namespace {
    constexpr uint32_t empty = 0xffffffff;
    constexpr std::pair<uint32_t, uint32_t> table1[] = {
        {0xa0000000, 2}, {0x60000000, 6}, {0x40000000, 10}, {0x20000000, 18},
        {0x12000000, 25}, {0x0c000000, 31}, {0x07000000, 41}, {0x03000000, 57},
        {0x01600000, 70}, {0x00f00000, 77}, {0x00c00000, 83}, {0x00b00000, 87},
        {0x00a00000, 95}, {0, 255}
    };
    constexpr uint32_t table2[] = {
        0x08, 0x09, 0x0A, 0x00, 0x07, 0x0B, 0x0C, 0x06, 0x29, 0x2A, 0xE0, 0x04, 0x05, 0x20, 0x28, 0x2B, 0x2C, 0x40, 0x4A, 0x03,
        0x0D, 0x25, 0x26, 0x27, 0x48, 0x49, 0x24, 0x47, 0x4B, 0x4C, 0x69, 0x6A, 0x23, 0x46, 0x60, 0x63, 0x67, 0x68, 0x88, 0x89,
        0xA0, 0xE8, 0x01, 0x02, 0x2D, 0x43, 0x44, 0x45, 0x65, 0x66, 0x80, 0x87, 0x8A, 0xA8, 0xA9, 0xC0, 0xC9, 0xE9, 0x0E, 0x4D,
        0x64, 0x6B, 0x6C, 0x84, 0x85, 0x8B, 0xA4, 0xA5, 0xAA, 0xC8, 0xE5, 0x83, 0x86, 0xA6, 0xA7, 0xC7, 0xCA, 0xE7, 0x22, 0x2E,
        0x8C, 0xC4, 0xE4, 0xE6, 0x4E, 0x6D, 0xC6, 0xEC, 0x0F, 0x10, 0x11, 0x8D, 0xAB, 0xAC, 0xCC, 0xEA, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x21, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x41, 0x42, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x61, 0x62, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x81, 0x82, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA1, 0xA2, 0xA3, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
        0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC1, 0xC2, 0xC3, 0xC5, 0xCB, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2,
        0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xEB, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    };
    constexpr uint32_t lengths[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28,
        32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 255
    };
    constexpr uint32_t length_bits[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    constexpr uint32_t distance_bits[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};
    constexpr uint32_t distances[] = {
        0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192,
        256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576,
        0x0100, 0x0302
    };

    [[noreturn]] void Corrupt()
    {
        throw std::runtime_error("Invalid Guild Wars compressed file");
    }

    class BitStream {
    public:
        explicit BitStream(std::span<const uint8_t> input) : input(input)
        {
            if (input.size() < 8 || input.size() % 4)
                Corrupt();
            first = Word(0);
            second = Word(4);
        }

        uint32_t Peek(uint32_t count) const { return first >> ((32 - count) & 31); }
        uint32_t Read(uint32_t count)
        {
            const auto value = Peek(count);
            Consume(count);
            return value;
        }

        void Consume(uint32_t count)
        {
            if (!count || count > 32)
                Corrupt();
            first = (first << (count & 31)) | (second >> ((32 - count) & 31));
            if (available < count) {
                if (next == input.size()) {
                    padding_bits += count;
                    if (padding_bits > 32)
                        Corrupt();
                    available = 0;
                    second = 0;
                }
                else {
                    second = Word(next);
                    next += 4;
                    const auto new_available = available + 32 - count;
                    first += second >> (new_available & 31);
                    second <<= ((count - available) & 31);
                    available = new_available;
                }
            }
            else {
                available -= count;
                second <<= (count & 31);
            }
        }

    private:
        uint32_t Word(size_t offset) const
        {
            if (offset + 4 > input.size())
                Corrupt();
            uint32_t word;
            memcpy(&word, input.data() + offset, 4);
            return word;
        }

        std::span<const uint8_t> input;
        size_t next = 8;
        uint32_t first = 0, second = 0, available = 32, padding_bits = 0;
    };

    class HuffmanTable {
    public:
        explicit HuffmanTable(BitStream& stream)
        {
            std::array<uint32_t, 32> roots;
            roots.fill(empty);
            const auto symbols = stream.Read(16);
            if (!symbols || symbols > 0x120)
                Corrupt();
            auto follows = std::vector<uint32_t>(symbols);
            auto symbol = static_cast<int32_t>(symbols) - 1;
            size_t total = 0;
            while (symbol >= 0) {
                const auto value = stream.Peek(32);
                size_t entry = 0;
                while (entry < std::size(table1) && table1[entry].first > value)
                    ++entry;
                if (entry == std::size(table1))
                    Corrupt();
                const auto bits = static_cast<uint32_t>(entry) + 3;
                const auto index = static_cast<int64_t>(table1[entry].second) - ((value - table1[entry].first) >> (32 - bits));
                if (index < 0 || index >= std::ssize(table2))
                    Corrupt();
                stream.Consume(bits);
                const auto packed = table2[index];
                const auto length = packed & 31;
                const auto count = (packed >> 5) + 1;
                if (count > static_cast<uint32_t>(symbol + 1))
                    Corrupt();
                if (length || symbols < 2) {
                    total += count;
                    for (uint32_t i = 0; i < count; ++i) {
                        follows[symbol] = roots[length];
                        roots[length] = static_cast<uint32_t>(symbol--);
                    }
                }
                else {
                    symbol -= static_cast<int32_t>(count);
                }
            }
            if (!total) {
                follows[symbols - 1] = roots[0];
                roots[0] = symbols - 1;
                total = 1;
            }
            if (roots[0] != empty) {
                for (auto& node : nodes)
                    node = {0, roots[0]};
                return;
            }
            int64_t next_bits = 1;
            size_t encoded = 0;
            for (uint32_t length = 1; length <= 8; ++length) {
                for (auto current = roots[length]; current != empty; current = follows[current]) {
                    if (current >= symbols || next_bits < 0 || next_bits >= (int64_t{1} << length))
                        Corrupt();
                    const auto start = static_cast<size_t>(next_bits << (8 - length));
                    const auto count = size_t{1} << (8 - length);
                    for (auto i = start; i < start + count; ++i)
                        nodes[i] = {length, current};
                    ++encoded;
                    --next_bits;
                }
                next_bits = next_bits * 2 + 1;
            }
            if (encoded == total)
                return;
            for (uint32_t length = 9; length < 32; ++length) {
                for (auto current = roots[length]; current != empty; current = follows[current]) {
                    if (current >= symbols || next_bits < 0 || next_bits >= (int64_t{1} << length))
                        Corrupt();
                    nodes[static_cast<size_t>(next_bits >> (length - 8))] = {empty, 0};
                    large_values.push_back(current);
                    --next_bits;
                }
                groups[length - 9] = {static_cast<uint32_t>(next_bits + 1) << (32 - length), static_cast<int64_t>(large_values.size()) - 1, length};
                next_bits = next_bits * 2 + 1;
            }
        }

        uint32_t Read(BitStream& stream) const
        {
            auto [length, value] = nodes[stream.Peek(8)];
            if (length == empty) {
                const auto bits = stream.Peek(32);
                bool found = false;
                for (const auto& group : groups) {
                    if (!group.length || group.first > bits)
                        continue;
                    const auto index = group.last - ((bits - group.first) >> (32 - group.length));
                    if (index < 0 || index >= std::ssize(large_values))
                        Corrupt();
                    value = large_values[index];
                    length = group.length;
                    found = true;
                    break;
                }
                if (!found)
                    Corrupt();
            }
            else if (length == empty - 1) {
                Corrupt();
            }
            if (length)
                stream.Consume(length);
            return value;
        }

    private:
        struct Group { uint32_t first = 0; int64_t last = 0; uint32_t length = 0; };
        std::array<std::pair<uint32_t, uint32_t>, 256> nodes = [] {
            std::array<std::pair<uint32_t, uint32_t>, 256> result;
            result.fill({empty - 1, 0});
            return result;
        }();
        std::array<Group, 24> groups{};
        std::vector<uint32_t> large_values;
    };
}

std::vector<uint8_t> GuildWarsFileServer::Decompress(std::span<const uint8_t> input, size_t expected_size, std::stop_token stop)
{
    if (!expected_size || expected_size > MaximumFileSize)
        Corrupt();
    auto stream = BitStream(input);
    stream.Consume(4);
    const auto first_four = stream.Read(4);
    auto output = std::vector<uint8_t>(expected_size);
    size_t written = 0;
    while (written < expected_size) {
        if (stop.stop_requested())
            throw std::runtime_error("File download cancelled");
        const auto literals = HuffmanTable(stream);
        const auto backtracks = HuffmanTable(stream);
        const auto block_size = (stream.Read(4) + 1) * 4096;
        for (uint32_t i = 0; i < block_size && written < expected_size; ++i) {
            const auto symbol = literals.Read(stream);
            if (symbol < 256) {
                output[written++] = static_cast<uint8_t>(symbol);
                continue;
            }
            const auto index = symbol - 256;
            if (index >= std::size(lengths))
                Corrupt();
            auto length = lengths[index];
            if (length_bits[index])
                length |= stream.Read(length_bits[index]);
            const auto count = first_four + length + 1;
            const auto distance_symbol = backtracks.Read(stream);
            if (distance_symbol >= std::size(distances))
                Corrupt();
            auto distance = distances[distance_symbol];
            if (distance_bits[distance_symbol])
                distance |= stream.Read(distance_bits[distance_symbol]);
            if (distance >= written || count > expected_size - written)
                Corrupt();
            const auto source = written - distance - 1;
            for (uint32_t j = 0; j < count; ++j)
                output[written++] = output[source + j];
        }
    }
    return output;
}
