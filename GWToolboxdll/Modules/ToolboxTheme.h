#pragma once

#include <ToolboxUIElement.h>

#include <Color.h>

class ToolboxTheme : public ToolboxUIElement {
    ToolboxTheme();

public:
    static ToolboxTheme& Instance()
    {
        static ToolboxTheme instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Theme"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PALETTE; }

    // MSVC can't reflect member names of internal-linkage types, so glaze-serialized structs are nested in the class
    struct ThemeSettings {
        float FontGlobalScale = 1.0f;
        float GlobalAlpha = 1.0f;
        std::array<float, 2> WindowPadding{};
        float WindowRounding = 0.0f;
        std::array<float, 2> FramePadding{};
        float FrameRounding = 0.0f;
        std::array<float, 2> ItemSpacing{};
        std::array<float, 2> ItemInnerSpacing{};
        float IndentSpacing = 0.0f;
        float ScrollbarSize = 0.0f;
        float ScrollbarRounding = 0.0f;
        float GrabMinSize = 0.0f;
        float GrabRounding = 0.0f;
        std::array<float, 2> WindowTitleAlign{};
        std::array<float, 2> ButtonTextAlign{};
        std::map<std::string, ::Colors::SettingColor> Colors;
    };

    struct WindowLayout {
        std::array<float, 2> pos{};
        std::array<float, 2> size{};
        bool collapsed = false;
    };

    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Draw(IDirect3DDevice9* device) override;

    void ShowVisibleRadio() override { }

    void DrawSizeAndPositionSettings() override { }

    void SaveUILayout();
    void LoadUILayout();

    void ApplyWindowSettings(ImGuiWindow* window) const;

    void DrawSettingsInternal() override;

    ToolboxIni* GetLayoutIni(bool reload = true);
    ToolboxIni* GetThemeIni(bool reload = true);

private:
    static ImGuiStyle DefaultTheme();

    float font_scale_main = 1.0;
    ImGuiStyle ini_style;
    bool layout_dirty = false;
    bool imgui_style_loaded = false;

    std::map<std::string, WindowLayout> layout;

    ToolboxIni* theme_ini = nullptr;
    ToolboxIni* layout_ini = nullptr;
};
