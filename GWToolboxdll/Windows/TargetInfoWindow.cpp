#include "stdafx.h"

#include <regex>

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
    bool auto_hide = true;
    std::map<GW::Constants::SkillID, std::unique_ptr<GuiUtils::EncString>> skill_names_by_id;
    std::unordered_map<std::string, GW::Constants::SkillID> skill_ids_by_name;

    // Make sure you pass valid html e.g. start with a < tag
    std::string& strip_tags(std::string& html)
    {

        while(true) {
            const auto startpos = html.find('<');
            if (startpos == std::string::npos) break;
            const auto endpos = html.find('>', startpos);
            if (endpos == std::string::npos)
                break;
            html.erase(startpos, (endpos + 1) - startpos);
        }

        return html;
    }

    std::string& from_html(std::string& html)
    {
        strip_tags(html);
        html = TextUtils::trim(html);
        return html;
    }

    std::string native_html_to_text(const std::string& html)
    {
        std::string text = html;
        text = TextUtils::str_replace_all(text, "&nbsp;", " ");
        text = TextUtils::str_replace_all(text, "&amp;", "&");
        text = TextUtils::str_replace_all(text, "&lt;", "<");
        text = TextUtils::str_replace_all(text, "&gt;", ">");
        text = TextUtils::str_replace_all(text, "&quot;", "\"");
        text = TextUtils::str_replace_all(text, "&apos;", "'");
        text = TextUtils::str_replace_all(text, "&#39;", "'");
        return text;
    }

    struct AgentInfo {
        GuiUtils::EncString name;
        std::string image_url;
        IDirect3DTexture9** image = nullptr;
        std::string wiki_content;
        std::string wiki_search_term;
        std::vector<GW::Constants::SkillID> wiki_skills;
        std::vector<GW::Constants::SkillID> skills_offered;
        std::vector<std::string> items_dropped;
        std::vector<std::string> notes;
        std::unordered_map<std::string, std::string> wiki_armor_ratings; // rating type, rating value
        std::unordered_map<std::string, std::string> infobox_deets;      // key, value
        enum class TargetInfoState {
            DecodingName,
            FetchingWikiPage,
            ParsingWikiPage,
            Done
        } state;

        AgentInfo(const wchar_t* _name)
        {
            state = TargetInfoState::DecodingName;
            name.language(GW::Constants::Language::English);
            name.reset(_name);
            name.StartDecode();
        }

        void ParseContent()
        {
            if (state != TargetInfoState::ParsingWikiPage)
                return;

            // Use std::regex instead of CTRE to avoid MSVC template depth limits
            static const std::regex skill_list_regex("<h2><span class=\"mw-headline\" id=\"Skills\">(?:.*?)<ul.*?>(.*?)(?:</ul>|<h2><span class=\"mw-headline\")", std::regex::optimize);
            static const std::regex skill_link_regex("<a href=\"[^\"]+\" title=\"([^\"]+)\"", std::regex::optimize);
            static const std::regex skill_section_regex("<h2><span class=\"mw-headline\" id=\"Skills\">(.*?)<h2><span class=\"mw-headline\"", std::regex::optimize);
            static const std::regex build_table_regex("<table class=\"skill-progression\" (?:.*?)>(.*?)</table>", std::regex::optimize);
            static const std::regex armor_ratings_regex("<h2><span class=\"mw-headline\" id=\"Armor_ratings\">(.*?)(?:</table>|<h2><span class=\"mw-headline\")", std::regex::optimize);
            static const std::regex armor_cell_regex("<td>.*?title=\"([^\"]+)\".*?<td>([0-9 ()]+).*?</td>", std::regex::optimize);
            static const std::regex items_dropped_regex("<h2><span class=\"mw-headline\" id=\"Items_dropped\">(?:.*?)<ul(.*?)(?:</ul>|<h2><span class=\"mw-headline\")", std::regex::optimize);
            static const std::regex link_regex("<a href=\"[^\"]+\" title=\"([^\"]+)\"", std::regex::optimize);
            static const std::regex skills_offered_regex("<h2><span class=\"mw-headline\" id=\"Skills_offered\">(?:.*?)<table(.*?)</table>", std::regex::optimize);
            static const std::regex notes_regex("<h2><span class=\"mw-headline\" id=\"Notes\">(?:.*?)<ul.*?>(.*?)(?:</ul>|<h2><span class=\"mw-headline\")", std::regex::optimize);
            static const std::regex list_item_regex("<li>(.*?)</li>", std::regex::optimize);
            static const std::regex infobox_regex("<table[^>]+ class=\"[^\"]+infobox(.*?)</table>", std::regex::optimize);
            static const std::regex infobox_image_regex("<td[^>]+class=\"[^\"]*?infobox-image.*?<a .*?href=\"[^\"]*?File:([^\"]+)", std::regex::optimize);
            static const std::regex infobox_row_regex("(?:<tr>|<tr[^>]+>).*?(?:<th>|<th[^>]+>)(.*?)</th>.*?(?:<td>|<td[^>]+>)(.*?)</td>.*?</tr>", std::regex::optimize);

            std::smatch match;

            if (std::regex_search(wiki_content, match, skill_list_regex)) {
                const std::string skill_list_found = match[1].str();
                auto skills_begin = std::sregex_iterator(skill_list_found.begin(), skill_list_found.end(), skill_link_regex);
                auto skills_end = std::sregex_iterator();
                for (auto it = skills_begin; it != skills_end; ++it) {
                    const auto skill_name = (*it)[1].str();
                    const auto skill_name_text = native_html_to_text(skill_name);
                    if (skill_ids_by_name.contains(skill_name_text) && !std::ranges::contains(wiki_skills, skill_ids_by_name[skill_name_text])) {
                        wiki_skills.push_back(skill_ids_by_name[skill_name_text]);
                    }
                }
            }

            if (std::regex_search(wiki_content, match, skill_section_regex)) {
                const std::string section_html = match[1].str();
                auto tables_begin = std::sregex_iterator(section_html.begin(), section_html.end(), build_table_regex);
                auto tables_end = std::sregex_iterator();
                for (auto table_it = tables_begin; table_it != tables_end; ++table_it) {
                    const std::string table_html = (*table_it)[1].str();
                    auto skills_begin = std::sregex_iterator(table_html.begin(), table_html.end(), skill_link_regex);
                    auto skills_end = std::sregex_iterator();
                    for (auto skill_it = skills_begin; skill_it != skills_end; ++skill_it) {
                        std::string skill_name = (*skill_it)[1].str();
                        std::string skill_name_text = native_html_to_text(skill_name);
                        if (skill_ids_by_name.contains(skill_name_text) && !std::ranges::contains(wiki_skills, skill_ids_by_name[skill_name_text])) {
                            wiki_skills.push_back(skill_ids_by_name[skill_name_text]);
                        }
                    }
                }
            }

            if (std::regex_search(wiki_content, match, armor_ratings_regex)) {
                const std::string armor_table_found = match[1].str();
                auto armor_begin = std::sregex_iterator(armor_table_found.begin(), armor_table_found.end(), armor_cell_regex);
                auto armor_end = std::sregex_iterator();
                for (auto it = armor_begin; it != armor_end; ++it) {
                    std::string key = (*it)[1].str();
                    std::string val = (*it)[2].str();
                    from_html(key);
                    from_html(val);
                    if (!key.empty() && !val.empty()) wiki_armor_ratings[key] = val;
                }
            }

            if (std::regex_search(wiki_content, match, items_dropped_regex)) {
                const std::string list_found = match[1].str();
                auto items_begin = std::sregex_iterator(list_found.begin(), list_found.end(), link_regex);
                auto items_end = std::sregex_iterator();
                for (auto it = items_begin; it != items_end; ++it) {
                    std::string item_dropped_html = (*it)[1].str();
                    const auto item_dropped_text = native_html_to_text(item_dropped_html);
                    if (!std::ranges::contains(items_dropped, item_dropped_text)) items_dropped.push_back(item_dropped_text);
                }
            }

            if (std::regex_search(wiki_content, match, skills_offered_regex)) {
                const std::string skill_list_found = match[1].str();
                auto skills_begin = std::sregex_iterator(skill_list_found.begin(), skill_list_found.end(), skill_link_regex);
                auto skills_end = std::sregex_iterator();
                for (auto it = skills_begin; it != skills_end; ++it) {
                    const auto skill_name = (*it)[1].str();
                    const auto skill_name_text = native_html_to_text(skill_name);
                    if (skill_ids_by_name.contains(skill_name_text) && !std::ranges::contains(skills_offered, skill_ids_by_name[skill_name_text])) {
                        skills_offered.push_back(skill_ids_by_name[skill_name_text]);
                    }
                }
            }

            if (std::regex_search(wiki_content, match, notes_regex)) {
                const std::string list_found = match[1].str();
                auto notes_begin = std::sregex_iterator(list_found.begin(), list_found.end(), list_item_regex);
                auto notes_end = std::sregex_iterator();
                for (auto it = notes_begin; it != notes_end; ++it) {
                    auto line = (*it)[1].str();
                    from_html(line);
                    if (!line.empty()) {
                        notes.push_back(line);
                    }
                }
            }

            if (std::regex_search(wiki_content, match, infobox_regex)) {
                const std::string infobox_content = match[1].str();

                std::smatch img_match;
                if (std::regex_search(infobox_content, img_match, infobox_image_regex)) {
                    image_url = img_match[1].str();
                    image = Resources::GetGuildWarsWikiImage(image_url.c_str(), 0, false);
                }

                auto rows_begin = std::sregex_iterator(infobox_content.begin(), infobox_content.end(), infobox_row_regex);
                auto rows_end = std::sregex_iterator();
                for (auto it = rows_begin; it != rows_end; ++it) {
                    std::string key = (*it)[1].str();
                    std::string val = (*it)[2].str();
                    from_html(key);
                    from_html(val);
                    if (!key.empty() && !val.empty()) infobox_deets[key] = val;
                }
            }

            state = AgentInfo::TargetInfoState::Done;
        }

        static void OnFetchedWikiPage(bool success, const std::string& response, void* context)
        {
            ASSERT(context);
            auto ctx_agent_info = (AgentInfo*)context;
            if (!success) {
                Log::Log(response.c_str());
                ctx_agent_info->state = TargetInfoState::Done;
            }
            else {
                ctx_agent_info->wiki_content = std::move(response);
                ctx_agent_info->state = TargetInfoState::ParsingWikiPage;
                Resources::EnqueueWorkerTask([ctx_agent_info] {
                    ctx_agent_info->ParseContent();
                });
            }
        }
    };

    std::unordered_map<std::wstring, AgentInfo*> agent_info_by_name;

    void ClearAgentInfo()
    {
        loop:
        for (auto& agent_info : agent_info_by_name) {
            if (agent_info.second->state != AgentInfo::TargetInfoState::Done) continue;
            delete agent_info.second;
            agent_info_by_name.erase(agent_info.first);
            goto loop;
        }
    }

    AgentInfo* current_agent_info = nullptr;

    GW::HookEntry ui_message_entry;

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void*, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kChangeTarget: {
                current_agent_info = nullptr;
                const auto info_target = GW::Agents::GetTarget();
                const wchar_t* name = nullptr;
                if (info_target && info_target->GetIsLivingType() && info_target->GetAsAgentLiving()->IsNPC()) {
                    name = GW::Agents::GetAgentEncName(info_target);
                }
                if (info_target && info_target->GetIsGadgetType()) {
                    name = GW::Agents::GetAgentEncName(info_target);
                }
                if (!(name && *name)) return;
                if (!agent_info_by_name.contains(name)) {
                    const auto agent_info = new AgentInfo(name);
                    agent_info_by_name[name] = agent_info;
                }
                current_agent_info = agent_info_by_name[name];
                break;
            }
            case GW::UI::UIMessage::kMapLoaded: {
                current_agent_info = nullptr;
                ClearAgentInfo();
                break;
            }
        }
    }

    bool SkillNamesDecoded()
    {
        return !skill_ids_by_name.empty();
    }

    void DecodeSkillNames()
    {
        if (SkillNamesDecoded())
            return;
        if (skill_names_by_id.empty()) {
            std::map<uint32_t, GuiUtils::EncString*> skill_ids_by_name_id;
            for (size_t i = 0; i < (size_t)GW::Constants::SkillID::Count; i++) {
                const auto skill_data = GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)i);
                if (!(skill_data && skill_data->name && skill_ids_by_name_id.find(skill_data->name) == skill_ids_by_name_id.end()))
                    continue;
                auto enc = std::make_unique<GuiUtils::EncString>();
                enc->language(GW::Constants::Language::English);
                enc->reset(skill_data->name);
                enc->StartDecode();
                skill_ids_by_name_id[skill_data->name] = enc.get();
                skill_names_by_id[(GW::Constants::SkillID)i] = std::move(enc);
            }
        }
        for (const auto& it : skill_names_by_id) {
            if (it.second->IsDecoding())
                return;
        }
        for (const auto& it : skill_names_by_id) {
            // Only match the first of this name to the id
            if (!skill_ids_by_name.contains(it.second->string()))
                skill_ids_by_name[it.second->string()] = it.first;
        }
        skill_names_by_id.clear();
    }

    void UpdateAgentInfos()
    {
        if (!SkillNamesDecoded())
            return;
        for (const auto& agent_info : agent_info_by_name | std::views::values) {
            switch (agent_info->state) {
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
                    TextUtils::trim(agent_info->wiki_search_term);
                    std::string wiki_url = "https://wiki.guildwars.com/wiki/?search=";
                    wiki_url.append(TextUtils::UrlEncode(agent_info->wiki_search_term, '_'));
                    Resources::Download(wiki_url, AgentInfo::OnFetchedWikiPage, agent_info);
                }
                break;
            }
        }
    }
}

