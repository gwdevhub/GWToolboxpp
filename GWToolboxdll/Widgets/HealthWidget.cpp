#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Widgets/HealthWidget.h>
#include <Utils/FontLoader.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Color.h>

namespace {
    bool click_to_print_health = false;

    std::wstring agent_name_ping;
    bool hide_in_outpost = false;
    float font_size_header = static_cast<float>(FontLoader::FontSize::header1);
    float font_size_perc_value = static_cast<float>(FontLoader::FontSize::widget_small);
    float font_size_abs_value = static_cast<float>(FontLoader::FontSize::widget_label);

    bool thresholds_changed = false;
    ToolboxIni inifile{};
}

constexpr auto HEALTH_THRESHOLD_INIFILENAME = L"HealthThreshold.ini";

void HealthWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(click_to_print_health);
    LOAD_BOOL(hide_in_outpost);
    LOAD_FLOAT(font_size_header);
    LOAD_FLOAT(font_size_perc_value);
    LOAD_FLOAT(font_size_abs_value);
    auto_size = true;

    ASSERT(inifile.LoadIfExists(Resources::GetSettingFile(HEALTH_THRESHOLD_INIFILENAME)) == SI_OK);

    ToolboxIni::TNamesDepend entries;
    inifile.GetAllSections(entries);

    for (const ToolboxIni::Entry& entry : entries) {
        auto threshold = new Threshold(&inifile, entry.pItem);
        threshold->index = thresholds.size();
        thresholds.push_back(threshold);
    }

    if (thresholds.empty()) {
        const auto thresholdFh = new Threshold("\"Finish Him!\"", Colors::RGB(255, 255, 0), 50);
        thresholdFh->skillId = static_cast<int>(GW::Constants::SkillID::Finish_Him);
        thresholdFh->active = false;
        thresholds.push_back(thresholdFh);
        thresholds.back()->index = thresholds.size() - 1;

        const auto thresholdEoe = new Threshold("Edge of Extinction", Colors::RGB(0, 255, 0), 90);
        thresholdEoe->active = false;
        thresholds.push_back(thresholdEoe);
        thresholds.back()->index = thresholds.size() - 1;
    }
}

void HealthWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(click_to_print_health);
    SAVE_BOOL(hide_in_outpost);
    SAVE_FLOAT(font_size_header);
    SAVE_FLOAT(font_size_perc_value);
    SAVE_FLOAT(font_size_abs_value);

    if (thresholds_changed) {
        inifile.Reset();

        constexpr size_t buffer_size = 32;
        char buf[buffer_size];
        for (size_t i = 0; i < thresholds.size(); i++) {
            snprintf(buf, sizeof(buf), "threshold%03zu", i);
            thresholds[i]->SaveSettings(&inifile, buf);
        }

        ASSERT(inifile.SaveFile(Resources::GetSettingFile(HEALTH_THRESHOLD_INIFILENAME).c_str()) == SI_OK);
        thresholds_changed = false;
    }
}

void HealthWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::SameLine();
    ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::SameLine();
    ImGui::Checkbox("Ctrl+Click to print target health", &click_to_print_health);
    ImGui::Text("Text sizes:");
    ImGui::ShowHelp("A text size of 0 means that it's not drawn.");
    ImGui::Indent();
    ImGui::DragFloat("'Distance' header", &font_size_header, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("Percent value", &font_size_perc_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::DragFloat("Absolute value", &font_size_abs_value, 1.f, FontLoader::text_size_min, FontLoader::text_size_max);
    ImGui::Unindent();

    const bool thresholdsNode = ImGui::TreeNodeEx("Thresholds", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The first matching threshold will be used.");
    }
    if (thresholdsNode) {
        bool changed = false;
        for (size_t i = 0; i < thresholds.size(); i++) {
            Threshold* threshold = thresholds[i];

            if (!threshold) {
                continue;
            }

            ImGui::PushID(static_cast<int>(threshold->ui_id));

            auto op = Threshold::Operation::None;
            changed |= threshold->DrawSettings(op);

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

        if (ImGui::Button("Add Threshold")) {
            thresholds.push_back(new Threshold("<name>", 0xFFFFFFFF, 0));
            thresholds.back()->index = thresholds.size() - 1;
            changed = true;
        }

        ImGui::TreePop();

        if (changed) {
            thresholds_changed = true;
        }
    }
}

void HealthWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && click_to_print_health)))) {
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
            if (font_size_header > 0.f) {
                // 'health'
                ImGui::PushFont(FontLoader::GetFontByPx(font_size_header));
                ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
                ImGui::TextColored(background, "Health");
                ImGui::SetCursorPos(cur);
                ImGui::Text("Health");
                ImGui::PopFont();
            }

            // perc
            if (font_size_perc_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFontByPx(font_size_perc_value));
                cur = ImGui::GetCursorPos();
                const auto health_perc = target->hp >= 0 ? std::format("{:.0f} %%", target->hp * 100.0f) : "-";
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, health_perc.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, health_perc.c_str());
                ImGui::PopFont();
            }

            // abs
            if (font_size_abs_value > 0.f) {
                ImGui::PushFont(FontLoader::GetFontByPx(font_size_abs_value));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                const auto health_abs = target->max_hp > 0 ? std::format("{:.0f} / {}", target->hp * target->max_hp, target->max_hp) : "-";
                ImGui::TextColored(background, health_abs.c_str());
                ImGui::SetCursorPos(cur);
                ImGui::Text(health_abs.c_str());
                ImGui::PopFont();
            }

            if (click_to_print_health) {
                if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsWindowHovered()) {
                    if (target) {
                        GW::Agents::AsyncGetAgentName(target, agent_name_ping);
                        if (!agent_name_ping.empty()) {
                            const std::string agent_name_str = TextUtils::WStringToString(agent_name_ping);
                            const auto current_hp = static_cast<int>(target->hp * target->max_hp);
                            const auto message = std::format("{}'s Health is {} of {}. ({:.0f} %%)", agent_name_str.c_str(), current_hp, target->max_hp, target->hp * 100.f);
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

HealthWidget::Threshold::Threshold(const ToolboxIni* ini, const char* section)
    : ui_id(++cur_ui_id)
{
    active = ini->GetBoolValue(section, VAR_NAME(active));

    std::snprintf(name, sizeof(name), "%s", ini->GetValue(section, VAR_NAME(name), ""));
    modelId = ini->GetLongValue(section, VAR_NAME(modelId), modelId);
    skillId = ini->GetLongValue(section, VAR_NAME(skillId), skillId);
    mapId = ini->GetLongValue(section, VAR_NAME(mapId), mapId);
    value = ini->GetLongValue(section, VAR_NAME(value), value);
    color = Colors::Load(ini, section, VAR_NAME(color), color);
}

HealthWidget::Threshold::Threshold(const char* _name, const Color _color, const int _value)
    : ui_id(++cur_ui_id)
    , value(_value)
    , color(_color)
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
    ImGui::Text("%s (<%d%%) %s", name, value, Resources::GetMapName((GW::Constants::MapID)mapId)->string().c_str());
    return changed;
}

bool HealthWidget::Threshold::DrawSettings(Operation& op)
{
    bool changed = false;

    if (ImGui::TreeNodeEx("##params", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap)) {
        changed |= DrawHeader();

        ImGui::PushID(static_cast<int>(ui_id));

        changed |= ImGui::InputText("Name", name, 128);
        ImGui::ShowHelp("A name to help you remember what this is. Optional.");
        changed |= ImGui::InputInt("Model ID", &modelId);
        ImGui::ShowHelp("The Agent to which this threshold will be applied. Optional. Leave 0 for any agent");
        changed |= ImGui::InputInt("Skill ID", &skillId);
        ImGui::ShowHelp("Only apply if this skill is on your bar. Optional. Leave 0 for any skills");
        changed |= ImGui::InputInt("Map ID", &mapId);
        ImGui::ShowHelp("The map where it will be applied. Optional. Leave 0 for any map");
        changed |= ImGui::InputInt("Percentage", &value);
        ImGui::ShowHelp("Percentage below which this color should be used");
        changed |= Colors::DrawSettingHueWheel("Color", &color, 0);
        ImGui::ShowHelp("The custom color for this threshold.");

        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
        if (ImGui::Button("Move Up", ImVec2(width, 0))) {
            op = Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move the threshold up in the list");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Move Down", ImVec2(width, 0))) {
            op = Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move the threshold down in the list");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Delete", ImVec2(width, 0))) {
            op = Operation::Delete;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete the threshold");
        }

        ImGui::TreePop();
        ImGui::PopID();
    }
    else {
        changed |= DrawHeader();
    }

    return changed;
}

void HealthWidget::Threshold::SaveSettings(ToolboxIni* ini, const char* section) const
{
    ini->SetBoolValue(section, VAR_NAME(active), active);
    ini->SetValue(section, VAR_NAME(name), name);
    ini->SetLongValue(section, VAR_NAME(modelId), modelId);
    ini->SetLongValue(section, VAR_NAME(skillId), skillId);
    ini->SetLongValue(section, VAR_NAME(mapId), mapId);

    ini->SetLongValue(section, VAR_NAME(value), value);
    Colors::Save(ini, section, VAR_NAME(color), color);
}
