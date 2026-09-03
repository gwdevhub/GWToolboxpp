#include "TargetDetector.h"

#include <PluginUtils.h>
#include <Rendering.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <cstring>
#include <DirectXMath.h>
#include <numbers>

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static TargetDetector instance;
    return &instance;
}
#endif

namespace {
    constexpr auto MarkTargetDelay = std::chrono::milliseconds(50);
    constexpr uint32_t MaximumMarkAttempts = 51;
    constexpr float AggroReleaseDelay = 10.f;
    constexpr float AggroGroupPadding = 150.f;
    constexpr float CompassRange = 5'000.f;
    constexpr float EarshotRange = 1'012.f;
    constexpr uint32_t ZoneColor = D3DCOLOR_ARGB(190, 255, 180, 40);

    struct Vertex {
        float x;
        float y;
        float z;
        D3DCOLOR color;
    };

    int ResizeString(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
            return 0;
        }
        const auto value = static_cast<std::string*>(data->UserData);
        value->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = value->data();
        return 0;
    }

    bool InputString(const char* label, std::string& value)
    {
        if (value.capacity() < 128) {
            value.reserve(128);
        }
        const auto changed = ImGui::InputText(
            label,
            value.data(),
            value.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize,
            ResizeString,
            &value);
        value.resize(std::strlen(value.c_str()));
        return changed;
    }

    float SquareDistance(const float x1, const float y1, const float x2, const float y2)
    {
        const auto dx = x1 - x2;
        const auto dy = y1 - y2;
        return dx * dx + dy * dy;
    }

    void DrawLineStrip(IDirect3DDevice9* device, const std::vector<Vertex>& vertices)
    {
        if (vertices.size() < 2) {
            return;
        }
        device->DrawPrimitiveUP(
            D3DPT_LINESTRIP,
            static_cast<UINT>(vertices.size() - 1),
            vertices.data(),
            sizeof(Vertex));
    }

    void DrawTriangleFan(IDirect3DDevice9* device, const std::vector<Vertex>& vertices)
    {
        if (vertices.size() < 3) {
            return;
        }
        device->DrawPrimitiveUP(
            D3DPT_TRIANGLEFAN,
            static_cast<UINT>(vertices.size() - 2),
            vertices.data(),
            sizeof(Vertex));
    }

    std::vector<Vertex> CircleVertices(
        const float x, const float y, const float radius, const D3DCOLOR color)
    {
        constexpr size_t Segments = 64;
        std::vector<Vertex> vertices;
        vertices.reserve(Segments + 1);
        for (auto index = size_t{0}; index <= Segments; ++index) {
            const auto angle = std::numbers::pi_v<float> * 2.f
                * static_cast<float>(index) / static_cast<float>(Segments);
            vertices.push_back({
                x + std::cos(angle) * radius,
                y + std::sin(angle) * radius,
                0.f,
                color,
            });
        }
        return vertices;
    }

}

std::vector<TargetDetector::Zone> TargetDetector::DefaultZones()
{
    const auto mark = Action{ActionType::MarkTargets, true, {}};
    const auto objective = [](const uint32_t id, const char* description) {
        return ObjectiveCondition{id, ObjectiveStatus::NotStarted, description};
    };
    return {
        {
            .name = "UW Pits - Bridge patrol skele",
            .description = "Marks the bridge patrol skele in Pits",
            .map_id = 0x48,
            .polygon = {
                {12415.7f, 672.f, 0}, {12366.1f, 2826.9f, 0},
                {14532.5f, 3396.5f, 0}, {15456.f, 3349.1f, 0},
                {15766.5f, 2767.4f, 0}, {15840.f, 2161.9f, 0},
                {16032.f, -569.1f, 0}, {13955.3f, -786.5f, 0},
            },
            .actions = {mark},
            .model_ids = {0x958},
            .objective_conditions = {objective(0x69, "Quest: Imprisoned Spirits")},
        },
        {
            .name = "UW Pits - Top needs touch",
            .description = "Alerts when the top group may need a touch",
            .map_id = 0x48,
            .zone_type = ZoneType::Both,
            .polygon = {
                {14489.1f, 4342.1f, 0}, {13839.6f, 3428.3f, 0},
                {12959.2f, 4008.5f, 0}, {13684.1f, 5015.f, 0},
            },
            .center_x = 12813.f,
            .center_y = 4696.f,
            .radius = 1015.f,
            .actions = {
                {ActionType::PartyChat, true, "Top might need a touch!"},
                {ActionType::LogMessage, false, "Top might need a touch!"},
                {ActionType::PingTarget, true, {}},
            },
            .model_ids = {0x94e, 0x94f},
            .objective_conditions = {objective(0x69, "Quest: Imprisoned Spirits")},
        },
        {
            .name = "UW Plains - Pits-side patrol skele",
            .description = "Marks the pits-side patrol skele in Plains",
            .map_id = 0x48,
            .polygon = {
                {13633.6f, -17421.6f, 0}, {13099.2f, -16800.f, 0},
                {12720.f, -15792.f, 0}, {13344.f, -15168.f, 0},
                {14208.f, -15264.f, 0}, {14304.f, -16512.f, 0},
            },
            .actions = {mark},
            .model_ids = {0x958},
            .objective_conditions = {objective(0x6a, "Quest: The Four Horsemen")},
        },
        {
            .name = "UW Wastes - King patrol coldfires",
            .description = "Marks the King patrol coldfires in Wastes",
            .map_id = 0x48,
            .polygon = {
                {5728.9f, 19586.8f, 0}, {6643.1f, 19134.6f, 0},
                {4800.f, 15470.8f, 0}, {3434.3f, 16261.7f, 0},
            },
            .actions = {mark},
            .model_ids = {0x950},
            .aggro_origin_enabled = true,
            .aggro_origin_skill_ids = {0x1cf},
        },
    };
}

