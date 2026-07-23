#pragma once

#include <string>
#include <string_view>
#include <thread>

#include <uWebsockets/App.h>

#include <ToolboxModule.h>

// ---------------------------------------------------------------------------
// Standalone LiveSplit-compatible websocket broadcaster.
// ---------------------------------------------------------------------------
// Split out from ObjectiveTimerWindow so Splits (its only real consumer today — see
// SplitsWindow.cpp's Send(...) call sites) doesn't depend on OT at all. Deliberately runs on
// its own port (default 9002, one above OT's own 9001) rather than replacing OT's existing
// server outright — OT's own websocket stays completely untouched, so nothing regresses for
// anyone still using plain OT without Splits while Splits settles in. Once OT is eventually
// retired, this is the one that survives.
//
// No settings/window/widget of its own (HasSettings() is false, and it overrides
// RegisterSettingsContent() to skip the base class's default self-section entirely) — its
// enable/port/mode controls render directly inside Splits' own settings page instead, since
// that's the only place anyone would go looking for it.
class WebSocketModule : public ToolboxModule {
    WebSocketModule()           = default;
    ~WebSocketModule() override = default;

public:
    static WebSocketModule& Instance()
    {
        static WebSocketModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "WebSocket"; }
    [[nodiscard]] bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void RegisterSettingsContent() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    // Broadcasts a LiveSplit command ("start", "split", "reset") to connected clients. context
    // is an optional human-readable reason, unused by the wire format but handy for the debug
    // log at the call site.
    void Send(std::string_view msg, std::string_view context = {});

private:
    // int-backed (not the more natural uint32_t) so ImGui::RadioButton can bind an int* to it
    // directly without a punning cast.
    enum class Mode : int { None, LiveSplitOneJSON, LiveSplitServerCommand, Count };

    void EnableServer(bool enable);
    void DrawSettings(); // rendered inside Splits' settings section, see RegisterSettingsContent()

    std::thread* server_thread_ = nullptr;
    uWS::App*    app_           = nullptr;
    Mode         mode_          = Mode::None;
    int          port_          = 9002;
    std::string  last_command_; // shown in DrawSettings() so live testing can confirm sends are happening
};
