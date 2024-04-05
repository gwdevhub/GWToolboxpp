#pragma once

#include <Action.h>
#include <Condition.h>

#include <ToolboxPlugin.h>

struct Script {
    std::vector<std::shared_ptr<Condition>> conditions;
    std::vector<std::shared_ptr<Action>> actions;
    bool enabled = true;
    std::string name = "TestScript:                   ";
};

class SpeedrunScriptingTools : public ToolboxPlugin {
public:
    const char* Name() const override { return "SpeedrunScriptingTools"; }

    void Update(float) override;

    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    bool CanTerminate() override;
    void SignalTerminate() override;
    void Terminate() override;

private:
    std::vector<Script> m_scripts;
    std::optional<Script> m_currentScript = std::nullopt;
};
