#include "stdafx.h"

#include <WinSock2.h>
#include <bcrypt.h>

#include <Logger.h>
#include <Utils/TextUtils.h>
#include <Utils/WebSocketServer.h>

namespace {
    struct Client {
        SOCKET socket = INVALID_SOCKET;
        std::string received; // the handshake request, then frames, until they are complete
        std::string unsent;   // frames the socket hasn't accepted yet
        bool upgraded = false;
    };

    // A server frame: FIN, the opcode, then the payload length in 7, 16 or 64 bits. Servers never mask.
    std::string Frame(const uint8_t opcode, const std::string_view payload)
    {
        std::string frame(1, static_cast<char>(0x80 | opcode));
        const auto size = payload.size();
        if (size < 126) {
            frame += static_cast<char>(size);
        }
        else if (size < 65536) {
            frame += static_cast<char>(126);
            frame += static_cast<char>(size >> 8);
            frame += static_cast<char>(size);
        }
        else {
            frame += static_cast<char>(127);
            for (int shift = 56; shift >= 0; shift -= 8) {
                frame += static_cast<char>(static_cast<uint64_t>(size) >> shift);
            }
        }
        return frame.append(payload);
    }

    // Sec-WebSocket-Accept: base64(SHA-1(client key + the RFC 6455 GUID)).
    std::string AcceptKey(const std::string_view client_key)
    {
        const auto input = std::string(client_key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        unsigned char hash[20];
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0) return {};
        const auto status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), hash, sizeof(hash));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status != 0) return {};
        return TextUtils::Base64Encode(std::string_view(reinterpret_cast<const char*>(hash), sizeof(hash)));
    }

    // Answers the HTTP upgrade request once all of it has arrived. False if the client should be dropped.
    bool Handshake(Client& client)
    {
        const auto end = client.received.find("\r\n\r\n");
        if (end == std::string::npos) return client.received.size() < 16 * 1024;
        const std::string request = client.received.substr(0, end);
        client.received.erase(0, end + 4);
        // Header names are case-insensitive, the key's value isn't: find it in a lowercased copy, read it from the request.
        std::string lowered = request;
        std::ranges::transform(lowered, lowered.begin(), [](const unsigned char c) { return static_cast<char>(tolower(c)); });
        constexpr std::string_view key_header = "\r\nsec-websocket-key:";
        const auto key_at = lowered.find(key_header);
        if (key_at == std::string::npos) return false;
        const auto key_start = key_at + key_header.size();
        auto key = std::string_view(request).substr(key_start, request.find("\r\n", key_start) - key_start);
        while (!key.empty() && key.front() == ' ') key.remove_prefix(1);
        while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
        const auto response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + AcceptKey(key) + "\r\n\r\n";
        client.upgraded = send(client.socket, response.data(), static_cast<int>(response.size()), 0) == static_cast<int>(response.size());
        return client.upgraded;
    }

    // Consumes the complete frames a client has sent. Only a close frame matters: it is answered and the client dropped.
    bool ReadFrames(Client& client)
    {
        auto& data = client.received;
        while (data.size() >= 2) {
            const auto opcode = static_cast<uint8_t>(data[0]) & 0x0F;
            uint64_t length = static_cast<uint8_t>(data[1]) & 0x7F;
            size_t header = 2;
            if (length == 126 || length == 127) {
                const size_t bytes = length == 126 ? 2 : 8;
                if (data.size() < header + bytes) return true;
                length = 0;
                for (size_t i = 0; i < bytes; i++) length = length << 8 | static_cast<uint8_t>(data[header + i]);
                header += bytes;
            }
            if (static_cast<uint8_t>(data[1]) & 0x80) header += 4; // the client's masking key
            if (length > 16 * 1024) return false;
            if (data.size() < header + length) return true;
            data.erase(0, header + static_cast<size_t>(length));
            if (opcode == 0x8) {
                const auto close = Frame(0x8, {});
                send(client.socket, close.data(), static_cast<int>(close.size()), 0);
                return false;
            }
        }
        return true;
    }
}

