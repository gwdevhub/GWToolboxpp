#pragma once

#include <ToolboxWindow.h>
#include <cstdint>

namespace GW {
    namespace Constants {
        enum class ItemType : uint8_t;
        enum class Bag : uint8_t;
    }
}

class InventorySorting : public ToolboxWindow {
    InventorySorting() = default;
    ~InventorySorting() override = default;

public:
    static InventorySorting& Instance()
    {
        static InventorySorting instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "物品排序"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SORT; }

    struct Settings {
        bool sort_equipment_pack = false;
    };

    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9* device) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    void RegisterSettingsContent() override;

    static bool CombineStacks(GW::Constants::Bag start, GW::Constants::Bag end);

    static bool StoreMaterials(GW::Constants::Bag start, GW::Constants::Bag end);

    static bool SortInventory(GW::Constants::Bag start, GW::Constants::Bag end);
    
    static void CancelSort();

private:

};
