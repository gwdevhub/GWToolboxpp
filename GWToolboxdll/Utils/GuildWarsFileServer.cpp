#include "stdafx.h"

#include "GuildWarsFileServer.h"
#include "ArenaNetFileParser.h"
#include "GwDat/xentax.h"
#include <Modules/CodeOptimiserModule.h>

#include <atomic>

// Protocol adapted from gwdevhub/gwlauncher and gwdevhub/gwtts.
namespace {
    using Status = GuildWarsFileServer::Status;
    using Clock = std::chrono::steady_clock;
    struct Packet { uint16_t opcode; std::vector<uint8_t> payload; };
    struct FileInfo { uint32_t file_id, decompressed_size, compressed_size, crc; };

    class Connection {
    public:
        explicit Connection(std::stop_token stop) : stop(stop) {}
        ~Connection() { if (socket != INVALID_SOCKET) closesocket(socket); }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        Status status = Status::Success;

        bool Connect(const std::string& host)
        {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* addresses = nullptr;
            if (getaddrinfo(host.c_str(), "6112", &hints, &addresses))
                return Fail(Status::NetworkError);
            const auto owner = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>(addresses, freeaddrinfo);
            for (auto* address = addresses; address && !stop.stop_requested(); address = address->ai_next) {
                socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (socket == INVALID_SOCKET)
                    continue;
                u_long nonblocking = 1;
                const auto configured = !ioctlsocket(socket, FIONBIO, &nonblocking);
                const auto connected = configured && (!::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) || WSAGetLastError() == WSAEWOULDBLOCK);
                int error = 0, size = sizeof(error);
                if (connected && Wait(true, Clock::now() + std::chrono::seconds(5)) &&
                    !getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) && !error)
                    return true;
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
            return Fail(stop.stop_requested() ? Status::Cancelled : status == Status::Success ? Status::NetworkError : status);
        }

        Status Fetch(uint32_t file_id, std::vector<uint8_t>& output)
        {
            auto hello = std::array<uint8_t, 21>{1, 0, 0, 0, 0, 0xf1, 0, 0x10, 0, 1};
            Packet packet;
            if (!Send(hello) || !Receive(packet))
                return status;
            if (packet.opcode != 0x2f1 || packet.payload.size() < 28 || packet.payload.size() % 4)
                return Status::InvalidResponse;
            auto request = std::array<uint32_t, 3>{0x000c03f2, file_id, 0};
            if (!Send(request) || !Receive(packet))
                return status;
            for (auto updates = 0; (packet.opcode >> 8) == 2 && updates < 8; ++updates) {
                if (!Receive(packet))
                    return status;
            }
            if (packet.opcode == 0x4f2)
                return Status::FileNotFound;
            FileInfo info;
            if (packet.opcode != 0x5f2 || packet.payload.size() != sizeof(info))
                return Status::InvalidResponse;
            memcpy(&info, packet.payload.data(), sizeof(info));
            if (info.file_id != file_id || !info.decompressed_size || info.decompressed_size > GuildWarsFileServer::MaximumFileSize ||
                info.compressed_size < 8 || info.compressed_size > GuildWarsFileServer::MaximumFileSize || info.compressed_size % 4)
                return Status::InvalidResponse;
            std::vector<uint8_t> compressed;
            compressed.reserve(info.compressed_size);
            auto credit = std::array<uint32_t, 2>{0x000807f3, 0x4000};
            const auto deadline = Clock::now() + std::chrono::seconds(60);
            while (compressed.size() < info.compressed_size) {
                if (Clock::now() >= deadline)
                    return Status::Timeout;
                if (!Receive(packet))
                    return status;
                if ((packet.opcode != 0x6f2 && packet.opcode != 0x6f3) || packet.payload.empty() || packet.payload.size() > info.compressed_size - compressed.size())
                    return Status::InvalidResponse;
                compressed.insert(compressed.end(), packet.payload.begin(), packet.payload.end());
                if (compressed.size() < info.compressed_size && !Send(credit))
                    return status;
            }
            output.resize(info.decompressed_size);
            if (!UnpackGWFile(compressed, output, stop))
                return stop.stop_requested() ? Status::Cancelled : Status::InvalidCompressedData;
            const auto crc = CodeOptimiserModule::Crc32(output.data(), output.size());
            return CodeOptimiserModule::Crc32(&file_id, sizeof(file_id), crc) == info.crc ? Status::Success : Status::ChecksumMismatch;
        }

    private:
        bool Fail(Status error) { status = error; return false; }

        bool Wait(bool write, Clock::time_point deadline)
        {
            while (!stop.stop_requested()) {
                if (Clock::now() >= deadline)
                    return Fail(Status::Timeout);
                fd_set sockets;
                FD_ZERO(&sockets);
                FD_SET(socket, &sockets);
                auto errors = sockets;
                timeval timeout{0, 100000};
                const auto ready = select(0, write ? nullptr : &sockets, write ? &sockets : nullptr, &errors, &timeout);
                if (ready == SOCKET_ERROR)
                    return Fail(Status::NetworkError);
                if (ready)
                    return true;
            }
            return Fail(Status::Cancelled);
        }

