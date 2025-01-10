#pragma once

#include <ToolboxModule.h>

class AudioSettings : public ToolboxModule {
    AudioSettings() = default;
    ~AudioSettings() override = default;

public:
    static AudioSettings& Instance()
    {
        static AudioSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Audio Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_MUSIC; }

    static bool PlaySound(const wchar_t* filename);
    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void DrawSettingsInternal() override;
};
