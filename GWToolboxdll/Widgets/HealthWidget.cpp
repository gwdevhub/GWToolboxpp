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

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Widgets/HealthWidget.h>

constexpr const wchar_t* HEALTH_THRESHOLD_INIFILENAME = L"HealthThreshold.ini";

void HealthWidget::LoadSettings(ToolboxIni *ini) {
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(click_to_print_health);
    LOAD_BOOL(hide_in_outpost);
    LOAD_BOOL(show_abs_value);
    LOAD_BOOL(show_perc_value);

    if (inifile == nullptr) inifile = new ToolboxIni();
    ASSERT(inifile->LoadIfExists(Resources::GetPath(HEALTH_THRESHOLD_INIFILENAME)) == SI_OK);

    ToolboxIni::TNamesDepend entries;
    inifile->GetAllSections(entries);

    for (const ToolboxIni::Entry& entry : entries) {
        Threshold* threshold = new Threshold(inifile, entry.pItem);
        threshold->index = thresholds.size();
        thresholds.push_back(threshold);
    }

    if (thresholds.empty()) {
        Threshold* thresholdFh = new Threshold("\"Finish Him!\"", Colors::RGB(255, 255, 0), 50);
        thresholdFh->skillId = static_cast<int>(GW::Constants::SkillID::Finish_Him);
        thresholdFh->active = false;
        thresholds.push_back(thresholdFh);
        thresholds.back()->index = thresholds.size() - 1;

        Threshold* thresholdEoe = new Threshold("Edge of Extinction", Colors::RGB(0, 255, 0), 90);
        thresholdEoe->active = false;
        thresholds.push_back(thresholdEoe);
        thresholds.back()->index = thresholds.size() - 1;
    }
}

void HealthWidget::SaveSettings(ToolboxIni *ini) {
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(click_to_print_health);
    SAVE_BOOL(hide_in_outpost);
    SAVE_BOOL(show_abs_value);
    SAVE_BOOL(show_perc_value);

    if (thresholds_changed && inifile) {
        inifile->Reset();

        constexpr size_t buffer_size = 32;
        char buf[buffer_size];
        for (size_t i = 0; i < thresholds.size(); ++i) {
            snprintf(buf, sizeof(buf), "threshold%03zu", i);
            thresholds[i]->SaveSettings(inifile, buf);
        }

        ASSERT(inifile->SaveFile(Resources::GetPath(HEALTH_THRESHOLD_INIFILENAME).c_str()) == SI_OK);
        thresholds_changed = false;
    }
}

