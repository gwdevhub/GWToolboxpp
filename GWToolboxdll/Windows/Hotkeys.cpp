#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Packets/Opcodes.h>

#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <ImGuiAddons.h>
#include <Keys.h>
#include <Logger.h>

#include <Modules/ChatCommands.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/Hotkeys.h>
#include <Windows/HotkeysWindow.h>
#include <Windows/PconsWindow.h>


bool TBHotkey::show_active_in_header = true;
bool TBHotkey::show_run_in_header = true;
bool TBHotkey::hotkeys_changed = false;
unsigned int TBHotkey::cur_ui_id = 0;

TBHotkey *TBHotkey::HotkeyFactory(CSimpleIni *ini, const char *section)
{
    std::string str(section);
    if (str.compare(0, 7, "hotkey-") != 0)
        return nullptr;
    size_t first_sep = 6;
    size_t second_sep = str.find(L':', first_sep);
    std::string id = str.substr(first_sep + 1, second_sep - first_sep - 1);
    std::string type = str.substr(second_sep + 1);

    if (type.compare(HotkeySendChat::IniSection()) == 0) {
        return new HotkeySendChat(ini, section);
    } else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
        return new HotkeyUseItem(ini, section);
    } else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
        return new HotkeyDropUseBuff(ini, section);
    } else if (type.compare(HotkeyToggle::IniSection()) == 0 &&
               HotkeyToggle::IsValid(ini, section)) {
        return new HotkeyToggle(ini, section);
    } else if (type.compare(HotkeyAction::IniSection()) == 0) {
        return new HotkeyAction(ini, section);
    } else if (type.compare(HotkeyTarget::IniSection()) == 0) {
        return new HotkeyTarget(ini, section);
    } else if (type.compare(HotkeyMove::IniSection()) == 0) {
        return new HotkeyMove(ini, section);
    } else if (type.compare(HotkeyDialog::IniSection()) == 0) {
        return new HotkeyDialog(ini, section);
    } else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
        return new HotkeyPingBuild(ini, section);
    } else if (type.compare(HotkeyHeroTeamBuild::IniSection()) == 0) {
        return new HotkeyHeroTeamBuild(ini, section);
    } else if (type.compare(HotkeyEquipItem::IniSection()) == 0) {
        return new HotkeyEquipItem(ini, section);
    } else {
        return nullptr;
    }
}

TBHotkey::TBHotkey(CSimpleIni *ini, const char *section)
    : ui_id(++cur_ui_id)
{
    if (ini) {
        hotkey = ini->GetLongValue(section, VAR_NAME(hotkey), hotkey);
        modifier = ini->GetLongValue(section, VAR_NAME(modifier), modifier);
        active = ini->GetBoolValue(section, VAR_NAME(active), active);
        map_id = ini->GetLongValue(section, VAR_NAME(map_id), map_id);
        prof_id = ini->GetLongValue(section, VAR_NAME(prof_id), prof_id);
        show_message_in_emote_channel =
            ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                              show_message_in_emote_channel);
        show_error_on_failure = ini->GetBoolValue(
            section, VAR_NAME(show_error_on_failure), show_error_on_failure);
        block_gw = ini->GetBoolValue(section, VAR_NAME(block_gw), block_gw);
        trigger_on_explorable = ini->GetBoolValue(
            section, VAR_NAME(trigger_on_explorable), trigger_on_explorable);
        trigger_on_outpost = ini->GetBoolValue(
            section, VAR_NAME(trigger_on_outpost), trigger_on_outpost);
    }
}
bool TBHotkey::CanUse()
{
    return !isLoading() && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
}
void TBHotkey::Save(CSimpleIni *ini, const char *section) const
{
    ini->SetLongValue(section, VAR_NAME(hotkey), hotkey);
    ini->SetLongValue(section, VAR_NAME(map_id), map_id);
    ini->SetLongValue(section, VAR_NAME(prof_id), prof_id);
    ini->SetLongValue(section, VAR_NAME(modifier), modifier);
    ini->SetBoolValue(section, VAR_NAME(active), active);
    ini->SetBoolValue(section, VAR_NAME(block_gw), block_gw);
    ini->SetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                      show_message_in_emote_channel);
    ini->SetBoolValue(section, VAR_NAME(show_error_on_failure),
                      show_error_on_failure);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_explorable),
                      trigger_on_explorable);
    ini->SetBoolValue(section, VAR_NAME(trigger_on_outpost),
                      trigger_on_outpost);
}
static const char *professions[] = {"Any",          "Warrior",     "Ranger",
                                    "Monk",         "Necromancer", "Mesmer",
                                    "Elementalist", "Assassin",    "Ritualist",
                                    "Paragon",      "Dervish"};
