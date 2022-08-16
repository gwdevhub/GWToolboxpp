#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Resources.h>
#include <Windows/DialogsWindow.h>
#include <Modules/DialogModule.h>
#include <GWCA/Managers/ChatMgr.h>

namespace {
    constexpr const char* const questnames[] = {
    "UW - Chamber",
    "UW - Wastes",
    "UW - UWG",
    "UW - Mnt",
    "UW - Pits",
    "UW - Planes",
    "UW - Pools",
    "UW - Escort",
    "UW - Restore",
    "UW - Vale",
    "FoW - Defend",
    "FoW - Army Of Darkness",
    "FoW - WailingLord",
    "FoW - Griffons",
    "FoW - Slaves",
    "FoW - Restore",
    "FoW - Hunt",
    "FoW - Forgemaster",
    "FoW - Tos",
    "FoW - Toc",
    "FoW - Khobay",
    "DoA - Gloom 1: Deathbringer Company",
    "DoA - Gloom 2: The Rifts Between Us",
    "DoA - Gloom 3: To The Rescue",
    "DoA - City",
    "DoA - Veil 1: Breaching Stygian Veil",
    "DoA - Veil 2: Brood Wars",
    "DoA - Foundry 1: Foundry Of Failed Creations",
    "DoA - Foundry 2: Foundry Breakout"
    };

    std::map<const char*, std::vector<int>> dialogs_by_name = {
        {"Craft fow armor",{GW::Constants::DialogID::FowCraftArmor}},
        {"Prof Change - Warrior",{GW::Constants::DialogID::ProfChangeWarrior + 1, GW::Constants::DialogID::ProfChangeWarrior}},
        {"Prof Change - Ranger",{GW::Constants::DialogID::ProfChangeRanger + 1, GW::Constants::DialogID::ProfChangeRanger}},
        {"Prof Change - Monk",{GW::Constants::DialogID::ProfChangeMonk + 1, GW::Constants::DialogID::ProfChangeMonk}},
        {"Prof Change - Necro",{GW::Constants::DialogID::ProfChangeNecro + 1, GW::Constants::DialogID::ProfChangeNecro}},
        {"Prof Change - Mesmer",{GW::Constants::DialogID::ProfChangeMesmer + 1, GW::Constants::DialogID::ProfChangeMesmer}},
        {"Prof Change - Elementalist",{GW::Constants::DialogID::ProfChangeEle + 1, GW::Constants::DialogID::ProfChangeEle}},
        {"Prof Change - Assassin",{GW::Constants::DialogID::ProfChangeAssassin + 1, GW::Constants::DialogID::ProfChangeAssassin}},
        {"Prof Change - Ritualist",{GW::Constants::DialogID::ProfChangeRitualist + 1, GW::Constants::DialogID::ProfChangeRitualist}},
        {"Prof Change - Paragon",{GW::Constants::DialogID::ProfChangeParagon + 1, GW::Constants::DialogID::ProfChangeParagon}},
        {"Prof Change - Dervish",{GW::Constants::DialogID::ProfChangeDervish + 1, GW::Constants::DialogID::ProfChangeDervish}},
        {"Kama -> Docks @ Hahnna",{GW::Constants::DialogID::FerryKamadanToDocks}},
        {"Docks -> Kaineng @ Mhenlo",{GW::Constants::DialogID::FerryDocksToKaineng}},
        {"Docks -> LA Gate @ Mhenlo",{GW::Constants::DialogID::FerryDocksToLA}},
        {"LA Gate -> LA @ Neiro",{GW::Constants::DialogID::FerryGateToLA}},
        {"Faction mission outpost",{GW::Constants::DialogID::FactionMissionOutpost}},
        {"Nightfall mission outpost",{GW::Constants::DialogID::NightfallMissionOutpost}}
    };

    void GoNPCSendDialogs(std::initializer_list<uint32_t> dialog_ids) {
        if (!DialogModule::GetDialogAgent()) {
            if (const auto target = GW::Agents::GetTarget()) {
                GW::Agents::GoNPC(target);
            }
        }
        DialogModule::SendDialogs(dialog_ids);
    }
}


void DialogsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    auto DialogButton = [](int x_idx, int x_qty, const char* text, const char* help, uint32_t dialog_id) -> void {
        if (x_idx != 0) ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        float w = (ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x
                   - ImGui::GetStyle().ItemInnerSpacing.x * (x_qty - 1)) / x_qty;
        if (ImGui::Button(text, ImVec2(w, 0))) {
            GoNPCSendDialogs({dialog_id});

        }
        if (text != nullptr && ImGui::IsItemHovered()) {
            ImGui::SetTooltip(help);
        }
    };

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {

        if (show_common) {
            DialogButton(
                0, 2, "Four Horseman", "Take quest in Planes", QuestAcceptDialog(GW::Constants::QuestID::UW_Planes));
            DialogButton(
                1, 2, "Demon Assassin", "Take quest in Mountains", QuestAcceptDialog(GW::Constants::QuestID::UW_Mnt));
            DialogButton(0, 2, "Tower of Strength", "Take quest", QuestAcceptDialog(GW::Constants::QuestID::Fow_Tos));
            DialogButton(1, 2, "Foundry Reward", "Accept reward", QuestRewardDialog(GW::Constants::QuestID::Doa_FoundryBreakout));
            ImGui::Separator();
        }
        if (show_uwteles) {
            DialogButton(0, 4, "Lab", "Teleport Lab", GW::Constants::DialogID::UwTeleLab);
            DialogButton(1, 4, "Vale", "Teleport Vale", GW::Constants::DialogID::UwTeleVale);
            DialogButton(2, 4, "Pits", "Teleport Pits", GW::Constants::DialogID::UwTelePits);
            DialogButton(3, 4, "Pools", "Teleport Pools", GW::Constants::DialogID::UwTelePools);
            DialogButton(0, 3, "Planes", "Teleport Planes", GW::Constants::DialogID::UwTelePlanes);
            DialogButton(1, 3, "Wastes", "Teleport Wastes", GW::Constants::DialogID::UwTeleWastes);
            DialogButton(2, 3, "Mountains", "Teleport Mountains", GW::Constants::DialogID::UwTeleMnt);
            ImGui::Separator();
        }
        constexpr size_t n_quests = _countof(questnames);
        if (show_favorites) {
            for (auto i = 0; i < fav_count; ++i) {
                ImGui::PushID(i);
                ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::Combo("", &fav_index[i], questnames, n_quests);
                ImGui::PopItemWidth();
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Send", ImVec2(60.0f, 0))) {
                    GoNPCSendDialogs({
                        QuestAcceptDialog(IndexToQuestID(fav_index[i])),
                        QuestRewardDialog(IndexToQuestID(fav_index[i]))
                    });
                }
                ImGui::PopID();
            }
            ImGui::Separator();
        }
        if (show_custom) {
            static int dialogindex = 0;
            ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
            const auto current = std::next(dialogs_by_name.begin(), dialogindex);
            if (ImGui::BeginCombo("###dialogcombo", current->first)) {
                int offset = 0;
                for (auto it = dialogs_by_name.begin(); it != dialogs_by_name.end(); it++) {
                    if (ImGui::Selectable(it->first, it == current)) {
                        dialogindex = offset;
                        break;
                    }
                    offset++;
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Send##1", ImVec2(60.0f, 0))) {
                for (const auto dialog_id : current->second) {
                    DialogModule::SendDialog(dialog_id);
                }
            }

            ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::InputText("###dialoginput", customdialogbuf, _countof(customdialogbuf), ImGuiInputTextFlags_None);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("You can prefix the number by \"0x\" to specify an hexadecimal number");
            }
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Send##2", ImVec2(60.0f, 0))) {
                char buf[32];
                ASSERT(snprintf(buf, _countof(buf), "dialog %s",customdialogbuf) != -1);
                GW::Chat::SendChat('/', buf);
            }
        }
    }
    ImGui::End();
}

void DialogsWindow::DrawSettingInternal() {
    ImGui::PushItemWidth(100.0f);
    if (ImGui::InputInt("Number of favorites", &fav_count)) {
        if (fav_count < 0) fav_count = 0;
        if (fav_count > 100) fav_count = 100;
        size_t count = static_cast<size_t>(fav_count);
        fav_index.resize(count, 0);
    }
    ImGui::PopItemWidth();
    ImGui::Text("Show:");
    ImGui::StartSpacedElements(200.f * ImGui::FontScale());
    ImGui::Checkbox("Common 4", &show_common);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("UW Teles", &show_uwteles);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Favorites", &show_favorites);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Custom", &show_custom);
}

void DialogsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

    fav_count = ini->GetLongValue(Name(), VAR_NAME(fav_count), 3);
    size_t count = static_cast<size_t>(fav_count);
    fav_index.resize(count, 0);
    for (size_t i = 0; i < count; ++i) {
        char key[32];
        snprintf(key, 32, "Quest%zu", i);
        fav_index[i] = ini->GetLongValue(Name(), key, 0);
    }
    show_common = ini->GetBoolValue(Name(), VAR_NAME(show_common), show_common);
    show_uwteles = ini->GetBoolValue(Name(), VAR_NAME(show_uwteles), show_uwteles);
    show_favorites = ini->GetBoolValue(Name(), VAR_NAME(show_favorites), show_favorites);
    show_custom = ini->GetBoolValue(Name(), VAR_NAME(show_custom), show_custom);
}

void DialogsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetLongValue(Name(), "fav_count", fav_count);
    size_t count = static_cast<size_t>(fav_count);
    for (size_t i = 0; i < count; ++i) {
        char key[32];
        snprintf(key, 32, "Quest%zu", i);
        ini->SetLongValue(Name(), key, fav_index[i]);
    }
    ini->SetBoolValue(Name(), VAR_NAME(show_common), show_common);
    ini->SetBoolValue(Name(), VAR_NAME(show_uwteles), show_uwteles);
    ini->SetBoolValue(Name(), VAR_NAME(show_favorites), show_favorites);
    ini->SetBoolValue(Name(), VAR_NAME(show_custom), show_custom);
}

GW::Constants::QuestID DialogsWindow::IndexToQuestID(const int index) {
    switch (index) {
    case 0: return GW::Constants::QuestID::UW_Chamber;
    case 1: return GW::Constants::QuestID::UW_Wastes;
    case 2: return GW::Constants::QuestID::UW_UWG;
    case 3: return GW::Constants::QuestID::UW_Mnt;
    case 4: return GW::Constants::QuestID::UW_Pits;
    case 5: return GW::Constants::QuestID::UW_Planes;
    case 6: return GW::Constants::QuestID::UW_Pools;
    case 7: return GW::Constants::QuestID::UW_Escort;
    case 8: return GW::Constants::QuestID::UW_Restore;
    case 9: return GW::Constants::QuestID::UW_Vale;
    case 10: return GW::Constants::QuestID::Fow_Defend;
    case 11: return GW::Constants::QuestID::Fow_ArmyOfDarknesses;
    case 12: return GW::Constants::QuestID::Fow_WailingLord;
    case 13: return GW::Constants::QuestID::Fow_Griffons;
    case 14: return GW::Constants::QuestID::Fow_Slaves;
    case 15: return GW::Constants::QuestID::Fow_Restore;
    case 16: return GW::Constants::QuestID::Fow_Hunt;
    case 17: return GW::Constants::QuestID::Fow_Forgemaster;
    case 18: return GW::Constants::QuestID::Fow_Tos;
    case 19: return GW::Constants::QuestID::Fow_Toc;
    case 20: return GW::Constants::QuestID::Fow_Khobay;
    case 21: return GW::Constants::QuestID::Doa_DeathbringerCompany;
    case 22: return GW::Constants::QuestID::Doa_RiftBetweenUs;
    case 23: return GW::Constants::QuestID::Doa_ToTheRescue;
    case 24: return GW::Constants::QuestID::Doa_City;
    case 25: return GW::Constants::QuestID::Doa_BreachingStygianVeil;
    case 26: return GW::Constants::QuestID::Doa_BroodWars;
    case 27: return GW::Constants::QuestID::Doa_FoundryOfFailedCreations;
    case 28: return GW::Constants::QuestID::Doa_FoundryBreakout;
    default: return GW::Constants::QuestID::None;
    }
}

uint32_t DialogsWindow::IndexToDialogID(const int index) {
    switch (index) {
    case 0: return GW::Constants::DialogID::FowCraftArmor;
    case 1: return GW::Constants::DialogID::ProfChangeWarrior;
    case 2: return GW::Constants::DialogID::ProfChangeRanger;
    case 3: return GW::Constants::DialogID::ProfChangeMonk;
    case 4: return GW::Constants::DialogID::ProfChangeNecro;
    case 5: return GW::Constants::DialogID::ProfChangeMesmer;
    case 6: return GW::Constants::DialogID::ProfChangeEle;
    case 7: return GW::Constants::DialogID::ProfChangeAssassin;
    case 8: return GW::Constants::DialogID::ProfChangeRitualist;
    case 9: return GW::Constants::DialogID::ProfChangeParagon;
    case 10: return GW::Constants::DialogID::ProfChangeDervish;
    case 11: return GW::Constants::DialogID::FerryKamadanToDocks;
    case 12: return GW::Constants::DialogID::FerryDocksToKaineng;
    case 13: return GW::Constants::DialogID::FerryDocksToLA;
    case 14: return GW::Constants::DialogID::FerryGateToLA;
    case 15: return GW::Constants::DialogID::FactionMissionOutpost;
    case 16: return GW::Constants::DialogID::NightfallMissionOutpost;
    default: return 0;
    }
}
