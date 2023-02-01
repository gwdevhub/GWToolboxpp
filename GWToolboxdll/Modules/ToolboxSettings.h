#pragma once

#include <ToolboxUIElement.h>

namespace GW {
    namespace Constants {
        enum class MapID;
    }
}
class ToolboxSettings : public ToolboxUIElement {
    ToolboxSettings() = default;
    ~ToolboxSettings() = default;

public:
    static ToolboxSettings& Instance() {
        static ToolboxSettings instance;
        return instance;
    }

    const char* Name() const override { return "Toolbox Settings"; }
    const char* Icon() const override { return ICON_FA_TOOLBOX;  }

    void LoadModules(ToolboxIni* ini);

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    void Draw(IDirect3DDevice9*) override;
    void ShowVisibleRadio() override {};

    void DrawFreezeSetting();
    void DrawSizeAndPositionSettings() override {}

    static bool move_all;

private:
    // === location stuff ===
    clock_t location_timer = 0;
    GW::Constants::MapID location_current_map = static_cast<GW::Constants::MapID>(0);
    std::wofstream location_file;
    bool save_location_data = false;
};
