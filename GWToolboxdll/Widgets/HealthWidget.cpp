#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Color.h>
#include <Defines.h>
#include <Modules/Resources.h>
#include <Utils/FontLoader.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/PartyDamage.h>

namespace {
    HealthWidget::Settings settings;

    std::wstring agent_name_ping;
} // namespace

constexpr auto HEALTH_THRESHOLD_INIFILENAME = L"HealthThreshold.ini";

void HealthWidget::Initialize()
{
    ToolboxWidget::Initialize();
    SettingsRegistry::Register(this, settings);
}

void HealthWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    auto_size = true;

    for (const auto* threshold : thresholds) {
        delete threshold;
    }
    thresholds.clear();

    std::vector<ThresholdSettings> stored;
    if (doc.Get(Name(), "thresholds", stored)) {
        for (const auto& t : stored) {
            const auto threshold = new Threshold(t.name.c_str(), t.color, t.value);
            threshold->active = t.active;
            threshold->modelId = t.modelId;
            threshold->skillId = t.skillId;
            threshold->mapId = t.mapId;
            threshold->index = thresholds.size();
            thresholds.push_back(threshold);
        }
    }
    else {
        ToolboxIni inifile{};
        ASSERT(inifile.LoadIfExists(Resources::GetLegacySettingFile(HEALTH_THRESHOLD_INIFILENAME)) == SI_OK);

        TNamesDepend entries;
        inifile.GetAllSections(entries);

        for (const auto& entry : entries) {
            auto threshold = new Threshold(&inifile, entry.pItem);
            threshold->index = thresholds.size();
            thresholds.push_back(threshold);
        }
    }

    if (thresholds.empty()) {
        const auto thresholdFh = new Threshold("\"终结他！\"", Colors::RGB(255, 255, 0), 50);
        thresholdFh->skillId = static_cast<int>(GW::Constants::SkillID::Finish_Him);
        thresholdFh->active = false;
        thresholds.push_back(thresholdFh);
        thresholds.back()->index = thresholds.size() - 1;

        const auto thresholdEoe = new Threshold("灭绝之刃", Colors::RGB(0, 255, 0), 90);
        thresholdEoe->active = false;
        thresholds.push_back(thresholdEoe);
        thresholds.back()->index = thresholds.size() - 1;
    }
}

void HealthWidget::SaveSettings(SettingsDoc& doc)
{
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    std::vector<ThresholdSettings> stored;
    stored.reserve(thresholds.size());
    for (const auto* threshold : thresholds) {
        stored.push_back({threshold->active, threshold->name, threshold->modelId, threshold->skillId, threshold->mapId, threshold->value, threshold->color});
    }
    doc.Set(Name(), "thresholds", stored);
}

void HealthWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::SameLine();
    ImGui::Checkbox("在前哨站隐藏", &settings.hide_in_outpost);
    ImGui::SameLine();
    ImGui::Checkbox("Ctrl+点击打印目标生命值", &settings.click_to_print_health);
    ImGui::Text("文字大小：");
    ImGui::ShowHelp("文字大小为 0 表示不绘制。");
    ImGui::Indent();
    ImGui::DragFloat("'生命值' 标题", &settings.font_size_header, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("百分比数值", &settings.font_size_perc_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("绝对数值", &settings.font_size_abs_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::Unindent();

    const bool thresholdsNode = ImGui::TreeNodeEx("阈值", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("将使用第一个匹配的阈值。");
    }
    if (thresholdsNode) {
        for (size_t i = 0; i < thresholds.size(); i++) {
            Threshold* threshold = thresholds[i];

            if (!threshold) {
                continue;
            }

            ImGui::PushID(static_cast<int>(threshold->ui_id));

            auto op = Threshold::Operation::None;
            threshold->DrawSettings(op);

            switch (op) {
                case Threshold::Operation::None:
                    break;
                case Threshold::Operation::MoveUp:
                    if (i > 0) {
                        std::swap(thresholds[i], thresholds[i - 1]);
                    }
                    break;
                case Threshold::Operation::MoveDown:
                    if (i + 1 < thresholds.size()) {
                        std::swap(thresholds[i], thresholds[i + 1]);
                    }
                    break;
                case Threshold::Operation::Delete:
                    thresholds.erase(thresholds.begin() + static_cast<int>(i));
                    delete threshold;
                    threshold = nullptr;
                    --i;
                    break;
            }

            ImGui::PopID();
        }

        if (ImGui::Button("添加阈值")) {
            thresholds.push_back(new Threshold("<名称>", 0xFFFFFFFF, 0));
            thresholds.back()->index = thresholds.size() - 1;
        }

        ImGui::TreePop();
    }
}

void HealthWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (settings.hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && settings.click_to_print_health)))) {
        const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
        if (target) {
            ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            constexpr auto background = ImColor(Colors::Black());

            for (size_t i = 0; i < thresholds.size(); i++) {
                Threshold* threshold = thresholds[i];

                if (!threshold) {
                    continue;
                }
                if (!threshold->active) {
                    continue;
                }
                if (threshold->modelId && threshold->modelId != target->player_number) {
                    continue;
                }
                if (threshold->skillId) {
                    GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
                    if (!(skillbar && skillbar->IsValid())) {
                        continue;
                    }
                    const GW::SkillbarSkill* skill = skillbar->GetSkillById(static_cast<GW::Constants::SkillID>(threshold->skillId));
                    if (!skill) {
                        continue;
                    }
                }
                if (threshold->mapId) {
                    if (static_cast<GW::Constants::MapID>(threshold->mapId) != GW::Map::GetMapID()) {
                        continue;
                    }
                }

                if (target->hp * 100 < threshold->value) {
                    color = ImColor(threshold->color);
                    break;
                }
            }

            ImVec2 cur = ImGui::GetCursorPos();
            if (settings.font_size_header > 0.f && show_titlebar) {
                ImGui::PushFont(FontLoader::GetFont(), settings.font_size_header);
                ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
                ImGui::TextColored(background, "生命值");
                ImGui::SetCursorPos(cur);
                ImGui::Text("生命值");
                ImGui::PopFont();
            }

            if (settings.font_size_perc_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFont(), settings.font_size_perc_value);
                cur = ImGui::GetCursorPos();
                const auto health_perc = target->hp >= 0 ? std::format("{:.0f}%%", target->hp * 100.0f) : "-";
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, health_perc.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, health_perc.c_str());
                ImGui::PopFont();
            }

            if (settings.font_size_abs_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFont(), settings.font_size_abs_value);
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                uint32_t display_max_hp = PartyDamage::GetMaxHp(target);
                const auto health_abs = display_max_hp > 0 ? std::format("{:.0f} / {}", target->hp * display_max_hp, display_max_hp) : std::string("-");
                ImGui::TextColored(background, health_abs.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::Text(health_abs.c_str());
                ImGui::PopFont();
            }

            if (settings.click_to_print_health) {
                if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsWindowHovered()) {
                    if (target) {
                        GW::Agents::AsyncGetAgentName(target, agent_name_ping);
                        if (!agent_name_ping.empty()) {
                            const std::string agent_name_str = TextUtils::WStringToString(agent_name_ping);
                            const auto current_hp = static_cast<int>(target->hp * target->max_hp);
                            const auto message = std::format("{} 的生命值为 {} / {}（{:.0f}%）。", agent_name_str.c_str(), current_hp, target->max_hp, target->hp * 100.f);
                            GW::Chat::SendChat('#', message.c_str());
                        }
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

unsigned int HealthWidget::Threshold::cur_ui_id = 0;

HealthWidget::Threshold::Threshold(const ToolboxIni* ini, const char* section) : ui_id(++cur_ui_id)
{
    active = ini->GetBoolValue(section, VAR_NAME(active));

    std::snprintf(name, sizeof(name), "%s", ini->GetValue(section, VAR_NAME(name), ""));
    modelId = ini->GetLongValue(section, VAR_NAME(modelId), modelId);
    skillId = ini->GetLongValue(section, VAR_NAME(skillId), skillId);
    mapId = ini->GetLongValue(section, VAR_NAME(mapId), mapId);
    value = ini->GetLongValue(section, VAR_NAME(value), value);
    color = Colors::Load(ini, section, VAR_NAME(color), color);
}

HealthWidget::Threshold::Threshold(const char* _name, const Color _color, const int _value) : ui_id(++cur_ui_id), value(_value), color(_color)
{
    std::snprintf(name, sizeof(name), "%s", _name);
}

bool HealthWidget::Threshold::DrawHeader()
{
    ImGui::SameLine(0, 18);
    const bool changed = ImGui::Checkbox("##active", &active);
    ImGui::SameLine();
    ImGui::ColorButton("", ImColor(color));
    ImGui::SameLine();
    ImGui::Text("%s（<%d%%）%s", name, value, Resources::GetMapName((GW::Constants::MapID)mapId)->string().c_str());
    return changed;
}

bool HealthWidget::Threshold::DrawSettings(Operation& op)
{
    bool changed = false;

    if (ImGui::TreeNodeEx("##params", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap)) {
        changed |= DrawHeader();

        ImGui::PushID(static_cast<int>(ui_id));

        changed |= ImGui::InputText("名称", name, 128);
        ImGui::ShowHelp("帮助你记住此阈值的名称。可选。");
        changed |= ImGui::InputInt("模型 ID", &modelId);
        ImGui::ShowHelp("此阈值将应用的单位。可选。留 0 表示任意单位");
        changed |= ImGui::InputInt("技能 ID", &skillId);
        ImGui::ShowHelp("仅当此技能在你的技能栏上时应用。可选。留 0 表示任意技能");
        changed |= ImGui::InputInt("地图 ID", &mapId);
        ImGui::ShowHelp("将应用的地图。可选。留 0 表示任意地图");
        changed |= ImGui::InputInt("百分比", &value);
        ImGui::ShowHelp("低于此百分比时使用此颜色");
        changed |= Colors::DrawSettingHueWheel("颜色", &color, 0);
        ImGui::ShowHelp("此阈值的自定义颜色。");

        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
        if (ImGui::Button("上移", ImVec2(width, 0))) {
            op = Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在列表中上移此阈值");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("下移", ImVec2(width, 0))) {
            op = Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("在列表中下移此阈值");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("删除", ImVec2(width, 0))) {
            op = Operation::Delete;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("删除此阈值");
        }

        ImGui::TreePop();
        ImGui::PopID();
    }
    else {
        changed |= DrawHeader();
    }

    return changed;
}