void TargetInfoWindow::Initialize()
{
    ToolboxWindow::Initialize();
    constexpr std::array ui_messages = {
        GW::UI::UIMessage::kChangeTarget,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kMapChange
    };
    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&ui_message_entry, message_id, OnUIMessage, 0x100);
    }
}

void TargetInfoWindow::Terminate()
{
    ToolboxWindow::Terminate();
    ClearAgentInfo();
    GW::UI::RemoveUIMessageCallback(&ui_message_entry);
}

void TargetInfoWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(auto_hide);
}

void TargetInfoWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(auto_hide);
}

void TargetInfoWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    const auto target = GW::Agents::GetTarget();
    const auto is_valid_target = target && target->GetIsLivingType() && target->GetAsAgentLiving()->IsNPC();
    const auto need_to_collapse = auto_hide && !is_valid_target;
    const auto window_name = std::format("Target Info - {}###TargetInfo", is_valid_target && current_agent_info ? current_agent_info->name.string() : "(No target)");
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 208), ImGuiCond_FirstUseEver);
    const auto window = ImGui::FindWindowByName(window_name.c_str());
    if (window && need_to_collapse && !window->Collapsed) {
        ImGui::SetWindowCollapsed(window, true);
        ImGui::Begin(window_name.c_str(), GetVisiblePtr(), GetWinFlags());
        ImGui::End();
        ImGui::SetWindowCollapsed(window, false);
        return;
    }
    if (!ImGui::Begin(window_name.c_str(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }
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
    const auto has_image = current_agent_info->image;
    if (ImGui::BeginTable("table1", has_image ? 2 : 1)) {
        ImGui::TableSetupColumn("col0", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        if (has_image)
            ImGui::TableSetupColumn("col1", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableNextRow();
        if (has_image) {
            ImGui::TableNextColumn();
            ImGui::ImageFit(*current_agent_info->image, ImGui::GetContentRegionAvail());
        }
        ImGui::TableNextColumn();
        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header2));
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
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.f, .5f});
            for (const auto skill_id : current_agent_info->wiki_skills) {
                ImGui::TableNextColumn();
                const float btnw = ImGui::GetContentRegionAvail().x;
                const ImVec2 btn_dims = {btnw, .0f};
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
        if (current_agent_info->skills_offered.size()) {
            ImGui::Text("Skills Offered:");
            ImGui::BeginTable("skills_offered_table", 2);
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.f, .5f});
            for (const auto skill_id : current_agent_info->skills_offered) {
                ImGui::TableNextColumn();
                const float btnw = ImGui::GetContentRegionAvail().x;
                const ImVec2 btn_dims = {btnw, .0f};
                const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                const auto skill_img = Resources::GetSkillImage(skill_id);
                if (ImGui::IconButton(Resources::DecodeStringId(skill->name)->string().c_str(), *skill_img, btn_dims)) {
                    wchar_t url_buf[64];
                    swprintf(url_buf, _countof(url_buf), L"Game_link:Skill_%d", skill_id);
                    GuiUtils::OpenWiki(url_buf);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", Resources::DecodeStringId(skill->description)->string().c_str());
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
                const ImVec2 btn_dims = {btnw, .0f};
                const auto open_wiki = [&] {
                    auto damage_type_wstr = TextUtils::StringToWString(damage_type);
                    std::ranges::replace(damage_type_wstr, L' ', L'_');
                    GuiUtils::OpenWiki(damage_type_wstr.c_str());
                };
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.f, .5f});
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
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.f, .5f});
            for (const auto& item_name : current_agent_info->items_dropped) {
                ImGui::TableNextColumn();
                const float btnw = ImGui::GetContentRegionAvail().x;
                const ImVec2 btn_dims = {btnw, .0f};
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
                ImGui::TextWrapped("%s", note.c_str());
            }
        }
        ImGui::Separator();
        if (ImGui::Button("View More on Guild Wars Wiki")) {
            GuiUtils::SearchWiki(TextUtils::StringToWString(current_agent_info->wiki_search_term));
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void TargetInfoWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::Checkbox("Automatically hide when not targeting anything", &auto_hide);
}

void TargetInfoWindow::Update(const float x)
{
    ToolboxWindow::Update(x);
}
