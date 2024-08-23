#pragma once

#include <ToolboxModule.h>
#include <string.h>
#include <ToolboxIni.h>

class ToolboxCommandsModule : public ToolboxModule {
    ToolboxCommandsModule() = default;
    ~ToolboxCommandsModule() override = default;

public:
    static ToolboxCommandsModule& Instance()
    {
        static ToolboxCommandsModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Toolbox Commands"; }
    [[nodiscard]] const char* Description() const override { return "Registers a toolbox:// scheme and listens to user commands"; }
    bool HasSettings() override { return true; }
    [[nodiscard]] const char* SettingsName() const override { return "Toolbox Settings"; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(ToolboxIni* ini) override;
};
