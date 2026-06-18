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
    static glz::generic ToJSON_V_0_1();
    static glz::generic ToJSON_V_1_0();
    static void ExportToJSON(Version version);

    [[nodiscard]] const char* Name() const override { return "Observer Export"; };
    [[nodiscard]] const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    struct Settings {
        std::string gwrank_api_key;
        std::string gwrank_endpoint = "https://gwrank.com/api/v1/matches";

        std::string match_type = "";
        std::string match_date = "";
        std::string mat_round = "";
    };

    static void ExportToGWRank();

protected:
    float text_long = 0;
    float text_medium = 0;
    float text_short = 0;
    float text_tiny = 0;
};
