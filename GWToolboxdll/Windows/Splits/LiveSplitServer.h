#pragma once

#include <memory>
#include <string_view>

// Websocket server that LiveSplit connects to for start/split/reset. Own port (default 9002) so it can run beside ObjectiveTimer's (9001).
class LiveSplitServer {
public:
    enum class Format : int { LiveSplitOneJSON = 0, LiveSplitServerCommand = 1 };

    LiveSplitServer();
    ~LiveSplitServer();

    void Start(int port);
    void Stop();
    [[nodiscard]] bool IsRunning() const;

    // No-op while stopped.
    void Send(std::string_view command, Format format);

private:
    // uWS stays out of this header: it would otherwise spread through SplitsWindow.h to every includer.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
