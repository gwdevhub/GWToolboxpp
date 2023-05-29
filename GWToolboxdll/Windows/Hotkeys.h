#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>


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
    };

    static const char* professions[];
    static const char* instance_types[];

    static bool show_active_in_header;
    static bool show_run_in_header;
    static bool hotkeys_changed;
    static WORD* key_out;
    static DWORD* mod_out;

    static TBHotkey* HotkeyFactory(ToolboxIni* ini, const char* section);
    static void HotkeySelector(WORD* key, DWORD* modifier = nullptr);

    char group[128] = "";
    bool pressed = false;   // if the key has been pressed
    bool active = true;     // if the hotkey is enabled/active
    bool show_message_in_emote_channel = true; // if hotkey should show message in emote channel when triggered
    bool show_error_on_failure = true; // if hotkey should show error message on failure
    bool ongoing = false; // used for hotkeys that need to execute more than once per toggle.
    bool block_gw = false; // true to consume the keypress before passing to gw.
    bool trigger_on_explorable = false; // Trigger when entering explorable area
    bool trigger_on_outpost = false; // Trigger when entering outpost area
    bool trigger_on_pvp_character = false; // Trigger when playing a PvP character
    bool trigger_on_lose_focus = false; // Trigger when GW window is no longer focussed
    bool trigger_on_gain_focus = false; // Trigger when GW window is focussed
    bool can_trigger_on_map_change = true; // Some hotkeys cant trigger on map change e.g. Guild Wars Key

    std::vector<uint32_t> map_ids;
    bool prof_ids[11];
    int instance_type = -1;
    char player_name[20] = "";
    uint32_t in_range_of_npc_id = 0;
    float in_range_of_distance = 0.f;

    long hotkey = 0;
    long modifier = 0;

    // Create hotkey, load from file if 'ini' is not null
    TBHotkey(ToolboxIni* ini, const char* section);
    virtual ~TBHotkey() = default;

    virtual bool CanUse();

    // Whether this hotkey is limited to professions. Returns number of professions applicable
    size_t HasProfession();
    // Whether this hotkey is valid for the current player/map
    virtual bool IsValid(const char* _player_name, GW::Constants::InstanceType _instance_type, GW::Constants::Profession _profession, GW::Constants::MapID _map_id, bool is_pvp_character);


    virtual void Save(ToolboxIni* ini, const char* section) const;

    bool Draw(Op* op);

    [[nodiscard]] virtual const char* Name() const = 0;
    virtual bool Draw() = 0;
    virtual int Description(char *buf, size_t bufsz) = 0;
    virtual void Execute() = 0;
    virtual void Toggle() { return Execute(); }
protected:

    static bool isLoading() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading; }
    static bool isExplorable() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable; }
    static bool isOutpost() { return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost; }
    bool IsInRangeOfNPC();

    const unsigned int ui_id = 0;   // an internal id to ensure interface consistency

private:
    static unsigned int cur_ui_id;

};

// hotkey to send a message in chat
// can be used for anything that uses SendChat
class HotkeySendChat : public TBHotkey {
    char message[139];
    char channel = '/';

public:
    inline static const char* IniSection() { return "SendChat"; }
    const char* Name() const override { return IniSection(); }

