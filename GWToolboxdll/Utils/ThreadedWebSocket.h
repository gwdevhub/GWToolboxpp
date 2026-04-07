#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <Utils/RateLimiter.h>
#include <easywsclient/easywsclient.hpp>
#include <winsock2.h>

// ThreadedWebSocket wraps easywsclient with a self-managed background thread,
// send queue, rate-limited reconnect, and lifecycle callbacks.
//
// Intended usage pattern:
//
//   ThreadedWebSocket ws;
//   ws.SetUrl("wss://example.com");
//   ws.SetHeadersFactory([] { return easywsclient::HeaderKeyValuePair{{"X-Key", "value"}}; });
//   ws.SetOnMessage([](const std::string& msg) { /* handle */ });
//   ws.SetOnOpen([]  { /* connected */ });
//   ws.SetOnClose([] { /* disconnected */ });
//
//   ws.Connect();              // Start connecting immediately (no message needed)
//   ws.Send(payload);          // Enqueue a message; also starts connecting if not already
//   ws.Disconnect();           // Requests graceful shutdown (non-blocking)
//   ws.Update();               // Call from your main/game thread each tick to join finished threads
//   bool done = ws.IsIdle();   // True once thread has fully stopped after Disconnect()
//
// Thread safety:
//   Connect()    - safe to call from any thread
//   Send()       - safe to call from any thread
//   Disconnect() - safe to call from any thread
//   Update()     - must be called from a single owner thread (e.g. game/main thread)
//   All callbacks are invoked on the background worker thread.

class ThreadedWebSocket {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using NotifyCallback = std::function<void()>;
    using HeadersFactory = std::function<easywsclient::HeaderKeyValuePair()>;

    ThreadedWebSocket();
    ~ThreadedWebSocket();

    // Non-copyable, non-movable (owns a thread and raw pointers)
    ThreadedWebSocket(const ThreadedWebSocket&) = delete;
    ThreadedWebSocket& operator=(const ThreadedWebSocket&) = delete;
    ThreadedWebSocket(ThreadedWebSocket&&) = delete;
    ThreadedWebSocket& operator=(ThreadedWebSocket&&) = delete;

    // --- Configuration (call before first Send) ---

    void SetUrl(const char* url);
    void SetUrl(std::string url);

    // Called each time a new connection attempt is made; return the headers to use.
    // Evaluated on the worker thread, so ensure thread-safety of any captured state.
    void SetHeadersFactory(HeadersFactory factory);

    // Convenience: fixed headers that never change
    void SetHeaders(easywsclient::HeaderKeyValuePair headers);

    // Rate-limiting between reconnect attempts (mirrors RateLimiter::AddTime params)
    void SetReconnectCost(uint32_t cost_ms, uint32_t max_ms);

    // Worker thread poll interval
    void SetPollIntervalMs(uint32_t ms);

    void SetOnMessage(MessageCallback cb);
    void SetOnOpen(NotifyCallback cb);
    void SetOnClose(NotifyCallback cb);

    // --- Runtime API ---

    // Start the background thread and connect immediately, without waiting for a Send().
    // Safe to call from any thread. No-op if already connecting or connected.
    void Connect();

    // Enqueue a message for sending. Starts the background thread if not already running.
    // Returns false if WSA is not ready or a disconnect is pending.
    bool Send(std::string payload);

    // Request a graceful shutdown. Non-blocking: the thread will finish its current
    // poll loop iteration, then close the socket and invoke on_close_.
    void Disconnect();

    // Call once per tick from your owner/game thread.
    // Joins and cleans up the worker thread after it has finished.
    // Returns true if the thread was just joined (useful for resetting state etc.)
    bool Update();

    // True when no thread is running and no disconnect is pending.
    bool IsIdle() const;

    // True when the socket is open and ready.
    bool IsReady() const;

    // False if WSAStartup failed; Send() will always return false in this state.
    bool IsWsaReady() const;

private:
    void EnsureThreadRunning();
    void ThreadLoop();
    void TickSocket();
    void DrainSendQueue();
    void CloseSocket();

    // WSA
    WSAData wsa_data_ = {};
    bool wsa_ok_ = false;

    // Config
    std::string url_;
    HeadersFactory headers_factory_;
    uint32_t reconnect_cost_ms_ = 30'000;
    uint32_t reconnect_max_ms_ = 60'000;
    uint32_t poll_interval_ms_ = 50;
    MessageCallback on_message_;
    NotifyCallback on_open_;
    NotifyCallback on_close_;

    // Runtime state (worker thread)
    easywsclient::WebSocket* ws_ = nullptr;
    RateLimiter rate_limiter_;

    // Thread management
    std::thread* thread_ = nullptr;
    std::atomic<bool> thread_done_ = true;
    std::atomic<bool> pending_disconnect_ = false;
    std::atomic<bool> connect_requested_ = false;

    // Send queue
    std::queue<std::string> send_queue_;
    std::recursive_mutex queue_mutex_;
};
