#include "stdafx.h"
#include "HealthWidget.h"

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include "GuiUtils.h"
#include "Modules/ToolboxSettings.h"
#include <Modules/Resources.h>

#define HEALTH_THRESHOLD_INIFILENAME L"HealthThreshold.ini"

void HealthWidget::LoadSettings(CSimpleIni *ini) {
	ToolboxWidget::LoadSettings(ini);
	click_to_print_health = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_health), click_to_print_health);
	hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);

	if (inifile == nullptr) inifile = new CSimpleIni();
	inifile->LoadFile(Resources::GetPath(HEALTH_THRESHOLD_INIFILENAME).c_str());

	CSimpleIni::TNamesDepend entries;
	inifile->GetAllSections(entries);

	for (const CSimpleIni::Entry& entry : entries) {
		Threshold* threshold = new Threshold(inifile, entry.pItem);
		threshold->index = thresholds.size();
		thresholds.push_back(threshold);
	}

	if (thresholds.empty()) {
		Threshold* thresholdFh = new Threshold("\"Finish Him!\"", 50);
		thresholdFh->active = false;
		thresholdFh->color = Colors::RGB(255, 255, 0);
		thresholds.push_back(thresholdFh);
		thresholds.back()->index = thresholds.size() - 1;

		Threshold* thresholdEoe = new Threshold("Edge of Extinction", 90);
		thresholdEoe->active = false;
		thresholdEoe->color = Colors::RGB(0, 255, 0);
		thresholds.push_back(thresholdEoe);
		thresholds.back()->index = thresholds.size() - 1;
	}
}

void HealthWidget::SaveSettings(CSimpleIni *ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_print_health), click_to_print_health);
	ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);

	if (thresholds_changed && inifile) {
		inifile->Reset();

		char buf[256];
		for (size_t i = 0; i < thresholds.size(); ++i) {
			snprintf(buf, 256, "threshold%03d", i);
			thresholds[i]->SaveSettings(inifile, buf);
		}

		inifile->SaveFile(Resources::GetPath(HEALTH_THRESHOLD_INIFILENAME).c_str());
		thresholds_changed = false;
	}
}

