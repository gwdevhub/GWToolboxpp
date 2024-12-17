#include "ExamplePlugin.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include "GWCA/Managers/ChatMgr.h"

namespace {
    bool redirect_slash_ee_to_eee = false;
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ExamplePlugin instance;
    return &instance;
}

void ExamplePlugin::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    PLUGIN_LOAD_BOOL(redirect_slash_ee_to_eee);
}

void ExamplePlugin::SaveSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::SaveSettings(folder);
    PLUGIN_SAVE_BOOL(redirect_slash_ee_to_eee);
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

void ExamplePlugin::DrawSettings()
{
    if (!toolbox_handle) {
        return;
    }
    ImGui::Checkbox("Redirect ee to eee", &redirect_slash_ee_to_eee);
}

void ExamplePlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::Chat::CreateCommand(L"ee", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        if (redirect_slash_ee_to_eee) {
            GW::Chat::SendChat('/', "eee");
        }
    });
}
void ExamplePlugin::SignalTerminate()
{
    ToolboxUIPlugin::SignalTerminate();
    GW::Chat::DeleteCommand(L"ee");
}

bool ExamplePlugin::CanTerminate() {
    return true;
}

void ExamplePlugin::Draw(IDirect3DDevice9*)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    if (ImGui::Begin(Name(), GetVisiblePtr(), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::Text("Example plugin: area loading...");
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
