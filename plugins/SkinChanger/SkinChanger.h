#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

namespace GW 
{
    struct Item;
    struct ItemModifier;
    enum class DyeColor : uint8_t;
}
struct InventoryItem 
{
    uint32_t modelID = 0;
    std::wstring encodedName;
    std::vector<GW::ItemModifier> modifiers = {};
};

struct ItemChange 
{
    InventoryItem item;
    std::string modelFileID = "0x";

    bool enableDyes = false;
    std::array<GW::DyeColor, 4> dyes;
    uint8_t tint = 255;
};

struct NpcTransmog 
{
    std::string nameToApply = "";

    int npcID = 0;
    int scale = 0;

    std::string npcModelFileID = "0x";
    std::string npcModelFileData = "0x";
    std::string flags = "0x";
};

struct MinipetTransmog 
{
    uint32_t itemToReplaceModelID = 0;
    uint32_t agentToReplaceModelID = 0;
    std::string replacementItemModelFileID = "0x";

    // Agent transmo model information
    NpcTransmog npcTransmog;
};

class SkinChanger : public ToolboxPlugin {
public:
    SkinChanger() {}
    ~SkinChanger() override = default;

    const char* Name() const override { return "SkinChanger"; }
    const char* Icon() const override { return ICON_FA_PEOPLE_ARROWS; }

    void DrawSettings() override;
    bool HasSettings() const override { return true; }
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

    void Update(float) override;

    void loadFromIniFile(const ToolboxIni& ini);

    std::vector<ItemChange> itemChanges;
    std::vector<MinipetTransmog> minipetTransmogs;

private:
    // Fixed: moved from public to private — this is an internal helper called only from
    // within the class (packet callbacks and instance-load hooks) and should not be part
    // of the public plugin interface.
    void applyOverrideToItem(GW::Item* item) const;
};
