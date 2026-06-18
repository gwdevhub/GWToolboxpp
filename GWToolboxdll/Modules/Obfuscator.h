#pragma once

#include <ToolboxModule.h>

class Obfuscator : public ToolboxModule {
public:
    static Obfuscator& Instance()
    {
        static Obfuscator instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Obfuscator"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_USER_SECRET; }

    struct Settings {
        bool obfuscate = false;
        bool rename_other_players = false;
        bool rename_friends_to_alias = true;
        bool rename_self = false;
        std::string own_player_name;
    };

    void Initialize() override;
    void Update(float) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void DrawSettingsInternal() override;
    void SaveSettings(SettingsDoc& doc) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;

    static void Obfuscate(bool obfuscate);
    static bool IsObfuscatedName(const std::wstring&);
};
