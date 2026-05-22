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

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void DrawHelp() override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
