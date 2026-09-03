#pragma once

#include <MinimapPlugin.h>
#include <ToolboxPlugin.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Utilities/Hook.h>

class TargetDetector final : public ToolboxPlugin, public MinimapRenderer {
public:
    enum class ZoneType : uint8_t {
        Polygon,
        DistanceFrom,
        Both,
    };

    enum class TriggerCondition : uint8_t {
        FullyVisible,
        FullyVisibleOrTimeout,
    };

    enum class ObjectiveStatus : uint8_t {
        NotStarted,
        Running,
        Completed,
    };

    enum class ActionType : uint8_t {
        MarkTargets,
        PingTarget,
        SelectTarget,
        PartyChat,
        LogMessage,
    };

    struct Point {
        float x = 0.f;
        float y = 0.f;
        uint32_t zplane = 0;
    };

    struct ObjectiveCondition {
        uint32_t objective_id = 0;
        ObjectiveStatus status = ObjectiveStatus::NotStarted;
        std::string description;
    };

    struct Action {
        ActionType type = ActionType::MarkTargets;
        bool enabled = true;
        std::string message;
    };

    struct Zone {
        std::string name;
        std::string description;
        uint32_t map_id = 0;
        bool enabled = true;
        bool log_when_triggered = false;
        ZoneType zone_type = ZoneType::Polygon;
        TriggerCondition trigger_condition = TriggerCondition::FullyVisibleOrTimeout;
        float trigger_timeout = 4.f;
        std::vector<Point> polygon;
        float center_x = 0.f;
        float center_y = 0.f;
        float radius = 1'000.f;
        std::vector<Action> actions;
        std::vector<uint32_t> model_ids;
        std::vector<ObjectiveCondition> objective_conditions;
        bool aggro_origin_enabled = false;
        bool aggro_origin_render_minimap = true;
        bool aggro_origin_render_terrain = true;
        std::vector<uint32_t> aggro_origin_skill_ids;
    };

    TargetDetector() = default;
    ~TargetDetector() override = default;

    [[nodiscard]] const char* Name() const override { return "TargetDetector"; }
    [[nodiscard]] bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Terminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;
    void RenderMinimap(IDirect3DDevice9* device, const MinimapRenderContext& context) override;

private:
    struct ZoneRuntime {
        bool triggered = false;
        bool aggro_consumed = false;
        std::optional<float> detection_elapsed;
        std::vector<uint32_t> targets;
    };

    struct AggroRuntime {
        bool active = false;
        size_t zone_index = 0;
        GW::GamePos origin;
        GW::GamePos current;
        float group_radius = 0.f;
        float outside_earshot_elapsed = 0.f;
        std::vector<uint32_t> targets;
    };

    void ResetInstance();
    void SyncRuntime();
    void EvaluateZones(float delta);
    void UpdateAggroOrigin(float delta);
    void TriggerZone(size_t index, const std::vector<uint32_t>& targets);
    void ExecuteActions(const Zone& zone, const std::vector<uint32_t>& targets);
    void ProcessPendingMarks();
    void RebuildTerrainPreview();
    [[nodiscard]] size_t TerrainSignature() const;
    [[nodiscard]] std::vector<uint32_t> MatchingTargets(const Zone& zone) const;
    [[nodiscard]] bool ObjectivesMatch(const Zone& zone) const;
    [[nodiscard]] bool Contains(const Zone& zone, const GW::GamePos& point) const;
    [[nodiscard]] bool IsFullyVisible(const Zone& zone) const;
    [[nodiscard]] bool SkillFilterMatches(const Zone& zone) const;
    [[nodiscard]] static bool PointInPolygon(const std::vector<Point>& polygon, float x, float y);
    [[nodiscard]] static std::vector<Zone> DefaultZones();
    [[nodiscard]] static const char* ActionLabel(ActionType type);
    [[nodiscard]] static const char* ObjectiveStatusLabel(ObjectiveStatus status);
    [[nodiscard]] static uint32_t AggroColor(float distance, float alpha);
    static void WriteLocal(const std::string& message);

    GW::HookEntry objective_add_hook_;
    GW::HookEntry objective_update_hook_;
    GW::HookEntry objective_done_hook_;
    std::vector<Zone> zones_ = DefaultZones();
    std::vector<ZoneRuntime> runtime_;
    AggroRuntime aggro_;
    std::unordered_map<uint32_t, ObjectiveStatus> objectives_;
    std::deque<uint32_t> pending_marks_;
    uint32_t mark_restore_target_ = 0;
    uint32_t mark_attempts_ = 0;
    std::optional<std::chrono::steady_clock::time_point> mark_target_selected_at_;
    uint32_t selected_zone_ = 0;
    uint32_t last_map_id_ = 0;
    uint32_t last_instance_time_ = 0;
    uint32_t new_model_id_ = 0;
    uint32_t new_skill_id_ = 0;
    size_t terrain_signature_ = 0;
    bool show_preview_ = false;
    bool minimap_rotation_enabled_ = true;
    bool terrain_preview_present_ = false;
    bool terrain_signature_valid_ = false;
    bool mark_restore_pending_ = false;
    bool initialized_ = false;
    bool terminating_ = false;
};
