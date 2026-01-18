#pragma once

#include <ToolboxWindow.h>
#include <string>
#include <vector>
#include <map>

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
    
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