void TargetDetector::Initialize(
    ImGuiContext* context, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(context, allocator_fns, toolbox_dll);
    terminating_ = false;
    initialized_ = true;
    SyncRuntime();
    MinimapRenderer::RegisterRenderer(this);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveAdd>(
        &objective_add_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveAdd* packet) {
            if (!terminating_ && packet) {
                objectives_[packet->objective_id] = ObjectiveStatus::Running;
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(
        &objective_update_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
            if (!terminating_ && packet) {
                objectives_[packet->objective_id] = ObjectiveStatus::Running;
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &objective_done_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
            if (!terminating_ && packet) {
                objectives_[packet->objective_id] = ObjectiveStatus::Completed;
            }
        });
}

void TargetDetector::SignalTerminate()
{
    terminating_ = true;
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveAdd>(&objective_add_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveUpdateName>(&objective_update_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveDone>(&objective_done_hook_);
    if (initialized_) {
        MinimapRenderer::UnregisterRenderer(this);
    }
    RenderingUtils::clearDrawingList(this);
    terrain_preview_present_ = false;
    terrain_signature_valid_ = false;
    if (!pending_marks_.empty() && mark_restore_pending_) {
        GW::Agents::ChangeTarget(mark_restore_target_);
    }
    pending_marks_.clear();
    aggro_ = {};
    mark_target_selected_at_.reset();
    mark_restore_target_ = 0;
    mark_attempts_ = 0;
    mark_restore_pending_ = false;
    ToolboxPlugin::SignalTerminate();
}

void TargetDetector::Terminate()
{
    initialized_ = false;
    runtime_.clear();
    objectives_.clear();
    ToolboxPlugin::Terminate();
}

void TargetDetector::SyncRuntime()
{
    if (runtime_.size() != zones_.size()) {
        runtime_.resize(zones_.size());
    }
}

void TargetDetector::ResetInstance()
{
    runtime_.assign(zones_.size(), {});
    aggro_ = {};
    objectives_.clear();
    pending_marks_.clear();
    mark_target_selected_at_.reset();
    mark_restore_target_ = 0;
    mark_attempts_ = 0;
    mark_restore_pending_ = false;
    RenderingUtils::clearDrawingList(this);
    terrain_preview_present_ = false;
    terrain_signature_valid_ = false;
}

void TargetDetector::Update(const float delta)
{
    if (terminating_) {
        return;
    }
    SyncRuntime();
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    const auto instance_time = GW::Map::GetInstanceTime();
    if (map_id != last_map_id_ || instance_time < last_instance_time_) {
        ResetInstance();
        last_map_id_ = map_id;
    }
    last_instance_time_ = instance_time;
    ProcessPendingMarks();
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable
        || !GW::Map::GetIsMapLoaded()) {
        return;
    }
    EvaluateZones(delta);
    UpdateAggroOrigin(delta);
}

bool TargetDetector::PointInPolygon(
    const std::vector<Point>& polygon, const float x, const float y)
{
    if (polygon.size() < 3) {
        return false;
    }
    auto inside = false;
    for (auto current = size_t{0}, previous = polygon.size() - 1;
         current < polygon.size(); previous = current++) {
        const auto& a = polygon[current];
        const auto& b = polygon[previous];
        const auto crosses = (a.y > y) != (b.y > y)
            && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x;
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

bool TargetDetector::Contains(const Zone& zone, const GW::GamePos& point) const
{
    const auto in_polygon = PointInPolygon(zone.polygon, point.x, point.y);
    const auto in_distance = zone.radius > 0.f
        && SquareDistance(point.x, point.y, zone.center_x, zone.center_y) <= zone.radius * zone.radius;
    switch (zone.zone_type) {
        case ZoneType::Polygon: return in_polygon;
        case ZoneType::DistanceFrom: return in_distance;
        case ZoneType::Both: return in_polygon && in_distance;
    }
    return false;
}

bool TargetDetector::ObjectivesMatch(const Zone& zone) const
{
    return std::ranges::all_of(zone.objective_conditions, [this](const auto& condition) {
        const auto found = objectives_.find(condition.objective_id);
        const auto status = found == objectives_.end()
            ? ObjectiveStatus::NotStarted
            : found->second;
        return status == condition.status;
    });
}

std::vector<uint32_t> TargetDetector::MatchingTargets(const Zone& zone) const
{
    std::vector<uint32_t> result;
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents || !agents->valid()) {
        return result;
    }
    for (const auto agent : *agents) {
        const auto living = agent ? agent->GetAsAgentLiving() : nullptr;
        if (!living
            || !living->GetIsAlive()
            || !std::ranges::contains(zone.model_ids, static_cast<uint32_t>(living->player_number))
            || !Contains(zone, living->pos)) {
            continue;
        }
        result.push_back(living->agent_id);
    }
    return result;
}

bool TargetDetector::IsFullyVisible(const Zone& zone) const
{
    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) {
        return false;
    }
    const auto polygon_visible = zone.polygon.size() >= 3
        && std::ranges::all_of(zone.polygon, [player](const auto& point) {
               return SquareDistance(point.x, point.y, player->x, player->y)
                   <= CompassRange * CompassRange;
           });
    const auto center_distance = std::sqrt(
        SquareDistance(zone.center_x, zone.center_y, player->x, player->y));
    const auto distance_visible = zone.radius > 0.f && center_distance + zone.radius <= CompassRange;
    switch (zone.zone_type) {
        case ZoneType::Polygon: return polygon_visible;
        case ZoneType::DistanceFrom: return distance_visible;
        case ZoneType::Both: return polygon_visible && distance_visible;
    }
    return false;
}

void TargetDetector::EvaluateZones(const float delta)
{
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    for (auto index = size_t{0}; index < zones_.size(); ++index) {
        const auto& zone = zones_[index];
        auto& runtime = runtime_[index];
        if (!zone.enabled || runtime.triggered || zone.map_id != map_id || !ObjectivesMatch(zone)) {
            continue;
        }
        const auto targets = MatchingTargets(zone);
        if (targets.empty()) {
            continue;
        }
        if (!runtime.detection_elapsed) {
            runtime.detection_elapsed = 0.f;
        }
        else {
            *runtime.detection_elapsed += std::max(0.f, delta);
        }
        const auto timeout = zone.trigger_condition == TriggerCondition::FullyVisibleOrTimeout
            && *runtime.detection_elapsed >= zone.trigger_timeout;
        if (IsFullyVisible(zone) || timeout) {
            TriggerZone(index, targets);
        }
    }
}

void TargetDetector::TriggerZone(const size_t index, const std::vector<uint32_t>& targets)
{
    auto& runtime = runtime_[index];
    runtime.triggered = true;
    runtime.targets = targets;
    const auto& zone = zones_[index];
    if (zone.log_when_triggered) {
        WriteLocal(zone.name + " - Trigger zone fired");
    }
    ExecuteActions(zone, targets);
}

void TargetDetector::ExecuteActions(const Zone& zone, const std::vector<uint32_t>& targets)
{
    if (targets.empty()) {
        return;
    }
    const auto selection_action = std::ranges::any_of(zone.actions, [](const auto& action) {
        return action.enabled && action.type == ActionType::SelectTarget;
    });
    for (const auto& action : zone.actions) {
        if (!action.enabled) {
            continue;
        }
        switch (action.type) {
            case ActionType::MarkTargets:
                if (pending_marks_.empty()) {
                    mark_restore_target_ = selection_action ? targets.front() : GW::Agents::GetTargetId();
                    mark_attempts_ = 0;
                    mark_restore_pending_ = true;
                }
                for (const auto target : targets) {
                    if (!std::ranges::contains(pending_marks_, target)) {
                        pending_marks_.push_back(target);
                    }
                }
                break;
            case ActionType::PingTarget: {
                auto packet = GW::UI::UIPacket::kSendCallTarget{
                    .call_type = GW::CallTargetType::AttackingOrTargetting,
                    .agent_id = targets.front(),
                };
                GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &packet);
                break;
            }
            case ActionType::SelectTarget:
                GW::Agents::ChangeTarget(targets.front());
                break;
            case ActionType::PartyChat: {
                const auto message = PluginUtils::StringToWString(action.message);
                if (!message.empty()) GW::Chat::SendChat('#', message.c_str());
                break;
            }
            case ActionType::LogMessage:
                if (!action.message.empty()) WriteLocal(action.message);
                break;
        }
    }
}

void TargetDetector::ProcessPendingMarks()
{
    if (pending_marks_.empty()) {
        return;
    }
    const auto finish_current = [this] {
        pending_marks_.pop_front();
        mark_target_selected_at_.reset();
        mark_attempts_ = 0;
        if (pending_marks_.empty() && mark_restore_pending_) {
            GW::Agents::ChangeTarget(mark_restore_target_);
            mark_restore_target_ = 0;
            mark_restore_pending_ = false;
        }
    };
    const auto target = pending_marks_.front();
    if (!GW::Agents::GetAgentByID(target)) {
        finish_current();
        return;
    }
    if (GW::Agents::GetTargetId() != target) {
        if (mark_attempts_ >= MaximumMarkAttempts) {
            finish_current();
            return;
        }
        ++mark_attempts_;
        if (GW::Agents::ChangeTarget(target)) {
            mark_target_selected_at_ = std::chrono::steady_clock::now();
        }
        return;
    }
    mark_attempts_ = 0;
    const auto now = std::chrono::steady_clock::now();
    if (!mark_target_selected_at_) {
        mark_target_selected_at_ = now;
        return;
    }
    if (now - *mark_target_selected_at_ < MarkTargetDelay) {
        return;
    }
    GW::Chat::SendChat('/', "marktarget");
    finish_current();
}

bool TargetDetector::SkillFilterMatches(const Zone& zone) const
{
    return zone.aggro_origin_skill_ids.empty()
        || std::ranges::any_of(zone.aggro_origin_skill_ids, [](const auto skill_id) {
               return GW::SkillbarMgr::GetSkillSlot(
                          static_cast<GW::Constants::SkillID>(skill_id)) >= 0;
           });
}

void TargetDetector::UpdateAggroOrigin(const float delta)
{
    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) {
        aggro_ = {};
        return;
    }

    struct GroupSnapshot {
        GW::GamePos centroid;
        float radius = 0.f;
        bool within_earshot = false;
        bool within_compass = false;
        std::vector<uint32_t> targets;
    };
    const auto snapshot = [player](const std::vector<uint32_t>& targets)
        -> std::optional<GroupSnapshot> {
        auto result = GroupSnapshot{};
        auto positions = std::vector<GW::GamePos>{};
        positions.reserve(targets.size());
        result.targets.reserve(targets.size());
        for (const auto target_id : targets) {
            const auto agent = GW::Agents::GetAgentByID(target_id);
            const auto living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living || !living->GetIsAlive()) continue;
            const auto distance = SquareDistance(living->x, living->y, player->x, player->y);
            result.within_earshot |= distance <= EarshotRange * EarshotRange;
            result.within_compass |= distance <= CompassRange * CompassRange;
            positions.push_back(living->pos);
            result.targets.push_back(target_id);
        }
        if (positions.empty()) return std::nullopt;
        for (const auto& position : positions) {
            result.centroid.x += position.x;
            result.centroid.y += position.y;
        }
        const auto count = static_cast<float>(positions.size());
        result.centroid.x /= count;
        result.centroid.y /= count;
        result.centroid.zplane = positions.front().zplane;
        for (const auto& position : positions) {
            result.radius = std::max(
                result.radius,
                std::sqrt(SquareDistance(
                    position.x, position.y, result.centroid.x, result.centroid.y)));
        }
        return result;
    };

    if (aggro_.active) {
        const auto current = snapshot(aggro_.targets);
        if (!current || !current->within_compass) {
            aggro_ = {};
            return;
        }
        aggro_.current = current->centroid;
        if (current->within_earshot) {
            aggro_.outside_earshot_elapsed = 0.f;
        }
        else {
            aggro_.outside_earshot_elapsed += std::max(0.f, delta);
            if (aggro_.outside_earshot_elapsed >= AggroReleaseDelay) aggro_ = {};
        }
        return;
    }

    for (auto index = size_t{0}; index < zones_.size(); ++index) {
        const auto& zone = zones_[index];
        auto& zone_runtime = runtime_[index];
        const auto has_mark_action = std::ranges::any_of(zone.actions, [](const auto& action) {
            return action.enabled && action.type == ActionType::MarkTargets;
        });
        if (!zone_runtime.triggered || zone_runtime.aggro_consumed
            || !zone.aggro_origin_enabled || !has_mark_action || !SkillFilterMatches(zone)) {
            continue;
        }
        auto initial = snapshot(zone_runtime.targets);
        if (!initial || !initial->within_earshot) continue;
        zone_runtime.aggro_consumed = true;
        aggro_.active = true;
        aggro_.zone_index = index;
        aggro_.origin = initial->centroid;
        aggro_.current = initial->centroid;
        aggro_.group_radius = initial->radius;
        aggro_.targets = std::move(initial->targets);
        return;
    }
}

