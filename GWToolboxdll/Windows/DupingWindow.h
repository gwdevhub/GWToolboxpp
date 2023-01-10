#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <ToolboxWindow.h>

class DupingWindow : public ToolboxWindow {
    DupingWindow() = default;
    ~DupingWindow() = default;
public:
    static DupingWindow& Instance() {
        static DupingWindow instance;
        return instance;
    }

    const char* Name() const override { return "Duping"; }
    const char* Icon() const override { return ICON_FA_COPY; }

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override {}

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

private:
    struct DupeInfo {
        DupeInfo(GW::AgentID agent_id, long long last_duped, int dupe_count) : agent_id(agent_id), last_duped(last_duped), dupe_count(dupe_count) {};

        GW::AgentID agent_id;
        long long last_duped;
        int dupe_count;
    };

    static bool OrderDupeInfo(DupeInfo& a, DupeInfo& b);
    void DrawDuping(const char* label, long long now, std::vector<DupeInfo> vec);

    std::vector<DupeInfo> souls{};
    std::vector<DupeInfo> waters{};
    std::vector<DupeInfo> minds{};
    float range = 1600.0f;
    bool hide_when_nothing = true;
    bool show_souls_counter = true;
    bool show_waters_counter = true;
    bool show_minds_counter = true;
    float souls_threshhold = 0.6f;
    float waters_threshhold = 0.5f;
    float minds_threshhold = 0.0f;
};
