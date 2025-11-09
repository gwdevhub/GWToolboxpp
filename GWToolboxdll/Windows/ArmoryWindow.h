#pragma once

#include <ToolboxWindow.h>

namespace GW {
    struct Item;
}
class ArmoryWindow : public ToolboxWindow {
public:
    static ArmoryWindow& Instance()
    {
        static ArmoryWindow instance;
        return instance;
    }
    ArmoryWindow() { 
        show_menubutton = can_show_in_main_window;
    }

    [[nodiscard]] const char* Name() const override { return "Armory"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_VEST; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float) override;
    static void PreviewItem(GW::Item*);
    static bool CanPreviewItem(GW::Item*);
};
