#pragma once

#include <ToolboxModule.h>

class MouseWalkingModule : public ToolboxModule {
    MouseWalkingModule() = default;
    ~MouseWalkingModule() override = default;

public:
    static MouseWalkingModule& Instance()
    {
        static MouseWalkingModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Auto Mouse Walking"; }
    [[nodiscard]] const char* Description() const override { return "Enables mouse-walking only when LeftAlt key is pressed"; }
    bool HasSettings() override { return true; }
    [[nodiscard]] const char* SettingsName() const override { return "Inventory Settings"; }

    void Update(float) override;
    void DrawSettingsInternal() override;
    void Terminate() override;
    void Initialize() override;
};
