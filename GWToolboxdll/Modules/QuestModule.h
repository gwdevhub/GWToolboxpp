#pragma once
#include <memory>
#include <IconsFontAwesome5.h>

#include <ToolboxModule.h>

namespace GW {
    namespace Constants {
        enum class QuestID : uint32_t;
    }
    struct Quest;
}
namespace GuiUtils {
    class EncString;
}

struct QuestObjective {
    QuestObjective(GW::Constants::QuestID quest_id, const wchar_t* objective_enc, bool is_completed);
    ~QuestObjective() = default;
    // copy not allowed
    QuestObjective(const QuestObjective& other) = delete;
    QuestObjective(QuestObjective&& other) noexcept = default;

    // copy not allowed
    QuestObjective& operator=(const QuestObjective& other) = delete;
    QuestObjective& operator=(QuestObjective&& other) noexcept = default;

    GW::Constants::QuestID quest_id = (GW::Constants::QuestID)0;
    std::unique_ptr<GuiUtils::EncString> objective_enc;
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

    struct Settings {
        bool draw_quest_path_on_minimap = true;
        bool draw_quest_path_on_mission_map = true;
        bool draw_quest_path_on_terrain = false;
        bool show_paths_to_all_quests = false;
        float custom_quest_marker_world_pos_x = 0.f;
        float custom_quest_marker_world_pos_y = 0.f;
        bool double_click_to_travel_to_quest = true;
        bool keep_current_quest_when_new_quest_added = false;
    };

    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Initialize() override;
    void Terminate() override;
    void SignalTerminate() override;
    void Update(float) override;
    bool CanTerminate() override;
    static void FetchMissingQuestInfo();

    static const GW::Quest* GetCustomQuestMarker();
    // If `quest_id` is the custom marker quest and a marker is set, fills `out` with its
    // exact world-map position and returns true. Lets the world map plot the real spot
    // instead of falling back to the destination map's label.
    static bool GetCustomQuestMarkerWorldPos(GW::Constants::QuestID quest_id, GW::Vec2f& out);

    static void SetCustomQuestMarker(const GW::Vec2f& world_pos, bool set_active = false);
    static void ClearCustomQuestMarker();

    // Callback fired when the custom quest marker is set or cleared.
    using CustomMarkerChangedCallback = void(*)();
    static void AddCustomMarkerChangedCallback(CustomMarkerChangedCallback cb);
    static void RemoveCustomMarkerChangedCallback(CustomMarkerChangedCallback cb);
    // Fake an action of the user selecting an active quest, without making any server request.
    static bool SetActiveQuestId(GW::Constants::QuestID quest_id, bool notify_server = true);

    static ImU32& GetQuestColor(GW::Constants::QuestID);
    static ImU32& GetQuestLineColor(GW::Constants::QuestID);
    static std::vector<QuestObjective> ParseQuestObjectives(GW::Constants::QuestID quest_id);
};
