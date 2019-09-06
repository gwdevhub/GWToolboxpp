#include "stdafx.h"
#include "SkillListingWindow.h"

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>

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
struct SkillName {
    SkillName(uint32_t _name_enc_id, uint32_t _desc_enc_id) : name_enc_id(_name_enc_id), desc_enc_id(_desc_enc_id) {
        if (GW::UI::UInt32ToEncStr(name_enc_id, name_encoded, 16)) {
            GW::UI::AsyncDecodeStr(name_encoded, name_decoded, 256);
        }
            
        if (GW::UI::UInt32ToEncStr(desc_enc_id, desc_encoded, 16)) {
            GW::UI::AsyncDecodeStr(desc_encoded, desc_decoded, 256);
        }
    }
    uint32_t name_enc_id = 0;
    uint32_t desc_enc_id = 0;
    wchar_t name_encoded[16] = { 0 };
    wchar_t desc_encoded[16] = { 0 };
    wchar_t name_decoded[256] = { 0 };
    wchar_t desc_decoded[256] = { 0 };
};
std::map<uint32_t, SkillName*> decoded_skill_names;

static uintptr_t skill_array_addr;

static void printchar(wchar_t c) {
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    }
    else {
        printf("0x%X ", c);
    }
}


void SkillListingWindow::AsyncDecodeStr(wchar_t* buffer, const size_t n, uint32_t enc_num) {
    static wchar_t enc_str[16];
    if (!GW::UI::UInt32ToEncStr(enc_num, enc_str, 16)) {
        buffer[0] = 0;
        return;
    }
    GW::UI::AsyncDecodeStr(enc_str, buffer, n);
}


void SkillListingWindow::Initialize() {
    ToolboxWindow::Initialize();

    {
        uintptr_t address = GW::Scanner::Find(
            "\x8D\x04\xB6\x5E\xC1\xE0\x05\x05", "xxxxxxxx", 8);
        printf("[SCAN] SkillArray = %p\n", (void*)address);
        if (Verify(address))
            skill_array_addr = *(uintptr_t*)address;
    }

    const unsigned int max_skills = 3410;
    if(!skill_array_addr)
        return;
    GW::Skill* skill_constants = (GW::Skill*)skill_array_addr;

    size_t added = 0;
    for (size_t i = 0; i < max_skills && added < 10; i++) {
        GW::Skill s = skill_constants[i];
        if (!s.skill_id) continue;
        if (s.skill_id != 1032) continue;
        if (s.h0098) {
            SkillName* n = new SkillName(s.name,s.h0098);
            decoded_skill_names[s.skill_id] = n;
            added++;
        }
    }
    Log::Log("%d Added\n", added);  
}
void SkillListingWindow::Update(float delta) {
    for (std::map<uint32_t, SkillName*>::iterator it = decoded_skill_names.begin(); it != decoded_skill_names.end(); ++it) {
        if (it->second->name_decoded[0] && it->second->desc_decoded[0]) {
            for (int i = 0; it->second->name_encoded[i] != 0; ++i) printchar(it->second->name_encoded[i]);
            printf("\n");
            Log::LogW(L"Skill ID %d = %d = %ls = %ls\n", it->first, it->second->desc_enc_id, it->second->desc_encoded, it->second->desc_decoded);
            decoded_skill_names.erase(it);
            if (!decoded_skill_names.size()) break;
        }
    }
}
