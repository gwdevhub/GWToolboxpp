#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <Widgets/AlcoholWidget.h>
#include <Defines.h>

#include "Utils/FontLoader.h"

namespace {
    AlcoholWidget::Settings settings;
}

void AlcoholWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
    // 饮酒累积的时间
    alcohol_time = 0;
    // 上次玩家使用酒精饮品的时间
    last_alcohol = 0;
    alcohol_level = 0;
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PostProcess>(&PostProcess_Entry, &AlcoholWidget::AlcUpdate,-0x8000);
}

void AlcoholWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void AlcoholWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

uint32_t AlcoholWidget::GetAlcoholTitlePoints()
{
    const GW::GameContext* gameContext = GW::GetGameContext();
    if (!gameContext || !gameContext->world || !gameContext->world->titles.valid()) {
        return 0; // 合理性检查；上下文未就绪。
    }
    constexpr auto title_idx = std::to_underlying(GW::Constants::TitleID::Drunkard);
    if (!(gameContext->world->titles.size() > title_idx)) {
        return 0; // 没有酒鬼称号
    }
    return gameContext->world->titles[title_idx].current_points;
}

uint32_t AlcoholWidget::GetAlcoholTitlePointsGained()
{
    const uint32_t current_title_points = GetAlcoholTitlePoints();
    const uint32_t points_gained = current_title_points - prev_alcohol_title_points;
    prev_alcohol_title_points = current_title_points; // 更新之前的变量。
    return points_gained <= 0 ? 0 : points_gained;
}

void AlcoholWidget::Update(const float)
{
    if (map_id != GW::Map::GetMapID()) {
        last_alcohol = 0;
        alcohol_time = alcohol_level = prev_packet_tint_6_level = 0;
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
            map_id = GW::Map::GetMapID();
            prev_alcohol_title_points = GetAlcoholTitlePoints(); // 在地图开始时获取基础酒精点数。
        }
    }
}

uint32_t AlcoholWidget::GetAlcoholLevel() const
{
    return alcohol_level;
}

void AlcoholWidget::AlcUpdate(GW::HookStatus*, const GW::Packet::StoC::PostProcess* packet)
{
    AlcoholWidget& instance = Instance();
    if (packet->tint == 8 && packet->level == 5) {
        return; // 帕奈沙拉
    }
    const uint32_t pts_gained = instance.GetAlcoholTitlePointsGained();

    if (packet->tint == 6) {
        // 染色 6，等级 5 - 月神的麻烦区域！
        // 也用于克里坦白兰地（酒精等级 5）
        if (packet->level == 5 &&
            (instance.prev_packet_tint_6_level < packet->level - 1
             || (instance.prev_packet_tint_6_level == 5 && pts_gained < 1))) {
            // 如果我们跳了一级，或者上一个数据包也是等级 5 且没有获得点数，则不是酒精。
            // 注意：所有酒精从 1 到 5 递增，但月神直接跳到等级 5。
            instance.prev_packet_tint_6_level = packet->level;
            return;
        }
        instance.prev_packet_tint_6_level = packet->level;
    }
    // 如果玩家使用了酒精饮品
    if (packet->level > instance.alcohol_level) {
        // 如果玩家已经在饮酒状态
        if (instance.alcohol_level) {
            instance.alcohol_time = static_cast<int>(instance.alcohol_time + static_cast<long>(instance.last_alcohol) - static_cast<long>(time(nullptr)));
        }
        instance.alcohol_time += 60 * static_cast<int>(packet->level - instance.alcohol_level);
        instance.last_alcohol = time(nullptr);
    }
    else if (packet->level <= instance.alcohol_level) {
        instance.alcohol_time = 60 * static_cast<int>(packet->level);
        instance.last_alcohol = time(nullptr);
    }
    instance.alcohol_level = packet->level;
}

void AlcoholWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return;
    }

    if (settings.only_show_when_drunk && alcohol_level == 0) {
        return;
    }

    long t = 0;
    if (alcohol_level != 0) {
        t = static_cast<long>(static_cast<int>(last_alcohol) + static_cast<int>(alcohol_time)) - static_cast<long>(time(nullptr));
        // 注意：有时游戏不会发送移除后期处理的信号。
        if (t < 0) {
            alcohol_level = 0;
        }
    }

    if (settings.only_show_when_drunk && t < 0) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header1));
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "喝酒");
        ImGui::SetCursorPos(cur);
        ImGui::Text("喝酒");
        ImGui::PopFont();

        static char timer[32];
        snprintf(timer, 32, "%1ld:%02ld", t / 60 % 60, t % 60);

        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::widget_large));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), timer);
        ImGui::SetCursorPos(cur);
        ImGui::Text(timer);
#if 0
        ImGui::Text("喝酒等级 %d", GetAlcoholLevel());
#endif
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void AlcoholWidget::DrawSettingsInternal()
{
    ImGui::CheckboxWithHelp("仅在喝酒时显示", &settings.only_show_when_drunk, "未喝酒时隐藏小部件");
    ImGui::Text("注意：仅在探索区域可见。");
}
