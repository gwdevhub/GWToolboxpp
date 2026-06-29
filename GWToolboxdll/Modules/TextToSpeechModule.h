#pragma once

#include <ToolboxModule.h>

#include <GWCA/Constants/Constants.h>

class TextToSpeechModule : public ToolboxModule {
public:
    static TextToSpeechModule& Instance()
    {
        static TextToSpeechModule instance;
        return instance;
    }

    const char* Name() const override { return "Text-to-speech"; }
    const char* Description() const override { return "AI-generated voices for NPCs using ElevenLabs or OpenAI TTS with cost optimization"; }
    const char* Icon() const override { return ICON_FA_VOLUME_UP; }

    struct Settings {
        std::string current_tts_provider = "GWDevHub TTS";
        bool stop_speech_when_dialog_closed = false;
        bool play_goodbye_messages = true;
        bool only_use_first_dialog = false;
        bool only_use_first_sentence = true;
        bool play_speech_from_non_friendly_npcs = true;
        bool play_speech_bubbles_in_explorable = false;
        bool play_speech_bubbles_in_outpost = true;
        bool play_speech_bubbles_from_party_members = false;
        float npc_speech_bubble_range = GW::Constants::Range::Earshot;
        bool play_speech_from_vendors = true;
        bool play_tts_in_explorable_areas = true;
        bool play_tts_in_outposts = true;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
