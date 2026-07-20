#pragma once

#include <ToolboxUIElement.h>

namespace GW::Constants {
    enum class MapID : uint32_t;
}

class ToolboxSettings : public ToolboxUIElement {
    ToolboxSettings() = default;
    ~ToolboxSettings() override = default;

public:
    static ToolboxSettings& Instance()
    {
        static ToolboxSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Toolbox Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TOOLBOX; }

    static void LoadModules(ToolboxIni* ini);

    // The "Enable the following features" checkboxes as {name, description}, for settings search
    static const std::vector<std::pair<const char*, const char*>>& GetOptionalModuleToggles();

    void Update(float delta) override;

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    void Draw(IDirect3DDevice9*) override;

    void ShowVisibleRadio() override { };

    static void DrawFreezeSetting();

    void DrawSizeAndPositionSettings() override { }

    static void DrawSettingsCogButtons();
    // Run at end-of-frame, after ImGui has finished rendering, to actually
    // write the queued window screenshot to disk. No-op when no capture is
    // pending.
    static void FlushPendingScreenshot(IDirect3DDevice9* device);
    // Queue a full-backbuffer capture (same end-of-frame pipeline as the
    // title-bar camera button); captures the next rendered frame.
    static void RequestFullscreenScreenshot(const std::filesystem::path& path);

    static inline bool move_all = false;
    static inline bool clamp_windows_to_screen = false;
    static inline bool hide_on_loading_screen = false;
    static inline bool send_anonymous_gameplay_info = true;
    static inline bool show_cog_in_outpost = false;
    static inline bool show_cog_in_explorable = false;
    static inline bool show_close_in_outpost = true;
    static inline bool show_close_in_explorable = true;
    // Off by default — this is a docs-authoring affordance, not a player
    // feature.
    static inline bool show_screenshot_button_in_outpost = false;
    static inline bool show_screenshot_button_in_explorable = false;
    static inline bool is_in_explorable = false;
    static inline bool is_in_mobile_mode = false;
private:
    // === location stuff ===
    clock_t location_timer = 0;
    GW::Constants::MapID location_current_map = static_cast<GW::Constants::MapID>(0);
    std::wofstream location_file;
    bool save_location_data = false;
};
