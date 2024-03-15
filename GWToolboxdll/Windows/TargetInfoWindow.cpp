#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Modules/Resources.h>

#include <Windows/TargetInfoWindow.h>
#include <Utils/GuiUtils.h>

namespace {

    std::map<GW::Constants::SkillID, GuiUtils::EncString*> skill_names_by_id;
    std::unordered_map<std::string, GW::Constants::SkillID> skill_ids_by_name;


    struct AgentInfo {
        GuiUtils::EncString name;
        std::string wiki_content;
        std::string wiki_search_term;
        std::vector<GW::Constants::SkillID> wiki_skills;
        std::unordered_map<std::string, std::string> wiki_armor_ratings; // rating type, rating value
        enum class TargetInfoState {
            DecodingName,
            FetchingWikiPage,
            ParsingWikiPage,
            Done
        } state;
        AgentInfo(const wchar_t* _name) {
            state = TargetInfoState::DecodingName;
            name.language(GW::Constants::Language::English);
            name.reset(_name);
            name.wstring();
        }
        static void OnFetchedWikiPage(bool success, const std::string& response, void* context) {
            ASSERT(context);
            auto ctx_agent_info = (AgentInfo*)context;
            if (!success) {
                Log::Error(response.c_str());
                ctx_agent_info->state = TargetInfoState::Done;
            }
            else {
                ctx_agent_info->wiki_content = response;
                ctx_agent_info->state = TargetInfoState::ParsingWikiPage;
            }
        }
    };

    std::unordered_map<std::wstring,AgentInfo*> agent_info_by_name;

    GW::Agent* info_target = 0;
    AgentInfo* current_agent_info = nullptr;

    GW::HookEntry ui_message_entry;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        const auto current_target = GW::Agents::GetTarget();
        if (info_target == current_target)
            return;
        info_target = current_target;
        current_agent_info = nullptr;
        if (!(info_target && info_target->GetIsLivingType() && info_target->GetAsAgentLiving()->IsNPC()))
            return;
        const auto name = GW::Agents::GetAgentEncName(info_target);
        if (!(name && *name))
            return;
        if (agent_info_by_name.find(name) == agent_info_by_name.end()) {
            const auto agent_info = new AgentInfo(name);
            agent_info_by_name[name] = agent_info;
        }
        current_agent_info = agent_info_by_name[name];
    }
    bool SkillNamesDecoded() {
        return !skill_ids_by_name.empty();
    }
    void DecodeSkillNames() {
        if (SkillNamesDecoded())
            return;
        if (skill_names_by_id.empty()) {
            std::map<uint32_t, GuiUtils::EncString*> skill_ids_by_name_id;
            for (size_t i = 0; i < (size_t)GW::Constants::SkillID::Count; i++) {
                const auto skill_data = GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)i);
                if (!(skill_data && skill_data->name && skill_ids_by_name_id.find(skill_data->name) == skill_ids_by_name_id.end()))
                    continue;
                auto enc = new GuiUtils::EncString();
                enc->language(GW::Constants::Language::English);
                enc->reset(skill_data->name);
                enc->wstring();
                skill_ids_by_name_id[skill_data->name] = enc;
                skill_names_by_id[(GW::Constants::SkillID)i] = enc;
            }
        }
        for (auto it : skill_names_by_id) {
            if (it.second->IsDecoding())
                return;
        }
        for (auto it : skill_names_by_id) {
            skill_ids_by_name[it.second->string()] = it.first;
            delete it.second;
        }
        skill_names_by_id.clear();
    }


    void UpdateAgentInfos() {
        if (!SkillNamesDecoded())
            return;
        for (auto& it : agent_info_by_name) {
            auto agent_info = it.second;
            const auto state = agent_info->state;
            switch (state) {
            case AgentInfo::TargetInfoState::DecodingName: {
                if (agent_info->name.IsDecoding())
                    break;
                if (agent_info->name.string().empty()) {
                    agent_info->state = AgentInfo::TargetInfoState::Done;
                    break;
                }
                agent_info->state = AgentInfo::TargetInfoState::FetchingWikiPage;

                agent_info->wiki_search_term = GuiUtils::WStringToString(
                    GuiUtils::SanitizePlayerName(agent_info->name.wstring())
                );  
                std::string wiki_url = "https://wiki.guildwars.com/wiki/?search=";
                wiki_url.append(GuiUtils::UrlEncode(agent_info->wiki_search_term, '_'));
                Resources::Download(wiki_url, AgentInfo::OnFetchedWikiPage, agent_info);
            } break;
            case AgentInfo::TargetInfoState::ParsingWikiPage:
                Log::InfoW(L"Got wiki page for %ls, need to write the code to parse!!", agent_info->name.wstring().c_str());

                const std::regex skill_list_regex("<h2><span class=\"mw-headline\" id=\"Skills\">([\\s\\S]*)</ul>");
                std::smatch m;
                if (std::regex_search(agent_info->wiki_content, m, skill_list_regex)) {
                    std::string skill_list_found = m[1].str();

                    // Iterate over all skills in this list.
                    const auto skill_link_regex = std::regex("<a href=\"[^\"]+\" title=\"([^\"]+)\"");
                    auto words_begin = std::sregex_iterator(skill_list_found.begin(), skill_list_found.end(), skill_link_regex);
                    auto words_end = std::sregex_iterator();
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        const auto skill_name = i->str(1);
                        const auto found = skill_ids_by_name.find(skill_name);
                        if (found == skill_ids_by_name.end())
                            continue;
                        if (std::ranges::find(agent_info->wiki_skills, found->second) != agent_info->wiki_skills.end())
                            continue;
                        agent_info->wiki_skills.push_back(found->second);
                    }
                }

                const std::regex armor_ratings_regex("<h2><span class=\"mw-headline\" id=\"Armor_ratings\">([\\s\\S]*)</table>");
                if (std::regex_search(agent_info->wiki_content, m, armor_ratings_regex)) {
                    std::string armor_table_found = m[1].str();

                    // Iterate over all skills in this list.
                    const auto armor_cell_regex = std::regex("<td>[\\s\\S]*title=\"([^\"]+)\"[\\s\\S]*<td>([0-9 \\(\\)]+)</td>");
                    auto words_begin = std::sregex_iterator(armor_table_found.begin(), armor_table_found.end(), armor_cell_regex);
                    auto words_end = std::sregex_iterator();
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        const auto armor_type = i->str(1);
                        const auto armor_rating = i->str(2);
                        agent_info->wiki_armor_ratings[armor_type] = armor_rating;
                    }
                }
                agent_info->state = AgentInfo::TargetInfoState::Done;
                break;
            }
        }
    }
}

