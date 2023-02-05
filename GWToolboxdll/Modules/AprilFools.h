#pragma once
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>

class AprilFools : public ToolboxModule {
    AprilFools() {};
    ~AprilFools() {};
public:
    static AprilFools& Instance() {
        static AprilFools instance;
        return instance;
    }

    const char* Name() const override { return "April Fools"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void SetInfected(GW::Agent* agent, bool is_infected);
    void SetEnabled(bool is_enabled);
};
