#pragma once

#include <Condition.h>
#include <ToolboxUIPlugin.h>
#include <Utils/FontLoader.h>
#include <Enums.h>

struct Split 
{
    // Runtime Info
    bool isPB = false;
    bool completed = false;
    int currentTime = 0;
    std::string displayTrackedTime = "0:00.0";
    std::string displayPBTime = "0:00.0";

    // Serialized
    std::string name = "New split";
    int trackedTime   = 0;
    int pbSegmentTime = 0;
    std::vector<ConditionPtr> conditions;
    Trigger trigger = Trigger::None;
    TriggerData triggerData = {};
};

struct Run 
{
    std::string name = "New run";
    std::string topText = "";
    std::vector<Split> splits;
};

class GWSplits : public ToolboxUIPlugin {
public:
    GWSplits()
    {
        show_closebutton = false;
        show_title = false;
        can_collapse = false;
    }
    const char* Name() const override { return "GWSplits"; }
    const char* Icon() const override { return ICON_FA_CLOCK; }

    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    
    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
     
    void handleTrigger(Trigger triggerType, std::function<bool(const Split&)> extraConditions = [](const Split&) { return true; });
    void loadFromIniFile(const wchar_t*);

private:
    void completeSplit(std::vector<Split>::iterator);

    std::shared_ptr<Run> currentRun = nullptr;
    std::vector<std::shared_ptr<Run>> runs;

    bool showRunTime = true;
    bool showSegmentTime = true;
    bool showBestPossibleTime = true;
    bool showSumOfBest = true;
    bool showLastSegment = true;
    int fontSizeIndex = 2;

    // Store these here to be able to keep the value when traveling to outpost
    int segmentStart = 0;
    int runTime = 0;
    int segmentTime = 0;
    int bestPossibleTime = 0;
    int sumOfBest = 0;
    int lastSegmentGain = 0;
    ImU32 lastSegmentColor = 0;

    void drawRuns();
};
