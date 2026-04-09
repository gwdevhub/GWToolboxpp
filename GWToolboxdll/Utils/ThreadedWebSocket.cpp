#include "stdafx.h"

#include "ThreadedWebSocket.h"
#include <Timer.h>

ThreadedWebSocket::ThreadedWebSocket()
{
    wsa_ok_ = (WSAStartup(MAKEWORD(2, 2), &wsa_data_) == 0);
}

ThreadedWebSocket::~ThreadedWebSocket()
{
    // Best-effort cleanup; caller should have called Disconnect() + polled Update() to idle.
    Disconnect(true);
    if (wsa_ok_) {
        WSACleanup();
        wsa_ok_ = false;
    }
}

// --- Configuration ---

void ThreadedWebSocket::SetUrl(const char* url)
{
    url_ = url;
}

void ThreadedWebSocket::SetUrl(std::string url)
{
    url_ = std::move(url);
}

void ThreadedWebSocket::SetHeadersFactory(HeadersFactory factory)
{
    headers_factory_ = std::move(factory);
}

void ThreadedWebSocket::SetHeaders(easywsclient::HeaderKeyValuePair headers)
{
    headers_factory_ = [h = std::move(headers)]() {
        return h;
    };
}

void ThreadedWebSocket::SetReconnectCost(uint32_t cost_ms, uint32_t max_ms)
{
    reconnect_cost_ms_ = cost_ms;
    reconnect_max_ms_ = max_ms;
}

void ThreadedWebSocket::SetPollIntervalMs(uint32_t ms)
{
    poll_interval_ms_ = ms;
}

void ThreadedWebSocket::SetOnMessage(MessageCallback cb)
{
    on_message_ = std::move(cb);
}
void ThreadedWebSocket::SetOnOpen(NotifyCallback cb)
{
    on_open_ = std::move(cb);
}
void ThreadedWebSocket::SetOnClose(NotifyCallback cb)
{
    on_close_ = std::move(cb);
}

// --- Runtime API ---

void ThreadedWebSocket::Connect()
{
    if (!wsa_ok_) return;
    if (pending_disconnect_.load()) return;
    connect_requested_ = true;
    EnsureThreadRunning();
}

bool ThreadedWebSocket::Send(std::string payload)
{
    if (!wsa_ok_) return false;
    if (pending_disconnect_.load()) return false;
    connect_requested_ = true;
    EnsureThreadRunning();
    std::lock_guard lk(queue_mutex_);
    send_queue_.push(std::move(payload));
    return true;
}

void ThreadedWebSocket::Disconnect(bool blocking)
{
    pending_disconnect_ = true;
    if (blocking) {
        for (clock_t i = TIMER_INIT(), timeout = i + 3000; i < timeout && !IsIdle(); i = TIMER_INIT()) {
            Update();
        }
        ASSERT(IsIdle() && "Failed to Disconnect websocket after 3000ms");
    }

}

bool ThreadedWebSocket::Update()
{
    // If disconnect was requested but the thread was never started, just clear the flag.
    if (pending_disconnect_.load() && !thread_) {
        pending_disconnect_ = false;
        connect_requested_ = false;
        return false;
    }

    if (!thread_) return false;
    if (!thread_done_.load()) return false;

    ASSERT(thread_->joinable());
    thread_->join();
    delete thread_;
    thread_ = nullptr;

    pending_disconnect_ = false;
    connect_requested_ = false;
    return true;
}

bool ThreadedWebSocket::IsIdle() const
{
    return !thread_ && !pending_disconnect_.load();
}

bool ThreadedWebSocket::IsReady() const
{
    return ws_ && ws_->getReadyState() == easywsclient::WebSocket::OPEN;
}

bool ThreadedWebSocket::IsWsaReady() const
{
    return wsa_ok_;
}

// --- Private ---

void ThreadedWebSocket::EnsureThreadRunning()
{
    if (thread_) return;
    thread_done_ = false;
    thread_ = new std::thread([this] {
        ThreadLoop();
    });
}

void ThreadedWebSocket::ThreadLoop()
{
    while (!pending_disconnect_.load()) {
        TickSocket();
        DrainSendQueue();
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
    }
    CloseSocket();
    thread_done_ = true;
}

void ThreadedWebSocket::TickSocket()
{
    if (!ws_) {
        if (!connect_requested_.load()) return;

        if (!rate_limiter_.AddTime(reconnect_cost_ms_, reconnect_max_ms_)) return;

        auto headers = headers_factory_ ? headers_factory_() : easywsclient::HeaderKeyValuePair{};
        ws_ = easywsclient::WebSocket::from_url(url_, headers);
        if (ws_ && on_open_) on_open_();
        return;
    }

    switch (ws_->getReadyState()) {
        case easywsclient::WebSocket::CONNECTING:
        case easywsclient::WebSocket::CLOSING:
        case easywsclient::WebSocket::OPEN:
            ws_->poll();
            if (on_message_) {
                ws_->dispatch(on_message_);
            }
            else {
                ws_->dispatch([](const std::string&) {
                });
            }
            break;

        case easywsclient::WebSocket::CLOSED:
            delete ws_;
            ws_ = nullptr;
            if (on_close_) on_close_();
            break;
    }
}

void ThreadedWebSocket::DrainSendQueue()
{
    while (ws_ && ws_->getReadyState() == easywsclient::WebSocket::OPEN) {
        std::string msg;
        {
            std::lock_guard lk(queue_mutex_);
            if (send_queue_.empty()) break;
            msg = std::move(send_queue_.front());
            send_queue_.pop();
        }
        ws_->send(msg);
    }
}

void ThreadedWebSocket::CloseSocket()
{
    if (!ws_) return;
    ws_->close();
    ws_->poll();
    delete ws_;
    ws_ = nullptr;
    {
        std::lock_guard lk(queue_mutex_);
        while (!send_queue_.empty())
            send_queue_.pop();
    }
    if (on_close_) on_close_();
}
