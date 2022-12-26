
#pragma once

#include <ToolboxWindow.h>

class ObserverExportWindow : public ToolboxWindow {
public:
    ObserverExportWindow() = default;
    ~ObserverExportWindow() = default;

    static ObserverExportWindow& Instance() {
        static ObserverExportWindow instance;
        return instance;
    }

    enum class Version {
        V_0_1,
        V_1_0
    };

    std::string PadLeft(std::string input, uint8_t count, char c);
    nlohmann::json ToJSON_V_0_1();
    nlohmann::json ToJSON_V_1_0();
    void ExportToJSON(Version version);

    const char* Name() const override { return "Observer Export"; };
    const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

protected:
    float text_long     = 0;
    float text_medium   = 0;
    float text_short    = 0;
    float text_tiny     = 0;
};
