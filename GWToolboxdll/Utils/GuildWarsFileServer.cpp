#include "stdafx.h"

#include "GuildWarsFileServer.h"

#include <atomic>
#include <stdexcept>

// Protocol adapted from gwdevhub/gwlauncher and gwdevhub/gwtts.
namespace {
    using Clock = std::chrono::steady_clock;

    template <typename T>
    T Read(std::span<const uint8_t> bytes, size_t offset)
    {
        if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
            throw std::runtime_error("Truncated file-server packet");
        T result;
        memcpy(&result, bytes.data() + offset, sizeof(result));
        return result;
    }

    template <typename T>
    void Append(std::vector<uint8_t>& bytes, T value)
    {
        const auto* begin = reinterpret_cast<const uint8_t*>(&value);
        bytes.insert(bytes.end(), begin, begin + sizeof(value));
    }

    class Connection {
    public:
        explicit Connection(std::stop_token stop) : stop(stop) {}
        ~Connection()
        {
            if (socket != INVALID_SOCKET)
                closesocket(socket);
        }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        void Connect(const std::string& host)
        {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            addrinfo* addresses = nullptr;
            if (getaddrinfo(host.c_str(), "6112", &hints, &addresses))
                throw std::runtime_error("Could not resolve ArenaNet file server");
            const auto owner = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>(addresses, freeaddrinfo);
            for (auto* address = addresses; address; address = address->ai_next) {
                if (stop.stop_requested())
                    throw std::runtime_error("File download cancelled");
                socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (socket == INVALID_SOCKET)
                    continue;
                u_long nonblocking = 1;
                if (ioctlsocket(socket, FIONBIO, &nonblocking)) {
                    closesocket(socket);
                    socket = INVALID_SOCKET;
                    continue;
                }
                const auto connected = ::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen));
                if (connected == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
                    closesocket(socket);
                    socket = INVALID_SOCKET;
                    continue;
                }
                Wait(true, Clock::now() + std::chrono::seconds(5));
                int error = 0;
                int size = sizeof(error);
                if (!getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) && !error)
                    return;
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
            throw std::runtime_error("Could not connect to ArenaNet file server");
        }

        void Send(std::span<const uint8_t> bytes)
        {
            const auto deadline = Clock::now() + std::chrono::seconds(10);
            while (!bytes.empty()) {
                Wait(true, deadline);
                const auto sent = send(socket, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
                if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                    continue;
                if (sent <= 0)
                    throw std::runtime_error("File-server send failed");
                bytes = bytes.subspan(static_cast<size_t>(sent));
            }
        }

        std::vector<uint8_t> Receive(size_t count)
        {
            auto bytes = std::vector<uint8_t>(count);
            size_t received = 0;
            const auto deadline = Clock::now() + std::chrono::seconds(10);
            while (received < count) {
                Wait(false, deadline);
                const auto size = recv(socket, reinterpret_cast<char*>(bytes.data() + received), static_cast<int>(count - received), 0);
                if (size == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                    continue;
                if (size <= 0)
                    throw std::runtime_error("File server closed the connection");
                received += static_cast<size_t>(size);
            }
            return bytes;
        }

        std::vector<uint8_t> ReceivePacket()
        {
            auto header = Receive(4);
            const auto length = Read<uint16_t>(header, 2);
            if (length < 4 || length > 0x8004)
                throw std::runtime_error("Invalid file-server packet length");
            const auto payload = Receive(length - 4);
            header.insert(header.end(), payload.begin(), payload.end());
            return header;
        }

    private:
        void Wait(bool write, Clock::time_point deadline)
        {
            while (!stop.stop_requested()) {
                if (Clock::now() >= deadline)
                    throw std::runtime_error("ArenaNet file-server request timed out");
                fd_set sockets;
                FD_ZERO(&sockets);
                FD_SET(socket, &sockets);
                auto errors = sockets;
                timeval timeout{0, 100000};
                const auto ready = select(0, write ? nullptr : &sockets, write ? &sockets : nullptr, &errors, &timeout);
                if (ready == SOCKET_ERROR)
                    throw std::runtime_error("File-server socket failed");
                if (ready)
                    return;
            }
            throw std::runtime_error("File download cancelled");
        }

        SOCKET socket = INVALID_SOCKET;
        std::stop_token stop;
    };

    uint32_t FileCrc(std::span<const uint8_t> bytes, uint32_t file_id)
    {
        static constexpr auto table = [] {
            std::array<uint32_t, 256> result{};
            for (uint32_t i = 0; i < result.size(); ++i) {
                auto crc = i;
                for (auto bit = 0; bit < 8; ++bit)
                    crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
                result[i] = crc;
            }
            return result;
        }();
        auto crc = uint32_t{0xffffffff};
        for (const auto byte : bytes)
            crc = table[(crc ^ byte) & 255] ^ (crc >> 8);
        for (auto i = 0; i < 4; ++i) {
            crc = table[(crc ^ file_id) & 255] ^ (crc >> 8);
            file_id >>= 8;
        }
        return crc ^ 0xffffffff;
    }
}

