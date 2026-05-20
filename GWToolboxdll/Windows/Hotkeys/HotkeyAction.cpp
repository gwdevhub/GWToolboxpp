#include "stdafx.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ImGuiAddons.h>

#include <Windows/Hotkeys/HotkeyAction.h>

const char* HotkeyAction::GetText(void*, int idx)
{
    switch (static_cast<Action>(idx)) {
        case OpenXunlaiChest:
            return "Open Xunlai Chest";
        case DropGoldCoin:
            return "Drop Gold Coin";
        case ReapplyTitle:
            return "Reapply appropriate Title";
        case EnterChallenge:
            return "Enter Challenge";
    }
    return nullptr;
}

HotkeyAction::HotkeyAction(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    action = ini ? static_cast<Action>(ini->GetLongValue(section, "ActionID", OpenXunlaiChest)) : OpenXunlaiChest;
}

void HotkeyAction::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", action);
}

int HotkeyAction::Description(char* buf, const size_t bufsz)
{
    const char* name = GetText(nullptr, action);
    return snprintf(buf, bufsz, "%s", name ? name : "Unknown");
}

bool HotkeyAction::Draw()
{
    if (ImGui::BeginCombo("Actions###actionscombo", GetText(nullptr, action))) {
        for (const auto hk_action : { OpenXunlaiChest, DropGoldCoin, ReapplyTitle, EnterChallenge }) {
            const bool selected = action == hk_action;
            if (ImGui::Selectable(GetText(nullptr, hk_action), selected)) {
                action = hk_action;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return true;
}

void HotkeyAction::Execute()
{
    if (!CanUse()) {
        return;
    }
    switch (action) {
        case OpenXunlaiChest:
            GW::GameThread::Enqueue([] {
                const auto frame = GW::UI::GetFrameByLabel(L"InvAccount");
                if (frame) {
                    GW::UI::DestroyUIComponent(frame);
                }
                else {
                    GW::Items::OpenXunlaiWindow();
                }
                });
            break;
        case DropGoldCoin:
            if (isExplorable()) {
                GW::Items::DropGold(1);
            }
            break;
        case ReapplyTitle: {
            GW::Chat::SendChat('/', L"title");
            break;
        }
        case EnterChallenge:
            GW::Chat::SendChat('/', L"enter");
            break;
    }
}
