#pragma once

#include <ToolboxModule.h>

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

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
