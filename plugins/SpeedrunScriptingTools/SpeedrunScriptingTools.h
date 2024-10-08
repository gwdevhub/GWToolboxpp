#pragma once

#include <Action.h>
#include <Condition.h>
#include <Enums.h>

#include <ToolboxPlugin.h>

struct Hotkey {
    long keyData = 0;
    long modifier = 0;
};

struct Script {
    std::vector<ConditionPtr> conditions;
    std::vector<ActionPtr> actions;
    std::string name = "New script";
    Trigger trigger = Trigger::None;
    bool enabled = true;
    bool triggered = false;
    bool showMessageWhenTriggered = false;
    bool showMessageWhenToggled = false;

    bool canLaunchInParallel = false;
    bool globallyExclusive = false;

    Hotkey enabledToggleHotkey{};
    Hotkey triggerHotkey{};
    std::string triggerMessage{};
};

struct Group 
{
    std::vector<ConditionPtr> conditions;
    std::vector<Script> scripts;

    bool enabled = true;
    std::string name = "New group";
};

class SpeedrunScriptingTools : public ToolboxPlugin {
public:
    const char* Name() const override { return "SpeedrunScriptingTools"; }
    const char* Icon() const override { return ICON_FA_KEYBOARD; }

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
    void clear();

    std::vector<Group> m_groups;
    std::vector<Script> m_scripts;
    std::vector<Script> m_currentScripts;
    bool runInOutposts = false;
    bool alwaysBlockHotkeyKeys = false;
    bool isInLoadingScreen = false;
    Hotkey clearScriptsKey{};
};
