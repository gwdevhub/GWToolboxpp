#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class Dialogs final : public ToolboxUIPlugin {
public:
    Dialogs();
    ~Dialogs() override = default;

    [[nodiscard]] const char* Name() const override { return "Dialogs"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COMMENT_DOTS; }

    void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;
    void DrawSettings() override;

private:
    int fav_count = 0;
    std::vector<int> fav_index;

    bool show_foundry_reward = true;
    bool show_tower_of_strength = true;
    bool show_demon_assassin = true;
    bool show_four_horsemen = true;
    bool show_dhuum = true;
    bool show_uwteles = true;
    bool show_favorites = true;
    bool show_custom = true;
    bool use_function_ptr = false;

    int custom_dialog_index = 0;
    std::array<char, 64> custom_dialog_buffer{};
};
