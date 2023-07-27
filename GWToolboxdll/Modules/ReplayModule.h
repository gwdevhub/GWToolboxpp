#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <ToolboxModule.h>

class ReplayModule : public ToolboxModule {
    ReplayModule() = default;
    ~ReplayModule() = default;

public:
    static ReplayModule& Instance() {
        static ReplayModule instance;
        return instance;
    }

    const char* Name() const override { return "Replay"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
};
