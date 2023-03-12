#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <Timer.h>

class Pcon {
public:
    static int pcons_delay;
    static int lunar_delay;
    static bool map_has_effects_array;
    static float size;
    static bool disable_when_not_found;
    static bool refill_if_below_threshold;
    static bool pcons_by_character;
    static Color enabled_bg_color;
    static DWORD player_id;

    static DWORD alcohol_level;
    static bool suppress_drunk_effect;
    static bool suppress_drunk_text;
    static bool suppress_drunk_emotes;
    static bool suppress_lunar_skills;

    static std::array<std::array<clock_t, 25>, 22> reserved_bag_slots;
    static bool hide_city_pcons_in_explorable_areas;

protected:

    Pcon(const char* chatname,
        const char* abbrevname,
        const char* ininame,
        const wchar_t* filename,
        ImVec2 uv0, ImVec2 uv1, int threshold,
        const char* desc = nullptr);
    Pcon(const wchar_t* file, int threshold = 20)
        :Pcon(0, 0, 0, file, { 0,0 }, { 1,1 }, threshold) {}
    Pcon(const Pcon&) = delete;
    virtual ~Pcon();
    bool* GetSettingsByName(const wchar_t* name);
    static bool UnreserveSlotForMove(size_t bagId, size_t slot); // Unlock slot.
    // Prevents more than 1 pcon from trying to add to the same slot at the same time.
    static bool ReserveSlotForMove(size_t bagId, size_t slot);
    // Checks whether another pcon has reserved this slot.
    static bool IsSlotReservedForMove(size_t bagId, size_t slot);

    void UpdateRefill();

    GW::Bag* pending_move_to_bag = nullptr;
    uint32_t pending_move_to_slot = 0;
    uint32_t pending_move_to_quantity = 0;
public:
    void Terminate();

    virtual void Draw(IDirect3DDevice9* device);
    virtual void Update(int delay = -1);
    // Similar to GW::Items::MoveItem, except this returns amount moved and uses the split stack header when needed.
    // Most of this logic should be integrated back into GWCA repo, but I've written it here for GWToolbox
    static uint32_t MoveItem(GW::Item *item, GW::Bag *bag, size_t slot,
                             size_t quantity = 0);
    wchar_t* SetPlayerName();
    // Pass true to start refill, or false to stop.
    void Refill(bool do_refill = true);
    void SetEnabled(bool enabled);
    const bool IsEnabled() { return IsVisible() && *enabled; }
    virtual bool IsVisible() const;
    void AfterUsed(bool used, int qty);
    void Toggle() { SetEnabled(!IsEnabled()); }
    // Resets pcon counters so it needs to recalc number and refill.
    void ResetCounts();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section);

    bool* enabled; // This is a ptr to the current char's status if applicable.
    bool pcon_quantity_checked = false;
    bool refilling = false; // Set when a refill is in progress. Dont touch.
    bool refill_attempted = false; // Set to true when refill thread has run for this map
    int threshold = 0;
    bool visible = true;
    int quantity = 0;
    std::wstring filename;
    int quantity_storage = 0;

    clock_t timer = 0;

    std::string chat;
    std::string abbrev;
    std::string& description() { return desc; }
    std::string ini;

protected:
    std::string desc;
    // Cycles through character's inventory to find a matching (incomplete) stack, or an empty pane.
    GW::Item* FindVacantStackOrSlotInInventory(GW::Item* likeItem = nullptr);
    GW::AgentLiving* player = nullptr;

    // "default" is the fallback
    std::map<std::wstring, bool*> settings_by_charname;

    GW::Constants::MapID mapid = GW::Constants::MapID::None;
    GW::Constants::InstanceType maptype = GW::Constants::InstanceType::Loading;
    // loops over the inventory, counting the items according to QuantityForEach
    // if 'used' is not null, it will also use the first item found,
    // and, if so, used *used to true
    // returns the number of items found, or -1 in case of error
    int CheckInventory(
        bool *used = nullptr, size_t *used_qty = nullptr,
        size_t from_bag = static_cast<size_t>(GW::Constants::Bag::Backpack),
        size_t to_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2)) const;

    virtual bool CanUseByInstanceType() const;
    virtual bool CanUseByEffect() const = 0;
    virtual void OnButtonClick() { Toggle(); }
    virtual size_t QuantityForEach(const GW::Item* item) const = 0;

