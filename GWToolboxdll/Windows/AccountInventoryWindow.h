#pragma once

#include <GWCA/Constants/Constants.h>
#include <ToolboxWindow.h>

class AccountInventoryWindow : public ToolboxWindow {
    AccountInventoryWindow() { show_menubutton = can_show_in_main_window; }

public:
    static AccountInventoryWindow& Instance()
    {
        static AccountInventoryWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Account Inventory"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USERS; }

    struct Settings {
        bool detailed_view = false;
        bool merge_stacks = false;
        bool hide_other_accounts = false;
        bool hide_equipment = false;
        bool hide_equipment_pack = false;
        bool hide_hero_armor = false;
        bool hide_unclaimed_items = false;
    };

    // callbacks

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    void HandleHeroBag(GW::Constants::HeroID hero_id);
    void GatherAllInventories();

    // handle inventory generation during map load
    void PreMapLoad();
    void PostMapLoad();
};
