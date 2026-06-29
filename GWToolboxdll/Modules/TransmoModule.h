#pragma once

#include <ToolboxModule.h>

class TransmoModule : public ToolboxModule {
    TransmoModule() = default;
    ~TransmoModule() override = default;

public:
    static TransmoModule& Instance()
    {
        static TransmoModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Transmo"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    // Persisted shape of one transmo NPC list entry
    struct TransmoEntrySetting {
        std::string name;
        int npc_id = 0;
        int scale = 100;
        int model_file_id = 0;
        int model_file_data = 0;
        int flags = 0;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawHelp() override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
