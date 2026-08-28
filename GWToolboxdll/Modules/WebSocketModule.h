#pragma once

#include <string>
#include <string_view>
#include <thread>

#include <uWebsockets/App.h>

#include <ToolboxModule.h>

// ---------------------------------------------------------------------------
// Standalone LiveSplit-compatible websocket broadcaster.
// ---------------------------------------------------------------------------
// Split out of ObjectiveTimerWindow so Splits doesn't depend on OT; runs on its own port
// (9002, OT stays on 9001) so both can coexist until OT is eventually retired.
//
// No settings/window/widget of its own — enable/port/mode render inside Splits' own settings page instead (see RegisterSettingsContent()).
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

    // Broadcasts a LiveSplit command ("start"/"split"/"reset"); context is an optional reason, unused on the wire but shown in the debug log.
    void Send(std::string_view msg, std::string_view context = {});

private:
    enum class Mode : int { None, LiveSplitOneJSON, LiveSplitServerCommand, Count }; // int-backed so ImGui::RadioButton can bind an int* without a punning cast

    void EnableServer(bool enable);
    void DrawSettings(); // rendered inside Splits' settings section, see RegisterSettingsContent()

    std::thread* server_thread_ = nullptr;
    uWS::App*    app_           = nullptr;
    Mode         mode_          = Mode::None;
    int          port_          = 9002;
    std::string  last_command_; // shown in DrawSettings() so live testing can confirm sends are happening
};
