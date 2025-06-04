#pragma once

#include <ToolboxModule.h>

namespace GW {
    struct GamePos;
    struct HookStatus;
    struct Vec3f;
} // namespace GW

struct SoundProps {
    uint32_t flags;
    uint32_t h0004[4];
    uint32_t h0014;
    uint32_t h0018;
    uint32_t h001c;
    GW::Vec3f position;
    uint32_t h002c;
    void* h0030;
    uint32_t h0034[5];
    void* h0048;
    uint32_t h004c[5];
    void* h0060;
    uint32_t h0064[5];
    ~SoundProps();
};
using PlaySoundCallback = void(__cdecl*)(GW::HookStatus*, const wchar_t* filename, struct SoundProps* props);

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
    static void RegisterPlaySoundCallback(GW::HookEntry* hook_entry, PlaySoundCallback callback);
    static void RemovePlaySoundCallback(GW::HookEntry* hook_entry);
    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void DrawSettingsInternal() override;
};
