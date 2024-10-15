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
#include <Utils/TextUtils.h>
#include <Utils/FontLoader.h>

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
        std::vector<std::string> items_dropped;
        std::vector<std::string> notes;
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

    AgentInfo* current_agent_info = nullptr;

    GW::HookEntry ui_message_entry;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        const auto info_target = GW::Agents::GetTarget();
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
            // Only match the first of this name to the id
            if(!skill_ids_by_name.contains(it.second->string()))
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

                agent_info->wiki_search_term = TextUtils::WStringToString(
                    TextUtils::SanitizePlayerName(agent_info->name.wstring())
                );
                trim(agent_info->wiki_search_term);
                std::string wiki_url = "https://wiki.guildwars.com/wiki/?search=";
                wiki_url.append(TextUtils::UrlEncode(agent_info->wiki_search_term, '_'));
                Resources::Download(wiki_url, AgentInfo::OnFetchedWikiPage, agent_info);
            } break;
            case AgentInfo::TargetInfoState::ParsingWikiPage:
                static const std::regex skill_list_regex("<h2><span class=\"mw-headline\" id=\"Skills\">(?:[\\s\\S]*?)<ul([\\s\\S]*?)</ul>");
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
                        if (std::ranges::contains(agent_info->wiki_skills, found->second))
                            continue;
                        agent_info->wiki_skills.push_back(found->second);
                    }
                }

                static const std::regex armor_ratings_regex("<h2><span class=\"mw-headline\" id=\"Armor_ratings\">([\\s\\S]*?)</table>");
                if (std::regex_search(agent_info->wiki_content, m, armor_ratings_regex)) {
                    std::string armor_table_found = m[1].str();
                    armor_table_found.erase(std::ranges::remove(armor_table_found, '\n').begin(), armor_table_found.end());

                    // Iterate over all skills in this list.
                    static const std::regex armor_cell_regex(R"regex(<td>[\s\S]*?title="([^"]+)"[\s\S]*?<td>([0-9 \(\)]+).*?<\/td>)regex");
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

                static const std::regex items_dropped_regex("<h2><span class=\"mw-headline\" id=\"Items_dropped\">(?:[\\s\\S]*?)<ul([\\s\\S]*?)</ul>");
                if (std::regex_search(agent_info->wiki_content, m, items_dropped_regex)) {
                    std::string list_found = m[1].str();

                    // Iterate over all skills in this list.
                    const auto link_regex = std::regex("<a href=\"[^\"]+\" title=\"([^\"]+)\"");
                    auto words_begin = std::sregex_iterator(list_found.begin(), list_found.end(), link_regex);
                    auto words_end = std::sregex_iterator();
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        const auto title_attr = i->str(1);
                        if (std::ranges::contains(agent_info->items_dropped, title_attr))
                            continue;
                        agent_info->items_dropped.push_back(title_attr);
                    }
                }
                static const std::regex notes_regex("<h2><span class=\"mw-headline\" id=\"Notes\">(?:[\\s\\S]*?)<ul([\\s\\S]*?)</ul>");
                if (std::regex_search(agent_info->wiki_content, m, notes_regex)) {
                    std::string list_found = m[1].str();

                    // Iterate over all skills in this list.
                    const auto link_regex = std::regex("<li>([\\s\\S]*?)</li>");
                    auto words_begin = std::sregex_iterator(list_found.begin(), list_found.end(), link_regex);
                    auto words_end = std::sregex_iterator();
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
                    {
                        auto line = i->str(1);
                        from_html(line);
                        if (line.empty()) continue;
                        agent_info->notes.push_back(line);
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

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        DecodeSkillNames();
        UpdateAgentInfos();
        if (!current_agent_info) {
            ImGui::TextUnformatted("No target info");
            return ImGui::End();
        }
        if (current_agent_info->state != AgentInfo::TargetInfoState::Done) {
            ImGui::TextUnformatted("Target info pending...");
            return ImGui::End();
        }
        if (ImGui::BeginTable("table1", current_agent_info->image ? 2 : 1))
        {
            ImGui::TableSetupColumn("col0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("col1", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableNextRow();
            if (current_agent_info->image) {
                ImGui::TableNextColumn();
                ImGui::ImageFit(*current_agent_info->image, ImGui::GetContentRegionAvail());
            }
            ImGui::TableNextColumn();
            ImGui::PushFont(FontLoader::GetFont(FontLoader::FontSize::header2));
            ImGui::TextUnformatted(current_agent_info->name.string().c_str());
            ImGui::PopFont();
            ImGui::Separator();
            if (current_agent_info->infobox_deets.size()) {
                for (const auto it : current_agent_info->infobox_deets) {
                    ImGui::Text("%s: %s", it.first.c_str(), it.second.c_str());
                }
            }
            if (current_agent_info->wiki_skills.size()) {
                ImGui::Text("Skills Used:");
                ImGui::BeginTable("skills_used_table", 2);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.f, .5f });
                for (const auto skill_id : current_agent_info->wiki_skills) {
                    ImGui::TableNextColumn();
                    const float btnw = ImGui::GetContentRegionAvail().x;
                    const ImVec2 btn_dims = { btnw,.0f };
                    const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                    const auto skill_img = Resources::GetSkillImage(skill_id);
                    if (ImGui::IconButton(Resources::DecodeStringId(skill->name)->string().c_str(), *skill_img, btn_dims)) {
                        wchar_t url_buf[64];
                        swprintf(url_buf, _countof(url_buf), L"Game_link:Skill_%d", skill_id);
                        GuiUtils::OpenWiki(url_buf);
                    }
                }
                ImGui::PopStyleVar();
                ImGui::EndTable();
            }
            if (current_agent_info->wiki_armor_ratings.size()) {
                ImGui::Text("Armor Ratings:");
                ImGui::BeginTable("armor_rating_table", 2);
                for (const auto [damage_type, armour_rating] : current_agent_info->wiki_armor_ratings) {
                    ImGui::TableNextColumn();
                    const float btnw = ImGui::GetContentRegionAvail().x;
                    const ImVec2 btn_dims = { btnw,.0f };
                    const auto open_wiki = [&] {
                        auto damage_type_wstr = TextUtils::StringToWString(damage_type);
                        std::ranges::replace(damage_type_wstr, L' ', L'_');
                        GuiUtils::OpenWiki(damage_type_wstr.c_str());
                    };
                    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.f, .5f });
                    const auto label = std::format("{}: {}", damage_type, armour_rating);
                    if (const auto dmgtype_img = Resources::GetDamagetypeImage(damage_type)) {
                        ImGui::PushID(damage_type.c_str());
                        if (ImGui::IconButton(label.c_str(), *dmgtype_img, btn_dims)) {
                            open_wiki();
                        }
                        ImGui::PopID();
                    }
                    else {
                        ImGui::TextUnformatted(label.c_str());
                        if (ImGui::IsItemClicked()) {
                            open_wiki();
                        }
                    }
                    ImGui::PopStyleVar();
                }
                ImGui::EndTable();
            }
            if (current_agent_info->items_dropped.size()) {
                ImGui::Text("Items Dropped:");
                ImGui::BeginTable("items_dropped_table", 2);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.f, .5f });
                for (const auto& item_name : current_agent_info->items_dropped) {
                    ImGui::TableNextColumn();
                    const float btnw = ImGui::GetContentRegionAvail().x;
                    const ImVec2 btn_dims = { btnw,.0f };
                    const auto item_name_ws = TextUtils::StringToWString(item_name);
                    const auto item_image = Resources::GetItemImage(item_name_ws);
                    if (ImGui::IconButton(item_name.c_str(), *item_image, btn_dims)) {
                        GuiUtils::SearchWiki(item_name_ws.c_str());
                    }
                }
                ImGui::PopStyleVar();
                ImGui::EndTable();
            }
            if (current_agent_info->notes.size()) {
                ImGui::Text("Notes:");
                for (const auto& note : current_agent_info->notes) {
                    ImGui::TextWrapped("%s",note.c_str());
                }
            }
            ImGui::Separator();
            if (ImGui::Button("View More on Guild Wars Wiki")) {
                GuiUtils::SearchWiki(TextUtils::StringToWString(current_agent_info->wiki_search_term));
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();


}
