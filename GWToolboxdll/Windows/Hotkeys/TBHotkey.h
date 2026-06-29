#pragma once

#include <bitset>
#include <string>
#include <vector>

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>

class ToolboxIni;
class HotkeyGroup;

namespace GW {
    enum class HeroBehavior : uint32_t;
}

// abstract class Toolbox Hotkey
// has the key code and pressed status
class TBHotkey {
public:
    enum Op {
        Op_None,
        Op_MoveUp,
        Op_MoveDown,
        Op_Delete,
        Op_BlockInput,
        Op_Duplicate,
    };

    static const char* professions[];
    static const char* instance_types[];

    static bool hotkeys_changed;

    static TBHotkey* HotkeyFactory(ToolboxIni* ini, const char* section);

    char label[128] = {};       // Optional display name; shown in header instead of description when non-empty
    int8_t open_state_override = -1; // -1 = no override, 0 = force close, 1 = force open (applied once)
    bool trigger_on_key_up = false;            // Should the key be triggered on key down, or key up
    bool pressed = false;                      // if the key has been pressed
    bool active = true;                        // if the hotkey is enabled/active
    bool show_message_in_emote_channel = true; // if hotkey should show message in emote channel when triggered
    bool show_error_on_failure = true;         // if hotkey should show error message on failure
    bool ongoing = false;                      // used for hotkeys that need to execute more than once per toggle.
    bool block_gw = false;                     // true to consume the keypress before passing to gw.
    bool trigger_on_explorable = false;        // Trigger when entering explorable area
    bool trigger_on_outpost = false;           // Trigger when entering outpost area
    bool trigger_on_pvp_character = false;     // Trigger when playing a PvP character
    bool trigger_on_lose_focus = false;        // Trigger when GW window is no longer focussed
    bool trigger_on_gain_focus = false;        // Trigger when GW window is focussed
    bool can_trigger_on_map_change = true;     // Some hotkeys cant trigger on map change e.g. Guild Wars Key
    bool block_other_hotkeys_on_trigger = false; // If this hotkey is triggered, block any further hotkeys from being processed

    size_t sort_order = 0xffff;

    bool trigger_in_desktop_mode = true;       // Trigger this hotkey in desktop mode
    bool trigger_in_controller_mode = true; // Trigger this hotkey in controller mode

    std::vector<uint32_t> map_ids{};
    std::vector<std::string> player_names{};
    bool prof_ids[11]{};
    int instance_type = -1;
    uint32_t in_range_of_npc_id = 0;
    float in_range_of_distance = 0.f;

    std::bitset<256> key_combo;
    bool strict_key_combo = false;

    // Create hotkey, load from file if 'ini' is not null
    TBHotkey(const ToolboxIni* ini, const char* section);
    virtual ~TBHotkey();

    virtual bool CanUse();

    // Whether this hotkey is limited to professions. Returns number of professions applicable
    size_t HasProfession();
    // Whether this hotkey is valid for the current player/map
    virtual bool IsValid(const char* _player_name, GW::Constants::InstanceType _instance_type, GW::Constants::Profession _profession, GW::Constants::MapID _map_id, bool is_pvp_character);


    virtual void Save(ToolboxIni* ini, const char* section) const;

    virtual bool Draw();
    virtual bool DrawSettings();

    [[nodiscard]] virtual const char* Name() const = 0;
    virtual int Description(char* buf, size_t bufsz) = 0;
    virtual void Execute() = 0;
    virtual void Toggle() { return Execute(); }

    TBHotkey* Duplicate();

    static std::vector<TBHotkey*> all_hotkeys;
    static std::vector<TBHotkey*> top_level_hotkeys;


    HotkeyGroup* group = 0;

    // Call this when you add or remove a hotkey.
    static void SortHotkeys();
    // Try to move this hotkey up. True if moved
    bool MoveUp();
    // Try to move this hotkey down. True if moved
    bool MoveDown();

    bool SetGroup(HotkeyGroup* group);

    bool IsGroup();

protected:
    static bool isLoading() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading; }
    static bool isExplorable() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable; }
    static bool isOutpost() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost; }
    bool IsInRangeOfNPC() const;



    const unsigned int ui_id = 0; // an internal id to ensure interface consistency

    static std::unordered_map<std::string, HotkeyGroup*> hotkey_groups;


    void Release();

private:
    static unsigned int cur_ui_id;
};
