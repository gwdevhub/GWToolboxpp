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

    void Initialize() override;
    void Terminate() override;
    
    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;
    
    static bool CanSellItem(GW::Item* item);
    static void AddItemToSell(GW::Item* item);
    static void SearchItem(const std::string& item_name);

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
