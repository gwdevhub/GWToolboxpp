#include "stdafx.h"

#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <ImGuiAddons.h>

#include <Windows/Hotkeys/HotkeyCommandPet.h>

namespace {
    constexpr std::array behaviors = {
        "Fight",
        "Guard",
        "Avoid Combat"
    };

    const char* GetBehaviorDesc(GW::HeroBehavior behaviour)
    {
        if (std::to_underlying(behaviour) < behaviors.size()) {
            return behaviors[std::to_underlying(behaviour)];
        }
        return nullptr;
    }
}

HotkeyCommandPet::HotkeyCommandPet(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    behavior = ini ? static_cast<GW::HeroBehavior>(ini->GetLongValue(section, "behavior", static_cast<long>(behavior))) : behavior;
    if (!GetBehaviorDesc(behavior)) {
        behavior = GW::HeroBehavior::Fight;
    }
}

void HotkeyCommandPet::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "behavior", static_cast<long>(behavior));
}

int HotkeyCommandPet::Description(char* buf, const size_t bufsz)
{
    return snprintf(buf, bufsz, "Command Pet: %s", GetBehaviorDesc(behavior));
}

bool HotkeyCommandPet::Draw()
{
    const bool changed = ImGui::Combo("Behavior###combo", reinterpret_cast<int*>(&behavior), behaviors.data(), behaviors.size(), behaviors.size());
    if (changed && !GetBehaviorDesc(behavior)) {
        behavior = GW::HeroBehavior::Fight;
    }
    return changed;
}

void HotkeyCommandPet::Execute()
{
    GW::GameThread::Enqueue([&] {
        for (auto& pet : GW::GetWorldContext()->pets) {
            GW::PartyMgr::SetPetBehavior(pet.owner_agent_id, behavior);
        }
    });
}
