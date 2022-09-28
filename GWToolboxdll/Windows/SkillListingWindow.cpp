#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/SkillListingWindow.h>

static uintptr_t skill_array_addr;

static void printchar(wchar_t c) {
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    }
    else {
        printf("0x%X ", c);
    }
}
const wchar_t* SkillListingWindow::Skill::Name() {
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(skill->name, name_enc, 16))
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
    return name_dec;
}
const wchar_t* SkillListingWindow::Skill::GWWDescription() {
    const wchar_t* raw_description = Description();
    if (raw_description[0] && !desc_gww[0]) {
        wchar_t scale1_txt[16] = { 0 };
        swprintf(scale1_txt, 16, L"%d..%d", skill->scale0, skill->scale15);
        wchar_t scale2_txt[16] = { 0 };
        swprintf(scale2_txt, 16, L"%d..%d", skill->bonusScale0, skill->bonusScale15);
        wchar_t scale3_txt[16] = { 0 };
        swprintf(scale3_txt, 16, L"%d..%d", skill->duration0, skill->duration15);
        std::wstring s(raw_description);
        size_t pos = std::wstring::npos;
        while ((pos = s.find(L"991")) != std::wstring::npos)
            s.replace(pos, 3, scale1_txt);
        while ((pos = s.find(L"992")) != std::wstring::npos)
            s.replace(pos, 3, scale2_txt);
        while ((pos = s.find(L"993")) != std::wstring::npos)
            s.replace(pos, 3, scale3_txt);
        wsprintfW(desc_gww, L"%s. %s", GetSkillType().c_str(), s.c_str());
    }
    return desc_gww;
}
const wchar_t* SkillListingWindow::Skill::GWWConcise() {
    const wchar_t* raw_description = Concise();
    if (raw_description[0] && !concise_gww[0]) {
        wchar_t scale1_txt[16] = { 0 };
        swprintf(scale1_txt, 16, L"%d..%d", skill->scale0, skill->scale15);
        wchar_t scale2_txt[16] = { 0 };
        swprintf(scale2_txt, 16, L"%d..%d", skill->bonusScale0, skill->bonusScale15);
        wchar_t scale3_txt[16] = { 0 };
        swprintf(scale3_txt, 16, L"%d..%d", skill->duration0, skill->duration15);
        std::wstring s(raw_description);
        size_t pos = std::wstring::npos;
        while ((pos = s.find(L"991")) != std::wstring::npos)
            s.replace(pos, 3, scale1_txt);
        while ((pos = s.find(L"992")) != std::wstring::npos)
            s.replace(pos, 3, scale2_txt);
        while ((pos = s.find(L"993")) != std::wstring::npos)
            s.replace(pos, 3, scale3_txt);
        wsprintfW(concise_gww, L"%s. %s", GetSkillType().c_str(), s.c_str());
    }
    return concise_gww;
}

void SkillListingWindow::ExportToJSON() {
    nlohmann::json json;
    for (size_t i = 0; i < skills.size(); i++) {
        if (!skills[i]) continue;
        json[(uint32_t)skills[i]->skill->skill_id] = skills[i]->ToJson();
    }
    auto file_location = Resources::GetPath(L"skills.json");
    if (std::filesystem::exists(file_location)) {
        std::filesystem::remove(file_location);
    }

    std::ofstream out(file_location);
    out << json.dump();
    out.close();
    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    auto message = file_location.wstring();

    size_t max_len = _countof(file_location_wc) - 1;

    for (size_t i = 0; i < message.length(); i++) {
        // Break on the end of the message
        if (!message[i])
            break;
        // Double escape backsashes
        if (message[i] == '\\')
            file_location_wc[msg_len++] = message[i];
        if (msg_len >= max_len)
            break;
        file_location_wc[msg_len++] = message[i];
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"Skills exported to <a=1>\x200C%s</a>", file_location_wc);
    GW::Chat::WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}