static const char *instance_types[] = {"Any", "Outpost", "Explorable"};
void TBHotkey::Draw(Op *op)
{
    auto ShowHeaderButtons = [&]() {
        if (show_active_in_header || show_run_in_header) {
            ImGui::PushID(static_cast<int>(ui_id));
            ImGui::PushID("header");
            ImGuiStyle &style = ImGui::GetStyle();
            const float btn_width = 50.0f * ImGui::GetIO().FontGlobalScale;
            if (show_active_in_header) {
                ImGui::SameLine(
                    ImGui::GetContentRegionAvailWidth() -
                    ImGui::GetTextLineHeight() - style.FramePadding.y * 2 -
                    (show_run_in_header
                         ? (btn_width + ImGui::GetStyle().ItemSpacing.x)
                         : 0));
                if (ImGui::Checkbox("", &active))
                    hotkeys_changed = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "The hotkey can trigger only when selected");
            }
            if (show_run_in_header) {
                ImGui::SameLine(ImGui::GetContentRegionAvailWidth() -
                                btn_width);
                if (ImGui::Button("Run", ImVec2(btn_width, 0.0f))) {
                    Execute();
                }
            }
            ImGui::PopID();
            ImGui::PopID();
        }
    };

    // === Header ===
    char header[256];
    char desbuf[128];
    char keybuf[128];
    char profbuf[64] = {'\0'};
    char mapbuf[64] = {'\0'};
    if (prof_id)
        snprintf(profbuf, 64, " [%s]", professions[prof_id]);
    if (map_id) {
        if (map_id <= 729)
            snprintf(mapbuf, 64, " [%s]", GW::Constants::NAME_FROM_ID[map_id]);
        else
            snprintf(mapbuf, 64, " [Map %d]", map_id);
    }
    Description(desbuf, 128);
    ModKeyName(keybuf, 128, modifier, hotkey, "<None>");
    snprintf(header, 128, "%s [%s]%s%s###header%u", desbuf, keybuf, profbuf,
             mapbuf, ui_id);
    ImGuiTreeNodeFlags flags = (show_active_in_header || show_run_in_header)
                                   ? ImGuiTreeNodeFlags_AllowItemOverlap
                                   : 0;
    if (!ImGui::CollapsingHeader(header, flags)) {
        ShowHeaderButtons();
    } else {
        ShowHeaderButtons();
        ImGui::PushID(static_cast<int>(ui_id));
        ImGui::PushItemWidth(-70.0f);
        // === Specific section ===
        Draw();

        // === Hotkey section ===
        if (ImGui::Checkbox("Block key in Guild Wars when triggered",
                            &block_gw))
            hotkeys_changed = true;
        ImGui::ShowHelp(
            "When triggered, this hotkey will prevent Guild Wars from receiving the keypress event");
        if (ImGui::Checkbox("Trigger hotkey when entering explorable area",
                            &trigger_on_explorable))
            hotkeys_changed = true;
        if (ImGui::Checkbox("Trigger hotkey when entering outpost",
                            &trigger_on_outpost))
            hotkeys_changed = true;
        if (ImGui::InputInt("Map ID", &map_id))
            hotkeys_changed = true;
        ImGui::ShowHelp(
            "The hotkey can only trigger in the selected map (0 = Any map)");

        if (ImGui::Combo("Profession", &prof_id, professions, 11))
            hotkeys_changed = true;
        ImGui::ShowHelp(
            "The hotkey can only trigger when player is the selected primary profession (0 = Any profession)");
        ImGui::Separator();
        if (ImGui::Checkbox("###active", &active))
            hotkeys_changed = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The hotkey can trigger only when selected");
        ImGui::SameLine();
        static LONG newkey = 0;
        char keybuf2[128];
        snprintf(keybuf2, 128, "Hotkey: %s", keybuf);
        if (ImGui::Button(keybuf2, ImVec2(-70.0f, 0))) {
            ImGui::OpenPopup("Select Hotkey");
            newkey = 0;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to change hotkey");
        if (ImGui::BeginPopup("Select Hotkey")) {
            *op = Op_BlockInput;
            ImGui::Text("Press key");
            int newmod = 0;
            if (ImGui::GetIO().KeyCtrl)
                newmod |= ModKey_Control;
            if (ImGui::GetIO().KeyAlt)
                newmod |= ModKey_Alt;
            if (ImGui::GetIO().KeyShift)
                newmod |= ModKey_Shift;

            if (newkey == 0) { // we are looking for the key
                for (int i = 0; i < 512; ++i) {
                    if (i == VK_CONTROL)
                        continue;
                    if (i == VK_SHIFT)
                        continue;
                    if (i == VK_MENU)
                        continue;
                    if (ImGui::GetIO().KeysDown[i]) {
                        newkey = i;
                    }
                }
            } else { // key was pressed, close if it's released
                if (!ImGui::GetIO().KeysDown[newkey]) {
                    hotkey = newkey;
                    modifier = newmod;
                    ImGui::CloseCurrentPopup();
                    hotkeys_changed = true;
                }
            }

            // write the key
            char newkey_buf[256];
            ModKeyName(newkey_buf, 256, newmod, newkey);
            ImGui::Text("%s", newkey_buf);
            if (ImGui::Button("Cancel", ImVec2(-1.0f, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Run", ImVec2(70.0f, 0.0f))) {
            Execute();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Execute the hotkey now");

        // === Move and delete buttons ===
        if (ImGui::Button("Move Up",
                          ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f,
                                 0))) {
            *op = Op_MoveUp;
            hotkeys_changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Move the hotkey up in the list");
        ImGui::SameLine();
        if (ImGui::Button("Move Down",
                          ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f,
                                 0))) {
            *op = Op_MoveDown;
            hotkeys_changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Move the hotkey down in the list");
        ImGui::SameLine();
        if (ImGui::Button("Delete",
                          ImVec2(ImGui::GetWindowContentRegionWidth() / 3.0f,
                                 0))) {
            ImGui::OpenPopup("Delete Hotkey?");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delete the hotkey");
        if (ImGui::BeginPopupModal("Delete Hotkey?", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n",
                        Name());
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                *op = Op_Delete;
                hotkeys_changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
}

HotkeySendChat::HotkeySendChat(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    strcpy_s(message, ini->GetValue(section, "msg", ""));
    channel = ini->GetValue(section, "channel", "/")[0];
}
void HotkeySendChat::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetValue(section, "msg", message);
    char buf[8];
    snprintf(buf, 8, "%c", channel);
    ini->SetValue(section, "channel", buf);
}
void HotkeySendChat::Description(char *buf, size_t bufsz) const
{
    snprintf(buf, bufsz, "Send chat '%c%s'", channel, message);
}
void HotkeySendChat::Draw()
{
    int index = 0;
    switch (channel) {
        case '/':
            index = 0;
            break;
        case '!':
            index = 1;
            break;
        case '@':
            index = 2;
            break;
        case '#':
            index = 3;
            break;
        case '$':
            index = 4;
            break;
        case '%':
            index = 5;
            break;
        case '"':
            index = 6;
            break;
    }
    static const char *channels[] = {"/ Commands", "! All",   "@ Guild",
                                     "# Group",    "$ Trade", "% Alliance",
                                     "\" Whisper"};
    if (ImGui::Combo("Channel", &index, channels, 7)) {
        switch (index) {
            case 0:
                channel = '/';
                break;
            case 1:
                channel = '!';
                break;
            case 2:
                channel = '@';
                break;
            case 3:
                channel = '#';
                break;
            case 4:
                channel = '$';
                break;
            case 5:
                channel = '%';
                break;
            case 6:
                channel = '"';
                break;
            default:
                channel = '/';
                break;
        }
        show_message_in_emote_channel = channel == '/' &&
                                        show_message_in_emote_channel;
        hotkeys_changed = true;
    }
    if (ImGui::InputText("Message", message, 139))
        hotkeys_changed = true;
    if (channel == '/' && ImGui::Checkbox("Display message when triggered",
                                          &show_message_in_emote_channel))
        hotkeys_changed = true;
}
void HotkeySendChat::Execute()
{
    if (!CanUse())
        return;
    if (show_message_in_emote_channel && channel == L'/') {
        Log::Info("/%s", message);
    }
    GW::Chat::SendChat(channel, message);
}

HotkeyUseItem::HotkeyUseItem(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    item_id = static_cast<size_t>(ini->GetLongValue(section, "ItemID", 0));
    strcpy_s(name, ini->GetValue(section, "ItemName", ""));
}
void HotkeyUseItem::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ItemID", static_cast<long>(item_id));
    ini->SetValue(section, "ItemName", name);
}
void HotkeyUseItem::Description(char *buf, size_t bufsz) const
{
    if (name[0] == '\0') {
        snprintf(buf, bufsz, "Use #%d", item_id);
    } else {
        snprintf(buf, bufsz, "Use %s", name);
    }
}
void HotkeyUseItem::Draw()
{
    if (ImGui::InputInt("Item ID", (int *)&item_id))
        hotkeys_changed = true;
    if (ImGui::InputText("Item Name", name, 140))
        hotkeys_changed = true;
    if (ImGui::Checkbox("Display error message on failure",
                        &show_error_on_failure))
        hotkeys_changed = true;
}
void HotkeyUseItem::Execute()
{
    if (!CanUse())
        return;
    if (item_id == 0)
        return;

    bool used = GW::Items::UseItemByModelId(item_id, 1, 4);
    if (!used &&
        GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        used = GW::Items::UseItemByModelId(item_id, 8, 16);
    }

    if (!used && show_error_on_failure) {
        if (name[0] == '\0') {
            Log::Error("Item #%d not found!", item_id);
        } else {
            Log::Error("%s not found!", name);
        }
    }
}

HotkeyEquipItem::HotkeyEquipItem(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    // @Cleanup: Add error handling
    bag_idx = static_cast<size_t>(ini->GetLongValue(section, "Bag", 0));
    slot_idx = static_cast<size_t>(ini->GetLongValue(section, "Slot", 0));
}
void HotkeyEquipItem::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "Bag", static_cast<long>(bag_idx));
    ini->SetLongValue(section, "Slot", static_cast<long>(slot_idx));
}
void HotkeyEquipItem::Description(char *buf, size_t bufsz) const
{
    snprintf(buf, bufsz, "Equip Item in bag %d slot %d", bag_idx, slot_idx);
}
void HotkeyEquipItem::Draw()
{
    if (ImGui::InputInt("Bag (1-5)", (int *)&bag_idx))
        hotkeys_changed = true;
    if (ImGui::InputInt("Slot (1-25)", (int *)&slot_idx))
        hotkeys_changed = true;
    if (ImGui::Checkbox("Display error message on failure",
                        &show_error_on_failure))
        hotkeys_changed = true;
}
bool HotkeyEquipItem::IsEquippable(GW::Item *_item)
{
    if (!_item)
        return false;
    switch (static_cast<GW::Constants::ItemType>(_item->type)) {
        case GW::Constants::ItemType::Axe:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Gloves:
        case GW::Constants::ItemType::Hammer:
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Leggings:
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Shield:
        case GW::Constants::ItemType::Staff:
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Daggers:
        case GW::Constants::ItemType::Scythe:
        case GW::Constants::ItemType::Spear:
        case GW::Constants::ItemType::Costume:
            break;
        default:
            return false;
            break;
    }
    if (!_item->customized)
        return true;
    GW::GameContext *g = GW::GameContext::instance();
    GW::CharContext *c = g ? g->character : nullptr;
    return c && c->player_name &&
           wcscmp(c->player_name, _item->customized) == 0;
}
void HotkeyEquipItem::Execute()
{
    if (!CanUse())
        return;
    if (!ongoing) {
        if (bag_idx < 1 || bag_idx > 5 || slot_idx < 1 || slot_idx > 25) {
            if (show_error_on_failure)
                Log::Error("Invalid bag slot %d/%d!", bag_idx, slot_idx);
            return;
        }
        GW::Bag *b = GW::Items::GetBag(bag_idx);
        if (!b) {
            if (show_error_on_failure)
                Log::Error("Bag #%d not found!", bag_idx);
            return;
        }
        GW::ItemArray items = b->items;
        if (!items.valid() || slot_idx > items.size()) {
            if (show_error_on_failure)
                Log::Error("Invalid bag slot %d/%d!", bag_idx, slot_idx);
            return;
        }
        item = items.at(slot_idx - 1);
        if (!IsEquippable(item)) {
            if (show_error_on_failure)
                Log::Error("No equippable item in bag %d slot %d", bag_idx,
                           slot_idx);
            item = nullptr;
            return;
        }
        ongoing = true;
        start_time = std::chrono::steady_clock::now();
    } else {
        last_try = std::chrono::steady_clock::now();
        __int64 diff_mills =
            std::chrono::duration_cast<std::chrono::milliseconds>(last_try -
                                                                  start_time)
                .count();
        if (diff_mills < 500) {
            return; // Wait 250ms between tries.
        }
        if (diff_mills > 5000) {
            if (show_error_on_failure)
                Log::Error("Failed to equip item in bag %d slot %d", bag_idx,
                           slot_idx);
            ongoing = false;
            item = nullptr;
            return;
        }
    }

    if (!item || !item->item_id) {
        if (show_error_on_failure)
            Log::Error("Failed to equip item in bag %d slot %d", bag_idx,
                       slot_idx);
        ongoing = false;
        item = nullptr;
        return;
    }
    if (item->bag && item->bag->bag_type == 2) {
        // Log::Info("Success!");
        ongoing = false;
        item = nullptr;
        return; // Success!
    }
    GW::AgentLiving *p = GW::Agents::GetPlayerAsAgentLiving();
    if (!p || p->GetIsKnockedDown()) {
        // Log::Info("knocked down or missing"); // Player knocked down or
        // casting; wait.
        return;
    }
    if (p->skill) {
        GW::CtoS::SendPacket(0x4, GAME_CMSG_CANCEL_MOVEMENT); // Cancel action
                                                              // if casting a
                                                              // skill. Return
                                                              // here and wait
                                                              // before
                                                              // equipping
                                                              // items.
        // Log::Info("cancel action");
        return;
    }
    if (p->GetIsIdle() || p->GetIsMoving()) {
        GW::Items::EquipItem(item);
        // Log::Info("equip %d", item->item_id);
    } else {
        GW::Agents::Move(p->pos); // Move to clear model state e.g. attacking,
                                  // aftercast
        // Log::Info("not idle nor moving, %d",p->model_state);
    }
}

bool HotkeyDropUseBuff::GetText(void *data, int idx, const char **out_text)
{
    static char other_buf[64];
    switch ((SkillIndex)idx) {
        case Recall:
            *out_text = "Recall";
            break;
        case UA:
            *out_text = "UA";
            break;
        case HolyVeil:
            *out_text = "Holy Veil";
            break;
        default:
            snprintf(other_buf, 64, "Skill#%d", (int)data);
            *out_text = other_buf;
            break;
    }
    return true;
}
HotkeyDropUseBuff::SkillIndex HotkeyDropUseBuff::GetIndex() const
{
    switch (id) {
        case GW::Constants::SkillID::Recall:
            return Recall;
        case GW::Constants::SkillID::Unyielding_Aura:
            return UA;
        case GW::Constants::SkillID::Holy_Veil:
            return HolyVeil;
        default:
            return Other;
    }
}
HotkeyDropUseBuff::HotkeyDropUseBuff(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    id = (GW::Constants::SkillID)ini->GetLongValue(
        section, "SkillID", (long)GW::Constants::SkillID::Recall);
}
void HotkeyDropUseBuff::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "SkillID", (long)id);
}
void HotkeyDropUseBuff::Description(char *buf, size_t bufsz) const
{
    const char *skillname;
    GetText((void *)id, GetIndex(), &skillname);
    snprintf(buf, bufsz, "Drop/Use %s", skillname);
}
void HotkeyDropUseBuff::Draw()
{
    SkillIndex index = GetIndex();
    if (ImGui::Combo("Skill", (int *)&index,
                     "Recall\0Unyielding Aura\0Holy Veil\0Other", 4)) {
        switch (index) {
            case HotkeyDropUseBuff::Recall:
                id = GW::Constants::SkillID::Recall;
                break;
            case HotkeyDropUseBuff::UA:
                id = GW::Constants::SkillID::Unyielding_Aura;
                break;
            case HotkeyDropUseBuff::HolyVeil:
                id = GW::Constants::SkillID::Holy_Veil;
                break;
            case HotkeyDropUseBuff::Other:
                id = (GW::Constants::SkillID)0;
                break;
            default:
                break;
        }
        hotkeys_changed = true;
    }
    if (index == Other) {
        if (ImGui::InputInt("Skill ID", (int *)&id))
            hotkeys_changed = true;
    }
}
void HotkeyDropUseBuff::Execute()
{
    if (!CanUse())
        return;

    GW::Buff *buff = GW::Effects::GetPlayerBuffBySkillId(id);
    if (buff && buff->skill_id) {
        GW::Effects::DropBuff(buff->buff_id);
    } else {
        int islot = GW::SkillbarMgr::GetSkillSlot(id);
        if (islot >= 0) {
            uint32_t slot = static_cast<uint32_t>(islot);
            if (GW::SkillbarMgr::GetPlayerSkillbar()->skills[slot].recharge == 0) {
                GW::GameThread::Enqueue([slot] () -> void {
                    GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId(),
                        static_cast<uint32_t>(ImGui::IsKeyDown(VK_CONTROL)));
                });
            }
        }
    }
}