private:
    IDirect3DTexture9** texture = nullptr;
    const ImVec2 uv0 = { 0,0 };
    const ImVec2 uv1 = { 1,1 };
};

// A generic Pcon has an item_id and effect_id
class PconGeneric : public Pcon {
public:
    PconGeneric(const wchar_t* file, DWORD item, GW::Constants::SkillID effect, int threshold = 20)
        :Pcon(file,  threshold),
        itemID(item), effectID(effect) {}
    PconGeneric(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        DWORD item, GW::Constants::SkillID effect,
        int threshold,
        const char* desc = nullptr)
        : Pcon(chat, abbrev, ini, file, uv0, uv1, threshold, desc),
        itemID(item), effectID(effect) {}
    PconGeneric(const PconGeneric&) = delete;

protected:
    bool CanUseByEffect() const override;
    size_t QuantityForEach(const GW::Item* item) const override;
    void OnButtonClick() override;

private:
    const DWORD itemID;
    const GW::Constants::SkillID effectID;
};

// Same as generic pcon, but with more restrictions on usage
class PconCons : public PconGeneric {
public:
    PconCons(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        DWORD item, GW::Constants::SkillID effect,
        int threshold,
            const char* desc = nullptr)
        : PconGeneric(chat, abbrev, ini, file, uv0, uv1, item, effect, threshold, desc) {}
    PconCons(const PconCons&) = delete;

    bool CanUseByEffect() const override;
};

class PconCity : public Pcon {
public:
    PconCity(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        int threshold,
            const char* desc = nullptr)
        : Pcon(chat, abbrev, ini, file, uv0, uv1, threshold, desc) {}
    PconCity(const PconCity&) = delete;

    bool CanUseByInstanceType() const;
    bool IsVisible() const override;
    bool CanUseByEffect() const override;
    size_t QuantityForEach(const GW::Item* item) const override;
};
// Used only in outposts for refilling
class PconRefiller : public PconCity {
public:
    PconRefiller(const wchar_t* file, DWORD item, int threshold = 250)
        : PconRefiller(0, 0, 0, file, { 0,0 }, { 1,1 }, item, threshold) {
        visible = false;
    };
    PconRefiller(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        DWORD item,
        int threshold,
        const char* desc_ = nullptr)
        : PconCity(chat, abbrev, ini, file, uv0, uv1, threshold, desc_), itemID(item) {
        if (desc.size())
            desc += "\n";
        desc += "Enable in an outpost to refill your inventory.";
    }

    PconRefiller(const PconRefiller&) = delete;

    bool CanUseByInstanceType() const override { return false; }
    bool CanUseByEffect() const override { return false; }
    bool IsVisible() const override { return visible && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost; }
    void Draw(IDirect3DDevice9* device) override;
    size_t QuantityForEach(const GW::Item* item) const override { return item->model_id == itemID ? 1u : 0u; }
private:
    const DWORD itemID;
};

class PconAlcohol : public Pcon {
public:
    PconAlcohol(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        int threshold,
            const char* desc = nullptr)
        : Pcon(chat, abbrev, ini, file, uv0, uv1, threshold, desc) {}

    PconAlcohol(const PconAlcohol&) = delete;

    bool CanUseByEffect() const override;
    size_t QuantityForEach(const GW::Item* item) const override;
    void ForceUse();
};

class PconLunar : public Pcon {
public:
    PconLunar(const char* chat,
        const char* abbrev,
        const char* ini,
        const wchar_t* file,
        ImVec2 uv0, ImVec2 uv1,
        int threshold,
            const char* desc = nullptr)
        : Pcon(chat, abbrev, ini, file, uv0, uv1, threshold, desc) {}

    PconLunar(const PconLunar&) = delete;

    void Update(int delay = -1) override;
    bool CanUseByEffect() const override;
    size_t QuantityForEach(const GW::Item* item) const override;
};
