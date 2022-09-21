#include "Minimap.h"
#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/AgentRenderer.h>

#define AGENTCOLOR_INIFILENAME L"AgentColors.ini"

namespace {
    unsigned int GetAgentProfession(const GW::AgentLiving* agent) {
        if (!agent) return 0;
        if (agent->primary) return agent->primary;
        const GW::NPC* npc = GW::Agents::GetNPCByID(agent->player_number);
        if (!npc) return 0;
        return npc->primary;
    }

    uint32_t auto_target_id = 0;
}

AgentRenderer* AgentRenderer::instance = 0;

unsigned int AgentRenderer::CustomAgent::cur_ui_id = 0;

void AgentRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
    LoadDefaultColors();
    color_agent_modifier = Colors::Load(ini, section, VAR_NAME(color_agent_modifier), color_agent_modifier);
    color_agent_damaged_modifier = Colors::Load(ini, section, VAR_NAME(color_agent_damaged_modifier), color_agent_damaged_modifier);
    color_eoe = Colors::Load(ini, section, VAR_NAME(color_eoe), color_eoe);
    color_qz = Colors::Load(ini, section, VAR_NAME(color_qz), color_qz);
    color_winnowing = Colors::Load(ini, section, VAR_NAME(color_winnowing), color_winnowing);
    color_target = Colors::Load(ini, section, VAR_NAME(color_target), color_target);
    color_player = Colors::Load(ini, section, VAR_NAME(color_player), color_player);
    color_player_dead = Colors::Load(ini, section, VAR_NAME(color_player_dead), color_player_dead);
    color_signpost = Colors::Load(ini, section, VAR_NAME(color_signpost), color_signpost);
    color_item = Colors::Load(ini, section, VAR_NAME(color_item), color_item);
    color_hostile = Colors::Load(ini, section, VAR_NAME(color_hostile), color_hostile);
    color_hostile_dead = Colors::Load(ini, section, VAR_NAME(color_hostile_dead), color_hostile_dead);
    color_neutral = Colors::Load(ini, section, VAR_NAME(color_neutral), color_neutral);
    color_ally = Colors::Load(ini, section, VAR_NAME(color_ally), color_ally);
    color_ally_npc = Colors::Load(ini, section, VAR_NAME(color_ally_npc), color_ally_npc);
    color_ally_spirit = Colors::Load(ini, section, VAR_NAME(color_ally_spirit), color_ally_spirit);
    color_ally_minion = Colors::Load(ini, section, VAR_NAME(color_ally_minion), color_ally_minion);
    color_ally_dead = Colors::Load(ini, section, VAR_NAME(color_ally_dead), color_ally_dead);
    boss_colors = ini->GetBoolValue(section, VAR_NAME(boss_colors), boss_colors);

    LoadDefaultSizes();
    size_default = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_default), size_default));
    size_player = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_player), size_player));
    size_signpost = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_signpost), size_signpost));
    size_item = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_item), size_item));
    size_boss = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_boss), size_boss));
    size_minion = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size_minion), size_minion));
    default_shape = static_cast<Shape_e>(ini->GetLongValue(section, VAR_NAME(default_shape), default_shape));
    agent_border_thickness =
        static_cast<uint32_t>(ini->GetLongValue(section, VAR_NAME(agent_border_thickness), agent_border_thickness));


    show_hidden_npcs = ini->GetBoolValue(section, VAR_NAME(show_hidden_npcs), show_hidden_npcs);

    LoadCustomAgents();

    Invalidate();
}

void AgentRenderer::LoadCustomAgents() {
    if (agentcolorinifile == nullptr) agentcolorinifile = new CSimpleIni();
    agentcolorinifile->LoadFile(Resources::GetPath(AGENTCOLOR_INIFILENAME).c_str());

    custom_agents.clear();
    custom_agents_map.clear();

    CSimpleIni::TNamesDepend entries;
    agentcolorinifile->GetAllSections(entries);

    for (const CSimpleIni::Entry& entry : entries) {
        // we know that all sections are agent colors, don't even check the section names
        auto* customAgent = new CustomAgent(agentcolorinifile, entry.pItem);
        customAgent->index = custom_agents.size();
        custom_agents.push_back(customAgent);
    }
    BuildCustomAgentsMap();
    agentcolors_changed = false;
}

void AgentRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
    Colors::Save(ini, section, VAR_NAME(color_agent_modifier), color_agent_modifier);
    Colors::Save(ini, section, VAR_NAME(color_agent_damaged_modifier), color_agent_damaged_modifier);
    Colors::Save(ini, section, VAR_NAME(color_eoe), color_eoe);
    Colors::Save(ini, section, VAR_NAME(color_qz), color_qz);
    Colors::Save(ini, section, VAR_NAME(color_winnowing), color_winnowing);
    Colors::Save(ini, section, VAR_NAME(color_target), color_target);
    Colors::Save(ini, section, VAR_NAME(color_player), color_player);
    Colors::Save(ini, section, VAR_NAME(color_player_dead), color_player_dead);
    Colors::Save(ini, section, VAR_NAME(color_signpost), color_signpost);
    Colors::Save(ini, section, VAR_NAME(color_item), color_item);
    Colors::Save(ini, section, VAR_NAME(color_hostile), color_hostile);
    Colors::Save(ini, section, VAR_NAME(color_hostile_dead), color_hostile_dead);
    Colors::Save(ini, section, VAR_NAME(color_neutral), color_neutral);
    Colors::Save(ini, section, VAR_NAME(color_ally), color_ally);
    Colors::Save(ini, section, VAR_NAME(color_ally_npc), color_ally_npc);
    Colors::Save(ini, section, VAR_NAME(color_ally_spirit), color_ally_spirit);
    Colors::Save(ini, section, VAR_NAME(color_ally_minion), color_ally_minion);
    Colors::Save(ini, section, VAR_NAME(color_ally_dead), color_ally_dead);

    ini->SetDoubleValue(section, VAR_NAME(size_default), size_default);
    ini->SetDoubleValue(section, VAR_NAME(size_player), size_player);
    ini->SetDoubleValue(section, VAR_NAME(size_signpost), size_signpost);
    ini->SetDoubleValue(section, VAR_NAME(size_item), size_item);
    ini->SetDoubleValue(section, VAR_NAME(size_boss), size_boss);
    ini->SetDoubleValue(section, VAR_NAME(size_minion), size_minion);
    ini->SetLongValue(section, VAR_NAME(default_shape), static_cast<long>(default_shape));
    ini->SetLongValue(section, VAR_NAME(agent_border_thickness), agent_border_thickness);

    ini->SetBoolValue(section, VAR_NAME(show_hidden_npcs), show_hidden_npcs);
    ini->SetBoolValue(section, VAR_NAME(boss_colors), boss_colors);

    SaveCustomAgents();
}