uint32_t TargetDetector::AggroColor(const float distance, const float alpha)
{
    const auto fraction = std::clamp(distance / CompassRange, 0.f, 1.f);
    auto red = 1.f;
    auto green = 0.f;
    if (fraction <= .6f) {
        red = fraction / .6f;
        green = red * .498f + .502f;
    }
    else if (fraction <= .8f) {
        green = 1.f - (fraction - .6f) / .2f * .5f;
    }
    else if (fraction <= .9f) {
        green = (1.f - (fraction - .8f) / .1f) * .5f;
    }
    return D3DCOLOR_ARGB(
        static_cast<uint32_t>(std::clamp(alpha, 0.f, 1.f) * 255.f),
        static_cast<uint32_t>(red * 255.f),
        static_cast<uint32_t>(green * 255.f),
        0);
}

size_t TargetDetector::TerrainSignature() const
{
    auto signature = std::hash<uint32_t>{}(static_cast<uint32_t>(GW::Map::GetMapID()));
    const auto combine = [&signature](const auto value) {
        const auto hash = std::hash<std::decay_t<decltype(value)>>{}(value);
        signature ^= hash + 0x9e3779b9u + (signature << 6) + (signature >> 2);
    };
    combine(show_preview_);
    for (auto index = size_t{0}; index < zones_.size(); ++index) {
        const auto& zone = zones_[index];
        combine(zone.map_id);
        combine(zone.enabled);
        combine(static_cast<uint8_t>(zone.zone_type));
        combine(zone.center_x);
        combine(zone.center_y);
        combine(zone.radius);
        combine(zone.aggro_origin_enabled);
        combine(zone.aggro_origin_render_terrain);
        combine(SkillFilterMatches(zone));
        for (const auto& point : zone.polygon) {
            combine(point.x);
            combine(point.y);
            combine(point.zplane);
        }
    }
    combine(aggro_.active);
    if (aggro_.active) {
        combine(aggro_.zone_index);
        combine(aggro_.origin.x);
        combine(aggro_.origin.y);
        combine(aggro_.origin.zplane);
        combine(AggroColor(
            std::sqrt(SquareDistance(
                aggro_.origin.x, aggro_.origin.y, aggro_.current.x, aggro_.current.y)),
            .85f));
    }
    return signature;
}

