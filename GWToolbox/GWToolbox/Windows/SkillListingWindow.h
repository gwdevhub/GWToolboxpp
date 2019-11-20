#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>
#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include "Timer.h"
#include "GuiUtils.h"

class SkillListingWindow : public ToolboxWindow {
public:
    class Skill {
    public:
        Skill(GW::Skill* _gw_skill) : skill(_gw_skill) {

        }
        wchar_t* Name() {
            if (!name_enc[0] && GW::UI::UInt32ToEncStr(skill->name, name_enc, 16))
                GW::UI::AsyncDecodeStr(name_enc, name_dec, 256);
            return name_dec;
        }
        wchar_t* Description() {
            if (!desc_enc[0] && GW::UI::UInt32ToEncStr(skill->description, desc_enc, 16)) {
                wchar_t buf[64] = { 0 };
                swprintf(buf, 64, L"%s\x10A\x108\x107%d..%d\x1\x1\x10B\x108\x107%d..%d\x1\x1\x10C\x108\x107%d..%d\x1\x1", desc_enc, skill->scale0, skill->scale15, skill->bonusScale0, skill->bonusScale15, skill->duration0, skill->duration15);
                for (size_t i = 0; buf[i]; i++)
                    desc_enc[i] = buf[i];
                GW::UI::AsyncDecodeStr(desc_enc, desc_dec, 256);
            }
            return desc_dec;
        }
        wchar_t* Concise() {
            if (!concise_enc[0] && GW::UI::UInt32ToEncStr(skill->h0098, concise_enc, 16)) {
                wchar_t buf[64] = { 0 };
                swprintf(buf, 64, L"%s\x10A\x108\x107%d..%d\x1\x1\x10B\x108\x107%d..%d\x1\x1\x10C\x108\x107%d..%d\x1\x1", concise_enc, skill->scale0, skill->scale15, skill->bonusScale0, skill->bonusScale15, skill->duration0, skill->duration15);
                for (size_t i = 0; buf[i]; i++)
                    concise_enc[i] = buf[i];
                GW::UI::AsyncDecodeStr(concise_enc, concise_dec, 256);
            }
            return concise_dec;
        }
        GW::Skill* skill;
    private:
        wchar_t name_enc[64] = { 0 };
        wchar_t name_dec[256] = { 0 };
        wchar_t desc_enc[64] = { 0 };
        wchar_t desc_dec[256] = { 0 };
        wchar_t concise_enc[64] = { 0 };
        wchar_t concise_dec[256] = { 0 };
    };
private:
    SkillListingWindow() {};
    ~SkillListingWindow() {
        for (unsigned int i = 0; i < skills.size(); i++) {
            delete skills[i];
        }
        skills.clear();
    };

    std::vector<Skill* > skills;
public:
    static SkillListingWindow& Instance() {
        static SkillListingWindow instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars Skill List"; }
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;
	void Terminate() override;

};
