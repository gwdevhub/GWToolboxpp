#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/SkillListingWindow.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Modules/GwDatModule.h>

static uintptr_t skill_array_addr;

static void printchar(const wchar_t c)
{
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    }
    else {
        printf("0x%X ", c);
    }
}

const wchar_t* SkillListingWindow::Skill::Name()
{
    if (!name_enc[0] && GW::UI::UInt32ToEncStr(skill->name, name_enc, 16)) {
        GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
    }
    return name_dec;
}

const std::wstring& SkillListingWindow::Skill::NameLower()
{
    if (name_lower.empty()) {
        name_lower = TextUtils::ToLower(Name());
    }
    return name_lower;
}

const wchar_t* SkillListingWindow::Skill::GWWDescription()
{
    const wchar_t* raw_description = Description();
    if (raw_description[0] && !desc_gww[0]) {
        wchar_t scale1_txt[16] = {0};
        swprintf(scale1_txt, 16, L"%d..%d", skill->scale0, skill->scale15);
        wchar_t scale2_txt[16] = {0};
        swprintf(scale2_txt, 16, L"%d..%d", skill->bonusScale0, skill->bonusScale15);
        wchar_t scale3_txt[16] = {0};
        swprintf(scale3_txt, 16, L"%d..%d", skill->duration0, skill->duration15);
        std::wstring s(raw_description);
        size_t pos = std::wstring::npos;
        while ((pos = s.find(L"991")) != std::wstring::npos) {
            s.replace(pos, 3, scale1_txt);
        }
        while ((pos = s.find(L"992")) != std::wstring::npos) {
            s.replace(pos, 3, scale2_txt);
        }
        while ((pos = s.find(L"993")) != std::wstring::npos) {
            s.replace(pos, 3, scale3_txt);
        }
        wsprintfW(desc_gww, L"%s。%s", GetSkillType().c_str(), s.c_str());
    }
    return desc_gww;
}

const wchar_t* SkillListingWindow::Skill::GWWConcise()
{
    const wchar_t* raw_description = Concise();
    if (raw_description[0] && !concise_gww[0]) {
        wchar_t scale1_txt[16] = {0};
        swprintf(scale1_txt, 16, L"%d..%d", skill->scale0, skill->scale15);
        wchar_t scale2_txt[16] = {0};
        swprintf(scale2_txt, 16, L"%d..%d", skill->bonusScale0, skill->bonusScale15);
        wchar_t scale3_txt[16] = {0};
        swprintf(scale3_txt, 16, L"%d..%d", skill->duration0, skill->duration15);
        std::wstring s(raw_description);
        size_t pos = std::wstring::npos;
        while ((pos = s.find(L"991")) != std::wstring::npos) {
            s.replace(pos, 3, scale1_txt);
        }
        while ((pos = s.find(L"992")) != std::wstring::npos) {
            s.replace(pos, 3, scale2_txt);
        }
        while ((pos = s.find(L"993")) != std::wstring::npos) {
            s.replace(pos, 3, scale3_txt);
        }
        wsprintfW(concise_gww, L"%s。%s", GetSkillType().c_str(), s.c_str());
    }
    return concise_gww;
}

void SkillListingWindow::ExportToJSON() const
{
    std::map<std::string, skilllist_export::SkillJson> output;
    for (size_t i = 0; i < skills.size(); i++) {
        if (!skills[i]) {
            continue;
        }
        output.emplace(std::to_string(std::to_underlying(skills[i]->skill->skill_id)), skills[i]->ToJson());
    }
    const auto file_location = Resources::GetPath(L"skills.json");
    if (exists(file_location)) {
        std::filesystem::remove(file_location);
    }

    std::ofstream out(file_location);
    out << glz::write_json(output).value_or(std::string{});
    out.close();
    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const auto message = file_location.wstring();

    constexpr size_t max_len = _countof(file_location_wc) - 1;

    for (size_t i = 0; i < message.length(); i++) {
        if (!message[i]) {
            break;
        }
        // 双反斜杠转义
        if (message[i] == '\\') {
            file_location_wc[msg_len++] = message[i];
        }
        if (msg_len >= max_len) {
            break;
        }
        file_location_wc[msg_len++] = message[i];
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"技能已导出到 <a=1>\x200C%s</a>", file_location_wc);
    WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}