bool HotkeyToggle::GetText(void *, int idx, const char **out_text)
{
    switch ((Toggle)idx) {
        case Clicker:
            *out_text = "Clicker";
            return true;
        case Pcons:
            *out_text = "Pcons";
            return true;
        case CoinDrop:
            *out_text = "Coin Drop";
            return true;
        case Tick:
            *out_text = "Tick";
            return true;
        default:
            return false;
    }
}

bool HotkeyToggle::IsValid(CSimpleIni *ini, const char *section)
{
    switch (ini->GetLongValue(section, "ToggleID", static_cast<long>(Clicker))) {
        case static_cast<long>(Clicker):
        case static_cast<long>(Pcons):
        case static_cast<long>(CoinDrop):
        case static_cast<long>(Tick):
            return true;
        default:
            return false;
    }
}
HotkeyToggle::HotkeyToggle(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    target = (Toggle)ini->GetLongValue(section, "ToggleID", (long)Clicker);
}
void HotkeyToggle::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ToggleID", (long)target);
}
void HotkeyToggle::Description(char *buf, size_t bufsz) const
{
    const char *name;
    GetText(nullptr, (int)target, &name);
    snprintf(buf, bufsz, "Toggle %s", name);
}
void HotkeyToggle::Draw()
{
    if (ImGui::Combo("Toggle###combo", (int *)&target, GetText, nullptr,
                     n_targets))
        hotkeys_changed = true;
    if (ImGui::Checkbox("Display message when triggered",
                        &show_message_in_emote_channel))
        hotkeys_changed = true;
}
void HotkeyToggle::Execute()
{
    bool _active;
    switch (target) {
        case HotkeyToggle::Clicker:
            _active = HotkeysWindow::Instance().ToggleClicker();
            if (show_message_in_emote_channel)
                Log::Info("Clicker is %s", _active ? "active" : "disabled");
            break;
        case HotkeyToggle::Pcons:
            PconsWindow::Instance().ToggleEnable();
            // writing to chat is done by ToggleActive if needed
            break;
        case HotkeyToggle::CoinDrop:
            _active = HotkeysWindow::Instance().ToggleCoinDrop();
            if (show_message_in_emote_channel)
                Log::Info("Coin dropper is %s",
                          _active ? "active" : "disabled");
            break;
        case HotkeyToggle::Tick:
            const auto ticked = GW::PartyMgr::GetIsPlayerTicked();
            GW::PartyMgr::Tick(!ticked);
            break;
    }
}

