#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace GuildWarsFileServer {
    constexpr uint32_t MaximumFileSize = 0x3f00000;

    std::vector<uint8_t> Decompress(std::span<const uint8_t> input, std::size_t expected_size, std::stop_token stop);
    std::vector<uint8_t> Download(uint32_t file_id, std::stop_token stop);
    std::unordered_map<uint32_t, uint32_t> ParseManifest(std::span<const uint8_t> bytes);
    bool ReadStream(std::span<const uint8_t> bytes, uint32_t stream_id, std::vector<uint8_t>& output);
}
