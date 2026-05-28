#include <ProjectileIndicator.h>

#include <PluginUtils.h>
#include <Rendering.h>
#include <ModelNames.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Packets/StoC.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ProjectileIndicator instance;
    return &instance;
}

namespace 
{
    GW::HookEntry projectileHook;

    constexpr auto ellipsisA = 125.f;
    constexpr auto ellipsisB = 50.f;

    bool shouldRender() 
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && GW::Agents::GetControlledCharacter() && !GW::Agents::IsObserving();
    }

    std::string getSkillName(GW::Constants::SkillID id)
    {
        if (id == GW::Constants::SkillID::No_Skill) return "Auto-Attack";
        static std::unordered_map<GW::Constants::SkillID, std::wstring> decodedNames;
        if (const auto it = decodedNames.find(id); it != decodedNames.end()) {
            return PluginUtils::WStringToString(it->second);
        }

        const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
        if (!skillData || (uint32_t)id >= (uint32_t)GW::Constants::SkillID::Count) return "";

        wchar_t out[8] = {0};
        if (GW::UI::UInt32ToEncStr(skillData->name, out, _countof(out))) 
        {
            auto& decoded = decodedNames[id];
            decoded.reserve(256);
            GW::UI::AsyncDecodeStr(out, decoded.data(), 256);
        }
        return "";
    }
}

void ProjectileIndicator::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(&projectileHook, [this](GW::HookStatus* status, GW::Packet::StoC::AgentProjectileLaunched* packet) -> void {
        if (!shouldRender()) return;
        
        const auto agent = GW::Agents::GetAgentByID(packet->agent_id);
        if (!agent || !agent->GetIsLivingType()) return;
        const auto living = agent->GetAsAgentLiving();

        status->blocked = std::ranges::contains(suppressedProjecitileAnimationSources, living->player_number);
        
        if (living->allegiance != GW::Constants::Allegiance::Enemy || !std::ranges::contains(trackedSkills, (GW::Constants::SkillID)agent->GetAsAgentLiving()->skill)) return;
        if (!trackedEnemyModels.empty() && !std::ranges::contains(trackedEnemyModels, (int)living->player_number)) return;

        const auto ADirection = GW::Normalize(GW::Vec2f{GW::Agents::GetControlledCharacter()->pos} - GW::Vec2f{living->pos});
        RenderingUtils::addEllipseToDraw(packet->destination, ADirection, ellipsisA, ellipsisB, ImGui::ColorConvertFloat4ToU32(color), filled, projectileTimer);        
    });
}

void ProjectileIndicator::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentProjectileLaunched>(&projectileHook);
}

void ProjectileIndicator::Terminate()
{
    RenderingUtils::clearDrawingList();
    ToolboxPlugin::Terminate();
}

void ProjectileIndicator::Draw(IDirect3DDevice9* device) 
{
    if (shouldRender())
        RenderingUtils::draw(device);
    else
        RenderingUtils::clearDrawingList();
}