void AgentRenderer::SaveCustomAgents() const {
    if (agentcolors_changed && agentcolorinifile != nullptr) {
        // clear colors from ini
        agentcolorinifile->Reset();

        // then save again
        for (unsigned int i = 0; i < custom_agents.size(); ++i) {
            char buf[256];
            snprintf(buf, 256, "customagent%03d", i);
            custom_agents[i]->SaveSettings(agentcolorinifile, buf);
        }
        agentcolorinifile->SaveFile(Resources::GetPath(AGENTCOLOR_INIFILENAME).c_str());
    }
}
void AgentRenderer::LoadDefaultSizes() {
    size_default = 75.0f;
    size_player = 100.0f;
    size_signpost = 50.0f;
    size_item = 25.0f;
    size_boss = 125.0f;
    size_minion = 50.0f;
}
void AgentRenderer::LoadDefaultColors() {
    color_agent_modifier = 0x001E1E1E;
    color_agent_damaged_modifier = 0x00505050;
    color_eoe = 0x3200FF00;
    color_qz = 0x320000FF;
    color_winnowing = 0x3200FFFF;
    color_target = 0xFFFFFF00;
    color_player = 0xFFFF8000;
    color_player_dead = 0x64FF8000;
    color_signpost = 0xFF0000C8;
    color_item = 0xFF0000F0;
    color_hostile = 0xFFF00000;
    color_hostile_dead = 0xFF320000;
    color_neutral = 0xFF0000DC;
    color_ally = 0xFF00B300;
    color_ally_npc = 0xFF99FF99;
    color_ally_spirit = 0xFF608000;
    color_ally_minion = 0xFF008060;
    color_ally_dead = 0x64006400;
    size_default = 75.0f;
    size_player = 100.0f;
    size_signpost = 50.0f;
    size_item = 25.0f;
    size_boss = 125.0f;
    size_minion = 50.0f;
    boss_colors = true;
    agent_border_thickness = 0;
}
void AgentRenderer::DrawSettings() {

    if (ImGui::TreeNodeEx("Agent Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        bool confirmed = false;
        if (ImGui::SmallConfirmButton("Restore Defaults", &confirmed, "Are you sure?\nThis will reset all agent sizes to the default values.\nThis operation cannot be undone.\n\n")) {
            LoadDefaultColors();
        }
        Colors::DrawSettingHueWheel("EoE", &color_eoe);
        ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
        Colors::DrawSettingHueWheel("QZ", &color_qz);
        ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
        Colors::DrawSettingHueWheel("Winnowing", &color_winnowing, 0);
        ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
        Colors::DrawSettingHueWheel("Target", &color_target);
        Colors::DrawSettingHueWheel("Player (alive)", &color_player);
        Colors::DrawSettingHueWheel("Player (dead)", &color_player_dead);
        Colors::DrawSettingHueWheel("Signpost", &color_signpost);
        Colors::DrawSettingHueWheel("Item", &color_item);
        Colors::DrawSettingHueWheel("Hostile (>90%%)", &color_hostile);
        Colors::DrawSettingHueWheel("Hostile (dead)", &color_hostile_dead);
        Colors::DrawSettingHueWheel("Neutral", &color_neutral);
        Colors::DrawSettingHueWheel("Ally (player)", &color_ally);
        Colors::DrawSettingHueWheel("Ally (NPC)", &color_ally_npc);
        Colors::DrawSettingHueWheel("Ally (spirit)", &color_ally_spirit);
        Colors::DrawSettingHueWheel("Ally (minion)", &color_ally_minion);
        Colors::DrawSettingHueWheel("Ally (dead)", &color_ally_dead);
        Colors::DrawSettingHueWheel("Agent modifier", &color_agent_modifier);
        ImGui::ShowHelp("Each agent has this value removed on the border and added at the center\nZero makes agents have solid color, while a high number makes them appear more shaded.");
        Colors::DrawSettingHueWheel("Agent damaged modifier", &color_agent_damaged_modifier);
        ImGui::ShowHelp("Each hostile agent has this value subtracted from it when under 90% HP.");

        ImGui::TreePop();
    }


    if (ImGui::TreeNodeEx("Agent Sizes", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        bool confirmed = false;
        if (ImGui::SmallConfirmButton("Restore Defaults", &confirmed, "Are you sure?\nThis will reset all agent sizes to the default values.\nThis operation cannot be undone.\n\n")) {
            LoadDefaultSizes();
        }
        ImGui::DragFloat("Default Size", &size_default, 1.0f, 1.0f, 0.0f, "%.0f");
        ImGui::DragFloat("Player Size", &size_player, 1.0f, 1.0f, 0.0f, "%.0f");
        ImGui::DragFloat("Signpost Size", &size_signpost, 1.0f, 1.0f, 0.0f, "%.0f");
        ImGui::DragFloat("Item Size", &size_item, 1.0f, 1.0f, 0.0f, "%.0f");
        ImGui::DragFloat("Boss Size", &size_boss, 1.0f, 1.0f, 0.0f, "%.0f");
        ImGui::DragFloat("Minion Size", &size_minion, 1.0f, 1.0f, 0.0f, "%.0f");
        static const char* items[] = {"Tear", "Circle", "Square", "Big Circle"};
        ImGui::Combo("Shape", reinterpret_cast<int*>(&default_shape), items, 4);
        ImGui::ShowHelp("The default shape of agents.");

        ImGui::TreePop();
    }


    if (ImGui::TreeNodeEx("Custom Agents", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        bool changed = false;
        for (unsigned i = 0; i < custom_agents.size(); ++i) {

            CustomAgent* custom = custom_agents[i];
            if (!custom) continue;

            ImGui::PushID(static_cast<int>(custom->ui_id));

            CustomAgent::Operation op = CustomAgent::Operation::None;
            if (custom->DrawSettings(op)) changed = true;

            ImGui::PopID();

            switch (op) {
            case CustomAgent::Operation::None:
                break;
            case CustomAgent::Operation::MoveUp:
                if (i > 0) std::swap(custom_agents[i], custom_agents[i - 1]);
                break;
            case CustomAgent::Operation::MoveDown:
                if (i < custom_agents.size() - 1) {
                    std::swap(custom_agents[i], custom_agents[i + 1]);
                    // render the moved one and increase i
                    ++i;
                    ImGui::PushID(static_cast<int>(custom_agents[i]->ui_id));
                    CustomAgent::Operation op2 = CustomAgent::Operation::None;
                    custom_agents[i]->DrawSettings(op2);
                    ImGui::PopID();
                }
                break;
            case CustomAgent::Operation::Delete:
                custom_agents.erase(custom_agents.begin() + static_cast<int>(i));
                delete custom;
                --i;
                break;
            case CustomAgent::Operation::ModelIdChange: {
                changed = true;
                break;
            }
            default:
                break;
            }

            switch (op) {
            case AgentRenderer::CustomAgent::Operation::MoveUp:
            case AgentRenderer::CustomAgent::Operation::MoveDown:
            case AgentRenderer::CustomAgent::Operation::Delete:
                for (size_t j = 0; j < custom_agents.size(); ++j) {
                    custom_agents[j]->index = j;
                }
                changed = true;
            default: break;
            }
        }
        if (changed) {
            agentcolors_changed = true;
            BuildCustomAgentsMap();
        }
        if (ImGui::Button("Add Agent Custom Color")) {
            custom_agents.push_back(new CustomAgent(0, color_hostile, "<name>"));
            custom_agents.back()->index = custom_agents.size() - 1;
            agentcolors_changed = true;
        }
        ImGui::TreePop();
    }
}

void AgentRenderer::Invalidate() {
    VBuffer::Invalidate();
}
AgentRenderer& AgentRenderer::Instance() { return *instance; }
AgentRenderer::AgentRenderer() {
    instance = this;
    shapes[Tear].AddVertex(1.8f, 0, Dark);      // A
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);   // B
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);   // B
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);   // C
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);   // C
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);  // D
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);  // D
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);  // E
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);  // E
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark); // F
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark); // F
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);  // G
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);  // G
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);  // H
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);  // H
    shapes[Tear].AddVertex(1.8f, 0.0f, Dark);   // A
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);  // O

    constexpr auto pi = static_cast<float>(M_PI);
    for (int i = 0; i < num_triangles; ++i) {
        const float angle1 = 2 * (i + 0) * pi / num_triangles;
        const float angle2 = 2 * (i + 1) * pi / num_triangles;
        shapes[Circle].AddVertex(std::cos(angle1), std::sin(angle1), Dark);
        shapes[Circle].AddVertex(std::cos(angle2), std::sin(angle2), Dark);
        shapes[Circle].AddVertex(0.0f, 0.0f, Light);
    }

    for (int i = 0; i < num_triangles; ++i) {
        const float angle1 = 2 * (i + 0) * pi / num_triangles;
        const float angle2 = 2 * (i + 1) * pi / num_triangles;
        shapes[BigCircle].AddVertex(std::cos(angle1), std::sin(angle1), None);
        shapes[BigCircle].AddVertex(std::cos(angle2), std::sin(angle2), None);
        shapes[BigCircle].AddVertex(0.0f, 0.0f, CircleCenter);
    }

    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);

    max_shape_verts = 0;
    for (int shape = 0; shape < shape_size; ++shape) {
        if (max_shape_verts < shapes[shape].vertices.size()) {
            max_shape_verts = shapes[shape].vertices.size();
        }
    }
    constexpr GW::UI::UIMessage hook_messages[] = {
        GW::UI::UIMessage::kShowAgentNameTag,
        GW::UI::UIMessage::kSetAgentNameTagAttribs
    };
    for (const auto message_id : hook_messages) {
        GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, message_id, OnUIMessage);
    }
}
void AgentRenderer::OnUIMessage(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void*) {
    switch (msgid) {
    case GW::UI::UIMessage::kShowAgentNameTag:
    case GW::UI::UIMessage::kSetAgentNameTagAttribs: {
        const auto msg = static_cast<GW::UI::AgentNameTagInfo*>(wParam);

        GW::Agent* agent = GW::Agents::GetAgentByID(msg->agent_id);
        if (!agent) return;
        const GW::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living) return;
        auto& custom_agents_map = Instance().custom_agents_map;
        const auto it = custom_agents_map.find(living->player_number);
        if (it != custom_agents_map.end()) {
            for (const CustomAgent* ca : it->second) {
                if (!ca->active) continue;
                if (!ca->color_text_active) continue;
                if (ca->mapId > 0 && ca->mapId != static_cast<DWORD>(GW::Map::GetMapID())) continue;
                msg->text_color = ca->color_text;
            }
        }
        break;
    }
    }
}
void AgentRenderer::Shape_t::AddVertex(float x, float y, AgentRenderer::Color_Modifier mod) {
    vertices.push_back(Shape_Vertex(x, y, mod));
}

