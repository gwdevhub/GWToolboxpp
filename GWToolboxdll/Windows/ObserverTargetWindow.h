
#pragma once

#include <Windows/ObserverPlayerWindow.h>

#define NO_AGENT 0

class ObserverTargetWindow : public ObserverPlayerWindow {
    ObserverTargetWindow() = default;
    ~ObserverTargetWindow() = default;

public:
    static ObserverTargetWindow& Instance() {
        static ObserverTargetWindow instance;
        return instance;
    }

    void Prepare() override;
    uint32_t GetTracking() override;
    uint32_t GetComparison() override;

    const char* Name() const override { return "Observer Target"; }
    const char* Icon() const override { return ICON_FA_EYE; }

protected:
    uint32_t current_tracked_agent_id = NO_AGENT;
    uint32_t current_compared_agent_id = NO_AGENT;
};
