#pragma once

#include <ToolboxModule.h>

namespace GW {
    struct GamePos;
    struct HookStatus;
    struct Vec3f;
} // namespace GW

// Bits/fields of SoundProps::flags, confirmed by decompiling Gw.388118.exe's native sound dispatcher (PlaySoundInternal @0x007aacd0 + wrappers @0x007ab460/0x007ab4c0/0x007b6750); [confirmed] = named/tested in the binary, [inferred] = observed but unnamed.
// Unscoped (not enum class) so flag combinations don't need a static_cast to uint32_t.
enum SoundFlags : uint32_t {
    SoundFlags_None = 0,

    // Bits 0-3 are a 4-bit SND_TYPE enum (asserted `type < SND_NUM_TYPES(5)`) [confirmed field width], not independent bits — don't OR this with another SND_TYPE value.
    SoundFlags_TypeMask = 0xf,
    // SND_TYPE == 4: NPC dialog/voice-over audio [inferred]; TextToSpeechModule mirrors it so OnPlaySound can recognise/suppress the in-game clip it's replacing.
    SoundFlags_Dialog = 0x4,

    // SND_FLAG_LOOPING [confirmed by name]; the engine asserts this can't combine with SoundFlags_NoHandle (looping needs a handle to stop it — see StopSound()'s caveat below).
    SoundFlags_Looping = 0x10,

    // SND_FLAG_NO_HANDLE [confirmed by name]: fire-and-forget, no handle returned; mutually exclusive with SoundFlags_Looping.
    SoundFlags_NoHandle = 0x40,

    // 3D playback relative to SoundProps::position [confirmed]: PlaySound3D (0x007ab4c0) hardcodes `flags |= 0x1400`; combine with SoundFlags_Dialog for overhead NPC speech (0x1404).
    SoundFlags_Positional = 0x1400,

    // SND_FLAG_STREAMING [confirmed by name]: streams from disk instead of fully decoding; the native PlayMusic path forces this on for every track.
    SoundFlags_Streaming = 0x40000000,

    // Default PlayMusic flags (matches the game's own): SND_TYPE==3 plus bit 0x80, an internal "preserve priority/volume" bit public wrappers refuse from callers [inferred]. Does NOT set SoundFlags_Looping — see WeatherModule's timer re-trigger pattern for repeating audio instead.
    SoundFlags_MusicDefault = 0x83,
};

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

    static bool PlayMusic(const wchar_t* filename, uint32_t flags = SoundFlags_MusicDefault);

    static bool PlaySound(const wchar_t* filename, const GW::Vec3f* position = nullptr, uint32_t flags = SoundFlags_None, void** handle_out = nullptr);
    static bool PlaySoundFileId(const uint32_t file_id, const GW::Vec3f* position = nullptr, uint32_t flags = SoundFlags_None, void** handle_out = nullptr);
    static bool StopSound(void* handle);
    static void RegisterPlaySoundCallback(GW::HookEntry* hook_entry, PlaySoundCallback callback);
    static void RemovePlaySoundCallback(GW::HookEntry* hook_entry);
    static void BlockSoundForMs(const wchar_t* filename, clock_t ms);

    void Initialize() override;
    void SignalTerminate() override;
    void Update(float) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
