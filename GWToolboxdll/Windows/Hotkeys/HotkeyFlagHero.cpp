#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <ImGuiAddons.h>

#include <Windows/Hotkeys/HotkeyFlagHero.h>

HotkeyFlagHero::HotkeyFlagHero(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) return;
    degree = static_cast<float>(ini->GetDoubleValue(section, "degree", degree));
    distance = static_cast<float>(ini->GetDoubleValue(section, "distance", distance));
    hero = ini->GetLongValue(section, "hero", hero);
    if (hero < 0) {
        hero = 0;
    }
    if (hero > 11) {
        hero = 11;
    }
}

void HotkeyFlagHero::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetDoubleValue(section, "degree", degree);
    ini->SetDoubleValue(section, "distance", distance);
    ini->SetLongValue(section, "hero", hero);
}

int HotkeyFlagHero::Description(char* buf, const size_t bufsz)
{
    if (hero == 0) {
        return snprintf(buf, bufsz, "Flag All Heroes");
    }
    return snprintf(buf, bufsz, "Flag Hero %d", hero);
}

bool HotkeyFlagHero::Draw()
{
    bool hotkey_changed = false;
    hotkey_changed |= ImGui::DragFloat("Degree", &degree, 0.0f, -360.0f, 360.f);
    hotkey_changed |= ImGui::DragFloat("Distance", &distance, 0.0f, 0.0f, 10'000.f);
    if (hotkeys_changed && distance < 0.f) {
        distance = 0.f;
    }
    hotkey_changed |= ImGui::InputInt("Hero", &hero, 1);
    if (hotkey_changed && hero < 0) {
        hero = 0;
    }
    else if (hotkey_changed && hero > 11) {
        hero = 11;
    }
    ImGui::ShowHelp("The hero number that should be flagged (1-11).\nUse 0 to flag all");
    ImGui::Text("For a minimap flagging hotkey, please create a chat hotkey with:");
    ImGui::TextColored({1.f, 1.f, 0.f, 1.f}, "/flag %d toggle", hero);
    return hotkey_changed;
}

void HotkeyFlagHero::Execute()
{
    if (!isExplorable()) {
        return;
    }

    const GW::Vec3f allflag = GW::GetGameContext()->world->all_flag;

    if (hero < 0) {
        return;
    }
    if (hero == 0) {
        if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
            GW::PartyMgr::UnflagAll();
            return;
        }
    }
    else {
        const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
        if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
            return;
        }

        const GW::HeroFlag& flag = flags[hero - 1];
        if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
            GW::PartyMgr::UnflagHero(hero);
            return;
        }
    }

    const GW::AgentLiving* player = GW::Agents::GetControlledCharacter();
    if (!player) {
        return;
    }
    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

    float reference_radiant = player->rotation_angle;

    if (target && target != player) {
        const float dx = target->x - player->x;
        const float dy = target->y - player->y;

        reference_radiant = std::atan(dx == 0 ? dy : dy / dx);
        if (dx < 0) {
            reference_radiant += DirectX::XM_PI;
        }
        else if (dx > 0 && dy < 0) {
            reference_radiant += 2 * DirectX::XM_PI;
        }
    }

    const float radiant = degree * DirectX::XM_PI / 180.f;
    const float x = player->x + distance * std::cos(reference_radiant - radiant);
    const float y = player->y + distance * std::sin(reference_radiant - radiant);

    const auto pos = GW::GamePos(x, y, 0);

    if (hero == 0) {
        GW::PartyMgr::FlagAll(pos);
    }
    else {
        GW::PartyMgr::FlagHero(hero, pos);
    }
}
