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


    const ImVec2 get_texture_size(IDirect3DTexture9* texture) {
        const ImVec2 uv1 = { 0.f, 0.f };
        if (!texture)
            return uv1;
        D3DSURFACE_DESC desc;
        const HRESULT res = texture->GetLevelDesc(0, &desc);
        if (!SUCCEEDED(res)) {
            return uv1; // Don't throw anything into the log here; this function is called every frame by modules that use it!
        }
        return { static_cast<float>(desc.Width),static_cast<float>(desc.Height) };
    }

    const char* ws = " \t\n\r\f\v";

    // trim from end of string (right)
    inline std::string& rtrim(std::string& s, const char* t = ws)
    {
        s.erase(s.find_last_not_of(t) + 1);
        return s;
    }

    // trim from beginning of string (left)
    inline std::string& ltrim(std::string& s, const char* t = ws)
    {
        s.erase(0, s.find_first_not_of(t));
        return s;
    }

    // trim from both ends of string (right then left)
    inline std::string& trim(std::string& s, const char* t = ws)
    {
        return ltrim(rtrim(s, t), t);
    }
    // Make sure you pass valid html e.g. start with a < tag
    std::string& strip_tags(std::string& html) {
        while (1)
        {
            auto startpos = html.find("<");
            if (startpos == std::string::npos)
                break;
            auto endpos = html.find(">", startpos) + 1;
            if (endpos == std::string::npos)
                break;
            html.erase(startpos, endpos - startpos);
        }

        return html;
    }
    std::string& from_html(std::string& html) {
        strip_tags(html);
        trim(html);
        return html;
    }


    struct AgentInfo {
        GuiUtils::EncString name;
        std::string image_url;
        IDirect3DTexture9** image = nullptr;
        std::string wiki_content;
        std::string wiki_search_term;
        std::vector<GW::Constants::SkillID> wiki_skills;
        std::unordered_map<std::string, std::string> wiki_armor_ratings; // rating type, rating value
        std::unordered_map<std::string, std::string> infobox_deets; // key, value
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
                trim(agent_info->wiki_search_term);
                std::string wiki_url = "https://wiki.guildwars.com/wiki/?search=";
                wiki_url.append(GuiUtils::UrlEncode(agent_info->wiki_search_term, '_'));
                Resources::Download(wiki_url, AgentInfo::OnFetchedWikiPage, agent_info);
            } break;
            case AgentInfo::TargetInfoState::ParsingWikiPage:
                Log::InfoW(L"Got wiki page for %ls, need to write the code to parse!!", agent_info->name.wstring().c_str());

                const std::regex skill_list_regex("<h2><span class=\"mw-headline\" id=\"Skills\">([\\s\\S]*?)</ul>");
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

                const std::regex armor_ratings_regex("<h2><span class=\"mw-headline\" id=\"Armor_ratings\">([\\s\\S]*?)</table>");
                if (std::regex_search(agent_info->wiki_content, m, armor_ratings_regex)) {
                    std::string armor_table_found = m[1].str();
                    armor_table_found.erase(std::ranges::remove(armor_table_found, '\n').begin(), armor_table_found.end());

                    // Iterate over all skills in this list.
                    const auto armor_cell_regex = std::regex(R"regex(<td>[\s\S]*?title="([^"]+)"[\s\S]*?<td>([0-9 \(\)]+).*?<\/td>)regex");
                    auto words_begin = std::sregex_iterator(armor_table_found.begin(), armor_table_found.end(), armor_cell_regex);
                    auto words_end = std::sregex_iterator();
                    for (auto i = words_begin; i != words_end; ++i)
                    {
                        auto key = i->str(1);
                        auto val = i->str(2);
                        from_html(key);
                        from_html(val);
                        if (key.empty() || val.empty())
                            continue;
                        agent_info->wiki_armor_ratings[key] = val;
                    }
                }

                const std::regex infobox_regex("<table[^>]+ class=\"[^\"]+infobox([\\s\\S]*?)</table>");
                if (std::regex_search(agent_info->wiki_content, m, infobox_regex)) {
                    std::string infobox_content = m[1].str();

                    const auto infobox_image_regex = std::regex("<td[^>]+class=\"[^\"]*?infobox-image[\\s\\S]*?<a [\\s\\S]*?href=\"[^\"]*?File:([^\"]+)");
                    if (std::regex_search(infobox_content, m, infobox_image_regex)) {
                        const auto img_src = m[1].str();
                        agent_info->image_url = img_src;
                        agent_info->image = Resources::GetGuildWarsWikiImage(img_src.c_str());
                    }

                    // Iterate over all skills in this list.
                    const auto infobox_row_regex = std::regex(
                        "(?:<tr>|<tr[^>]+>)[\\s\\S]*?(?:<th>|<th[^>]+>)([\\s\\S]*?)</th>[\\s\\S]*?(?:<td>|<td[^>]+>)([\\s\\S]*?)</td>[\\s\\S]*?</tr>"
                    );
                    auto words_begin = std::sregex_iterator(infobox_content.begin(), infobox_content.end(), infobox_row_regex);
                    auto words_end = std::sregex_iterator();
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        auto key = i->str(1);
                        auto val = i->str(2);
                        from_html(key);
                        from_html(val);
                        if (key.empty() || val.empty())
                            continue;
                        agent_info->infobox_deets[key] = val;
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
        if (ImGui::BeginTable("table1", current_agent_info->image ? 2 : 1))
        {
            ImGui::TableNextRow();
            if (current_agent_info->image) {
                ImGui::TableNextColumn();
                const auto w = ImGui::GetContentRegionAvail().x;
                ImGui::ImageCropped(*current_agent_info->image, { w / 2.f, w });
            }
            ImGui::TableNextColumn();
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
            if (current_agent_info->infobox_deets.size()) {
                ImGui::Separator();
                for (const auto it : current_agent_info->infobox_deets) {
                    ImGui::Text("%s: %s", it.first.c_str(), it.second.c_str());
                }
            }
            if (current_agent_info->wiki_armor_ratings.size()) {
                ImGui::Text("Armor Ratings:");
                for (const auto [damage_type, armour_rating] : current_agent_info->wiki_armor_ratings) {
                    const float btnw = ImGui::GetContentRegionAvail().x / 2.f;
                    const ImVec2 btn_dims = { btnw,.0f };
                    const auto open_wiki = [&] {
                        auto damage_type_wstr = GuiUtils::StringToWString(damage_type);
                        std::ranges::replace(damage_type_wstr, L' ', L'_');
                        GuiUtils::OpenWiki(damage_type_wstr.c_str());
                    };
                    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.f, .5f });
                    if (const auto dmgtype_img = Resources::GetDamagetypeImage(damage_type)) {
                        ImGui::PushID(damage_type.c_str());
                        if (ImGui::IconButton(armour_rating.c_str(), *dmgtype_img, btn_dims)) {
                            open_wiki();
                        }
                        ImGui::PopID();
                    }
                    else {
                        ImGui::Text("%s: %s", damage_type.c_str(), armour_rating.c_str());
                        if (ImGui::IsItemClicked()) {
                            open_wiki();
                        }
                    }
                    ImGui::PopStyleVar();
                }
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        if (ImGui::Button("View More on Guild Wars Wiki")) {
            GuiUtils::SearchWiki(GuiUtils::StringToWString(current_agent_info->wiki_search_term));
        }
        
    }
    ImGui::End();


}