        bool Transfer(std::span<uint8_t> bytes, bool write)
        {
            const auto deadline = Clock::now() + std::chrono::seconds(10);
            while (!bytes.empty()) {
                if (!Wait(write, deadline))
                    return false;
                auto* data = reinterpret_cast<char*>(bytes.data());
                const auto count = static_cast<int>(bytes.size());
                const auto size = write ? send(socket, data, count, 0) : recv(socket, data, count, 0);
                if (size == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                    continue;
                if (size <= 0)
                    return Fail(Status::NetworkError);
                bytes = bytes.subspan(static_cast<size_t>(size));
            }
            return true;
        }

        template <typename T>
        bool Send(T& packet) { return Transfer({reinterpret_cast<uint8_t*>(&packet), sizeof(packet)}, true); }

        bool Receive(Packet& packet)
        {
            struct Header { uint16_t opcode, size; } header{};
            if (!Transfer({reinterpret_cast<uint8_t*>(&header), sizeof(header)}, false))
                return false;
            if (header.size < 4 || header.size > 0x8004)
                return Fail(Status::InvalidResponse);
            packet.opcode = header.opcode;
            packet.payload.resize(header.size - 4);
            return Transfer(packet.payload, false);
        }

        SOCKET socket = INVALID_SOCKET;
        std::stop_token stop;
    };
}

const char* GuildWarsFileServer::StatusMessage(Status status)
{
    static constexpr const char* messages[] = {
        "Success", "Download cancelled", "File-server connection failed", "File-server request timed out",
        "Invalid file-server response", "File not found", "Invalid compressed asset", "Asset checksum mismatch",
        "Invalid asset manifest", "Client asset manifest unavailable"
    };
    const auto index = static_cast<size_t>(status);
    return index < std::size(messages) ? messages[index] : "Unknown download status";
}

GuildWarsFileServer::Status GuildWarsFileServer::Download(uint32_t file_id, std::vector<uint8_t>& output, std::stop_token stop)
{
    static std::atomic<uint32_t> next_server = 0;
    const auto start = next_server.fetch_add(1);
    auto status = Status::NetworkError;
    output.clear();
    for (uint32_t attempt = 0; attempt < 12 && !stop.stop_requested(); ++attempt) {
        auto connection = Connection(stop);
        status = connection.Connect(std::format("file{}.arenanetworks.com", 1 + (start + attempt) % 12))
            ? connection.Fetch(file_id, output) : connection.status;
        if (status == Status::Success)
            return status;
        output.clear();
        if (status == Status::FileNotFound || status == Status::Cancelled)
            break;
    }
    return stop.stop_requested() ? Status::Cancelled : status;
}

GuildWarsFileServer::Status GuildWarsFileServer::ParseManifest(std::span<const uint8_t> bytes, std::unordered_map<uint32_t, uint32_t>& revisions)
{
    revisions.clear();
    if (bytes.size() < 4 || bytes.size() % 4)
        return Status::InvalidManifest;
    std::unordered_map<uint32_t, uint32_t> parsed;
    uint32_t current = 0, base = 0;
    for (size_t offset = 0; offset < bytes.size(); offset += 4) {
        uint32_t word;
        memcpy(&word, bytes.data() + offset, sizeof(word));
        if (!offset && word)
            return Status::InvalidManifest;
        if (word) {
            if (!current) current = word;
            else if (!base) base = word;
        }
        else if (current) {
            if (!base || current >= 0x80000000 || base >= 0x80000000)
                return Status::InvalidManifest;
            const auto [entry, inserted] = parsed.emplace(base, current);
            if (!inserted && entry->second != current)
                return Status::InvalidManifest;
            current = base = 0;
        }
    }
    if (current || parsed.empty())
        return Status::InvalidManifest;
    revisions = std::move(parsed);
    return Status::Success;
}

bool GuildWarsFileServer::ReadStream(std::span<const uint8_t> bytes, uint32_t stream_id, std::vector<uint8_t>& output)
{
    output.clear();
    if (!stream_id)
        output.assign(bytes.begin(), bytes.end());
    else if (stream_id == 1 || stream_id == 0xc) {
        using namespace ArenaNetFileParser;
        const auto* chunk = ArenaNetFile::FindChunk(bytes, stream_id == 1 ? ChunkType::FA3_InlineTextureDXT3 : ChunkType::FAA_InlineTextureDXTA);
        if (chunk) {
            const auto* payload = reinterpret_cast<const uint8_t*>(chunk + 1);
            output.assign(payload, payload + chunk->chunk_size);
        }
    }
    return !output.empty();
}
