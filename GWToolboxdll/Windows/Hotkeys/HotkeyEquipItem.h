#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Item.h>

#include <Utils/GuiUtils.h>

#include <Windows/Hotkeys/TBHotkey.h>

class HotkeyEquipItemAttributes {
public:
    HotkeyEquipItemAttributes(uint32_t _model_id = 0, const wchar_t* _name_enc = nullptr, const wchar_t* _info_string = nullptr, const GW::ItemModifier* _mod_struct = nullptr, size_t _mod_struct_size = 0);
    HotkeyEquipItemAttributes* set(uint32_t _model_id = 0, const wchar_t* _name_enc = nullptr, const wchar_t* _info_string = nullptr, const GW::ItemModifier* _mod_struct = nullptr, size_t _mod_struct_size = 0);
    HotkeyEquipItemAttributes* set(const HotkeyEquipItemAttributes& other);
    HotkeyEquipItemAttributes(const GW::Item* item);
    ~HotkeyEquipItemAttributes();

    HotkeyEquipItemAttributes& operator=(const HotkeyEquipItemAttributes& other) = delete;

    bool check(const GW::Item* item = nullptr) const;
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

    uint32_t item_id = 0;
    clock_t start_time = 0;
    clock_t last_try = 0;
    const wchar_t* item_name = L"";

public:
    HotkeyEquipItemAttributes item_attributes;
    static const char* IniSection() { return "EquipItem"; }
    [[nodiscard]] const char* Name() const override { return IniSection(); }

    HotkeyEquipItem(ToolboxIni* ini, const char* section);

    void Save(ToolboxIni* ini, const char* section) const override;

    bool DrawSettings() override;
    int Description(char* buf, size_t bufsz) override;
    void Execute() override;

    static bool IsEquippable(const GW::Item* item);

    GW::Item* FindMatchingItem(GW::Constants::Bag bag_idx, GW::Bag** bag) const;
};
