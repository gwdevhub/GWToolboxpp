#pragma once

#include <ToolboxModule.h>

namespace GW {
    struct GamePos;
    struct HookStatus;
    struct Vec3f;
} // namespace GW

// Bits/fields of SoundProps::flags / the `flags` param of AudioSettings::PlaySound(FileId)/PlayMusic.
// Confirmed by decompiling the game's native sound dispatcher in Gw.388118.exe: the core
// PlaySoundInternal(filename, SoundProps*) at 0x007aacd0, and its wrappers PlaySound (0x007ab460),
// PlaySound3D (0x007ab4c0) and PlayMusic (0x007b6750). Several SND_FLAG_* names below are recovered
// verbatim from an assert string embedded in the binary, not guessed. [confirmed] = named/tested
// explicitly in the decompiled code; [inferred] = behavior observed but no name recovered.
// Unscoped (not enum class) so flag combinations don't need a static_cast to uint32_t.
enum SoundFlags : uint32_t {
    SoundFlags_None = 0,

    // Bits 0-3 aren't independent flags: they're a 4-bit SND_TYPE enum, asserted
    // `type < SND_NUM_TYPES(5)` [confirmed field width]. It selects a playback category/volume
    // bucket; don't OR this with another SND_TYPE value. Only value 4 ("dialog") is named here.
    SoundFlags_TypeMask = 0xf,
    // SND_TYPE == 4: NPC dialog/voice-over audio [inferred from toolbox usage]. The game sets
    // this on its own in-game voice clips; TextToSpeechModule mirrors it so OnPlaySound can
    // recognise (and suppress) the clip it's about to replace with generated speech.
    SoundFlags_Dialog = 0x4,

    // SND_FLAG_LOOPING [confirmed by name: assert string is
    // "(info.flags & SND_FLAG_LOOPING) == 0"]. Repeats playback. The engine asserts this can't
    // be combined with SoundFlags_NoHandle — looping requires a handle so the caller can
    // eventually stop it (note: StopSound() below is marked as currently non-functional).
    SoundFlags_Looping = 0x10,

    // SND_FLAG_NO_HANDLE [confirmed by name]. Fire-and-forget: no handle is returned to the
    // caller. Mutually exclusive with SoundFlags_Looping (the engine asserts on the combination).
    SoundFlags_NoHandle = 0x40,

    // Play positionally in 3D space relative to SoundProps::position, rather than as a
    // flat/non-spatial sound [confirmed]: the game's own PlaySound3D wrapper (0x007ab4c0)
    // hardcodes `flags |= 0x1400` on every 3D call. Combine with SoundFlags_Dialog for overhead
    // NPC speech not shown in the dialog window (SoundFlags_Dialog | SoundFlags_Positional == 0x1404).
    SoundFlags_Positional = 0x1400,

    // SND_FLAG_STREAMING [confirmed by name]. Streams from disk instead of decoding fully into
    // memory; the native PlayMusic path forces this on for every track regardless of caller flags.
    SoundFlags_Streaming = 0x40000000,

    // Default flags for PlayMusic; matches what the game itself uses to start background music.
    // Decodes as SND_TYPE == 3 (bits 0-1) plus bit 0x80, an internal "preserve priority/volume
    // override" bit that the PlaySound/PlaySound3D public wrappers explicitly refuse to accept
    // from callers [inferred]. Notably this does NOT set SoundFlags_Looping — native background
    // music repeat isn't driven by this bit (see WeatherModule for the toolbox's own re-trigger-
    // on-a-timer pattern used elsewhere for repeating sounds).
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
