#pragma once

#include <ToolboxWindow.h>
#include <string>

namespace GW {
    struct Item;
}

class GWMarketWindow : public ToolboxWindow {
    GWMarketWindow() {};
    ~GWMarketWindow() {};

public:
    static GWMarketWindow& Instance() {
        static GWMarketWindow instance;
        return instance;
    }

    const char* Name() const override { return "Market Browser"; }
    const char* Icon() const override { return ICON_FA_SHOPPING_CART; }

    struct Settings {
        bool auto_refresh = true;
        int refresh_interval = 60;
    };

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;

    static bool CanSellItem(GW::Item* item);
    static void AddItemToSell(GW::Item* item);
    static void SearchItem(const std::string& item_name);

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