void SkillListingWindow::ExportHiResIconsAsDDS() const
{
    const auto folder = Resources::GetPath(L"hd_skill_icons");
    Resources::EnsureFolderExists(folder);

    size_t count = 0;
    for (size_t skill_id = 0; skill_id < skills.size(); skill_id++) {
        const auto skill = skills[skill_id];
        if (!skill) {
            continue;
        }
        // 优先使用高清图标，若无则回退到低分辨率版本
        const auto file_id = skill->skill->icon_file_id_2 ? skill->skill->icon_file_id_2 : skill->skill->icon_file_id;
        if (!file_id) {
            continue;
        }
        const auto filename = std::format(L"{}.dds", skill_id);
        GwDatModule::SaveTextureFromFileIdToFile(file_id, folder / filename);
        count++;
    }

    wchar_t folder_wc[512];
    size_t msg_len = 0;
    const auto message = folder.wstring();
    constexpr size_t max_len = _countof(folder_wc) - 1;
    for (size_t i = 0; i < message.length() && msg_len < max_len; i++) {
        if (message[i] == '\\' && msg_len < max_len) {
            folder_wc[msg_len++] = message[i];
        }
        if (msg_len >= max_len) {
            break;
        }
        folder_wc[msg_len++] = message[i];
    }
    folder_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"<quote>正在导出 %zu 个高清技能图标到 [%s,file://%s]", count, folder_wc, folder_wc);
    WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}

void SkillListingWindow::Initialize()
{
    ToolboxWindow::Initialize();
    skills.resize(GW::SkillbarMgr::GetSkillCount(), nullptr);
    for (size_t i = 0; i < skills.size(); i++) {
        GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(i));
        if (!s || s->skill_id == static_cast<GW::Constants::SkillID>(0)) {
            continue;
        }
        skills[i] = new Skill(s);
    }
}

void SkillListingWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (const auto skill : skills) {
        if (skill) {
            delete skill;
        }
    }
    skills.clear();
}

void SkillListingWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }
    float offset = 0.0f;
    const float tiny_text_width = 50.0f * ImGui::FontScale();
    const float long_text_width = 200.0f * ImGui::FontScale();

    ImGui::Text("#");
    ImGui::SameLine(offset += tiny_text_width + tiny_text_width);
    ImGui::Text("名称");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("属性");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("职业");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("类型");
    ImGui::Separator();
    char buf[16] = {};
    static std::wstring search_term;
    if (ImGui::InputText("搜索", buf, sizeof buf)) {
        search_term = TextUtils::ToLower(TextUtils::StringToWString(buf));
    }
    static std::vector<size_t> visible_skills;
    visible_skills.clear();
    for (size_t i = 0; i < skills.size(); i++) {
        if (!skills[i]) {
            continue;
        }
        if (!search_term.empty() && skills[i]->NameLower().find(search_term) == std::wstring::npos) {
            continue;
        }
        visible_skills.push_back(i);
    }
    constexpr float icon_size = 20.f;
    const float row_height = std::max(icon_size, ImGui::GetTextLineHeight()) + ImGui::GetStyle().ItemSpacing.y;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible_skills.size()), row_height);
    while (clipper.Step()) {
        for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            const auto i = visible_skills[row];
            offset = 0;
            ImGui::Text("%d", i);
            ImGui::SameLine(offset += tiny_text_width);
            const auto low_res_img = skills[i]->skill->icon_file_id ? GwDatModule::LoadTextureFromFileId(skills[i]->skill->icon_file_id) : nullptr;
            const auto hi_res_img = skills[i]->skill->icon_file_id_2 ? GwDatModule::LoadTextureFromFileId(skills[i]->skill->icon_file_id_2) : nullptr;
            const auto to_use = low_res_img ? low_res_img : hi_res_img;
            if (to_use)
                ImGui::ImageCropped(*to_use, {icon_size, icon_size});
            else
                ImGui::Dummy({icon_size, icon_size});
            ImGui::SameLine(offset += tiny_text_width);
            ImGui::Text("%S", skills[i]->Name());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%S,\n%S", skills[i]->GWWDescription(), "");
            }
            ImGui::SameLine(offset += long_text_width);
            ImGui::Text("%d", skills[i]->skill->attribute);
            ImGui::SameLine(offset += tiny_text_width);
            ImGui::Text("%s", ToolboxUtils::GetProfessionAcronym(static_cast<GW::Constants::Profession>(skills[i]->skill->profession))->string().c_str());
            ImGui::SameLine(offset += tiny_text_width);
            ImGui::Text("%d", skills[i]->skill->type);
            ImGui::SameLine();
            char buf2[32];
            snprintf(buf2, _countof(buf2), "Wiki###wiki_%d", i);
            if (ImGui::SmallButton(buf2)) {
                auto url = new char[128];
                snprintf(url, 128, "https://wiki.guildwars.com/wiki/Game_link:Skill_%d", skills[i]->skill->skill_id);
                GW::GameThread::Enqueue([url] {
                    SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, url);
                    delete[] url;
                });
            }
        }
    }
    if (ImGui::Button("导出为 JSON")) {
        ExportToJSON();
    }
    ImGui::SameLine();
    if (ImGui::Button("导出高清技能图标为 DDS")) {
        ExportHiResIconsAsDDS();
    }
    ImGui::End();
}