bool HotkeyAction::GetText(void *, int idx, const char **out_text)
{
    switch ((Action)idx) {
        case OpenXunlaiChest:
            *out_text = "Open Xunlai Chest";
            return true;
        case OpenLockedChest:
            *out_text = "Open Locked Chest";
            return true;
        case DropGoldCoin:
            *out_text = "Drop Gold Coin";
            return true;
        case ReapplyTitle:
            *out_text = "Reapply appropriate Title";
            return true;
        case EnterChallenge:
            *out_text = "Enter Challenge";
            return true;
        default:
            return false;
    }
}
HotkeyAction::HotkeyAction(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    action =
        (Action)ini->GetLongValue(section, "ActionID", (long)OpenXunlaiChest);
}
void HotkeyAction::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "ActionID", (long)action);
}
void HotkeyAction::Description(char *buf, size_t bufsz) const
{
    const char *name;
    GetText(nullptr, (int)action, &name);
    snprintf(buf, bufsz, "%s", name);
}
void HotkeyAction::Draw()
{
    if (ImGui::Combo("Action###combo", (int *)&action, GetText, nullptr,
                     n_actions))
        hotkeys_changed = true;
}
void HotkeyAction::Execute()
{
    if (!CanUse())
        return;
    switch (action) {
        case HotkeyAction::OpenXunlaiChest:
            if (isOutpost()) {
                GW::GameThread::Enqueue([]() { 
                    GW::Items::OpenXunlaiWindow();
                    });
            }
            break;
        case HotkeyAction::OpenLockedChest: {
            if (isExplorable()) {
                GW::Agent *target = GW::Agents::GetTarget();
                if (target && target->type == 0x200) {
                    GW::Agents::GoSignpost(target);
                    GW::Items::OpenLockedChest();
                }
            }
            break;
        }
        case HotkeyAction::DropGoldCoin:
            if (isExplorable()) {
                GW::Items::DropGold(1);
            }
            break;
        case HotkeyAction::ReapplyTitle:
            ChatCommands::CmdReapplyTitle(nullptr, 0, nullptr);
            break;
        case HotkeyAction::EnterChallenge:
            GW::Map::EnterChallenge();
            break;
    }
}

