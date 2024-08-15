#include "stdafx.h"

#include <GWCA/Constants/Maps.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <ImGuiAddons.h>
#include <Constants/EncStrings.h>


namespace {
    ImRect show_all_rect;
    ImRect hard_mode_rect;
    ImRect place_marker_rect;
    ImRect remove_marker_rect;
    ImRect show_lines_on_world_map_rect;

    bool showing_all_outposts = false;

    bool drawn = false;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t, uint32_t, uint32_t, uint32_t)
    {
        return 0xffffffff;
    }

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;

    GW::Constants::QuestID custom_quest_id = (GW::Constants::QuestID)0x0000fdd;
    GW::Quest custom_quest_marker;
    GW::Vec2f custom_quest_marker_world_pos;
    GW::Constants::QuestID player_chosen_quest_id = GW::Constants::QuestID::None;
    bool setting_custom_quest_marker = false;

    const wchar_t* custom_quest_marker_enc_name = L"\x452"; // "Map Travel"

    bool show_lines_on_world_map = true;

    bool WorldMapToGamePos(GW::Vec2f& world_map_pos, GW::GamePos* game_map_pos);

    bool MapContainsWorldPos(GW::Constants::MapID map_id, const GW::Vec2f& world_map_pos, GW::Constants::Campaign campaign)
    {
        const auto map = GW::Map::GetMapInfo(map_id);
        if (!(map && map->campaign == campaign && map->GetIsOnWorldMap()))
            return false;
        ImRect map_bounds;
        return GW::Map::GetMapWorldMapBounds(map, &map_bounds) && map_bounds.Contains(world_map_pos);
    }

    GW::Constants::MapID GetMapIdForLocation(const GW::Vec2f& world_map_pos)
    {
        auto map_id = GW::Map::GetMapID();
        auto campaign = GW::Map::GetMapInfo()->campaign;
        if (MapContainsWorldPos(map_id, world_map_pos, campaign))
            return map_id;
        for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
            map_id = static_cast<GW::Constants::MapID>(i);
            if (MapContainsWorldPos(map_id, world_map_pos, campaign))
                return map_id;
        }
        return GW::Constants::MapID::None;
    }

    void SetCustomQuestMarker(const GW::Vec2f world_pos)
    {
        custom_quest_marker_world_pos = world_pos;
        if (GW::QuestMgr::GetQuest(custom_quest_id)) {
            struct QuestRemovePacket : GW::Packet::StoC::PacketBase {
                GW::Constants::QuestID quest_id = custom_quest_id;
            };
            QuestRemovePacket quest_remove_packet;
            quest_remove_packet.header = GAME_SMSG_QUEST_REMOVE;
            GW::StoC::EmulatePacket(&quest_remove_packet);
            memset(&custom_quest_marker, 0, sizeof(custom_quest_marker));
        }
        if (custom_quest_marker_world_pos.x == 0 && custom_quest_marker_world_pos.y == 0)
            return;

        setting_custom_quest_marker = true;

        const auto map_id = GetMapIdForLocation(custom_quest_marker_world_pos);

        struct QuestInfoPacket : GW::Packet::StoC::PacketBase {
            GW::Constants::QuestID quest_id = custom_quest_id;
            uint32_t log_state = 0x20;
            wchar_t location[8] = {0};
            wchar_t name[8] = {0};
            wchar_t npc[8] = {0};
            GW::Constants::MapID map_from;
        };

        QuestInfoPacket quest_add_packet;
        quest_add_packet.header = GAME_SMSG_QUEST_GENERAL_INFO;
        quest_add_packet.quest_id = custom_quest_id;
        wcscpy(quest_add_packet.location, L"\x564"); // Primary Quests
        wcscpy(quest_add_packet.name, custom_quest_marker_enc_name);
        quest_add_packet.map_from = GW::Constants::MapID::Count;
        GW::StoC::EmulatePacket(&quest_add_packet);

        struct QuestDescriptionPacket : GW::Packet::StoC::PacketBase {
            GW::Constants::QuestID quest_id = custom_quest_id;
            wchar_t description[128] = {0};
            wchar_t objective[128] = {0};
        };
        QuestDescriptionPacket quest_description_packet;
        quest_description_packet.header = GAME_SMSG_QUEST_DESCRIPTION;

        swprintf(quest_description_packet.description, _countof(quest_description_packet.description),
                 L"\x108\x107You've placed a custom marker on the map.\x1");

        GW::StoC::EmulatePacket(&quest_description_packet);

        struct QuestMarkerPacket : GW::Packet::StoC::PacketBase {
            GW::Constants::QuestID quest_id = custom_quest_id;
            GW::GamePos marker;
            GW::Constants::MapID map_id;
        };
        QuestMarkerPacket quest_marker_packet;
        quest_marker_packet.header = GAME_SMSG_QUEST_UPDATE_MARKER;
        quest_marker_packet.marker = {INFINITY, INFINITY};
        quest_marker_packet.map_id = map_id;
        if (map_id == GW::Map::GetMapID())
            WorldMapToGamePos(custom_quest_marker_world_pos, &quest_marker_packet.marker);
        GW::StoC::EmulatePacket(&quest_marker_packet);

        setting_custom_quest_marker = false;

        const auto quest = GW::QuestMgr::GetQuest(custom_quest_id);
        ASSERT(quest);
        custom_quest_marker = *quest;
    }

    bool WorldMapContextMenu(void*)
    {
        if (!GW::Map::GetWorldMapContext())
            return false;
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapToGamePos(world_map_click_pos, &game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        const auto map_id = GetMapIdForLocation(world_map_click_pos);
        ImGui::TextUnformatted(Resources::GetMapName(map_id)->string().c_str());

        if (ImGui::Button("Place Marker")) {
            GW::GameThread::Enqueue([]() {
                SetCustomQuestMarker(world_map_click_pos);
                GW::QuestMgr::SetActiveQuestId(custom_quest_id);
            });
            return false;
        }
        place_marker_rect = c->LastItemData.Rect;
        place_marker_rect.Translate(viewport_offset);
        memset(&remove_marker_rect, 0, sizeof(remove_marker_rect));
        if (GW::QuestMgr::GetQuest(custom_quest_id)) {
            if (ImGui::Button("Remove Marker")) {
                GW::GameThread::Enqueue([]() {
                    SetCustomQuestMarker({0, 0});
                });
                return false;
            }
            remove_marker_rect = c->LastItemData.Rect;
            remove_marker_rect.Translate(viewport_offset);
        }
        return true;
    }

    GW::HookEntry OnUIMessage_HookEntry;

    // Fake an action of the user selecting an active quest, without making any server request.
    void EmulateQuestSelected(GW::Quest* quest)
    {
        if (!quest)
            return;
        GW::UI::UIPacket::kServerActiveQuestChanged packet = {
            .quest_id = quest->quest_id,
            .marker = quest->marker,
            .h0024 = quest->h0024,
            .map_id = quest->map_to,
            .log_state = quest->log_state
        };
        GW::UI::SendUIMessage(GW::UI::UIMessage::kClientActiveQuestChanged, &packet);
        GW::GetWorldContext()->active_quest_id = quest->quest_id;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kServerActiveQuestChanged, &packet);
    }

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (status->blocked)
            return;

        switch (message_id) {
            case GW::UI::UIMessage::kQuestAdded: {
                const auto quest_id = *(GW::Constants::QuestID*)wparam;
                if (quest_id == custom_quest_id) {
                    GW::QuestMgr::GetQuest(quest_id)->log_state |= 1; // Avoid asking for description about this quest
                }
            }
            break;
            case GW::UI::UIMessage::kSendSetActiveQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (setting_custom_quest_marker) {
                    // This triggers if the player has no quests, or the map has just loaded; we want to "undo" this by spoofing the previous quest selection if there is one
                    status->blocked = true;
                    EmulateQuestSelected(GW::QuestMgr::GetActiveQuest());
                    return;
                }
                player_chosen_quest_id = quest_id;
                if (quest_id == custom_quest_id) {
                    // If the player has chosen the custom quest, spoof the response without asking the server
                    status->blocked = true;
                    EmulateQuestSelected(GW::QuestMgr::GetQuest(quest_id));
                }
            }
            break;
            case GW::UI::UIMessage::kSendAbandonQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (quest_id == custom_quest_id) {
                    status->blocked = true;
                    SetCustomQuestMarker({0, 0});
                }
            }
            break;
            case GW::UI::UIMessage::kMapLoaded:
                if (custom_quest_marker.quest_id != (GW::Constants::QuestID)0) {
                    SetCustomQuestMarker(custom_quest_marker_world_pos);
                    if (player_chosen_quest_id == custom_quest_marker.quest_id) {
                        GW::QuestMgr::SetActiveQuestId(custom_quest_marker.quest_id);
                    }
                }
                break;
            case GW::UI::UIMessage::kOnScreenMessage: {
                // Block the on-screen message when the custom marker is placed
                if (setting_custom_quest_marker) {
                    status->blocked = true;
                }
            }
            break;
        }
    }

    void TriggerWorldMapRedraw()
    {
        GW::GameThread::Enqueue([] {
            // Trigger a benign ui message e.g. guild context update; world map subscribes to this, and automatically updates the view.
            // GW::UI::SendUIMessage((GW::UI::UIMessage)0x100000ca); // disables guild/ally chat until reloading char/map
            const auto world_map_context = GW::Map::GetWorldMapContext();
            const auto frame = GW::UI::GetFrameById(world_map_context->frame_id);
            GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kMapLoaded, nullptr);
            //GW::UI::SendFrameUIMessage(frame,(GW::UI::UIMessage)0x1000008e, nullptr);
        });
    }

    bool GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f* world_map_pos)
    {
        ImRect map_bounds;
        if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
            return false;
        const auto current_map_context = GW::GetMapContext();
        if (!current_map_context)
            return false;

        const auto game_map_rect = ImRect({
            current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
            current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

        // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
        const auto gwinches_per_unit = 96.f;
        GW::Vec2f map_mid_world_point = {
            map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
            map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
        };

        world_map_pos->x = (game_map_pos.x / gwinches_per_unit) + map_mid_world_point.x;
        world_map_pos->y = ((game_map_pos.y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
        return true;
    }

    bool WorldMapToGamePos(GW::Vec2f& world_map_pos, GW::GamePos* game_map_pos)
    {
        ImRect map_bounds;
        if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
            return false;
        if (!map_bounds.Contains({world_map_pos.x, world_map_pos.y}))
            return false; // Current map doesn't contain these coords; we can't plot a position

        const auto current_map_context = GW::GetMapContext();
        if (!current_map_context)
            return false;

        const auto game_map_rect = ImRect({
            current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
            current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

        // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
        constexpr auto gwinches_per_unit = 96.f;
        GW::Vec2f map_mid_world_point = {
            map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
            map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
        };

        game_map_pos->x = (world_map_pos.x - map_mid_world_point.x) * gwinches_per_unit;
        game_map_pos->y = ((world_map_pos.y - map_mid_world_point.y) * gwinches_per_unit) * -1.f; // Inverted Y Axis
        return true;
    }
}

void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();

    memset(&custom_quest_marker, 0, sizeof(custom_quest_marker));

    uintptr_t address = GW::Scanner::Find("\x8b\x45\xfc\xf7\x40\x10\x00\x00\x01\x00", "xxxxxxxxxx", 0xa);
    if (address) {
        view_all_outposts_patch.SetPatch(address, "\xeb", 1);
    }
    address = GW::Scanner::Find("\x8b\xd8\x83\xc4\x10\x8b\xcb\x8b\xf3\xd1\xe9", "xxxxxxxxxxx", -0x5);
    if (address) {
        view_all_carto_areas_patch.SetRedirect(address, GetCartographyFlagsForArea);
    }

    ASSERT(view_all_outposts_patch.IsValid());
    ASSERT(view_all_carto_areas_patch.IsValid());

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kOnScreenMessage,
        GW::UI::UIMessage::kSendAbandonQuest
    };
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage);
    }
}

void WorldMapWidget::SignalTerminate()
{
    ToolboxWidget::Terminate();
    GW::GameThread::Enqueue([]() {
        SetCustomQuestMarker({0, 0});
    });
    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void WorldMapWidget::ShowAllOutposts(const bool show = showing_all_outposts)
{
    if (view_all_outposts_patch.IsValid())
        view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid())
        view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(showing_all_outposts);
    LOAD_BOOL(show_lines_on_world_map);
    float custom_quest_marker_world_pos_x = .0f;
    float custom_quest_marker_world_pos_y = .0f;
    LOAD_FLOAT(custom_quest_marker_world_pos_x);
    LOAD_FLOAT(custom_quest_marker_world_pos_y);
    custom_quest_marker_world_pos = {custom_quest_marker_world_pos_x, custom_quest_marker_world_pos_y};
    GW::GameThread::Enqueue([]() {
        SetCustomQuestMarker(custom_quest_marker_world_pos);
    });
}

void WorldMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(showing_all_outposts);
    SAVE_BOOL(show_lines_on_world_map);
    float custom_quest_marker_world_pos_x = custom_quest_marker_world_pos.x;
    float custom_quest_marker_world_pos_y = custom_quest_marker_world_pos.y;
    SAVE_FLOAT(custom_quest_marker_world_pos_x);
    SAVE_FLOAT(custom_quest_marker_world_pos_y);
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (!GW::UI::GetIsWorldMapShowing()) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Checkbox("Show all areas", &showing_all_outposts);
        show_all_rect = c->LastItemData.Rect;
        show_all_rect.Translate(viewport_offset);
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            ImGui::Checkbox("Hard mode", &is_hard_mode);
            hard_mode_rect = c->LastItemData.Rect;
            hard_mode_rect.Translate(viewport_offset);
        }
        else {
            memset(&hard_mode_rect, 0, sizeof(hard_mode_rect));
        }

        ImGui::Checkbox("Show toolbox minimap lines", &show_lines_on_world_map);
        show_lines_on_world_map_rect = c->LastItemData.Rect;
        show_lines_on_world_map_rect.Translate(viewport_offset);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    const auto world_map_context = GW::Map::GetWorldMapContext();
    if (!(world_map_context && world_map_context->zoom == 1.0f))
        return;
    const auto viewport = ImGui::GetMainViewport();
    const auto& viewport_offset = viewport->Pos;

    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);

    // @Fixme: we need to do something with this to scale the point on the world map!!
    [[maybe_unused]] const auto ui_scale = GW::UI::GetFrameById(world_map_context->frame_id)->position.GetViewportScale(GW::UI::GetRootFrame());
    [[maybe_unused]] const auto ui_scale2 = GuiUtils::GetGWScaleMultiplier();

    // Draw custom quest marker on world map
    if (custom_quest_marker_world_pos.x || custom_quest_marker_world_pos.y) {
        static constexpr auto uv0 = ImVec2(0.0f, 0.0f);
        static constexpr auto ICON_SIZE = ImVec2(24.0f, 24.0f);

        const ImVec2 viewport_quest_pos = {
            custom_quest_marker_world_pos.x - world_map_context->top_left.x + viewport_offset.x - (ICON_SIZE.x / 2.f),
            custom_quest_marker_world_pos.y - world_map_context->top_left.y + viewport_offset.y - (ICON_SIZE.y / 2.f)
        };

        const ImRect quest_marker_image_rect = {
            viewport_quest_pos, {viewport_quest_pos.x + ICON_SIZE.x, viewport_quest_pos.y + ICON_SIZE.y}
        };

        const auto texture = GwDatTextureModule::LoadTextureFromFileId(0x1b4d5);

        const auto uv1 = ImGui::CalculateUvCrop(*texture, ICON_SIZE);
        draw_list->AddImage(*texture, quest_marker_image_rect.Min, quest_marker_image_rect.Max, uv0, uv1);
        if (quest_marker_image_rect.Contains(ImGui::GetMousePos())) {
            ImGui::SetTooltip("Custom marker placed @ %.2f, %.2f", custom_quest_marker_world_pos.x, custom_quest_marker_world_pos.y);
        }
    }
    if (show_lines_on_world_map) {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        const auto map_id = GW::Map::GetMapID();
        GW::Vec2f line_start;
        GW::Vec2f line_end;
        for (auto& line : lines | std::views::filter([](auto line) { return line->visible; })) {
            if (line->map != map_id)
                continue;
            if (!GamePosToWorldMap(line->p1, &line_start))
                continue;
            if (!GamePosToWorldMap(line->p2, &line_end))
                continue;

            line_start.x = (line_start.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_start.y = (line_start.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;
            line_end.x = (line_end.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_end.y = (line_end.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;

            draw_list->AddLine(line_start, line_end, line->color);
        }
    }
    drawn = true;
}

bool WorldMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    switch (Message) {
        case WM_RBUTTONDOWN: {
            world_map_clicking = true;
        }
        break;
        case WM_MOUSEMOVE: {
            world_map_clicking = false;
        }
        break;
        case WM_RBUTTONUP: {
            if (!world_map_clicking)
                break;
            world_map_clicking = false;
            const auto world_map_context = GW::Map::GetWorldMapContext();
            if (!(world_map_context && world_map_context->zoom == 1.0f))
                break;

            world_map_click_pos = {GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
            world_map_click_pos.x += world_map_context->top_left.x;
            world_map_click_pos.y += world_map_context->top_left.y;
            ImGui::SetContextMenu(WorldMapContextMenu);
        }
        break;
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing()) {
                return false;
            }
            auto check_rect = [lParam](const ImRect& rect) {
                ImVec2 p = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
                return rect.Contains(p);
            };
            if (check_rect(remove_marker_rect)) {
                return true;
            }
            if (check_rect(place_marker_rect)) {
                return true;
            }
            if (check_rect(show_lines_on_world_map_rect)) {
                show_lines_on_world_map = !show_lines_on_world_map;
                return true;
            }
            if (check_rect(hard_mode_rect)) {
                GW::GameThread::Enqueue([] {
                    GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                });
                return true;
            }
            if (check_rect(show_all_rect)) {
                showing_all_outposts = !showing_all_outposts;
                GW::GameThread::Enqueue([] {
                    ShowAllOutposts(showing_all_outposts);
                });
                return true;
            }
            break;
    }
    return false;
}

bool WorldMapWidget::CanTerminate()
{
    return !GW::QuestMgr::GetQuest(custom_quest_id);
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
