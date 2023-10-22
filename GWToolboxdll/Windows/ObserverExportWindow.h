#pragma once

#include <ToolboxWindow.h>

class ObserverExportWindow : public ToolboxWindow {
public:
    ObserverExportWindow() = default;
    ~ObserverExportWindow() override = default;

    static ObserverExportWindow& Instance()
    {
        static ObserverExportWindow instance;
        return instance;
    }

    enum class Version {
        V_0_1,
        V_1_0
    };

    static std::string PadLeft(std::string input, uint8_t count, char c);
    static nlohmann::json ToJSON_V_0_1();
    static nlohmann::json ToJSON_V_1_0();
    static void ExportToJSON(Version version);

    [[nodiscard]] const char* Name() const override { return "Observer Export"; };
    [[nodiscard]] const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

protected:
    float text_long = 0;
    float text_medium = 0;
    float text_short = 0;
    float text_tiny = 0;
};
