#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace GuildWarsFileServer {
    constexpr uint32_t MaximumFileSize = 0x3f00000;

    enum class Status {
        Success, Cancelled, NetworkError, Timeout, InvalidResponse, FileNotFound,
        InvalidCompressedData, ChecksumMismatch, InvalidManifest, ManifestUnavailable
    };

    const char* StatusMessage(Status status);
    Status Download(uint32_t file_id, std::vector<uint8_t>& output, std::stop_token stop);
    Status ParseManifest(std::span<const uint8_t> bytes, std::unordered_map<uint32_t, uint32_t>& revisions);
    bool ReadStream(std::span<const uint8_t> bytes, uint32_t stream_id, std::vector<uint8_t>& output);
}
