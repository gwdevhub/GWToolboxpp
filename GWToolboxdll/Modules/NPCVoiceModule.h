#pragma once

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hook.h>
#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

enum class TTSProvider { ElevenLabs = 0, OpenAI = 1 };

class NPCVoiceModule : public ToolboxModule {
public:
    static NPCVoiceModule& Instance()
    {
        static NPCVoiceModule instance;
        return instance;
    }

    const char* Name() const override { return "NPC Voice"; }
    const char* Description() const override { return "AI-generated voices for NPCs using ElevenLabs or OpenAI TTS with cost optimization"; }
    const char* Icon() const override { return ICON_FA_VOLUME_UP; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    // Cost optimization methods
    void SetTTSProvider(TTSProvider provider);
};
