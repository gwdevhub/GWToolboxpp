#pragma once

#include <ToolboxModule.h>

class IronManModule : public ToolboxModule {
    IronManModule() = default;
    ~IronManModule() override = default;

public:
    static IronManModule& Instance()
    {
        static IronManModule instance;
        return instance;
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_USER; }
    [[nodiscard]] const char* Name() const override { return "Iron Man Settings"; }
    [[nodiscard]] const char* Description() const override { return "Limit or restrict access to in-game features or areas to create a more challenging gameplay!"; }

    void Initialize() override;
    void Update(float) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void Terminate() override;
    void DrawSettingsInternal() override;
};
