#pragma once

#include <Action.h>
#include <Condition.h>
#include <Enums.h>

#include <ToolboxPlugin.h>
#include <GWCA/Constants/Skills.h>

struct Script {
    Script()
    { 
        static int idCounter = 0;
        id = idCounter++;
    }
    int getId() const { return id; }

    std::vector<ConditionPtr> conditions;
    std::vector<ActionPtr> actions;
    std::string name = "New script";

    bool enabled = true;
    bool showMessageWhenTriggered = false;
    bool showMessageWhenToggled = false;
    bool canLaunchInParallel = false;
    bool globallyExclusive = false;

    Hotkey enabledToggleHotkey{};
    Trigger trigger = Trigger::None;
    TriggerData triggerData{};

    // Runtime data, not serialized
    bool triggered = false;
    
  private:
    size_t id = 0;
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
    int framesSinceLoadingFinished = 0;
    Hotkey clearScriptsKey{};
};