HotkeyTarget::HotkeyTarget(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    // don't print target hotkey to chat by default
    show_message_in_emote_channel = false;
    name[0] = 0;
    if (!ini)
        return;
    long ini_id = ini->GetLongValue(section, "TargetID", -1);
    if (ini_id > 0)
        id = static_cast<uint32_t>(ini_id);
    long ini_type = ini->GetLongValue(section, "TargetType", -1);
    if (ini_type >= HotkeyTargetType::NPC && ini_type < HotkeyTargetType::Count)
        type = static_cast<HotkeyTargetType>(ini_type);
    std::string ini_name = ini->GetValue(section, "TargetName", "");
    strcpy_s(name, ini_name.substr(0, sizeof(name)-1).c_str());
    name[sizeof(name)-1] = 0;
    
    ini->GetBoolValue(section, VAR_NAME(show_message_in_emote_channel),
                        show_message_in_emote_channel);
}
void HotkeyTarget::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "TargetID", static_cast<long>(id));
    ini->SetLongValue(section, "TargetType", static_cast<long>(type));
    ini->SetValue(section, "TargetName", name);
}
void HotkeyTarget::Description(char *buf, size_t bufsz) const
{
    if (!name[0]) {
        snprintf(buf, bufsz, "Target %s #%d", type_labels[type], id);
    } else {
        snprintf(buf, bufsz, "Target %s", name);
    }
}
void HotkeyTarget::Draw()
{
    ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() / 3);
    hotkeys_changed |= ImGui::Combo("Type", (int *)&type, type_labels, 3);
    ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
    hotkeys_changed |= ImGui::InputInt(identifier_labels[type], (int *)&id);
    ImGui::PopItemWidth();
    hotkeys_changed |= ImGui::InputText("Name", name, 140);
    hotkeys_changed |= ImGui::Checkbox("Display message when triggered", &show_message_in_emote_channel);
}
void HotkeyTarget::Execute()
{
    if (!CanUse() || id == 0)
        return;

    const GW::AgentArray& agents = GW::Agents::GetAgentArray();
    if (!agents.valid())
        return;

    GW::Agent *me = agents[GW::Agents::GetPlayerId()];
    if (me == nullptr)
        return;

    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = (size_t)-1;

    for (size_t i = 0, size = agents.size();i < size; ++i) {
        if (!agents[i] || agents[i]->type != types[type])
            continue;
        switch (types[type]) {
            case 0x400: {
                GW::AgentItem *agent = agents[i]->GetAsAgentItem();
                if (!agent) continue;
                GW::Item *item = GW::Items::GetItemById(agent->item_id);
                if (!item || item->model_id != id)
                    continue;
            } break;
            case 0x200: {
                GW::AgentGadget *agent = agents[i]->GetAsAgentGadget();
                if (!agent || agent->gadget_id != id)
                    continue;
            } break;
            default: {
                GW::AgentLiving *agent = agents[i]->GetAsAgentLiving();
                if (!agent || agent->player_number != id || agent->hp <= 0)
                    continue;
            } break;
        }
        float newDistance = GW::GetSquareDistance(me->pos, agents[i]->pos);
        if (newDistance < distance) {
            closest = i;
            distance = newDistance;
        }
    }
    if (closest != (size_t)-1) {
        GW::Agent *agent = agents[closest];
        GW::GameThread::Enqueue([agent] { GW::Agents::ChangeTarget(agent); });
    }
    if (show_message_in_emote_channel) {
        char buf[256];
        Description(buf, 256);
        Log::Info("Triggered %s", buf);
    }
}