void TargetDetector::RebuildTerrainPreview()
{
    RenderingUtils::clearDrawingList(this);
    terrain_preview_present_ = false;
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    if (show_preview_) {
        for (const auto& zone : zones_) {
            if (!zone.enabled || zone.map_id != map_id) {
                continue;
            }
            if ((zone.zone_type == ZoneType::Polygon || zone.zone_type == ZoneType::Both)
                && zone.polygon.size() >= 3) {
                std::vector<GW::GamePos> polygon;
                polygon.reserve(zone.polygon.size() + 1);
                for (const auto& point : zone.polygon) {
                    polygon.emplace_back(point.x, point.y, point.zplane);
                }
                polygon.push_back(polygon.front());
                RenderingUtils::addPolylineToDraw(std::move(polygon), ZoneColor, this);
                terrain_preview_present_ = true;
            }
            if ((zone.zone_type == ZoneType::DistanceFrom || zone.zone_type == ZoneType::Both)
                && zone.radius > 0.f) {
                RenderingUtils::addCircleToDraw(
                    {zone.center_x, zone.center_y, 0}, zone.radius, ZoneColor, false, std::nullopt, this);
                terrain_preview_present_ = true;
            }
        }
    }
    if (aggro_.active && aggro_.zone_index < zones_.size()
        && zones_[aggro_.zone_index].aggro_origin_render_terrain) {
        const auto distance = std::sqrt(SquareDistance(
            aggro_.origin.x, aggro_.origin.y, aggro_.current.x, aggro_.current.y));
        RenderingUtils::addCircleToDraw(
            aggro_.origin, CompassRange, AggroColor(distance, .85f), false, std::nullopt, this);
        terrain_preview_present_ = true;
    }
}