void AgentRenderer::Initialize(IDirect3DDevice9* device) {
    type = D3DPT_TRIANGLELIST;
    vertices_max = max_shape_verts * 0x200; // support for up to 512 agents, should be enough
    vertices = nullptr;
    HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    if (FAILED(hr)) printf("AgentRenderer initialize error: HRESULT: 0x%lX\n", hr);
}

void AgentRenderer::Render(IDirect3DDevice9* device) {
    if (!initialized) {
        Initialize(device);
        initialized = true;
    }

    HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
    if (FAILED(res)) printf("AgentRenderer Lock() HRESULT: 0x%lX\n", res);

    vertices_count = 0;

    // get stuff
    GW::AgentArray* agents = GW::Agents::GetAgentArray();
    GW::NPCArray* npcs = agents ? GW::Agents::GetNPCArray() : nullptr;
    if (!npcs) return;

    const GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
    if (target) {
        auto_target_id = 0;
    }
    else if (auto_target_id) {
        auto* const target_ = GW::Agents::GetAgentByID(auto_target_id);
        target = target_ ? target_->GetAsAgentLiving() : nullptr;
    }

    // 1. eoes
    for (GW::Agent* agent_ptr : *agents) {
        if (!agent_ptr) continue;
        GW::AgentLiving* agent = agent_ptr->GetAsAgentLiving();
        if (agent == nullptr) continue;
        if (agent->GetIsDead()) continue;
        switch (agent->player_number) {
        case GW::Constants::ModelID::EoE:
            Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_eoe);
            break;
        case GW::Constants::ModelID::QZ:
            Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_qz);
            break;
        case GW::Constants::ModelID::Winnowing:
            Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_winnowing);
            break;
        default:
            break;
        }
    }
    // 2. non-player agents
    static std::vector<std::pair<const GW::Agent*, const CustomAgent*>> custom_agents_to_draw;
    custom_agents_to_draw.clear();

    // some helper lambads
    auto AddCustomAgentsToDraw = [this](const GW::Agent* agent) -> bool {
        const GW::AgentLiving* living = agent->GetAsAgentLiving();
        if (!living) return false;
        const auto it = custom_agents_map.find(living->player_number);
        bool found_custom_agent = false;
        if (it != custom_agents_map.end()) {
            for (const CustomAgent* ca : it->second) {
                if (!ca->active) continue;
                if (ca->mapId > 0 && ca->mapId != static_cast<DWORD>(GW::Map::GetMapID())) continue;
                custom_agents_to_draw.push_back({ agent, ca });
                found_custom_agent = true;
            }
        }
        return found_custom_agent;
    };
    auto sort_custom_agents_to_draw = []() -> void {
        std::ranges::sort(custom_agents_to_draw,
          [&](const std::pair<const GW::Agent*, const CustomAgent*>& pA,
              const std::pair<const GW::Agent*, const CustomAgent*>& pB) -> bool {
              return pA.second->index > pB.second->index;
          });
    };

    for (GW::Agent* agent_ptr : *agents) {
        if (!agent_ptr) continue;
        if (target == agent_ptr) continue; // will draw target at the end
        if (agent_ptr->GetIsGadgetType()) {
            GW::AgentGadget* agent = agent_ptr->GetAsAgentGadget();
            if(GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && agent->extra_type == 7602) continue;
        }
        else if (agent_ptr->GetIsLivingType()) {
            GW::AgentLiving* agent = agent_ptr->GetAsAgentLiving();
            if (agent->player_number <= 12) continue;
            if (!show_hidden_npcs
                && agent->IsNPC()
                && agent->player_number < npcs->size()
                && (npcs->at(agent->player_number).npc_flags & 0x10000) > 0) continue;
        }
        if (AddCustomAgentsToDraw(agent_ptr)) {
            // found a custom agent to draw, we'll draw them later
        } else {
            Enqueue(agent_ptr);
        }
        if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
    }
    // 3. custom colored models
    sort_custom_agents_to_draw();
    for (const auto& [fst, snd] : custom_agents_to_draw) {
        Enqueue(fst, snd);
        if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
    }
    custom_agents_to_draw.clear();

    // 4. target if it's a non-player
    if (target && target->player_number > 12) {
        if (AddCustomAgentsToDraw(target)) {
            sort_custom_agents_to_draw();
            for (const auto& [fst, snd] : custom_agents_to_draw) {
                Enqueue(fst, snd);
                if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
            }
            custom_agents_to_draw.clear();
        } else {
            Enqueue(target);
        }
    }

    // note: we don't support custom agents for players

    // 5. players
    for (GW::Agent* agent_ptr : *agents) {
        if (!agent_ptr) continue;
        GW::AgentLiving* agent = agent_ptr->GetAsAgentLiving();
        if (agent == nullptr) continue;
        if (agent->player_number > 12) continue;
        if (agent == player) continue; // will draw player at the end
        if (agent == target) continue; // will draw target at the end

        Enqueue(agent);

        if (vertices_count >= vertices_max - 4 * max_shape_verts) break;
    }

    // 6. target if it's a player
    if (target && target != player && target->player_number <= 12) {
        Enqueue(target);
    }

    // 7. player
    if (player) {
        Enqueue(player);
    }

    buffer->Unlock();

    if (vertices_count != 0) {
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices_count / 3);
        vertices_count = 0;
    }
}