HotkeyMove::HotkeyMove(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    x = (float)ini->GetDoubleValue(section, "x", 0.0);
    y = (float)ini->GetDoubleValue(section, "y", 0.0);
    range = (float)ini->GetDoubleValue(section, "distance",
                                       GW::Constants::Range::Compass);
    strcpy_s(name, ini->GetValue(section, "name", ""));
}
void HotkeyMove::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetDoubleValue(section, "x", x);
    ini->SetDoubleValue(section, "y", y);
    ini->SetDoubleValue(section, "distance", range);
    ini->SetValue(section, "name", name);
}
void HotkeyMove::Description(char *buf, size_t bufsz) const
{
    if (name[0] == '\0') {
        snprintf(buf, bufsz, "Move to (%.0f, %.0f)", x, y);
    } else {
        snprintf(buf, bufsz, "Move to %s", name);
    }
}
void HotkeyMove::Draw()
{
    if (ImGui::InputFloat("x", &x, 0.0f, 0.0f, 3))
        hotkeys_changed = true;
    if (ImGui::InputFloat("y", &y, 0.0f, 0.0f, 3))
        hotkeys_changed = true;
    if (ImGui::InputFloat("Range", &range, 0.0f, 0.0f, 0))
        hotkeys_changed = true;
    ImGui::ShowHelp(
        "The hotkey will only trigger within this range.\nUse 0 for no limit.");
    if (ImGui::InputText("Name", name, 140))
        hotkeys_changed = true;
    if (ImGui::Checkbox("Display message when triggered",
                        &show_message_in_emote_channel))
        hotkeys_changed = true;
}
void HotkeyMove::Execute()
{
    if (!CanUse())
        return;
    GW::Agent *me = GW::Agents::GetPlayer();
    double dist = GW::GetDistance(me->pos, GW::Vec2f(x, y));
    if (range != 0 && dist > range)
        return;
    GW::Agents::Move(x, y);
    if (name[0] == '\0') {
        if (show_message_in_emote_channel)
            Log::Info("Moving to (%.0f, %.0f)", x, y);
    } else {
        if (show_message_in_emote_channel)
            Log::Info("Moving to %s", name);
    }
}

