#pragma once

#include <GWCA/Constants/Constants.h>

#include <ToolboxWindow.h>

namespace GW::Constants {
    enum class QuestID : uint32_t;
}

class DailyQuests : public ToolboxWindow {
    DailyQuests() = default;
    ~DailyQuests() override = default;

public:
    static DailyQuests& Instance()
    {
        static DailyQuests instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Daily Quests"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CALENDAR_ALT; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void DrawHelp() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;



private:



};
