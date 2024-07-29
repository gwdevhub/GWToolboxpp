#pragma once

#include <ToolboxWidget.h>

#include "Utils/GuiUtils.h"
#include "Windows/DailyQuestsWindow.h"

class ActiveQuestWidget : public ToolboxWidget {
    ActiveQuestWidget() = default;
    ~ActiveQuestWidget() override = default;

public:
    static ActiveQuestWidget& Instance()
    {
        static ActiveQuestWidget instance;
        return instance;
    }

    struct QuestObjective {
        int index;
        std::string objective;
        bool completed;
    };

    [[nodiscard]] const char* Name() const override { return "Active Quest Info"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BAHAI; }

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void Draw(IDirect3DDevice9*) override;
    static bool Enqueue(GW::Constants::QuestID quest_id, std::function<void(GW::Constants::QuestID quest_id, std::string quest_name, std::vector<QuestObjective> quest_objectives)> func);
};