HotkeyDialog::HotkeyDialog(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    id = static_cast<size_t>(ini->GetLongValue(section, "DialogID", 0));
    strcpy_s(name, ini->GetValue(section, "DialogName", ""));
}
void HotkeyDialog::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "DialogID", static_cast<long>(id));
    ini->SetValue(section, "DialogName", name);
}
void HotkeyDialog::Description(char *buf, size_t bufsz) const
{
    if (name[0] == '\0') {
        snprintf(buf, bufsz, "Dialog #%zu", id);
    } else {
        snprintf(buf, bufsz, "Dialog %s", name);
    }
}
void HotkeyDialog::Draw()
{
    if (ImGui::InputInt("Dialog ID", (int *)&id))
        hotkeys_changed = true;
    if (ImGui::InputText("Dialog Name", name, 140))
        hotkeys_changed = true;
    if (ImGui::Checkbox("Display message when triggered",
                        &show_message_in_emote_channel))
        hotkeys_changed = true;
}
void HotkeyDialog::Execute()
{
    if (!CanUse())
        return;
    if (id == 0)
        return;
    GW::Agents::SendDialog(id);
    if (show_message_in_emote_channel)
        Log::Info("Sent dialog %s (%d)", name, id);
}

bool HotkeyPingBuild::GetText(void *, int idx, const char **out_text)
{
    if (idx >= (int)BuildsWindow::Instance().BuildCount())
        return false;
    *out_text = BuildsWindow::Instance().BuildName(static_cast<size_t>(idx));
    return true;
}
HotkeyPingBuild::HotkeyPingBuild(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
}
void HotkeyPingBuild::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "BuildIndex", static_cast<long>(index));
}
void HotkeyPingBuild::Description(char *buf, size_t bufsz) const
{
    const char *buildname = BuildsWindow::Instance().BuildName(index);
    if (buildname == nullptr)
        buildname = "<not found>";
    snprintf(buf, bufsz, "Ping build '%s'", buildname);
}
void HotkeyPingBuild::Draw()
{
    int icount = static_cast<int>(BuildsWindow::Instance().BuildCount());
    int iindex = static_cast<int>(index);
    if (ImGui::Combo("Build", &iindex, GetText, nullptr, icount)) {
        if (0 <= iindex)
            index = static_cast<size_t>(iindex);
        hotkeys_changed = true;
    }
}
void HotkeyPingBuild::Execute()
{
    if (!CanUse())
        return;

    BuildsWindow::Instance().Send(index);
}

