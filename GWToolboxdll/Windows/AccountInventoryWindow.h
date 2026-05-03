#pragma once

#include <GWCA/Utilities/Hook.h>
#include <Utils/ToolboxUtils.h>
#include <ToolboxWindow.h>
#include <mutex>
#include <memory>
#include <Timer.h>

class AccountInventoryWindow : public ToolboxWindow {


    AccountInventoryWindow()
    {
        show_menubutton = can_show_in_main_window;
    }

public:
    static AccountInventoryWindow& Instance()
    {
        static AccountInventoryWindow instance;
        return instance;
    }

    [[nodiscard]] const char *Name() const override { return "Account Inventory"; }
    [[nodiscard]] const char *Icon() const override { return ICON_FA_USERS; }

    // callbacks

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;

    void Draw(IDirect3DDevice9 * pDevice) override;
    void DrawSettingsInternal() override;

    void LoadSettings(ToolboxIni * ini) override;
    void SaveSettings(ToolboxIni * ini) override;

    void HandleHeroBag(GW::Constants::HeroID hero_id);
    void GatherAllInventories();
    
    // handle inventory generation during map load
    void PreMapLoad();
    void PostMapLoad();
    void OnPartyAddHero(GW::Constants::HeroID hero_id);
    // maintain hero_id <-> Equipped_Items bag tracking
    GW::Bag* OnPartyRemoveHero(GW::Constants::HeroID hero_id);
};