void SkillListingWindow::Initialize() {
    ToolboxWindow::Initialize();
    skills.resize((size_t)GW::Constants::SkillID::Count,0);
    for (size_t i = 0; i < skills.size(); i++) {
        GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)i);
        if (!s || s->skill_id == (GW::Constants::SkillID)0) continue;
        skills[i] = new Skill(s);
    }
}
void SkillListingWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (const auto skill : skills) {
        if (skill) delete skill;
    }
    skills.clear();
}
void SkillListingWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible)
        return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    float offset = 0.0f;
    const float tiny_text_width = 50.0f * ImGui::GetIO().FontGlobalScale;
    const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;

    ImGui::Text("#");
    ImGui::SameLine(offset += tiny_text_width + tiny_text_width);
    ImGui::Text("Name");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Attr");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Prof");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Type");
    ImGui::Separator();
    char buf[16] = {};
    static std::wstring search_term;
    if (ImGui::InputText("Search", buf, sizeof buf)) {
        search_term = GuiUtils::ToLower(GuiUtils::StringToWString(buf));
    }
    for (size_t i = 0; i < skills.size(); i++) {
        if (!skills[i]) continue;
        if (!search_term.empty() && GuiUtils::ToLower(skills[i]->Name()).find(search_term) == std::wstring::npos)
            continue;
        offset = 0;
        ImGui::Text("%d", i);
        if (!ImGui::IsItemVisible())
            continue;
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::ImageCropped(*Resources::GetSkillImage(skills[i]->skill->skill_id), { 20.f,20.f });
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%S",skills[i]->Name());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%S,\n%S", skills[i]->GWWDescription(), "");// skills[i]->GWWConcise());
        ImGui::SameLine(offset += long_text_width);
        ImGui::Text("%d", skills[i]->skill->attribute);
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%s", GW::Constants::GetProfessionAcronym((GW::Constants::Profession)skills[i]->skill->profession).c_str());
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%d", skills[i]->skill->type);
        ImGui::SameLine();
        char buf2[32];
        snprintf(buf2, _countof(buf2), "Wiki###wiki_%d", i);
        if (ImGui::SmallButton(buf2)) {
            char* url = new char[128];
            snprintf(url, 128, "https://wiki.guildwars.com/wiki/Game_link:Skill_%d", skills[i]->skill->skill_id);
            GW::GameThread::Enqueue([url]() {
                GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, url);
                delete[] url;
                });
        }
    }
    if (ImGui::Button("Export to JSON"))
        ExportToJSON();
    ImGui::End();
}
nlohmann::json SkillListingWindow::Skill::ToJson() {
    nlohmann::json json;
    json["n"] = GuiUtils::WStringToString(Name());
    json["d"] = GuiUtils::WStringToString(GWWDescription());
    json["cd"] = GuiUtils::WStringToString(GWWConcise());
    json["t"] = skill->type;
    json["p"] = skill->profession;
    json["a"] = IsPvE() ? 255 - skill->title : skill->attribute;
    if (IsElite())
        json["e"] = 1;
    json["c"] = skill->campaign;
    nlohmann::json z_json;
    if (HasExhaustion())
        z_json["x"] = skill->overcast;
    if (skill->recharge)
        z_json["r"] = skill->recharge;
    if (skill->activation)
        z_json["c"] = skill->activation;
    if (IsMaintained())
        z_json["d"] = 1;
    if (skill->adrenaline)
        z_json["a"] = skill->adrenaline;
    if (skill->energy_cost)
        z_json["e"] = skill->GetEnergyCost();
    if (skill->health_cost)
        z_json["s"] = skill->health_cost;
    z_json["sp"] = skill->special;
    z_json["co"] = skill->combo;
    z_json["q"] = skill->weapon_req;
    if (z_json.size())
        json["z"] = z_json;
    return json;
}
const std::wstring SkillListingWindow::Skill::GetSkillType() {
    std::wstring str(IsElite() ? L"Elite " : L"");
    switch ((uint32_t)skill->type) {
        case 3:
            return str += L"Stance", str;
        case 4:
            return str += L"Hex Spell", str;
        case 5:
            return str += L"Spell", str;
        case 6:
            if (skill->special & 0x800000)
                str += L"Flash ";
            return str += L"Enchantment Spell", str;
        case 7:
            return str += L"Signet", str;
        case 9:
            return str += L"Well Spell", str;
        case 10:
            return str += L"Touch Skill", str;
        case 11:
            return str += L"Ward Spell", str;
        case 12:
            return str += L"Glyph", str;
        case 14:
            switch (skill->weapon_req) {
                case 1:
                    return str += L"Axe Attack", str;
                case 2:
                    return str += L"Bow Attack", str;
                case 8:
                    switch (skill->combo) {
                        case 1:
                            return str += L"Lead Attack", str;
                        case 2:
                            return str += L"Off-Hand Attack", str;
                        case 3:
                            return str += L"Dual Attack", str;
                    }
                    return str += L"Dagger Attack", str;
                case 16:
                    return str += L"Hammer Attack", str;
                case 32:
                    return str += L"Scythe Attack", str;
                case 64:
                    return str += L"Spear Attack", str;
                case 70:
                    return str += L"Ranged Attack", str;
                case 128:
                    return str += L"Sword Attack", str;
            }
            return str += L"Melee Attack", str;
        case 15:
            return str += L"Shout", str;
        case 19:
            return str += L"Preparation", str;
        case 20:
            return str += L"Pet Attack", str;
        case 21:
            return str += L"Trap", str;
        case 22:
            switch (skill->profession) {
                case 8:
                    return str += L"Binding Ritual", str;
                case 2:
                    return str += L"Nature Ritual", str;
            }
            return str += L"Ebon Vanguard Ritual", str;
        case 24:
            return str += L"Item Spell", str;
        case 25:
            return str += L"Weapon Spell", str;
        case 26:
            return str += L"Form", str;
        case 27:
            return str += L"Chant", str;
        case 28:
            return str += L"Echo", str;
        default:
            return str += L"Skill", str;
    }
}
const wchar_t *SkillListingWindow::Skill::Description()
{
    if (!desc_enc[0] &&
        GW::UI::UInt32ToEncStr(skill->description, desc_enc, 16)) {
        wchar_t buf[64] = {0};
        swprintf(
            buf, 64,
            L"%s\x10A\x104\x101%c\x1\x10B\x104\x101%c\x1\x10C\x104\x101%c\x1",
            desc_enc,
            0x100 + (skill->scale0 == skill->scale15 ? skill->scale0 : 991),
            0x100 + (skill->bonusScale0 == skill->bonusScale15
                         ? skill->bonusScale0
                         : 992),
            0x100 + (skill->duration0 == skill->duration15 ? skill->duration0
                                                           : 993));
        wcscpy(desc_enc, buf);
        GW::UI::AsyncDecodeStr(desc_enc, desc_dec, 256);
    }
    return desc_dec;
}
const wchar_t *SkillListingWindow::Skill::Concise()
{
    if (!concise_enc[0] &&
        GW::UI::UInt32ToEncStr(skill->concise, concise_enc, 16)) {
        wchar_t buf[64] = {0};
        swprintf(
            buf, 64,
            L"%s\x10A\x104\x101%c\x1\x10B\x104\x101%c\x1\x10C\x104\x101%c\x1",
            concise_enc,
            0x100 + (skill->scale0 == skill->scale15 ? skill->scale0 : 991),
            0x100 + (skill->bonusScale0 == skill->bonusScale15
                         ? skill->bonusScale0
                         : 992),
            0x100 + (skill->duration0 == skill->duration15 ? skill->duration0
                                                           : 993));
        wcscpy(concise_enc, buf);
        GW::UI::AsyncDecodeStr(concise_enc, concise_dec, 256);
    }
    return concise_dec;
}