void AgentRenderer::Enqueue(const GW::Agent* agent, const CustomAgent* ca) {
    const auto color = GetColor(agent, ca);
    const auto alpha = color >> IM_COL32_A_SHIFT & 0xFFu;
    const auto size = GetSize(agent, ca);
    const auto shape = GetShape(agent, ca);
    // No target highlight if BigCircle
    if (shape != BigCircle) {
        // Target highlight if this is the current or next target
        if (auto_target_id == agent->agent_id || GW::Agents::GetTargetId() == agent->agent_id) {
            Enqueue(shape, agent, size + 50.0f, Colors::Sub(color_target, IM_COL32(0, 0, 0, 50)));
            return Enqueue(shape, agent, size, color);
        }
    }
    if (!alpha) return;
    if (agent_border_thickness && agent->GetIsLivingType()) {
        Enqueue(shape, agent, size + static_cast<float>(agent_border_thickness),
            Colors::ARGB(static_cast<int>(alpha * 0.8), 0, 0, 0));
    }
    return Enqueue(shape, agent, size, color);
}

Color AgentRenderer::GetColor(const GW::Agent* agent, const CustomAgent* ca) const {
    if (agent->agent_id == GW::Agents::GetPlayerId()) {
        if (agent->GetAsAgentLiving()->GetIsDead()) return color_player_dead;
        else return color_player;
    }

    if (agent->GetIsGadgetType()) return color_signpost;
    if (agent->GetIsItemType()) return color_item;
    if (!agent->GetIsLivingType()) return color_item;

    const GW::AgentLiving* living = agent->GetAsAgentLiving();

    // don't draw dead spirits

    auto* npc = living->GetIsDead() && living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (npc) {
        switch (npc->model_file_id) {
        case 0x22A34: // nature rituals
        case 0x2D0E4: // defensive binding rituals
        case 0x2D07E: // offensive binding rituals
            return IM_COL32(0, 0, 0, 0);
        default:
            break;
        }
    }

    if (ca && ca->color_active && !living->GetIsDead()) {
        if (living->allegiance == GW::Constants::Allegiance::Enemy && living->hp <= 0.9f) {
            return Colors::Sub(ca->color, color_agent_damaged_modifier);
        }
        return ca->color;
    }
    // hostiles
    if (living->allegiance == GW::Constants::Allegiance::Enemy) {
        if(living->GetIsDead()) return color_hostile_dead;
        const Color* c = &color_hostile;
        if (boss_colors && living->GetHasBossGlow()) {
            const auto prof = GetAgentProfession(living);
            if (prof) c = &profession_colors[prof];
        }
        const auto& polygons = Minimap::Instance().custom_renderer.polygons;
        const auto& markers = Minimap::Instance().custom_renderer.markers;
        const auto is_relevant = [living](const CustomRenderer::CustomPolygon& polygon)-> bool {
            return (polygon.visible && polygon.map == GW::Constants::MapID::None || polygon.map == GW::Map::GetMapID()) && !polygon.points.empty() && (polygon.color_sub & IM_COL32_A_MASK) != 0 &&
                   GW::GetDistance(living->pos, polygon.points.at(0)) < 2500.f;
        };
        const auto is_relevant_circle = [living](const CustomRenderer::CustomMarker& marker) {
            return (marker.visible && marker.map == GW::Constants::MapID::None || marker.map == GW::Map::GetMapID()) && (marker.color_sub & IM_COL32_A_MASK) != 0 &&
                   GW::GetDistance(living->pos, marker.pos) < 2500.f;
        };
        const auto is_inside = [](const GW::Vec2f pos, const std::vector<GW::Vec2f> points) -> bool {
            bool b = false;
            for (auto i = 0u, j = points.size() - 1; i < points.size(); j = i++) {
                if (points[i].y >= pos.y != points[j].y >= pos.y &&
                    pos.x <= (points[j].x - points[i].x) * (pos.y - points[i].y) / (points[j].y - points[i].y) +
                                 points[i].x) {
                    b = !b;
                }
            }
            return b;
        };

        auto is_inside_circle = [](const GW::Vec2f pos, const GW::Vec2f circle, const float radius) -> bool {
            return GW::GetSquareDistance(pos, circle) <= radius * radius;
        };
        for (const auto& polygon : polygons) {
            if (!is_relevant(polygon)) continue;
            if (is_inside(living->pos, polygon.points)) {
                c = &polygon.color_sub;
            }
        }
        for (const auto& marker : markers) {
            if (!is_relevant_circle(marker)) continue;
            if (is_inside_circle(living->pos, marker.pos, marker.size)) {
                c = &marker.color_sub;
            }
        }
        if (living->hp > 0.9f) return *c;
        return Colors::Sub(*c, color_agent_damaged_modifier);
    }

    // neutrals
    if (living->allegiance == GW::Constants::Allegiance::Neutral) return color_neutral;

    // friendly
    if (living->GetIsDead()) return color_ally_dead;
    switch (living->allegiance) {
    case GW::Constants::Allegiance::Ally_NonAttackable: return color_ally; // ally
    case GW::Constants::Allegiance::Npc_Minipet: return color_ally_npc; // npc / minipet
    case GW::Constants::Allegiance::Spirit_Pet: return color_ally_spirit; // spirit / pet
    case GW::Constants::Allegiance::Minion: return color_ally_minion; // minion
    default: break;
    }

    return IM_COL32(0, 0, 0, 0);
}

