#pragma once

#include <ToolboxModule.h>

class PetModule : public ToolboxModule {
    PetModule() = default;
    ~PetModule() override = default;

public:
    static PetModule& Instance()
    {
        static PetModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Pet"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PAW; }

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void Update(float) override;

private:
    bool keep_pet_window_open = false;
};
