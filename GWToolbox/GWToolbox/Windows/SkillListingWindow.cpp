#include "stdafx.h"
#include "SkillListingWindow.h"

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>

#include "GuiUtils.h"
#include <Modules\Resources.h>
#include "logger.h"

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

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
        json[skills[i]->skill->skill_id] = skills[i]->ToJson();
    }
    std::wstring file_location = Resources::GetPath(L"skills.json");
    if (PathFileExistsW(file_location.c_str())) {
        DeleteFileW(file_location.c_str());
    }

    std::ofstream out(file_location);
    out << json.dump();
    out.close();
    Log::Info("Skills exported to %ls", file_location.c_str());
}
void SkillListingWindow::Initialize() {
    ToolboxWindow::Initialize();
    const unsigned int max_skills = 3410;
    skills.resize(max_skills);
    size_t added = 0;
    for (size_t i = 0; i < max_skills && added < 1000; i++) {
        GW::Skill* s = &GW::SkillbarMgr::GetSkillConstantData(i);
        if (!s || s->skill_id || !s->skill_equip_type) continue;
        skills[i] = new Skill(s);
    }
}
void SkillListingWindow::Draw(IDirect3DDevice9* pDevice) {
    if (!visible)
        return;
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    float offset = 0.0f;
    const float tiny_text_width = 50.0f * ImGui::GetIO().FontGlobalScale;
    const float short_text_width = 80.0f * ImGui::GetIO().FontGlobalScale;
    const float avail_width = ImGui::GetContentRegionAvailWidth();
    const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;
    ImGui::Text("#");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Name");
    ImGui::SameLine(offset += long_text_width);
    ImGui::Text("Attr");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Prof");
    ImGui::SameLine(offset += tiny_text_width);
    ImGui::Text("Type");
    ImGui::Separator();
    bool has_entries = 0;
    for (size_t i = 0; i < skills.size(); i++) {
        if (!skills[i]) continue;
        ImGui::Text("%d", i);
        if (!ImGui::IsItemVisible())
            continue;
        offset = 0;
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%S",skills[i]->Name());
        if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%S,\n%S", skills[i]->GWWDescription(), "");// skills[i]->GWWConcise());
        ImGui::SameLine(offset += long_text_width);
        ImGui::Text("%d", skills[i]->skill->attribute);
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%s", GW::Constants::GetProfessionAcronym((GW::Constants::Profession)skills[i]->skill->profession));
        ImGui::SameLine(offset += tiny_text_width);
        ImGui::Text("%d", skills[i]->skill->type);
    }
    if (ImGui::Button("Export to JSON"))
        ExportToJSON();
    ImGui::End();
}