float AgentRenderer::GetSize(const GW::Agent* agent, const CustomAgent* ca) const {
    if (agent->agent_id == GW::Agents::GetPlayerId()) return size_player;
    if (agent->GetIsGadgetType()) return size_signpost;
    if (agent->GetIsItemType()) return size_item;
    if (!agent->GetIsLivingType()) return size_item;

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (ca && ca->size_active && ca->size >= 0) return ca->size;

    if (living->GetHasBossGlow()) return size_boss;

    switch (living->allegiance) {
    case GW::Constants::Allegiance::Ally_NonAttackable: // ally
    case GW::Constants::Allegiance::Neutral: // neutral
    case GW::Constants::Allegiance::Spirit_Pet: // spirit / pet
    case GW::Constants::Allegiance::Npc_Minipet: // npc / minipet
        return size_default;

    case GW::Constants::Allegiance::Minion: // minion
        return size_minion;

    case GW::Constants::Allegiance::Enemy: // hostile
        switch (living->player_number) {
        case GW::Constants::ModelID::Rotscale:

        case GW::Constants::ModelID::DoA::StygianLordNecro:
        case GW::Constants::ModelID::DoA::StygianLordMesmer:
        case GW::Constants::ModelID::DoA::StygianLordEle:
        case GW::Constants::ModelID::DoA::StygianLordMonk:
        case GW::Constants::ModelID::DoA::StygianLordDerv:
        case GW::Constants::ModelID::DoA::StygianLordRanger:
        case GW::Constants::ModelID::DoA::BlackBeastOfArgh:
        case GW::Constants::ModelID::DoA::SmotheringTendril:
        case GW::Constants::ModelID::DoA::LordJadoth:

        case GW::Constants::ModelID::UW::KeeperOfSouls:
        case GW::Constants::ModelID::UW::FourHorseman:
        case GW::Constants::ModelID::UW::Slayer:
        case GW::Constants::ModelID::UW::TerrorwebQueen:
        case GW::Constants::ModelID::UW::Dhuum:

        case GW::Constants::ModelID::FoW::ShardWolf:
        case GW::Constants::ModelID::FoW::SeedOfCorruption:
        case GW::Constants::ModelID::FoW::LordKhobay:
        case GW::Constants::ModelID::FoW::DragonLich:

        case GW::Constants::ModelID::Deep::Kanaxai:
        case GW::Constants::ModelID::Deep::KanaxaiAspect:
        case GW::Constants::ModelID::Urgoz::Urgoz:

        case GW::Constants::ModelID::EotnDungeons::DiscOfChaos:
        case GW::Constants::ModelID::EotnDungeons::PlagueOfDestruction:
        case GW::Constants::ModelID::EotnDungeons::ZhimMonns:
        case GW::Constants::ModelID::EotnDungeons::Khabuus:
        case GW::Constants::ModelID::EotnDungeons::DuncanTheBlack:
        case GW::Constants::ModelID::EotnDungeons::JusticiarThommis:
        case GW::Constants::ModelID::EotnDungeons::RandStormweaver:
        case GW::Constants::ModelID::EotnDungeons::Selvetarm:
        case GW::Constants::ModelID::EotnDungeons::Forgewright:
        case GW::Constants::ModelID::EotnDungeons::HavokSoulwail:
        case GW::Constants::ModelID::EotnDungeons::RragarManeater3:
        case GW::Constants::ModelID::EotnDungeons::RragarManeater12:
        case GW::Constants::ModelID::EotnDungeons::Arachni:
        case GW::Constants::ModelID::EotnDungeons::Hidesplitter:
        case GW::Constants::ModelID::EotnDungeons::PrismaticOoze:
        case GW::Constants::ModelID::EotnDungeons::IlsundurLordofFire:
        case GW::Constants::ModelID::EotnDungeons::EldritchEttin:
        case GW::Constants::ModelID::EotnDungeons::TPSRegulartorGolem:
        case GW::Constants::ModelID::EotnDungeons::MalfunctioningEnduringGolem:
        case GW::Constants::ModelID::EotnDungeons::CyndrTheMountainHeart:
        case GW::Constants::ModelID::EotnDungeons::InfernalSiegeWurm:
        case GW::Constants::ModelID::EotnDungeons::Frostmaw:
        case GW::Constants::ModelID::EotnDungeons::RemnantOfAntiquities:
        case GW::Constants::ModelID::EotnDungeons::MurakaiLadyOfTheNight:
        case GW::Constants::ModelID::EotnDungeons::ZoldarkTheUnholy:
        case GW::Constants::ModelID::EotnDungeons::Brigand:
        case GW::Constants::ModelID::EotnDungeons::FendiNin:
        case GW::Constants::ModelID::EotnDungeons::SoulOfFendiNin:
        case GW::Constants::ModelID::EotnDungeons::KeymasterOfMurakai:
        case GW::Constants::ModelID::EotnDungeons::AngrySnowman:

        case GW::Constants::ModelID::BonusMissionPack::WarAshenskull:
        case GW::Constants::ModelID::BonusMissionPack::RoxAshreign:
        case GW::Constants::ModelID::BonusMissionPack::AnrakTindershot:
        case GW::Constants::ModelID::BonusMissionPack::DettMortash:
        case GW::Constants::ModelID::BonusMissionPack::AkinCinderspire:
        case GW::Constants::ModelID::BonusMissionPack::TwangSootpaws:
        case GW::Constants::ModelID::BonusMissionPack::MagisEmberglow:
        case GW::Constants::ModelID::BonusMissionPack::MerciaTheSmug:
        case GW::Constants::ModelID::BonusMissionPack::OptimusCaliph:
        case GW::Constants::ModelID::BonusMissionPack::LazarusTheDire:
        case GW::Constants::ModelID::BonusMissionPack::AdmiralJakman:
        case GW::Constants::ModelID::BonusMissionPack::PalawaJoko:
        case GW::Constants::ModelID::BonusMissionPack::YuriTheHand:
        case GW::Constants::ModelID::BonusMissionPack::MasterRiyo:
        case GW::Constants::ModelID::BonusMissionPack::CaptainSunpu:
        case GW::Constants::ModelID::BonusMissionPack::MinisterWona:
            return size_boss;

        default:
            return size_default;
        }
        break;

    default:
        return size_default;
    }
}

