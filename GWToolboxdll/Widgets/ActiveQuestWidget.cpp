#include "stdafx.h"

#include <Widgets/ActiveQuestWidget.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/ToolboxSettings.h>
#include <Utils/GuiUtils.h>

#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/UIMgr.h>

namespace {
    struct CompletionState {
        int index;
        bool completed;
    };
    constexpr uint32_t OBJECTIVE_FLAG_BULLET = 0x1;
    constexpr uint32_t OBJECTIVE_FLAG_COMPLETED = 0x2;
    constexpr uint32_t QUEST_MARKER_FILE_ID = 0x1b4d5;

    constexpr ImU32 TEXT_COLOR_COMPLETED = 0xffbbbbbb;
    constexpr ImU32 TEXT_COLOR_ACTIVE = 0xff00ff00;

    GW::HookEntry hook_entry;
    GW::Constants::QuestID active_quest_id = (GW::Constants::QuestID)0;
    GuiUtils::EncString active_quest_name;
    std::vector<std::tuple<int, std::string, bool>> active_quest_objectives; // (index, objective, completed)
    IDirect3DTexture9** p_quest_marker_texture;
    bool is_loading_quest_objectives = false;
    bool force_update = false;

    void SetForceUpdate(GW::HookStatus*, GW::UI::UIMessage,void*,void*) {
        force_update = true;
    }

    void __cdecl OnQuestObjectivesDecoded(void*, const wchar_t* decoded) {
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

    void __cdecl OnMissionObjectiveDecoded(void* param, const wchar_t* decoded) {
        static std::mutex mutex;
        auto* completion_state = reinterpret_cast<CompletionState*>(param);
        auto& [index, completed] = *completion_state;

        std::wstring decoded_objective = decoded;
        static const std::wregex SANITIZE_REGEX(L"<[^>]+>");
        decoded_objective = std::regex_replace(decoded_objective, SANITIZE_REGEX, L"");

        std::lock_guard g(mutex);
        for(auto it = active_quest_objectives.cbegin(); it != active_quest_objectives.cend(); it++) {
            const auto& [ix, _a, _b] = *it;
            if (ix >= index) {
                active_quest_objectives.emplace(it, index, GuiUtils::WStringToString(decoded_objective), completed);
                delete completion_state;
                return;
            }
        }

        active_quest_objectives.emplace_back(index, GuiUtils::WStringToString(decoded_objective), completed);

        delete completion_state;
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

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestDetailsChanged,
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kClientActiveQuestChanged,
        GW::UI::UIMessage::kObjectiveComplete,
        GW::UI::UIMessage::kObjectiveAdd,
        GW::UI::UIMessage::kObjectiveUpdated
    };
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&hook_entry, message_id, SetForceUpdate);
    }
}

void ActiveQuestWidget::Terminate() {
    GW::UI::RemoveUIMessageCallback(&hook_entry);

    ToolboxWidget::Terminate();
}

void ActiveQuestWidget::Update(float)
{
    const auto qid = GW::QuestMgr::GetActiveQuestId();

    if (qid != active_quest_id || force_update) {
        force_update = false;
        active_quest_id = qid;

        const auto quest = GW::QuestMgr::GetQuest(qid);
        if (quest) {
            active_quest_name.reset(quest->name);
            active_quest_objectives.clear();

            if(quest->objectives) {
                GW::UI::AsyncDecodeStr(quest->objectives, OnQuestObjectivesDecoded);
                is_loading_quest_objectives = false;
            }
            else {
                is_loading_quest_objectives = true;
                GW::QuestMgr::RequestQuestInfo(quest);
            }
        }
        else if (static_cast<int32_t>(qid) == -1) {
            // Mission objectives
            const auto worldContext = GW::GetWorldContext();
            const auto areaInfo = GW::Map::GetCurrentMapInfo();
            active_quest_name.reset(areaInfo && areaInfo->name_id ? areaInfo->name_id : 3);

            active_quest_objectives.clear();

            int index = 0;

            for (const auto& objective : worldContext->mission_objectives) {
                if (!objective.enc_str)
                    continue;
                bool objective_completed = (objective.type & OBJECTIVE_FLAG_COMPLETED) != 0;
                if (objective.type & OBJECTIVE_FLAG_BULLET) {
                    auto* param = new CompletionState(index, objective_completed);
                    GW::UI::AsyncDecodeStr(objective.enc_str, OnMissionObjectiveDecoded, param);
                }
                index++;
            }
        }
        else {
            active_quest_name.reset(1);
            active_quest_objectives.clear();
        }
    }

    if (is_loading_quest_objectives) {
        const auto quest = GW::QuestMgr::GetQuest(qid);
        if(quest && quest->objectives) {
            is_loading_quest_objectives = false;
            GW::UI::AsyncDecodeStr(quest->objectives, OnQuestObjectivesDecoded);
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
        ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - cursor.x);
        DrawQuestIcon();

        ImGui::SameLine();

        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_ACTIVE);
        ImGui::Text("%s", active_quest_name.string().c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header2));
        for (const auto& objective : active_quest_objectives) {
            auto& [_, obj_str, completed] = objective;

            if(completed) {
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
            }
            ImGui::Bullet();
            ImGui::Text("%s", obj_str.data());
            if(completed) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopFont();
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}