void HealthWidget::DrawSettingInternal() {
    ToolboxWidget::DrawSettingInternal();
    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::SameLine(); ImGui::Checkbox("Show absolute value", &show_abs_value);
    ImGui::SameLine(); ImGui::Checkbox("Show percentage value", &show_perc_value);
    ImGui::Checkbox("Ctrl+Click to print target health", &click_to_print_health);

    const bool thresholdsNode = ImGui::TreeNodeEx("Thresholds", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("The first matching threshold will be used.");
    if (thresholdsNode) {
        bool changed = false;
        for (size_t i = 0; i < thresholds.size(); ++i) {
            Threshold* threshold = thresholds[i];

            if (!threshold) continue;

            ImGui::PushID(static_cast<int>(threshold->ui_id));

            Threshold::Operation op = Threshold::Operation::None;
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

void HealthWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && click_to_print_health)))) {
        constexpr size_t buffer_size = 32;
        static char health_perc[buffer_size];
        static char health_abs[buffer_size];
        const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
        if (target) {
            if (show_perc_value) {
                if (target->hp >= 0) {
                    snprintf(health_perc, buffer_size, "%.0f %%", target->hp * 100.0f);
                } else {
                    snprintf(health_perc, buffer_size, "-");
                }
            }
            if (show_abs_value) {
                if (target->max_hp > 0) {
                    float abs = target->hp * target->max_hp;
                    snprintf(health_abs, buffer_size, "%.0f / %d", abs, target->max_hp);
                } else {
                    snprintf(health_abs, buffer_size, "-");
                }
            }

            ImVec2 cur;
            ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImColor background = ImColor(Colors::Black());

            for (size_t i = 0; i < thresholds.size(); ++i) {
                Threshold* threshold = thresholds[i];

                if (!threshold) continue;
                if (!threshold->active) continue;
                if (threshold->modelId && threshold->modelId != target->player_number) continue;
                if (threshold->skillId) {
                    GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
                    if (!(skillbar && skillbar->IsValid()))
                        continue;
                    GW::SkillbarSkill* skill = skillbar->GetSkillById(static_cast<GW::Constants::SkillID>(threshold->skillId));
                    if (!skill) continue;
                }
                if (threshold->mapId) {
                    if (static_cast<GW::Constants::MapID>(threshold->mapId) != GW::Map::GetMapID()) continue;
                }

                if (target->hp * 100 < threshold->value) {
                    color = ImColor(threshold->color);
                    break;
                }
            }

            // 'health'
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
            cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
            ImGui::TextColored(background, "Health");
            ImGui::SetCursorPos(cur);
            ImGui::Text("Health");
            ImGui::PopFont();

            // perc
            if (show_perc_value) {
                ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_small));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, "%s", health_perc);
                ImGui::SetCursorPos(cur);
                ImGui::TextColored(color, "%s", health_perc);
                ImGui::PopFont();
            }

            // abs
            if (show_abs_value) {
                ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_label));
                cur = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
                ImGui::TextColored(background, health_abs);
                ImGui::SetCursorPos(cur);
                ImGui::Text(health_abs);
                ImGui::PopFont();
            }

            if (click_to_print_health) {
                if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsWindowHovered()) {
                    if (target) {
                        GW::Agents::AsyncGetAgentName(target, agent_name_ping);
                        if (agent_name_ping.size()) {
                            char buffer[512];
                            const std::string agent_name_str = GuiUtils::WStringToString(agent_name_ping);
                            const int current_hp = (int)(target->hp * target->max_hp);
                            snprintf(buffer, sizeof(buffer), "%s's Health is %d of %d. (%.0f %%)", agent_name_str.c_str(), current_hp, target->max_hp, target->hp * 100.f);
                            GW::Chat::SendChat('#', buffer);
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

HealthWidget::Threshold::Threshold(ToolboxIni* ini, const char* section) : ui_id(++cur_ui_id) {
    active = ini->GetBoolValue(section, VAR_NAME(active));
    GuiUtils::StrCopy(name, ini->GetValue(section, VAR_NAME(name), ""), sizeof(name));
    modelId = ini->GetLongValue(section, VAR_NAME(modelId), modelId);
    skillId = ini->GetLongValue(section, VAR_NAME(skillId), skillId);
    mapId = ini->GetLongValue(section, VAR_NAME(mapId), mapId);
    value = ini->GetLongValue(section, VAR_NAME(value), value);
    color = Colors::Load(ini, section, VAR_NAME(color), color);
}

HealthWidget::Threshold::Threshold(const char* _name, Color _color, int _value)
    : ui_id(++cur_ui_id)
    , value(_value)
    , color(_color)
{
    GuiUtils::StrCopy(name, _name, sizeof(name));
}

bool HealthWidget::Threshold::DrawHeader() {
    constexpr size_t buffer_size = 64;
    char mapbuf[buffer_size] = {'\0'};
    if (mapId) {
        if (mapId < sizeof(GW::Constants::NAME_FROM_ID) / sizeof(*GW::Constants::NAME_FROM_ID))
            snprintf(mapbuf, buffer_size, "[%s]", GW::Constants::NAME_FROM_ID[mapId]);
        else
            snprintf(mapbuf, buffer_size, "[Map %d]", mapId);
    }

    ImGui::SameLine(0, 18);
    bool changed = ImGui::Checkbox("##active", &active);
    ImGui::SameLine();
    ImGui::ColorButton("", ImColor(color));
    ImGui::SameLine();
    ImGui::Text("%s (<%d%%) %s", name, value, mapbuf);
    return changed;
}

bool HealthWidget::Threshold::DrawSettings(Operation& op) {
    bool changed = false;

    if (ImGui::TreeNodeEx("##params", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap)) {
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
            op = Threshold::Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the threshold up in the list");
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Move Down", ImVec2(width, 0))) {
            op = Threshold::Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the threshold down in the list");
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Delete", ImVec2(width, 0))) {
            op = Threshold::Operation::Delete;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the threshold");

        ImGui::TreePop();
        ImGui::PopID();
    } else {
        changed |= DrawHeader();
    }

    return changed;
}

void HealthWidget::Threshold::SaveSettings(ToolboxIni* ini, const char* section) {
    ini->SetBoolValue(section, VAR_NAME(active), active);
    ini->SetValue(section, VAR_NAME(name), name);
    ini->SetLongValue(section, VAR_NAME(modelId), modelId);
    ini->SetLongValue(section, VAR_NAME(skillId), skillId);
    ini->SetLongValue(section, VAR_NAME(mapId), mapId);

    ini->SetLongValue(section, VAR_NAME(value), value);
    Colors::Save(ini, section, VAR_NAME(color), color);
}