    HotkeySendChat(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyEquipItemAttributes {
public:
    HotkeyEquipItemAttributes(uint32_t _model_id = 0, const wchar_t* _name_enc = 0, const wchar_t* _info_string = 0, const GW::ItemModifier* _mod_struct = 0, size_t _mod_struct_size = 0);
    HotkeyEquipItemAttributes* set(uint32_t _model_id = 0, const wchar_t* _name_enc = 0, const wchar_t* _info_string = 0, const GW::ItemModifier* _mod_struct = 0, size_t _mod_struct_size = 0);
    HotkeyEquipItemAttributes* set(HotkeyEquipItemAttributes const& other);
    HotkeyEquipItemAttributes(const GW::Item* item);
    ~HotkeyEquipItemAttributes();

    HotkeyEquipItemAttributes& operator=(HotkeyEquipItemAttributes const& other) = delete;

    bool check(GW::Item* item = nullptr);
    uint32_t model_id = 0;
    GuiUtils::EncString enc_name;
    GuiUtils::EncString enc_desc;
    GW::ItemModifier* mod_struct = nullptr;
    uint32_t mod_struct_size = 0;
    std::string& name() { return enc_name.string(); }
    std::wstring& name_ws() { return enc_name.wstring(); }
    std::string& desc() { return enc_desc.string(); }
    std::wstring& desc_ws() { return enc_desc.wstring(); }
};
class HotkeyEquipItem : public TBHotkey {
    UINT bag_idx = 0;
    UINT slot_idx = 0;
    enum EquipBy : int {
        ITEM,
        SLOT
    } equip_by = SLOT;
    GW::Item* item = nullptr;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::chrono::time_point<std::chrono::steady_clock> last_try;
    const wchar_t* item_name = L"";
public:
    HotkeyEquipItemAttributes item_attributes;
    static const char* IniSection() { return "EquipItem"; }
    const char* Name() const override { return IniSection(); }

    HotkeyEquipItem(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;

    bool IsEquippable(const GW::Item* item);

    GW::Item* FindMatchingItem(GW::Constants::Bag bag_idx);
};

// hotkey to use an item
// will use the item in explorable areas, and display a warning with given name if not found
class HotkeyUseItem : public TBHotkey {
    UINT item_id = 0;
    char name[140];
public:
    static const char* IniSection() { return "UseItem"; }
    const char* Name() const override { return IniSection(); }

    HotkeyUseItem(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool CanUse() override { return TBHotkey::CanUse() && item_id != 0; }

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};

// hotkey to drop a buff if currently active, and cast it on the current target if not
// can be used for recall, ua, and maybe others?
class HotkeyDropUseBuff : public TBHotkey {
    enum SkillIndex {
        Recall,
        UA,
        HolyVeil,
        Other
    };
    static bool GetText(void* data, int idx, const char** out_text);
    SkillIndex GetIndex() const;

public:
    GW::Constants::SkillID id;

    static const char* IniSection() { return "DropUseBuff"; }
    const char* Name() const override { return IniSection(); }

    HotkeyDropUseBuff(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

// hotkey to toggle a toolbox function
class HotkeyToggle : public TBHotkey {
    enum ToggleTarget {
        Clicker,
        Pcons,
        CoinDrop,
        Tick,
        Count
    } target = Clicker;
    static bool GetText(void*, int idx, const char** out_text);


public:
    static bool IsValid(ToolboxIni *ini, const char *section);
    static const char* IniSection() { return "Toggle"; }
    const char* Name() const override { return IniSection(); }

    HotkeyToggle(ToolboxIni* ini, const char* section);
    ~HotkeyToggle() override;
    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
    void Toggle() override;
    bool IsToggled(bool force = false);

private:
    bool HasInterval() const { return target == Clicker || target == CoinDrop; };
    WORD togglekey = VK_LBUTTON;
    bool is_key_down = false;
    clock_t last_use = 0;
    // ms between uses - min 50ms, max 30s
    int interval = 50;
    static std::unordered_map<WORD, HotkeyToggle*> toggled;
};

class HotkeyAction : public TBHotkey {
    const int n_actions = 5;
    enum Action {
        OpenXunlaiChest,
        OpenLockedChest,
        DropGoldCoin,
        ReapplyTitle,
        EnterChallenge,
    };
    static bool GetText(void*, int idx, const char** out_text);

public:
    Action action;

    static const char* IniSection() { return "Action"; }
    const char* Name() const override { return IniSection(); }

    HotkeyAction(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

// hotkey to target something in-game
// it will target the closest agent with the given PlayerNumber (aka modelID)
class HotkeyTarget : public TBHotkey {
    const uint32_t types[3] = { 0xDB,0x200,0x400 };
    const char *type_labels[3] = { "NPC", "Signpost", "Item" };
    const char *identifier_labels[3] = { "Model ID or Name", "Gadget ID or Name", "Item ModelID or Name" };
    enum HotkeyTargetType : int
    {
        NPC,
        Signpost,
        Item,
        Count
    } type = HotkeyTargetType::NPC;
    char id[140] = "nearest";
    char name[140] = "";
public:
    static const char* IniSection() { return "Target"; }
    const char* Name() const override { return IniSection(); }

    HotkeyTarget(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

// hotkey to move to a specific position
// it will only move in explorables, and if in compass range from destination
class HotkeyMove : public TBHotkey {
public:
    enum class MoveType : int {
        Location,
        Target
    } type = MoveType::Location;
    float x = 0.0;
    float y = 0.0;
    float range = 0.0;
    float distance_from_location = 0.0;
    char name[140]{};

    static const char* IniSection() { return "Move"; }
    const char* Name() const override { return IniSection(); }

    HotkeyMove(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyDialog : public TBHotkey {
public:
    uint32_t id = 0;
    char name[140]{};

    static const char* IniSection() { return "Dialog"; }
    const char* Name() const override { return IniSection(); }

    HotkeyDialog(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyPingBuild : public TBHotkey {
    static bool GetText(void*, int idx, const char** out_text);

public:
    size_t index = 0;

    static const char* IniSection() { return "PingBuild"; }
    const char* Name() const override { return IniSection(); }

    HotkeyPingBuild(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyHeroTeamBuild : public TBHotkey {
    static bool GetText(void*, int idx, const char** out_text);

public:
    size_t index = 0;

    static const char* IniSection() { return "HeroTeamBuild"; }
    const char* Name() const override { return IniSection(); }

    HotkeyHeroTeamBuild(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyFlagHero : public TBHotkey
{
public:
    float degree = 0.0;
    float distance = 0.0;
    int hero = 0;

    static const char *IniSection()
    {
        return "FlagHero";
    }
    const char *Name() const override
    {
        return IniSection();
    }

    HotkeyFlagHero(ToolboxIni *ini, const char *section);

    void Save(ToolboxIni *ini, const char *section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};
class HotkeyGWKey : public TBHotkey {
private:
    GW::UI::ControlAction action = GW::UI::ControlAction::ControlAction_ActivateWeaponSet1;
    int action_idx = 0;
    static std::vector<const char*> labels;
public:
    static std::vector < std::pair< GW::UI::ControlAction, GuiUtils::EncString* > > control_labels;

    static const char* IniSection() { return "GWHotkey"; }
    const char* Name() const override { return IniSection(); }

    HotkeyGWKey(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool Draw() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;
};

class HotkeyCommandPet : public TBHotkey
{
public:
    GW::HeroBehavior behavior = (GW::HeroBehavior)0;;

    static const char *IniSection()
    {
        return "CommandPet";
    }
    const char *Name() const override
    {
        return IniSection();
    }

    HotkeyCommandPet(ToolboxIni *ini, const char *section);

    void Save(ToolboxIni *ini, const char *section) const override;

    bool Draw() override;
    int Description(char *buf, size_t bufsz) override;
    void Execute() override;
};