bool HotkeyHeroTeamBuild::GetText(void *, int idx, const char **out_text)
{
    if (idx >= (int)HeroBuildsWindow::Instance().BuildCount())
        return false;
    size_t index = static_cast<size_t>(idx);
    *out_text = HeroBuildsWindow::Instance().BuildName(index);
    return true;
}
HotkeyHeroTeamBuild::HotkeyHeroTeamBuild(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    index = static_cast<size_t>(ini->GetLongValue(section, "BuildIndex", 0));
}
void HotkeyHeroTeamBuild::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "BuildIndex", static_cast<long>(index));
}
void HotkeyHeroTeamBuild::Description(char *buf, size_t bufsz) const
{
    const char *buildname = HeroBuildsWindow::Instance().BuildName(index);
    if (buildname == nullptr)
        buildname = "<not found>";
    snprintf(buf, bufsz, "Load Team Hero Build '%s'", buildname);
}
void HotkeyHeroTeamBuild::Draw()
{
    int icount = static_cast<int>(HeroBuildsWindow::Instance().BuildCount());
    int iindex = static_cast<int>(index);
    if (ImGui::Combo("Build", &iindex, GetText, nullptr, icount)) {
        if (0 <= iindex)
            index = static_cast<size_t>(iindex);
        hotkeys_changed = true;
    }
}
void HotkeyHeroTeamBuild::Execute()
{
    if (!CanUse())
        return;
    HeroBuildsWindow::Instance().Load(index);
}

HotkeyFlagHero::HotkeyFlagHero(CSimpleIni *ini, const char *section)
    : TBHotkey(ini, section)
{
    degree = ini ? static_cast<float>(ini->GetDoubleValue(section, "degree", degree)) : degree;
    distance = ini ? static_cast<float>(ini->GetDoubleValue(section, "distance", distance)) : distance;
    hero = ini ? ini->GetLongValue(section, "hero", hero) : hero;
    if (hero < 0) hero = 0;
}
void HotkeyFlagHero::Save(CSimpleIni *ini, const char *section) const
{
    TBHotkey::Save(ini, section);
    ini->SetDoubleValue(section, "degree", degree);
    ini->SetDoubleValue(section, "distance", distance);
    ini->SetLongValue(section, "hero", hero);
}
void HotkeyFlagHero::Description(char *buf, size_t bufsz) const
{
    if (hero == 0) {
        snprintf(buf, bufsz, "Flag All Heroes");
    } else {
        snprintf(buf, bufsz, "Flag Hero %d", hero);
    }
}
void HotkeyFlagHero::Draw()
{
    hotkeys_changed |= ImGui::InputFloat("Degree", &degree, 0.0f, 0.0f, 3);
    hotkeys_changed |= ImGui::InputFloat("Distance", &distance, 0.0f, 0.0f, 3);
    if (hotkeys_changed && distance < 0.f)
        distance = 0.f;
    hotkeys_changed |= ImGui::InputInt("Hero", &hero, 0);
    if (hotkeys_changed && hero < 0)
        hero = 0;
    ImGui::ShowHelp("The hero id that should be flagged.\nUse 0 to flag all");
}
void HotkeyFlagHero::Execute()
{
    if (!isExplorable())
        return;

    GW::Vec3f allflag = GW::GameContext::instance()->world->all_flag;

    if (hero < 0)
        return;
    if (hero == 0) {
        if (!(std::isinf(allflag.x) && !std::isinf(allflag.y))) {
            GW::PartyMgr::UnflagAll();
            return;
        }
    } else {
        const GW::HeroFlagArray &flags = GW::GameContext::instance()->world->hero_flags;
        if (!flags.valid() || hero > (int)flags.size())
            return;

        const GW::HeroFlag &flag = flags[hero - 1];
        if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
            GW::PartyMgr::UnflagHero(hero);
            return; 
        }
    }

    const GW::AgentLiving *player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player)
        return;
    const GW::AgentLiving *target = GW::Agents::GetTargetAsAgentLiving();

    float reference_radiant = player->rotation_angle;

    if (target && target != player) {
        float dx = target->x - player->x;
        float dy = target->y - player->y;

        reference_radiant = std::atan(dx == 0 ? dy : dy / dx);
        if (dx < 0) {
            reference_radiant += static_cast<float>(M_PI);
        } else if (dx > 0 && dy < 0) {
            reference_radiant += 2 * static_cast<float>(M_PI);
        }
    }

    const float radiant = degree * static_cast<float>(M_PI) / 180.f;
    const float x = player->x + distance * std::cos(reference_radiant - radiant);
    const float y = player->y + distance * std::sin(reference_radiant - radiant);

    GW::GamePos pos = GW::GamePos(x, y, 0);

    if (hero == 0) {
        GW::PartyMgr::FlagAll(pos);
    } else {
        GW::PartyMgr::FlagHero(hero, pos);
    }
}