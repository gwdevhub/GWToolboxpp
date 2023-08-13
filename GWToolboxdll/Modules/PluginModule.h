#pragma once

#include <ToolboxUIElement.h>
#include <../plugins/Base/ToolboxPlugin.h>

class PluginModule final : public ToolboxUIElement {

    PluginModule();
    ~PluginModule() override = default;

public:
    struct Plugin {
        Plugin(std::filesystem::path _path)
            : path(std::move(_path)) { }
        std::filesystem::path path;
        HMODULE dll = nullptr;
        ToolboxPlugin* instance = nullptr;
        bool initialized = false;
        bool terminating = false;
        bool visible = false;
    };

    static PluginModule& Instance()
    {
        static PluginModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Plugins"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PUZZLE_PIECE; }

    void Initialize() override;

    void Draw(IDirect3DDevice9*) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Update(float) override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;

    static std::vector<Plugin*> GetPlugins();

    void ShowVisibleRadio() override { }

    void DrawSizeAndPositionSettings() override { }
};
