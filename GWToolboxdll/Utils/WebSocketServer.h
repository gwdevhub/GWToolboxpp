#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