void TargetDetector::Draw(IDirect3DDevice9* device)
{
    if (terminating_ || !GW::Map::GetIsMapLoaded()) {
        if (terrain_preview_present_) RenderingUtils::clearDrawingList(this);
        terrain_preview_present_ = false;
        terrain_signature_valid_ = false;
        return;
    }
    const auto signature = TerrainSignature();
    if (!terrain_signature_valid_ || terrain_signature_ != signature) {
        RebuildTerrainPreview();
        terrain_signature_ = signature;
        terrain_signature_valid_ = true;
    }
    if (terrain_preview_present_) RenderingUtils::draw(device);
}

void TargetDetector::RenderMinimap(
    IDirect3DDevice9* device, const MinimapRenderContext& context)
{
    if (terminating_ || !GW::Map::GetIsMapLoaded()) {
        return;
    }
    device->SetTexture(0, nullptr);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    auto restore_view = false;
    auto previous_view = D3DMATRIX{};
    if (!minimap_rotation_enabled_ && !context.view_override) {
        if (const auto player = GW::Agents::GetControlledCharacter()) {
            if (device->GetTransform(D3DTS_VIEW, &previous_view) == D3D_OK) {
                const auto translate_player = DirectX::XMMatrixTranslation(-player->x, -player->y, 0.f);
                const auto scale = DirectX::XMMatrixScaling(context.zoom_scale, context.zoom_scale, 1.f);
                const auto translate_map = DirectX::XMMatrixTranslation(
                    context.translation.x, context.translation.y, 0.f);
                const auto north_up_view = translate_player * scale * translate_map;
                restore_view = device->SetTransform(
                    D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&north_up_view)) == D3D_OK;
            }
        }
    }
    const auto zone_color = ZoneColor;
    const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    if (show_preview_) {
        for (const auto& zone : zones_) {
            if (!zone.enabled || zone.map_id != map_id) continue;
            if ((zone.zone_type == ZoneType::Polygon || zone.zone_type == ZoneType::Both)
                && zone.polygon.size() >= 3) {
                std::vector<Vertex> vertices;
                vertices.reserve(zone.polygon.size() + 1);
                for (const auto& point : zone.polygon) {
                    vertices.push_back({point.x, point.y, 0.f, zone_color});
                }
                vertices.push_back(vertices.front());
                DrawLineStrip(device, vertices);
            }
            if ((zone.zone_type == ZoneType::DistanceFrom || zone.zone_type == ZoneType::Both)
                && zone.radius > 0.f) {
                DrawLineStrip(device, CircleVertices(
                    zone.center_x, zone.center_y, zone.radius, zone_color));
            }
        }
    }
    if (aggro_.active && aggro_.zone_index < zones_.size()) {
        const auto& zone = zones_[aggro_.zone_index];
        const auto distance = std::sqrt(SquareDistance(
            aggro_.origin.x, aggro_.origin.y, aggro_.current.x, aggro_.current.y));
        const auto color = AggroColor(distance, .85f);
        const auto fill_color = AggroColor(distance, .34f);
        auto group = CircleVertices(
            aggro_.origin.x, aggro_.origin.y, aggro_.group_radius, fill_color);
        group.insert(group.begin(), {aggro_.origin.x, aggro_.origin.y, 0.f, fill_color});
        DrawTriangleFan(device, group);
        DrawLineStrip(device, CircleVertices(
            aggro_.origin.x,
            aggro_.origin.y,
            aggro_.group_radius + AggroGroupPadding,
            color));
        DrawLineStrip(device, {
            {aggro_.origin.x, aggro_.origin.y, 0.f, color},
            {aggro_.current.x, aggro_.current.y, 0.f, color},
        });
        if (zone.aggro_origin_render_minimap) {
            DrawLineStrip(device, CircleVertices(
                aggro_.origin.x, aggro_.origin.y, CompassRange, color));
        }
    }
    if (restore_view) device->SetTransform(D3DTS_VIEW, &previous_view);
}