AgentRenderer::Shape_e AgentRenderer::GetShape(const GW::Agent* agent, const CustomAgent* ca) const {
    if (agent->GetIsGadgetType()) return Quad;
    if (agent->GetIsItemType()) return Quad;
    if (!agent->GetIsLivingType()) return Quad; // shouldn't happen but just in case

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (living->login_number > 0) return Tear;  // players

    if (ca && ca->shape_active) {
        return ca->shape;
    }

    auto* npc = living->IsNPC() ? GW::Agents::GetNPCByID(living->player_number) : nullptr;
    if (npc) {
        switch (npc->model_file_id) {
        case 0x22A34: // nature rituals
        case 0x2D0E4: // defensive binding rituals
        case 0x2963E: // dummies
            return Circle;
        default:
            break;
        }
    }

    return default_shape;
}

void AgentRenderer::Enqueue(Shape_e shape, const GW::Agent* agent, float size, Color color) {
    if ((color & IM_COL32_A_MASK) == 0) return;

    size_t num_v = shapes[shape].vertices.size();
    for (size_t i = 0; i < num_v; ++i) {
        const Shape_Vertex& vert = shapes[shape].vertices[i];
        GW::Vec2f pos = (GW::Rotate(vert, agent->rotation_cos, agent->rotation_sin) * size) + agent->pos;
        switch (vert.modifier) {
        case Dark: vertices[i].color = Colors::Sub(color, color_agent_modifier); break;
        case Light: vertices[i].color = Colors::Add(color, color_agent_modifier); break;
        case CircleCenter: vertices[i].color = Colors::Sub(color, IM_COL32(0, 0, 0, 50)); break;
        default: vertices[i].color = color; break;
        }
        vertices[i].z = 0.0f;
        vertices[i].x = pos.x;
        vertices[i].y = pos.y;
    }
    vertices += num_v;
    vertices_count += num_v;
}

void AgentRenderer::BuildCustomAgentsMap() {
    custom_agents_map.clear();
    for (const CustomAgent* ca : custom_agents) {
        if (!custom_agents_map.contains(ca->modelId)) {
            custom_agents_map[ca->modelId] = std::vector<const CustomAgent*>();
        }
        custom_agents_map[ca->modelId].push_back(ca);
    }
}

AgentRenderer::CustomAgent::CustomAgent(CSimpleIni* ini, const char* section)
    : ui_id(++cur_ui_id) {

    active = ini->GetBoolValue(section, VAR_NAME(active), active);
    GuiUtils::StrCopy(name, ini->GetValue(section, VAR_NAME(name), ""), sizeof(name));
    modelId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(modelId), static_cast<long>(modelId)));
    mapId = static_cast<DWORD>(ini->GetLongValue(section, VAR_NAME(mapId), static_cast<long>(mapId)));

    color = Colors::Load(ini, section, VAR_NAME(color), color);
    color_text = Colors::Load(ini, section, VAR_NAME(color_text), color_text);
    int s = ini->GetLongValue(section, VAR_NAME(shape), shape);
    if (s >= 1 && s <= 4) {
        // this is a small hack because we used to have shape=0 -> default, now we just cast to Shape_e.
        // but shape=1 on file is still tear (which is Shape_e::Tear == 0).
        shape = static_cast<Shape_e>(s - 1);
    }
    size = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(size), size));

    color_active = ini->GetBoolValue(section, VAR_NAME(color_active), color_active);
    color_text_active = ini->GetBoolValue(section, VAR_NAME(color_text_active), color_text_active);
    shape_active = ini->GetBoolValue(section, VAR_NAME(shape_active), shape_active);
    size_active = ini->GetBoolValue(section, VAR_NAME(size_active), size_active);
}

