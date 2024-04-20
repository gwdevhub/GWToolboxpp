#pragma once

#include <stdint.h>
#include <Action.h>
#include <Condition.h>
#include <TriggerPacket.h>

#include <ToolboxPlugin.h>

struct HotkeyStatus {
    long keyData = 0;
    long modifier = 0;
    bool triggered = false;
};

struct Script {
    std::vector<std::shared_ptr<Condition>> conditions;
    std::vector<std::shared_ptr<Action>> actions;
    std::string name = "New script";
    Trigger trigger = Trigger::None;
    bool enabled = true;
    HotkeyStatus hotkeyStatus{};
};

class SpeedrunScriptingTools : public ToolboxPlugin {
public:
    const char* Name() const override { return "SpeedrunScriptingTools"; }

    void Update(float) override;

    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    bool HasSettings() const override { return true; }

    bool WndProc(UINT, WPARAM, LPARAM) override;

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    bool CanTerminate() override;
    void SignalTerminate() override;
    void Terminate() override;

private:
    std::vector<Script> m_scripts;
    std::optional<Script> m_currentScript = std::nullopt;
    Trigger currentTrigger = Trigger::None;
};