std::vector<uint8_t> GuildWarsFileServer::Download(uint32_t file_id, std::stop_token stop)
{
    static std::atomic<uint32_t> next_server = 0;
    const auto start = next_server.fetch_add(1);
    auto last_error = std::string("Could not reach ArenaNet file servers");
    for (uint32_t attempt = 0; attempt < 12 && !stop.stop_requested(); ++attempt) {
        try {
            auto connection = Connection(stop);
            connection.Connect(std::format("file{}.arenanetworks.com", 1 + (start + attempt) % 12));
            auto handshake = std::vector<uint8_t>{1};
            Append<uint32_t>(handshake, 0);
            Append<uint16_t>(handshake, 0xf1);
            Append<uint16_t>(handshake, 0x10);
            Append<uint32_t>(handshake, 1);
            Append<uint32_t>(handshake, 0);
            Append<uint32_t>(handshake, 0);
            connection.Send(handshake);
            const auto greeting = connection.ReceivePacket();
            if (Read<uint16_t>(greeting, 0) != 0x2f1 || greeting.size() < 32 || greeting.size() % 4)
                throw std::runtime_error("Invalid file-server greeting");

            auto request = std::vector<uint8_t>{};
            Append<uint16_t>(request, 0x3f2);
            Append<uint16_t>(request, 12);
            Append<uint32_t>(request, file_id);
            Append<uint32_t>(request, 0);
            connection.Send(request);
            auto metadata = connection.ReceivePacket();
            for (auto updates = 0; metadata[1] == 2 && updates < 8; ++updates)
                metadata = connection.ReceivePacket();
            if (Read<uint16_t>(metadata, 0) == 0x4f2)
                throw std::runtime_error("File not found on ArenaNet file server");
            if (Read<uint16_t>(metadata, 0) != 0x5f2 || metadata.size() != 20 || Read<uint32_t>(metadata, 4) != file_id)
                throw std::runtime_error("Unexpected file-server metadata");
            const auto decompressed_size = Read<uint32_t>(metadata, 8);
            const auto compressed_size = Read<uint32_t>(metadata, 12);
            const auto crc = Read<uint32_t>(metadata, 16);
            if (!decompressed_size || decompressed_size > MaximumFileSize || compressed_size < 8 || compressed_size > MaximumFileSize || compressed_size % 4)
                throw std::runtime_error("Invalid file-server asset size");
            auto compressed = std::vector<uint8_t>{};
            compressed.reserve(compressed_size);
            const auto deadline = Clock::now() + std::chrono::seconds(60);
            while (compressed.size() < compressed_size) {
                if (Clock::now() >= deadline)
                    throw std::runtime_error("File download timed out");
                const auto packet = connection.ReceivePacket();
                const auto opcode = Read<uint16_t>(packet, 0);
                if ((opcode != 0x6f2 && opcode != 0x6f3) || packet.size() <= 4 || packet.size() - 4 > compressed_size - compressed.size())
                    throw std::runtime_error("Invalid file-server data chunk");
                compressed.insert(compressed.end(), packet.begin() + 4, packet.end());
                if (compressed.size() < compressed_size) {
                    auto credit = std::vector<uint8_t>{};
                    Append<uint16_t>(credit, 0x7f3);
                    Append<uint16_t>(credit, 8);
                    Append<uint32_t>(credit, 0x4000);
                    connection.Send(credit);
                }
            }
            auto output = Decompress(compressed, decompressed_size, stop);
            if (FileCrc(output, file_id) != crc)
                throw std::runtime_error("File-server asset checksum mismatch");
            return output;
        }
        catch (const std::exception& error) {
            last_error = error.what();
        }
    }
    throw std::runtime_error(stop.stop_requested() ? "File download cancelled" : last_error);
}

std::unordered_map<uint32_t, uint32_t> GuildWarsFileServer::ParseManifest(std::span<const uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() % 4 || Read<uint32_t>(bytes, 0))
        throw std::runtime_error("Invalid Guild Wars asset manifest");
    auto revisions = std::unordered_map<uint32_t, uint32_t>{};
    size_t offset = 4;
    while (offset < bytes.size()) {
        const auto current = Read<uint32_t>(bytes, offset);
        offset += 4;
        if (!current)
            continue;
        const auto base = Read<uint32_t>(bytes, offset);
        offset += 4;
        if (!base || current >= 0x80000000 || base >= 0x80000000)
            throw std::runtime_error("Invalid asset manifest file ID");
        const auto [entry, inserted] = revisions.emplace(base, current);
        if (!inserted && entry->second != current)
            throw std::runtime_error("Conflicting asset manifest revisions");
        while (Read<uint32_t>(bytes, offset))
            offset += 4;
        offset += 4;
    }
    if (revisions.empty())
        throw std::runtime_error("Empty Guild Wars asset manifest");
    return revisions;
}

bool GuildWarsFileServer::ReadStream(std::span<const uint8_t> bytes, uint32_t stream_id, std::vector<uint8_t>& output)
{
    output.clear();
    if (!stream_id) {
        output.assign(bytes.begin(), bytes.end());
        return !output.empty();
    }
    if ((stream_id != 1 && stream_id != 0xc) || bytes.size() < 5 || memcmp(bytes.data(), "ffna", 4))
        return false;
    const auto wanted = stream_id == 1 ? 0xfa3u : 0xfaau;
    size_t offset = 5;
    while (bytes.size() - offset >= 8) {
        const auto tag = Read<uint32_t>(bytes, offset);
        const auto size = Read<uint32_t>(bytes, offset + 4);
        offset += 8;
        if (size > bytes.size() - offset)
            return false;
        if (tag == wanted) {
            output.assign(bytes.begin() + offset, bytes.begin() + offset + size);
            return !output.empty();
        }
        offset += size;
    }
    return false;
}