void ProjectileIndicator::DrawSettings() 
{
    ImGui::Checkbox("Fill circle", &filled);
    ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&color));
    ImGui::SliderInt("Display duration (ms)", &projectileTimer, 100, 4000);

    int drawID = 0;
    {
        ImGui::Text("Tracked skills:");
        auto toErase = trackedSkills.end();
        for (auto it = trackedSkills.begin(); it != trackedSkills.end(); ++it) 
        {
            ImGui::PushID(drawID++);
            ImGui::Bullet();
            if (ImGui::Button("X", ImVec2(20, 20)))toErase = it;
            ImGui::SameLine();
            ImGui::Text(getSkillName(*it).c_str());
            ImGui::SameLine();
            ImGui::PushItemWidth(100.f);
            ImGui::InputInt("", reinterpret_cast<int*>(&*it), 0);
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
        if (toErase != trackedSkills.end()) {
            trackedSkills.erase(toErase);
        }
        if (ImGui::Button("+")) {
            trackedSkills.push_back(GW::Constants::SkillID::No_Skill);
        }
    }

    const auto& modelNames = getModelNames();
    {
        ImGui::Text("Tracked enemy models:");
        if (trackedEnemyModels.empty()) {
            ImGui::SameLine();
            ImGui::Text("All");
        }
        auto toErase = trackedEnemyModels.end();
        for (auto it = trackedEnemyModels.begin(); it != trackedEnemyModels.end(); ++it) 
        {
            ImGui::PushID(drawID++);
            ImGui::Bullet();
            if (ImGui::Button("X", ImVec2(20, 20))) toErase = it;
            ImGui::SameLine();
            if (const auto modelIt = modelNames.find((uint16_t)*it); modelIt != modelNames.end()) {
                ImGui::Text(modelIt->second.data());
                ImGui::SameLine();
            }
            ImGui::PushItemWidth(100.f);
            ImGui::InputInt("", &*it, 0);
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
        if (toErase != trackedEnemyModels.end()) 
        {
            trackedEnemyModels.erase(toErase);
        }
        ImGui::PushID(drawID++);
        if (ImGui::Button("+")) 
        {
            trackedEnemyModels.push_back(0);
        }
        ImGui::PopID();
    }

    {
        ImGui::Text("Supress projectiles from agents with model ID:");
        auto toErase = suppressedProjecitileAnimationSources.end();
        for (auto it = suppressedProjecitileAnimationSources.begin(); it != suppressedProjecitileAnimationSources.end(); ++it) {
            ImGui::PushID(drawID++);
            ImGui::Bullet();
            if (ImGui::Button("X", ImVec2(20, 20))) toErase = it;
            ImGui::SameLine();
            if (const auto modelIt = modelNames.find((uint16_t)*it); modelIt != modelNames.end()) {
                ImGui::Text(modelIt->second.data());
                ImGui::SameLine();
            }
            ImGui::PushItemWidth(100.f);
            ImGui::InputInt("", &*it, 0);
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
        if (toErase != suppressedProjecitileAnimationSources.end()) {
            suppressedProjecitileAnimationSources.erase(toErase);
        }
        ImGui::PushID(drawID++);
        if (ImGui::Button("+")) {
            suppressedProjecitileAnimationSources.push_back(0);
        }
        ImGui::PopID();
    }

    ImGui::Text("Version 1.2.3");
}

void ProjectileIndicator::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);

    const auto ini = LoadIni(folder);

    const auto loadColor = [&](ImVec4& color, std::string varName) {
        color.x = (float)ini.GetDoubleValue(Name(), (varName + "x").c_str(), color.x);
        color.y = (float)ini.GetDoubleValue(Name(), (varName + "y").c_str(), color.y);
        color.z = (float)ini.GetDoubleValue(Name(), (varName + "z").c_str(), color.z);
        color.w = (float)ini.GetDoubleValue(Name(), (varName + "w").c_str(), color.w);
    };
    const auto split = [](std::string_view input, auto type)
    {
        std::vector<decltype(type)> result;
        std::istringstream iss(std::string{input});
        std::string item;
        while (std::getline(iss, item, ' ')) 
        {
            result.push_back((decltype(type))std::stoi(item));
        }
        return result;
    };

    loadColor(color, VAR_NAME(color));
    filled = ini.GetBoolValue(Name(), VAR_NAME(filled), filled);
    projectileTimer = ini.GetLongValue(Name(), VAR_NAME(projectileTimer), projectileTimer);

    trackedSkills = split(ini.GetValue(Name(), "skills", "3075 3074"), GW::Constants::SkillID{});
    trackedEnemyModels = split(ini.GetValue(Name(), "models", ""), int{});
    suppressedProjecitileAnimationSources = split(ini.GetValue(Name(), "suppressed", "2333"), int{});
}

void ProjectileIndicator::SaveSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::SaveSettings(folder);

    auto ini = LoadIni(folder);

    const auto saveColor = [&](const ImVec4& color, std::string varName) {
        ini.SetDoubleValue(Name(), (varName + "x").c_str(), color.x);
        ini.SetDoubleValue(Name(), (varName + "y").c_str(), color.y);
        ini.SetDoubleValue(Name(), (varName + "z").c_str(), color.z);
        ini.SetDoubleValue(Name(), (varName + "w").c_str(), color.w);
    };
    saveColor(color, VAR_NAME(color));
    ini.SetBoolValue(Name(), VAR_NAME(filled), filled);
    ini.SetLongValue(Name(), VAR_NAME(projectileTimer), projectileTimer);

    std::string skills;
    for (const auto& skill : trackedSkills) 
    {
        skills += std::to_string((int)skill) + " ";
    }
    ini.SetValue(Name(), "skills", skills.c_str());

    std::string models;
    for (const auto& model : trackedEnemyModels) {
        models += std::to_string(model) + " ";
    }
    ini.SetValue(Name(), "models", models.c_str());

    std::string suppressed;
    for (const auto& source : suppressedProjecitileAnimationSources) {
        suppressed += std::to_string(source) + " ";
    }
    ini.SetValue(Name(), "suppressed", suppressed.c_str());

    PLUGIN_ASSERT(ini.SaveFile(ini.location_on_disk) == SI_OK);
}
