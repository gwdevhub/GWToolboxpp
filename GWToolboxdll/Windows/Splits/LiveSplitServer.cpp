#include "stdafx.h"

#include "LiveSplitServer.h"

#include <Logger.h>
#include <uWebsockets/App.h>

#include <mutex>
#include <string>
#include <thread>

struct LiveSplitServer::Impl {
    std::thread thread;
    // Guards app/loop/running and stop_requested: Stop() and Send() only touch the loop while the thread is inside run().
    std::mutex mutex;
    uWS::App*  app            = nullptr;
    uWS::Loop* loop           = nullptr;
    bool       running        = false;
    bool       stop_requested = false;
};

LiveSplitServer::LiveSplitServer() : impl_(std::make_unique<Impl>()) {}

LiveSplitServer::~LiveSplitServer()
{
    Stop();
}

bool LiveSplitServer::IsRunning() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->running;
}

void LiveSplitServer::Start(const int port)
{
    Stop();
    impl_->thread = std::thread([impl = impl_.get(), port]() {
        // The thread owns the app: created, run and destroyed here, on the loop's own thread.
        uWS::App app;
        {
            std::lock_guard lock(impl->mutex);
            if (impl->stop_requested) return;
            impl->app     = &app;
            impl->loop    = uWS::Loop::get();
            impl->running = true;
        }
        app.ws<int>("/*",
                    {.compression            = uWS::SHARED_COMPRESSOR,
                     .maxPayloadLength       = 16 * 1024,
                     .idleTimeout            = 10,
                     .maxBackpressure        = 1 * 1024 * 1024,
                     .sendPingsAutomatically = true,
                     .upgrade                = nullptr,
                     .open                   = [](auto ws) { ws->subscribe("splits"); }})
            .listen(port,
                    [port](auto* listen_socket) {
                        if (listen_socket) Log::Log("Splits: LiveSplit server listening on port %d", port);
                        else Log::Error("Splits: LiveSplit server could not listen on port %d", port);
                    })
            .run();
        std::lock_guard lock(impl->mutex);
        impl->running = false;
        impl->app     = nullptr;
        impl->loop    = nullptr;
    });
}

void LiveSplitServer::Stop()
{
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stop_requested = true;
        // Only ask a live loop to close; closing the listener lets run() return on its own thread.
        if (impl_->running) impl_->loop->defer([app = impl_->app]() { app->close(); });
    }
    if (impl_->thread.joinable()) impl_->thread.join();
    std::lock_guard lock(impl_->mutex);
    impl_->stop_requested = false;
}

void LiveSplitServer::Send(const std::string_view command, const Format format)
{
    std::string payload = format == Format::LiveSplitOneJSON ? "{\"command\": \"" + std::string(command) + "\"}" : std::string(command);
    std::lock_guard lock(impl_->mutex);
    if (!impl_->running) return;
    // publish() isn't thread-safe: hand it to the server's own loop.
    impl_->loop->defer([app = impl_->app, payload = std::move(payload)]() {
        app->publish("splits", payload, uWS::OpCode::TEXT);
    });
}