const char* TargetDetector::ActionLabel(const ActionType type)
{
    switch (type) {
        case ActionType::MarkTargets: return "Mark Target(s)";
        case ActionType::PingTarget: return "Ping Target";
        case ActionType::SelectTarget: return "Select Target";
        case ActionType::PartyChat: return "Party Chat";
        case ActionType::LogMessage: return "Log Message";
    }
    return "Unknown";
}

const char* TargetDetector::ObjectiveStatusLabel(const ObjectiveStatus status)
{
    switch (status) {
        case ObjectiveStatus::NotStarted: return "Not Started";
        case ObjectiveStatus::Running: return "Running";
        case ObjectiveStatus::Completed: return "Completed";
    }
    return "Unknown";
}

void TargetDetector::WriteLocal(const std::string& message)
{
    const auto wide = PluginUtils::StringToWString(message);
    GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, wide.c_str());
}

void TargetDetector::DrawSettings()
{
    ImGui::TextUnformatted("Version 0.0.7 (recovered)");
    ImGui::TextWrapped(
        "Automatically triggers configured actions when target agents are detected inside trigger zones.");
    ImGui::Checkbox("Show trigger zones preview", &show_preview_);
    ImGui::Checkbox("Minimap rotates with camera", &minimap_rotation_enabled_);
    if (ImGui::Button("Clear Current Marks")) {
        GW::Chat::SendChat('/', "marktarget clearall");
        if (mark_restore_pending_) GW::Agents::ChangeTarget(mark_restore_target_);
        pending_marks_.clear();
        mark_target_selected_at_.reset();
        mark_restore_target_ = 0;
        mark_attempts_ = 0;
        mark_restore_pending_ = false;
        aggro_ = {};
    }
    ImGui::Separator();
    if (ImGui::Button("Add Trigger Zone")) {
        zones_.push_back({.name = "New Trigger Zone", .map_id = static_cast<uint32_t>(GW::Map::GetMapID())});
        runtime_.push_back({});
        selected_zone_ = static_cast<uint32_t>(zones_.size() - 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        zones_ = DefaultZones();
        selected_zone_ = 0;
        ResetInstance();
    }
    for (auto zone_index = size_t{0}; zone_index < zones_.size();) {
        auto& zone = zones_[zone_index];
        ImGui::PushID(static_cast<int>(zone_index));
        const auto expanded = ImGui::TreeNode(zone.name.empty() ? "Unnamed zone" : zone.name.c_str());
        ImGui::SameLine();
        ImGui::Checkbox("Enabled", &zone.enabled);
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
            if (aggro_.active) {
                if (aggro_.zone_index == zone_index) {
                    aggro_ = {};
                }
                else if (aggro_.zone_index > zone_index) {
                    --aggro_.zone_index;
                }
            }
            zones_.erase(zones_.begin() + static_cast<ptrdiff_t>(zone_index));
            if (zone_index < runtime_.size()) {
                runtime_.erase(runtime_.begin() + static_cast<ptrdiff_t>(zone_index));
            }
            if (expanded) ImGui::TreePop();
            ImGui::PopID();
            continue;
        }
        if (expanded) {
            InputString("Name", zone.name);
            InputString("Description", zone.description);
            ImGui::InputScalar("Map ID", ImGuiDataType_U32, &zone.map_id);
            ImGui::Checkbox("Log when triggered", &zone.log_when_triggered);
            auto zone_type = static_cast<int>(zone.zone_type);
            if (ImGui::Combo("Zone type", &zone_type, "Polygon\0Distance From\0Both\0")) {
                zone.zone_type = static_cast<ZoneType>(zone_type);
            }
            if (zone.zone_type == ZoneType::DistanceFrom || zone.zone_type == ZoneType::Both) {
                ImGui::InputFloat("Center X", &zone.center_x, 0.f, 0.f, "%.1f");
                ImGui::InputFloat("Center Y", &zone.center_y, 0.f, 0.f, "%.1f");
                ImGui::InputFloat("Radius", &zone.radius, 0.f, 0.f, "%.1f");
                if (ImGui::Button("Use Current Position as Center")) {
                    if (const auto player = GW::Agents::GetControlledCharacter()) {
                        zone.center_x = player->x;
                        zone.center_y = player->y;
                    }
                }
            }
            if (zone.zone_type == ZoneType::Polygon || zone.zone_type == ZoneType::Both) {
                ImGui::TextUnformatted("Polygon");
                for (auto point_index = size_t{0}; point_index < zone.polygon.size();) {
                    auto& point = zone.polygon[point_index];
                    ImGui::PushID(static_cast<int>(point_index));
                    ImGui::SetNextItemWidth(220.f);
                    ImGui::InputFloat2("##point", &point.x, "%.1f");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) {
                        zone.polygon.erase(zone.polygon.begin() + static_cast<ptrdiff_t>(point_index));
                        ImGui::PopID();
                        continue;
                    }
                    ImGui::PopID();
                    ++point_index;
                }
                if (ImGui::Button("Add Current Position##point")) {
                    if (const auto player = GW::Agents::GetControlledCharacter()) {
                        zone.polygon.push_back({player->x, player->y, player->plane});
                    }
                }
            }
            auto trigger_condition = static_cast<int>(zone.trigger_condition);
            if (ImGui::Combo(
                    "Trigger condition", &trigger_condition,
                    "Zone fully visible\0Zone fully visible or timeout\0")) {
                zone.trigger_condition = static_cast<TriggerCondition>(trigger_condition);
            }
            if (zone.trigger_condition == TriggerCondition::FullyVisibleOrTimeout) {
                ImGui::SliderFloat("Timeout", &zone.trigger_timeout, 1.f, 10.f, "%.0f s");
            }

            ImGui::TextUnformatted("Objective conditions");
            for (auto objective_index = size_t{0}; objective_index < zone.objective_conditions.size();) {
                auto& condition = zone.objective_conditions[objective_index];
                ImGui::PushID(static_cast<int>(objective_index));
                ImGui::SetNextItemWidth(90.f);
                ImGui::InputScalar("##objective", ImGuiDataType_U32, &condition.objective_id);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(110.f);
                if (ImGui::BeginCombo("##status", ObjectiveStatusLabel(condition.status))) {
                    for (auto value : {ObjectiveStatus::NotStarted, ObjectiveStatus::Running, ObjectiveStatus::Completed}) {
                        if (ImGui::Selectable(ObjectiveStatusLabel(value), condition.status == value)) {
                            condition.status = value;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180.f);
                InputString("##description", condition.description);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    zone.objective_conditions.erase(
                        zone.objective_conditions.begin() + static_cast<ptrdiff_t>(objective_index));
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++objective_index;
            }
            if (ImGui::SmallButton("Add Objective Condition")) {
                zone.objective_conditions.push_back({});
            }

            ImGui::TextUnformatted("Target model IDs");
            for (auto model_index = size_t{0}; model_index < zone.model_ids.size();) {
                ImGui::PushID(static_cast<int>(model_index));
                ImGui::SetNextItemWidth(120.f);
                ImGui::InputScalar("##model", ImGuiDataType_U32, &zone.model_ids[model_index]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    zone.model_ids.erase(zone.model_ids.begin() + static_cast<ptrdiff_t>(model_index));
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++model_index;
            }
            ImGui::InputScalar("New model ID", ImGuiDataType_U32, &new_model_id_);
            ImGui::SameLine();
            if (ImGui::Button("Add##model") && new_model_id_) {
                zone.model_ids.push_back(new_model_id_);
                new_model_id_ = 0;
            }

            ImGui::TextUnformatted("Actions");
            for (auto action_index = size_t{0}; action_index < zone.actions.size();) {
                auto& action = zone.actions[action_index];
                ImGui::PushID(static_cast<int>(action_index));
                ImGui::Checkbox("##enabled", &action.enabled);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(130.f);
                if (ImGui::BeginCombo("##type", ActionLabel(action.type))) {
                    for (auto value : {
                             ActionType::MarkTargets, ActionType::PingTarget, ActionType::SelectTarget,
                             ActionType::PartyChat, ActionType::LogMessage}) {
                        if (ImGui::Selectable(ActionLabel(value), action.type == value)) action.type = value;
                    }
                    ImGui::EndCombo();
                }
                if (action.type == ActionType::PartyChat || action.type == ActionType::LogMessage) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(220.f);
                    InputString("##message", action.message);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    zone.actions.erase(zone.actions.begin() + static_cast<ptrdiff_t>(action_index));
                    ImGui::PopID();
                    continue;
                }
                ImGui::PopID();
                ++action_index;
            }
            if (zone.actions.size() < 5 && ImGui::SmallButton("Add Action")) {
                for (auto value : {
                         ActionType::MarkTargets, ActionType::PingTarget, ActionType::SelectTarget,
                         ActionType::PartyChat, ActionType::LogMessage}) {
                    if (!std::ranges::any_of(zone.actions, [value](const auto& action) {
                            return action.type == value;
                        })) {
                        zone.actions.push_back({value, true, {}});
                        break;
                    }
                }
            }

            ImGui::Checkbox("Aggro origin tracker", &zone.aggro_origin_enabled);
            if (zone.aggro_origin_enabled) {
                ImGui::Checkbox("Render aggro range on minimap", &zone.aggro_origin_render_minimap);
                ImGui::Checkbox("Render aggro range on terrain", &zone.aggro_origin_render_terrain);
                ImGui::TextUnformatted("Skill filter (optional)");
                for (auto skill_index = size_t{0}; skill_index < zone.aggro_origin_skill_ids.size();) {
                    ImGui::PushID(static_cast<int>(skill_index));
                    ImGui::SetNextItemWidth(120.f);
                    ImGui::InputScalar("##skill", ImGuiDataType_U32, &zone.aggro_origin_skill_ids[skill_index]);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) {
                        zone.aggro_origin_skill_ids.erase(
                            zone.aggro_origin_skill_ids.begin() + static_cast<ptrdiff_t>(skill_index));
                        ImGui::PopID();
                        continue;
                    }
                    ImGui::PopID();
                    ++skill_index;
                }
                ImGui::InputScalar("New skill ID", ImGuiDataType_U32, &new_skill_id_);
                ImGui::SameLine();
                if (ImGui::Button("Add##skill") && new_skill_id_) {
                    zone.aggro_origin_skill_ids.push_back(new_skill_id_);
                    new_skill_id_ = 0;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        ++zone_index;
    }
}

void TargetDetector::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    LoadSetting("show_preview_", show_preview_);
    LoadSetting("minimap_rotation_enabled_", minimap_rotation_enabled_);
    LoadSetting("zones", zones_);
    if (zones_.empty()) zones_ = DefaultZones();
    ResetInstance();
}

void TargetDetector::SaveSettings(const wchar_t* folder)
{
    SaveSetting("show_preview_", show_preview_);
    SaveSetting("minimap_rotation_enabled_", minimap_rotation_enabled_);
    SaveSetting("zones", zones_);
    ToolboxPlugin::SaveSettings(folder);
}