void TargetInfoWindow::Initialize()
{
    ToolboxWindow::Initialize();
    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kChangeTarget,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kMapChange
    };
    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&ui_message_entry, message_id, OnUIMessage, 0x100);
    }
}

void TargetInfoWindow::Terminate()
{
    ToolboxWindow::Terminate();
    GW::UI::RemoveUIMessageCallback(&ui_message_entry);
}

void TargetInfoWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    DecodeSkillNames();
    UpdateAgentInfos();
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (!current_agent_info) {
            ImGui::TextUnformatted("No target info");
            return ImGui::End();
        }
        if (current_agent_info->state != AgentInfo::TargetInfoState::Done) {
            ImGui::TextUnformatted("Target info pending...");
            return ImGui::End();
        }
        ImGui::Text("Guild Wars Wiki info for %s", current_agent_info->name.string().c_str());
        ImGui::Separator();
        if (current_agent_info->wiki_skills.size()) {
            const float btnw = ImGui::GetContentRegionAvail().x / 2.f;
            const ImVec2 btn_dims = { btnw,.0f };
            ImGui::Text("Skills Used:");
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.f, .5f });
            for (const auto skill_id : current_agent_info->wiki_skills) {
                const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                const auto skill_img = Resources::GetSkillImage(skill_id);
                if (ImGui::IconButton(Resources::DecodeStringId(skill->name)->string().c_str(), *skill_img, btn_dims)) {
                    wchar_t url_buf[64];
                    swprintf(url_buf, _countof(url_buf), L"Game_link:Skill_%d", skill_id);
                    GuiUtils::OpenWiki(url_buf);
                }
            }
            ImGui::PopStyleVar();
        }
        if (current_agent_info->wiki_armor_ratings.size()) {
            ImGui::Text("Armor Ratings:");
            for (const auto it : current_agent_info->wiki_armor_ratings) {
                ImGui::Text("%s: %s", it.first.c_str(), it.second.c_str());
            }
        }
        if (ImGui::Button("View More on Guild Wars Wiki")) {
            GuiUtils::SearchWiki(GuiUtils::StringToWString(current_agent_info->wiki_search_term));
        }
    }
    ImGui::End();


}