void WebSocketServer::Start(const uint16_t port)
{
    if (Running()) return;
    stopping = false;
    thread = std::thread([this, port] { Run(port); });
}

void WebSocketServer::Stop()
{
    if (!Running()) return;
    stopping = true;
    thread.join();
    std::scoped_lock lock(queue_mutex);
    queue.clear();
}

void WebSocketServer::Broadcast(const std::string_view text)
{
    if (!Running()) return;
    std::scoped_lock lock(queue_mutex);
    queue.push_back(Frame(0x1, text));
}

void WebSocketServer::Run(const uint16_t port)
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) return;
    // Dual-stack, so both ws://localhost (often ::1) and ws://127.0.0.1 reach it.
    const SOCKET listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    DWORD v6_only = 0;
    setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&v6_only), sizeof(v6_only));
    sockaddr_in6 address{.sin6_family = AF_INET6, .sin6_port = htons(port)};
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, SOMAXCONN) != 0) {
        Log::Log("WebSocketServer: can't listen on port %u (%d)", port, WSAGetLastError());
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        return;
    }
    Log::Log("WebSocketServer listening on port %u", port);
    listening = true;

    std::vector<Client> clients;
    const auto drop = [&](Client& client) {
        closesocket(client.socket);
        client.socket = INVALID_SOCKET;
    };
    const auto ping = Frame(0x9, {});
    auto last_ping = std::chrono::steady_clock::now();
    while (!stopping) {
        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_ZERO(&writable);
        FD_SET(listener, &readable);
        for (const auto& client : clients) {
            FD_SET(client.socket, &readable);
            if (!client.unsent.empty()) FD_SET(client.socket, &writable);
        }
        timeval timeout{.tv_sec = 0, .tv_usec = 50'000}; // also how often queued messages go out
        if (select(0, &readable, &writable, nullptr, &timeout) == SOCKET_ERROR) break;

        if (FD_ISSET(listener, &readable) && clients.size() < FD_SETSIZE - 1) {
            const SOCKET accepted = accept(listener, nullptr, nullptr);
            if (accepted != INVALID_SOCKET) {
                // Non-blocking, so a client that reads slowly or not at all can't stall the others.
                u_long non_blocking = 1;
                ioctlsocket(accepted, FIONBIO, &non_blocking);
                clients.push_back({.socket = accepted});
            }
        }
        for (auto& client : clients) {
            if (!FD_ISSET(client.socket, &readable)) continue;
            char buffer[4096];
            const int received = recv(client.socket, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                drop(client);
                continue;
            }
            client.received.append(buffer, received);
            if (!(client.upgraded ? ReadFrames(client) : Handshake(client) && ReadFrames(client))) drop(client);
        }

        std::vector<std::string> outgoing;
        {
            std::scoped_lock lock(queue_mutex);
            outgoing.swap(queue);
        }
        // Keeps idle connections open through proxies and lets browsers notice a closed toolbox.
        if (std::chrono::steady_clock::now() - last_ping > std::chrono::seconds(5)) {
            outgoing.push_back(ping);
            last_ping = std::chrono::steady_clock::now();
        }
        for (auto& client : clients) {
            if (client.socket == INVALID_SOCKET || !client.upgraded) continue;
            for (const auto& frame : outgoing) client.unsent += frame;
            // A client more than 1 MB behind is dropped, as uWebSockets' maxBackpressure did.
            if (client.unsent.size() > 1024 * 1024) {
                drop(client);
                continue;
            }
            if (client.unsent.empty()) continue;
            const int sent = send(client.socket, client.unsent.data(), static_cast<int>(client.unsent.size()), 0);
            if (sent > 0) {
                client.unsent.erase(0, sent);
            }
            else if (WSAGetLastError() != WSAEWOULDBLOCK) {
                drop(client);
            }
        }
        std::erase_if(clients, [](const Client& client) { return client.socket == INVALID_SOCKET; });
    }

    listening = false;
    for (auto& client : clients) drop(client);
    closesocket(listener);
    WSACleanup();
}