AgentRenderer::CustomAgent::CustomAgent(DWORD _modelId, Color _color, const char* _name)
    : ui_id(++cur_ui_id) {

    modelId = _modelId;
    color = _color;
    GuiUtils::StrCopy(name, _name, sizeof(name));
    active = true;
}

void AgentRenderer::CustomAgent::SaveSettings(CSimpleIni* ini, const char* section) const {
    ini->SetBoolValue(section, VAR_NAME(active), active);
    ini->SetValue(section, VAR_NAME(name), name);
    ini->SetLongValue(section, VAR_NAME(modelId), static_cast<long>(modelId));
    ini->SetLongValue(section, VAR_NAME(mapId), static_cast<long>(mapId));

    Colors::Save(ini, section, VAR_NAME(color), color);
    Colors::Save(ini, section, VAR_NAME(color_text), color_text);
    ini->SetLongValue(section, VAR_NAME(shape), shape + 1);
    ini->SetDoubleValue(section, VAR_NAME(size), size);

    ini->SetBoolValue(section, VAR_NAME(color_active), color_active);
    ini->SetBoolValue(section, VAR_NAME(color_text_active), color_text_active);
    ini->SetBoolValue(section, VAR_NAME(shape_active), shape_active);
    ini->SetBoolValue(section, VAR_NAME(size_active), size_active);
}

bool AgentRenderer::CustomAgent::DrawHeader() {
    ImGui::SameLine(0, 18);
    bool changed = ImGui::Checkbox("##visible", &active);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float button_width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
    ImGui::SameLine();
    float cursor_pos = ImGui::GetCursorPosX();
    if (color_active) {
        changed |= ImGui::ColorButtonPicker("##color", &color);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
            ImGui::ColorTooltip("Minimap Color##color_tooltip", &col.x, 0);
        }

        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    if (color_text_active) {
        changed |= ImGui::ColorButtonPicker("##color_text", &color_text);
        if (ImGui::IsItemHovered()) {
            const ImVec4 col = ImGui::ColorConvertU32ToFloat4(color_text);
            ImGui::ColorTooltip("Name Tag Color##color_tooltip", &col.x, 0);
        }
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(cursor_pos += button_width);
    ImGui::Text(name);
    return changed;
}
bool AgentRenderer::CustomAgent::DrawSettings(AgentRenderer::CustomAgent::Operation& op) {
    bool changed = false;

    if (ImGui::TreeNodeEx("##params", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap)) {
        ImGui::PushID(static_cast<int>(ui_id));

        changed |= DrawHeader();

        if (ImGui::Checkbox("##visible2", &active)) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("If this custom agent is active");
        ImGui::SameLine();
        float x = ImGui::GetCursorPosX();
        if (ImGui::InputText("Name", name, 128)) changed = true;
        ImGui::ShowHelp("A name to help you remember what this is. Optional.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputInt("Model ID", (int*)(&modelId))) {
            op = Operation::ModelIdChange;
            changed = true;
        }
        ImGui::ShowHelp("The Agent to which this custom attributes will be applied. Required.");
        ImGui::SetCursorPosX(x);
        if (ImGui::InputInt("Map ID", (int*)&mapId)) changed = true;
        ImGui::ShowHelp("The map where it will be applied. Optional. Leave 0 for any map");

        ImGui::Spacing();

        if (ImGui::Checkbox("##color_active", &color_active)) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default color will be used");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Color", &color, 0)) changed = true;
        ImGui::ShowHelp("The custom color for this agent.");

        if (ImGui::Checkbox("##color_text_active", &color_text_active)) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default color will be used");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Text color", &color_text, 0)) changed = true;
        ImGui::ShowHelp("The custom text color for this agent.");

        if (ImGui::Checkbox("##size_active", &size_active)) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default size will be used");
        ImGui::SameLine();
        if (ImGui::DragFloat("Size", &size, 1.0f, 0.0f, 200.0f)) changed = true;
        ImGui::ShowHelp("The size for this agent.");

        if (ImGui::Checkbox("##shape_active", &shape_active)) changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default shape will be used");
        ImGui::SameLine();
        static const char* items[] = { "Tear", "Circle", "Square", "Big Circle" };
        if (ImGui::Combo("Shape", (int*)&shape, items, 4)) {
            changed = true;
        }
        ImGui::ShowHelp("The shape of this agent.");

        ImGui::Spacing();

        // === Move and delete buttons ===
        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
        if (ImGui::Button("Move Up", ImVec2(width, 0))) {
            op = Operation::MoveUp;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the color up in the list");
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Move Down", ImVec2(width, 0))) {
            op = Operation::MoveDown;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the color down in the list");
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Delete", ImVec2(width, 0))) {
            ImGui::OpenPopup("Delete Color?");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the color");

        if (ImGui::BeginPopupModal("Delete Color?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n");
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                op = Operation::Delete;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::TreePop();
        ImGui::PopID();
    } else {
        changed |= DrawHeader();
    }
    return changed;
}