skilllist_export::SkillJson SkillListingWindow::Skill::ToJson()
{
    skilllist_export::SkillJson out{
        .n = TextUtils::WStringToString(Name()),
        .d = TextUtils::WStringToString(GWWDescription()),
        .cd = TextUtils::WStringToString(GWWConcise()),
        .t = static_cast<uint32_t>(skill->type),
        .p = static_cast<uint32_t>(skill->profession),
        .a = static_cast<uint32_t>(IsPvE() ? 255 - skill->title : static_cast<int>(skill->attribute)),
        .c = static_cast<uint32_t>(skill->campaign),
    };
    if (IsElite()) out.e = 1u;

    skilllist_export::SkillExtras z{
        .sp = skill->special,
        .co = static_cast<uint32_t>(skill->combo),
        .q = skill->weapon_req,
    };
    if (HasExhaustion()) z.x = skill->overcast;
    if (skill->recharge) z.r = skill->recharge;
    if (skill->activation != 0.f) z.c = static_cast<uint32_t>(skill->activation);
    if (IsMaintained()) z.d = 1u;
    if (skill->adrenaline) z.a = skill->adrenaline;
    if (skill->energy_cost) z.e = skill->GetEnergyCost();
    if (skill->health_cost) z.s = skill->health_cost;
    out.z = z;
    return out;
}

const std::wstring SkillListingWindow::Skill::GetSkillType() const
{
    std::wstring str(IsElite() ? L"精英 " : L"");
    switch (skill->type) {
        case GW::Constants::SkillType::Stance:
            return str += L"姿态", str;
        case GW::Constants::SkillType::Hex:
            return str += L"咒文", str;
        case GW::Constants::SkillType::Spell:
            return str += L"魔法", str;
        case GW::Constants::SkillType::Enchantment:
            if (skill->special & 0x800000) {
                str += L"瞬发 ";
            }
            return str += L"增益魔法", str;
        case GW::Constants::SkillType::Signet:
            return str += L"纹章", str;
        case GW::Constants::SkillType::Well:
            return str += L"井", str;
        case GW::Constants::SkillType::Skill:
            return str += L"接触技能", str;
        case GW::Constants::SkillType::Ward:
            return str += L"结界", str;
        case GW::Constants::SkillType::Glyph:
            return str += L"雕文", str;
        case GW::Constants::SkillType::Attack:
            switch (skill->weapon_req) {
                case 1:
                    return str += L"斧系攻击", str;
                case 2:
                    return str += L"弓系攻击", str;
                case 8:
                    switch (skill->combo) {
                        case 1:
                            return str += L"起手攻击", str;
                        case 2:
                            return str += L"副手攻击", str;
                        case 3:
                            return str += L"双重攻击", str;
                    }
                    return str += L"匕首攻击", str;
                case 16:
                    return str += L"锤系攻击", str;
                case 32:
                    return str += L"镰刀攻击", str;
                case 64:
                    return str += L"长矛攻击", str;
                case 70:
                    return str += L"远程攻击", str;
                case 128:
                    return str += L"剑系攻击", str;
            }
            return str += L"近战攻击", str;
        case GW::Constants::SkillType::Shout:
            return str += L"呐喊", str;
        case GW::Constants::SkillType::Preparation:
            return str += L"准备", str;
        case GW::Constants::SkillType::PetAttack:
            return str += L"宠物攻击", str;
        case GW::Constants::SkillType::Trap:
            return str += L"陷阱", str;
        case GW::Constants::SkillType::Ritual:
            switch (skill->profession) {
            case GW::Constants::ProfessionByte::Ritualist:
                    return str += L"束缚仪式", str;
                case GW::Constants::ProfessionByte::Ranger:
                    return str += L"自然仪式", str;
            }
            return str += L"黑檀先锋仪式", str;
        case GW::Constants::SkillType::ItemSpell:
            return str += L"物品魔法", str;
        case GW::Constants::SkillType::WeaponSpell:
            return str += L"武器魔法", str;
        case GW::Constants::SkillType::Form:
            return str += L"形态", str;
        case GW::Constants::SkillType::Chant:
            return str += L"圣歌", str;
        case GW::Constants::SkillType::EchoRefrain:
            return str += L"回响", str;
        default:
            return str += L"技能", str;
    }
}

const wchar_t* SkillListingWindow::Skill::Description()
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

const wchar_t* SkillListingWindow::Skill::Concise()
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
