#pragma once

#include <ToolboxModule.h>

namespace GW {
    struct Agent;
}

class AprilFools : public ToolboxModule {
    AprilFools() = default;
    ~AprilFools() override = default;

public:
    static AprilFools& Instance()
    {
        static AprilFools instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "April Fools"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    static void SetInfected(GW::Agent* agent, bool is_infected);
    static void SetEnabled(bool is_enabled);
};
