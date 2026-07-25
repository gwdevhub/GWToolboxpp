#pragma once

#include <ToolboxWindow.h>
#include <Modules/QuestObservationService.h>
#include <Utils/EncString.h>

#include <memory>
#include <unordered_map>
#include <cstdint>

class QuestTrackerWindow : public ToolboxWindow {
    QuestTrackerWindow() = default;
    ~QuestTrackerWindow() override = default;

public:
    static QuestTrackerWindow& Instance()
    {
        static QuestTrackerWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Quest Tracker"; }
    [[nodiscard]] const char* Description() const override
    {
        return "Read-only live quest log, selected quest objectives, and mission objectives";
    }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_LIST; }

    void Initialize() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void SignalTerminate() override;
    void Terminate() override;

private:
    struct ObjectiveKey {
        GW::Constants::QuestID quest_id = GW::Constants::QuestID::None;
        size_t index = 0;

        bool operator==(const ObjectiveKey& other) const
        {
            return quest_id == other.quest_id && index == other.index;
        }
    };

    struct ObjectiveKeyHash {
        size_t operator()(const ObjectiveKey& key) const noexcept
        {
            return std::hash<uint32_t>{}(static_cast<uint32_t>(key.quest_id))
                ^ (std::hash<size_t>{}(key.index) << 1);
        }
    };

    void ClearDecodeCache();
    void SyncDecodeCache(const LiveQuestView& view);
    GuiUtils::EncString& NameDecoder(GW::Constants::QuestID quest_id, const std::wstring& encoded);
    GuiUtils::EncString& QuestObjectiveDecoder(GW::Constants::QuestID quest_id, size_t index, const std::wstring& encoded);
    GuiUtils::EncString& MissionObjectiveDecoder(uint32_t objective_id, const std::wstring& encoded);

    QuestObservationService observation_;
    uint64_t cached_revision_ = 0;

    std::unordered_map<GW::Constants::QuestID, std::unique_ptr<GuiUtils::EncString>> name_decoders_;
    std::unordered_map<ObjectiveKey, std::unique_ptr<GuiUtils::EncString>, ObjectiveKeyHash> quest_objective_decoders_;
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> mission_objective_decoders_;
};
