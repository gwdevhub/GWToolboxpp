#pragma once

#include <ToolboxModule.h>

namespace GW {
    struct GamePos;
}

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

    static bool PlaySound(const wchar_t* filename, const GW::Vec3f* position = nullptr, uint32_t flags = 0, void** handle_out = nullptr);
    static bool StopSound(void* handle);
    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void DrawSettingsInternal() override;
};
