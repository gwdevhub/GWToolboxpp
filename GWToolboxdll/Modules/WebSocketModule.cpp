#include "stdafx.h"
#include "WebSocketModule.h"

#include <Logger.h>
#include <ToolboxIni.h>
#include <Utils/SettingsDoc.h>

void WebSocketModule::Initialize()
{
    ToolboxModule::Initialize();
}

void WebSocketModule::Terminate()
{
    EnableServer(false);
    ToolboxModule::Terminate();
}

void WebSocketModule::RegisterSettingsContent()
{
    // No section of our own — "Splits" already registers its own icon, so pass nullptr rather than risk the icon-mismatch assert.
    ToolboxModule::RegisterSettingsContent(
        "Splits", nullptr,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) DrawSettings();
        },
        0.1f);
}

void WebSocketModule::LoadSettings(SettingsDoc& doc, ToolboxIni*)
{
    auto stored_mode = static_cast<int>(mode_);
    doc.Get(Name(), "mode", stored_mode);
    if (stored_mode < 0 || stored_mode >= static_cast<int>(Mode::Count))
        stored_mode = static_cast<int>(Mode::None);
    mode_ = static_cast<Mode>(stored_mode);
    doc.Get(Name(), "port", port_);
    if (port_ <= 0) port_ = 9002;
    EnableServer(mode_ != Mode::None);
}

void WebSocketModule::SaveSettings(SettingsDoc& doc)
{
    doc.Set(Name(), "mode", static_cast<int>(mode_));
    doc.Set(Name(), "port", port_);
}

void WebSocketModule::EnableServer(const bool enable)
{
    port_ = std::max(port_, 0);
    if (!enable) {
        if (app_) {
            app_->close();
            delete app_;
            app_ = nullptr;
        }
        if (server_thread_) {
            ASSERT(server_thread_->joinable());
            server_thread_->join();
            delete server_thread_;
            server_thread_ = nullptr;
        }
        return;
    }

    if (server_thread_) return;
    EnableServer(false);
    const int port = port_;
    server_thread_ = new std::thread([this, port]() {
        // The app needs to be created in the thread that handles the websocket connections.
        app_ = new uWS::App();
        app_->ws<int>(
                 "/*",
                 {/* Settings */
                  .compression       = uWS::SHARED_COMPRESSOR,
                  .maxPayloadLength  = 16 * 1024,
                  .idleTimeout       = 10,
                  .maxBackpressure   = 1 * 1024 * 1024,
                  .sendPingsAutomatically = true,
                  /* Handlers */
                  .upgrade = nullptr,
                  .open    = [](auto ws) { ws->subscribe("objective_events"); }}
             )
            .listen(port,
                [port](auto* listen_socket) {
                    if (listen_socket) {
                        Log::Log("WebSocketModule listening on port %d", port);
                    }
                })
            .run();
    });
}

void WebSocketModule::Send(const std::string_view msg, const std::string_view context)
{
    if (!context.empty()) last_command_ = std::string(context);
    if (!app_) return;
    // @Cleanup: same "should this send from a different thread" question OT's own version has always had — carried over as-is.
    if (mode_ == Mode::LiveSplitOneJSON) {
        const std::string command = "{\"command\": \"" + std::string(msg) + "\"}";
        app_->publish("objective_events", command, uWS::OpCode::TEXT);
    } else {
        app_->publish("objective_events", msg, uWS::OpCode::TEXT);
    }
}

void WebSocketModule::DrawSettings()
{
    ImGui::Separator();
    bool enabled = mode_ != Mode::None;
    if (ImGui::Checkbox("Enable LiveSplit websocket server", &enabled)) {
        mode_ = enabled ? Mode::LiveSplitOneJSON : Mode::None;
        EnableServer(enabled);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Broadcasts Start/Split/Reset commands to a connected LiveSplit instance.\n"
            "Independent of ObjectiveTimerWindow's own websocket server (different port) —\n"
            "safe to run both, or use this one alone once you no longer need OT.");
    }
    if (enabled) {
        ImGui::Indent();
        if (ImGui::InputInt("Websocket server port", &port_)) {
            EnableServer(false);
            EnableServer(enabled);
        }
        ImGui::Text("Status: %s", app_ && server_thread_ ? "Running" : "Stopped");
        if (app_ && server_thread_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(Port %d)", port_);
        }
        if (ImGui::SmallButton("Restart")) {
            EnableServer(false);
            EnableServer(enabled);
        }
        ImGui::RadioButton("LiveSplit One JSON Format", reinterpret_cast<int*>(&mode_), static_cast<int>(Mode::LiveSplitOneJSON));
        ImGui::RadioButton("LiveSplit Server Command Format", reinterpret_cast<int*>(&mode_), static_cast<int>(Mode::LiveSplitServerCommand));
        // Safe: Mode is explicitly int-backed (see enum declaration), so this isn't punning across sizes/signedness.
        ImGui::Spacing();
        if (last_command_.empty())
            ImGui::TextDisabled("Last command: none");
        else
            ImGui::Text("Last command: %s", last_command_.c_str());
        ImGui::Unindent();
    }
}
