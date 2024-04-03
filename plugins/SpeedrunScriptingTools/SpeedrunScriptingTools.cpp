#include "SpeedrunScriptingTools.h"

#include <ConditionImpls.h>
#include <ActionImpls.h>

#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Constants/Constants.h>

#include <Keys.h>

#include <imgui.h>
#include <SimpleIni.h>
#include <filesystem>


void SpeedrunScriptingTools::DrawSettings()
{

}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    /* shortcutKey = ini.GetLongValue(Name(), "key", shortcutKey);
    shortcutMod = ini.GetLongValue(Name(), "mod", shortcutMod);
    castDelayInMs = ini.GetLongValue(Name(), "delay", castDelayInMs);*/

    //ModKeyName(hotkeyDescription, _countof(hotkeyDescription), shortcutMod, shortcutKey);
}

void SpeedrunScriptingTools::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    /* ini.SetLongValue(Name(), "key", shortcutKey);
    ini.SetLongValue(Name(), "mod", shortcutMod);
    ini.SetLongValue(Name(), "delay", castDelayInMs);*/
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static SpeedrunScriptingTools instance;
    return &instance;
}
void SpeedrunScriptingTools::Update(float delta)
{
    ToolboxPlugin::Update(delta);

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        m_currentScript = std::nullopt;
        return;
    }

    if (m_currentScript && !m_currentScript->actions.empty()) {
        // Execute current script
        auto& currentActions = m_currentScript->actions;
        auto& currentAction = **currentActions.begin();
        if (currentAction.hasBeenStarted()) {
            if (currentAction.isComplete()) {
                currentAction.finalAction();
                currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
            }
        }
        else {
            currentAction.initialAction();
        }
    }
    else {
        // Find script to use
        for (const auto& script : m_scripts) {
            if (!script.enabled || script.conditions.empty() || script.actions.empty()) continue;
            if (std::ranges::all_of(script.conditions, [](const auto& condition) {return condition->check();})) {
                m_currentScript = script;
                break;
            }
        }
    }
}

void SpeedrunScriptingTools::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    GW::Initialize();

    /// TODO remove
    Script testScript;
    testScript.enabled = true;
    testScript.conditions.push_back(std::make_shared<PlayerHasSkillCondition>(GW::Constants::SkillID::Ebon_Escape));
    testScript.conditions.push_back(std::make_shared<IsInMapCondition>(GW::Constants::MapID::The_Black_Curtain));
    testScript.conditions.push_back(std::make_shared<CurrentTargetIsCastingSkillCondition>(GW::Constants::SkillID::Heart_of_Shadow));       

    testScript.actions.push_back(std::make_shared<MoveToAction>(GW::GamePos{-4503.89f, 14949.46f, 0}));
    testScript.actions.push_back(std::make_shared<WaitAction>(4312));
    testScript.actions.push_back(std::make_shared<CastOnTargetAction>(GW::Constants::SkillID::Ebon_Escape));
    testScript.actions.push_back(std::make_shared<UseItemAction>(6368));
    testScript.actions.push_back(std::make_shared<CastOnSelfAction>(GW::Constants::SkillID::Shadow_Form));

    m_scripts.push_back(std::move(testScript));
    ///
}
void SpeedrunScriptingTools::SignalTerminate()
{
    GW::DisableHooks();
}
bool SpeedrunScriptingTools::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void SpeedrunScriptingTools::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}
