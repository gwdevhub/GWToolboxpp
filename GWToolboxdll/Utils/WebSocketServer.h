#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// A minimal WebSocket server (RFC 6455) that broadcasts text messages to every connected client, such as a LiveSplit
// overlay. Clients' own messages are read and ignored. The sockets live on the server's thread; Broadcast queues
// from any thread.
class WebSocketServer {
public:
    ~WebSocketServer() { Stop(); }

    void Start(uint16_t port);
    void Stop();
    [[nodiscard]] bool Running() const { return thread.joinable(); }
    [[nodiscard]] bool Listening() const { return listening; }
    void Broadcast(std::string_view text);

private:
    void Run(uint16_t port);

    std::thread thread;
    std::atomic<bool> stopping = false;
    std::atomic<bool> listening = false;
    std::mutex queue_mutex;
    std::vector<std::string> queue; // framed messages waiting to be sent
};
