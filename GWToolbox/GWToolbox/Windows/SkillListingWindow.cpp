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


void SkillListingWindow::Initialize() {
    ToolboxWindow::Initialize();
    const unsigned int max_skills = 3410;
    skills.resize(max_skills);
    {
        uintptr_t address = GW::Scanner::Find(
            "\x8D\x04\xB6\x5E\xC1\xE0\x05\x05", "xxxxxxxx", 8);
        printf("[SCAN] SkillArray = %p\n", (void*)address);
        if (Verify(address))
            skill_array_addr = *(uintptr_t*)address;
    }

    
    if(!skill_array_addr)
        return;
    GW::Skill* skill_constants = (GW::Skill*)skill_array_addr;

    size_t added = 0;
    for (size_t i = 0; i < max_skills && added < 1000; i++) {
        if (!skill_constants[i].skill_id) continue;
        skills[i] = new Skill(&skill_constants[i]);
        //added++;
    }
    //Log::Log("%d Added\n", added);  
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
    //ImGui::SameLine(0,short_text_width);
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
            ImGui::SetTooltip("%S,\n%S", skills[i]->Description(), skills[i]->Concise());
        ImGui::SameLine(offset += long_text_width);
        ImGui::Text("%d, %d, %d", skills[i]->skill->attribute, skills[i]->skill->profession, skills[i]->skill->type);
        //ImGui::SameLine(0, short_text_width);
    }
    ImGui::End();
}
void SkillListingWindow::Update(float delta) {
    
}
void SkillListingWindow::Terminate() {
	for (auto sk : skills) {
		delete sk;
	}
	skills.clear();
}
