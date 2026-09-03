#pragma once

#include <Action.h>
#include <Condition.h>
#include <Enums.h>

#include <ToolboxUIPlugin.h>
#include <GWCA/Constants/Skills.h>
#include <IconsFontAwesome5.h>

#include <atomic>

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

class SpeedrunScriptingTools : public ToolboxUIPlugin {
public:
    SpeedrunScriptingTools();

    const char* Name() const override { return "SpeedrunScriptingTools"; }
    const char* Icon() const override { return ICON_FA_KEYBOARD; }

    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;

    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    bool HasSettings() const override { return true; }

    bool WndProc(UINT, WPARAM, LPARAM) override;

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;

    bool triggerScripts(Trigger triggerType, std::function<bool(const Script&)> extraConditions = [](const Script&) { return true; }, bool checkConditions = true);
    void loadFromIniFile(const ToolboxIni& ini);

private:
    static void OnDisplayDialogDecoded(void* context, const wchar_t* decoded);
    void CompleteDisplayDialogDecode(const wchar_t* decoded);
    void clear();
    void refreshDisabledKeys();

    std::vector<Group> m_groups;
    std::vector<Script> m_scripts;
    std::vector<Script> m_currentScripts;
    bool runInOutposts = false;
    bool alwaysBlockHotkeyKeys = false;
    bool isInLoadingScreen = false;
    int framesSinceLoadingFinished = 0;
    Hotkey clearScriptsKey{};
    bool terminating = false;
    std::atomic_size_t pendingDisplayDialogDecodes = 0;

    // Not serialized, derived from scripts at runtime.
    std::unordered_set<Hotkey> disabledKeys;
};