void HealthWidget::DrawSettingInternal() {
	ToolboxWidget::DrawSettingInternal();
	ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
	ImGui::Checkbox("Ctrl+Click to print target health", &click_to_print_health);

	if (ImGui::TreeNode("Thresholds")) {
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The first matching threshold will be used.");
		bool changed = false;
		for (size_t i = 0; i < thresholds.size(); ++i) {
			Threshold* threshold = thresholds[i];

			if (!threshold) continue;

			ImGui::PushID(threshold->ui_id);

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
					thresholds.erase(thresholds.begin() + i);
					delete threshold;
					--i;
					break;
			}

			ImGui::PopID();
		}

		if (ImGui::Button("Add Threshold")) {
			thresholds.push_back(new Threshold("<name>", 0));
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
	if (!visible) return;
	if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
		return;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(150, 100), ImGuiSetCond_FirstUseEver);
    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(ctrl_pressed && click_to_print_health)))) {
		static char health_perc[32];
		static char health_abs[32];
		GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
		if (target) {
			if (target->hp >= 0) {
				snprintf(health_perc, 32, "%.0f %s", target->hp * 100, "%%");
			} else {
				snprintf(health_perc, 32, "-");
			}
			if (target->max_hp > 0) {
				float abs = target->hp * target->max_hp;
				snprintf(health_abs, 32, "%.0f / %d", abs, target->max_hp);
			} else {
				snprintf(health_abs, 32, "-");
			}

			ImVec2 cur;

			Color color = Colors::RGB(255, 255, 255);
			Color background = Colors::RGB(0, 0, 0);

			for (size_t i = 0; i < thresholds.size(); ++i) {
				Threshold* threshold = thresholds[i];

				if (!threshold) continue;
				if (!threshold->active) continue;
				if (threshold->modelId && threshold->modelId != target->player_number) continue;
				if (threshold->mapId) {
					if (static_cast<GW::Constants::MapID>(threshold->mapId) != GW::Map::GetMapID()) continue;
				}

				if (target->hp * 100 < threshold->value) {
					color = threshold->color;
					background = threshold->background;
					break;
				}
			}

			// 'health'
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(background), "Health");
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), "Health");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(background), health_perc);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), health_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(background), health_abs);
			ImGui::SetCursorPos(cur);
			ImGui::TextColored(ImColor(color), health_abs);
			ImGui::PopFont();

            if (click_to_print_health) {
			    if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringWindow()) {
                    if (target) {
                        GW::Agents::AsyncGetAgentName(target, agent_name_ping);
                        if (agent_name_ping.size()) {
                            char buffer[512];
                            int current_hp = (int)(target->hp * target->max_hp);
                            snprintf(buffer, sizeof(buffer), "%S's Health is %d of %d. (%.0f %%)", agent_name_ping.c_str(), current_hp, target->max_hp, target->hp * 100.f);
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

HealthWidget::Threshold::Threshold(CSimpleIni* ini, const char* section) : ui_id(++cur_ui_id) {
	active = ini->GetBoolValue(section, VAR_NAME(active));
	GuiUtils::StrCopy(name, ini->GetValue(section, VAR_NAME(name), ""), sizeof(name));
	modelId = ini->GetLongValue(section, VAR_NAME(modelId), 0);
	mapId = ini->GetLongValue(section, VAR_NAME(mapId), 0);
	value = ini->GetLongValue(section, VAR_NAME(value), 0);
	color = Colors::Load(ini, section, VAR_NAME(color), 0xFFFFFFFF);
	background = Colors::Load(ini, section, VAR_NAME(background), 0xFF000000);
}

HealthWidget::Threshold::Threshold(const char* _name, int _value) : ui_id(++cur_ui_id) {
	GuiUtils::StrCopy(name, _name, sizeof(name));
	value = _value;
}

bool HealthWidget::Threshold::DrawHeader() {
	char mapbuf[64] = { '\0' };
	if (mapId) {
		if (mapId < sizeof(GW::Constants::NAME_FROM_ID) - 1)
			snprintf(mapbuf, 64, "[%s]", GW::Constants::NAME_FROM_ID[mapId]);
		else
			snprintf(mapbuf, 64, "[Map %d]", mapId);
	}

	ImGui::SameLine(0, 18);
	bool changed = ImGui::Checkbox("##active", &active);
	ImGui::SameLine();
	ImGui::ColorButton("", ImColor(color));
	ImGui::SameLine();
	ImGui::ColorButton("", ImColor(background));
	ImGui::SameLine();
	ImGui::Text("%s (<%d%%) %s", name, value, mapbuf);
	return changed;
}

bool HealthWidget::Threshold::DrawSettings(Operation& op) {
	bool changed = false;

	if (ImGui::TreeNode("##params")) {
		changed |= DrawHeader();

		ImGui::PushID(ui_id);

		if (ImGui::InputText("Name", name, 128)) changed = true;
		ImGui::ShowHelp("A name to help you remember what this is. Optional.");
		if (ImGui::InputInt("Model ID", &modelId)) changed = true;
		ImGui::ShowHelp("The Agent to which this threshold will be applied. Optional. Leave 0 for any agent");
		if (ImGui::InputInt("Map ID", &mapId)) changed = true;
		ImGui::ShowHelp("The map where it will be applied. Optional. Leave 0 for any map");
		if (ImGui::InputInt("Percentage", &value)) changed = true;
		ImGui::ShowHelp("Percentage below which this color should be used");
		if (Colors::DrawSetting("Color", &color)) changed = true;
		ImGui::ShowHelp("The custom color for this threshold.");
		if (Colors::DrawSetting("Background", &background)) changed = true;
		ImGui::ShowHelp("The custom background color for this threshold.");

		float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
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

void HealthWidget::Threshold::SaveSettings(CSimpleIni* ini, const char* section) {
	ini->SetBoolValue(section, VAR_NAME(active), active);
	ini->SetValue(section, VAR_NAME(name), name);
	ini->SetLongValue(section, VAR_NAME(modelId), modelId);
	ini->SetLongValue(section, VAR_NAME(mapId), mapId);

	ini->SetLongValue(section, VAR_NAME(value), value);
	Colors::Save(ini, section, VAR_NAME(color), color);
	Colors::Save(ini, section, VAR_NAME(background), background);
}