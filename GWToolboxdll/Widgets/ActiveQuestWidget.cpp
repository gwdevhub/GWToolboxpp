#include "stdafx.h"

#include <Widgets/ActiveQuestWidget.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/GuiUtils.h>

#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/Opcodes.h>

namespace {
    constexpr uint32_t OBJECTIVE_FLAG_BULLET = 0x1;
    constexpr uint32_t OBJECTIVE_FLAG_COMPLETED = 0x2;
    constexpr uint32_t QUEST_MARKER_FILE_ID = 0x1b4d5;

    constexpr ImU32 TEXT_COLOR_COMPLETED = 0xffbbbbbb;

    GW::HookEntry hook_entry;
    GW::Constants::QuestID active_quest_id;
    GuiUtils::EncString active_quest_name;
    std::vector<std::tuple<int, std::string, bool>> active_quest_objectives; // (index, objective, completed)
    IDirect3DTexture9** p_quest_marker_texture;
    bool is_loading_quest_objectives = false;
    bool force_update = false;

    void SetForceUpdate(GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
        force_update = true;
    }

    void __cdecl OnQuestObjectivesDecoded(void*, wchar_t* decoded) {
        std::wstring decoded_objectives = decoded;
        static const std::wregex SANITIZE_REGEX(L"<[^>]+>");
        static const std::wregex OBJECTIVE_REGEX(L"\\{s(c?)\\}([^\\{]+)");

        decoded_objectives = std::regex_replace(decoded_objectives, SANITIZE_REGEX, L"");

        int index = 0;

        auto regex_begin = std::wsregex_iterator(decoded_objectives.begin(), decoded_objectives.end(), OBJECTIVE_REGEX);
        auto regex_end = std::wsregex_iterator();
        for(auto& it = regex_begin; it != regex_end; it++) {
            const std::wsmatch& matches = *it;

            bool completed = (matches[1].compare(L"c") == 0);
            std::wstring obj = matches[2].str();

            active_quest_objectives.emplace_back(index++, GuiUtils::WStringToString(obj), completed);
        }
    }

    void __cdecl OnMissionObjectiveDecoded(void* param, wchar_t* decoded) {
        static std::mutex mutex;
        auto* param_tuple = reinterpret_cast<std::tuple<int, bool>*>(param);
        auto& [index, completed] = *param_tuple;

        std::wstring decoded_objective = decoded;
        static const std::wregex SANITIZE_REGEX(L"<[^>]+>");
        decoded_objective = std::regex_replace(decoded_objective, SANITIZE_REGEX, L"");

        std::lock_guard g(mutex);
        for(auto it = active_quest_objectives.cbegin(); it != active_quest_objectives.cend(); it++) {
            const auto& [ix, _a, _b] = *it;
            if (ix >= index) {
                active_quest_objectives.emplace(it, index, GuiUtils::WStringToString(decoded_objective), completed);
                delete param_tuple;
                return;
            }
        }

        active_quest_objectives.emplace_back(index, GuiUtils::WStringToString(decoded_objective), completed);

        delete param_tuple;
    }

    void DrawQuestIcon() {
        static const ImVec2 UV0 = ImVec2(0.0F, 0.0f);
        static const ImVec2 ICON_SIZE = ImVec2(24.0f, 24.0f);
        if(!p_quest_marker_texture || !*p_quest_marker_texture) {
            return;
        }

        ImGui::PushID("quest_icon");
        auto uv1 = ImGui::CalculateUvCrop(*p_quest_marker_texture, ICON_SIZE);
        ImGui::Image(*p_quest_marker_texture, ICON_SIZE, UV0, uv1);
        ImGui::PopID();
    }
}

void ActiveQuestWidget::Initialize() {
    ToolboxWidget::Initialize();

    p_quest_marker_texture = GwDatTextureModule::LoadTextureFromFileId(QUEST_MARKER_FILE_ID);

    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_QUEST_DESCRIPTION, SetForceUpdate, 1);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_QUEST_UPDATE_NAME, SetForceUpdate, 1);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_MISSION_OBJECTIVE_ADD, SetForceUpdate, 1);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_MISSION_OBJECTIVE_COMPLETE, SetForceUpdate, 1);
    GW::StoC::RegisterPacketCallback(&hook_entry, GAME_SMSG_MISSION_OBJECTIVE_UPDATE_STRING, SetForceUpdate, 1);
}

void ActiveQuestWidget::Terminate() {
    GW::StoC::RemoveCallbacks(&hook_entry);

    ToolboxWidget::Terminate();
}

void ActiveQuestWidget::Update(float)
{
    GW::Constants::QuestID qid = GW::QuestMgr::GetActiveQuestId();

    if (qid != active_quest_id || force_update) {
        force_update = false;
        active_quest_id = qid;

        GW::Quest* quest = GW::QuestMgr::GetQuest(qid);
        if (quest) {
            active_quest_name.reset(quest->name);
            active_quest_objectives.clear();

            if(quest->objectives) {
                wchar_t* objectives = quest->objectives;
                GW::GameThread::Enqueue([objectives] {
                    GW::UI::AsyncDecodeStr(objectives, OnQuestObjectivesDecoded);
                });
                is_loading_quest_objectives = false;
            }
            else {
                is_loading_quest_objectives = true;
                GW::QuestMgr::RequestQuestInfo(quest);
            }
        }
        else if(static_cast<int32_t>(qid) == -1) {
            // Mission objectives
            GW::WorldContext* worldContext = GW::GetWorldContext();
            GW::AreaInfo* areaInfo = GW::Map::GetCurrentMapInfo();
            active_quest_name.reset(areaInfo->name_id);

            active_quest_objectives.clear();

            int index = 0;

            for (const auto& objective : worldContext->mission_objectives) {
                bool objective_completed = (objective.type & OBJECTIVE_FLAG_COMPLETED) != 0;
                wchar_t* objective_text = objective.enc_str;
                if (objective.type & OBJECTIVE_FLAG_BULLET) {
                    GW::GameThread::Enqueue([objective_text, index, objective_completed] {
                        auto* param = new std::tuple<int,bool>(index, objective_completed);
                        GW::UI::AsyncDecodeStr(objective_text, OnMissionObjectiveDecoded, reinterpret_cast<void*>(param));
                    });
                }
                index++;
            }
        }
        else {
            active_quest_name.reset(L"\x108\x107No active quest\x1");
            active_quest_objectives.clear();
        }
    }

    if (is_loading_quest_objectives) {
        GW::Quest* quest = GW::QuestMgr::GetQuest(qid);
        if(quest->objectives) {
            wchar_t* objectives = quest->objectives;
            is_loading_quest_objectives = false;

            GW::GameThread::Enqueue([objectives] {
                GW::UI::AsyncDecodeStr(objectives, OnQuestObjectivesDecoded);
            });
        }
    }
}

void ActiveQuestWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
        DrawQuestIcon();

        ImGui::SameLine();

        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_label));
        ImGui::TextColored(ImColor(0, 255, 0), "%s", active_quest_name.string().c_str());
        ImGui::PopFont();

        ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - cursor.x);
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header2));
        for (const auto& objective : active_quest_objectives) {
            auto& [_, obj_str, completed] = objective;

            if(completed) {
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
            }
            ImGui::Bullet();
            ImGui::Text("%s", obj_str.c_str());
            if(completed) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopFont();
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}
