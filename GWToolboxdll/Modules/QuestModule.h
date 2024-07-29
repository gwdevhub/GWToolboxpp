#pragma once
#include <IconsFontAwesome5.h>

#include <ToolboxModule.h>

namespace GW {
    namespace Constants {
        enum class QuestID : uint32_t;
    }
}
namespace GuiUtils {
    class EncString;
}

struct QuestObjective {
    QuestObjective(GW::Constants::QuestID quest_id, const wchar_t* objective_enc, bool is_completed);
    ~QuestObjective();
    // copy not allowed
    QuestObjective(const QuestObjective& other) = delete;
    QuestObjective(QuestObjective&& other) noexcept {
        quest_id = other.quest_id;
        is_completed = other.is_completed;
        objective_enc = other.objective_enc;
        other.objective_enc = nullptr;
    }

    // copy not allowed
    QuestObjective& operator=(const QuestObjective& other) = delete;
    QuestObjective& operator=(QuestObjective&& other) noexcept
    {
        quest_id = other.quest_id;
        is_completed = other.is_completed;
        objective_enc = other.objective_enc;
        other.objective_enc = nullptr;
        return *this;
    }

    GW::Constants::QuestID quest_id = (GW::Constants::QuestID)0;
    GuiUtils::EncString* objective_enc = nullptr;
    bool is_completed = false;
};

class QuestModule : public ToolboxModule {
public:
    static QuestModule& Instance()
    {
        static QuestModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Quest Module"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COMPASS; }
    [[nodiscard]] const char* Description() const override { return "A set of QoL improvements to the quest log and related behavior"; }

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    void Update(float) override;

    static ImU32 GetQuestColor(GW::Constants::QuestID);
    static std::vector<QuestObjective> ParseQuestObjectives(GW::Constants::QuestID quest_id);
};
