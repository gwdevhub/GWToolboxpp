#pragma once

#include <ToolboxWindow.h>
#include <vector>
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

    [[nodiscard]] const char* Name() const override { return "Inventory Sorting"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SORT; }

    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9* device) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void RegisterSettingsContent() override;

    static bool CombineStacks(GW::Constants::Bag start, GW::Constants::Bag end);

    static bool StoreMaterials(GW::Constants::Bag start, GW::Constants::Bag end);

    /**
     * Sorts all items in inventory bags by item type according to 
     * the configured sort order. Runs on a worker thread with progress popup.
     */
    static bool SortInventory(GW::Constants::Bag start, GW::Constants::Bag end);
    
    /**
     * Cancels the current inventory sorting operation and cleans up state.
     */
    static void CancelSort();

private:

};
