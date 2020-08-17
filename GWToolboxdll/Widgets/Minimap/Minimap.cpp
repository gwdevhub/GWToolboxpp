#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <GuiUtils.h>
#include <ImGuiAddons.h>
#include <Logger.h>

#include <Widgets/Minimap/Minimap.h>

void Minimap::Terminate()
{
    ToolboxWidget::Terminate();
    range_renderer.Invalidate();
    pmap_renderer.Invalidate();
    agent_renderer.Invalidate();
    pingslines_renderer.Invalidate();
    symbols_renderer.Invalidate();
    custom_renderer.Invalidate();
    effect_renderer.Invalidate();
}

void Minimap::Initialize()
{
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentPinged>(&AgentPinged_Entry, [this](GW::HookStatus *, GW::Packet::StoC::AgentPinged *pak) -> void {
        if (visible) {
            pingslines_renderer.P046Callback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CompassEvent>(&CompassEvent_Entry, [this](GW::HookStatus *, GW::Packet::StoC::CompassEvent *pak) -> void {
        if (visible) {
            pingslines_renderer.P138Callback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&CompassEvent_Entry, [this](GW::HookStatus *status, GW::Packet::StoC::PlayEffect *pak) -> void {
        UNREFERENCED_PARAMETER(status);
        if (visible) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValueTarget_Entry, [this](GW::HookStatus *s, GW::Packet::StoC::GenericValue *pak) -> void {
        UNREFERENCED_PARAMETER(s);
        if (visible) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry, [this](GW::HookStatus *s, GW::Packet::StoC::GenericValueTarget *pak) -> void {
        UNREFERENCED_PARAMETER(s);
        if (visible) {
            pingslines_renderer.P153Callback(pak);
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
                effect_renderer.PacketCallback(pak);
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillActivate>(&SkillActivate_Entry, &SkillActivateCallback);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Entry, [this](GW::HookStatus *, GW::Packet::StoC::InstanceLoadInfo *packet) -> void { is_observing = packet->is_observer != 0; });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus *, GW::Packet::StoC::InstanceLoadFile *packet) -> void {
        UNREFERENCED_PARAMETER(packet);
        pmap_renderer.Invalidate();
        loading = false;
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry, [this](GW::HookStatus *, GW::Packet::StoC::GameSrvTransfer *pak) -> void {
        UNREFERENCED_PARAMETER(pak);
        loading = true;
        agent_renderer.auto_target_id = 0;
    });
    GW::UI::RegisterUIMessageCallback(&UIMsg_Entry, [this](GW::HookStatus *, uint32_t msgid, void *lParam, void *) -> void {
        if (msgid != GW::UI::kAutoTargetAgent || !lParam)
            return;
        agent_renderer.auto_target_id = *static_cast<uint32_t *>(lParam);
    });

    last_moved = TIMER_INIT();

    pmap_renderer.Invalidate();

    GW::Chat::CreateCommand(L"flag", &OnFlagHeroCmd);
}
void Minimap::SkillActivateCallback(GW::HookStatus*, GW::Packet::StoC::SkillActivate *pak)
{
    if (pak->agent_id == GW::Agents::GetPlayerId()) {
        if (pak->skill_id == (DWORD)GW::Constants::SkillID::Shadow_of_Haste || pak->skill_id == (DWORD)GW::Constants::SkillID::Shadow_Walk) {
            Instance().shadowstep_location = GW::Agents::GetPlayer()->pos;
        }
    }
}
GW::Vec2f Minimap::ShadowstepLocation() const
{
    return shadowstep_location;
}

void Minimap::OnFlagHeroCmd(const wchar_t *message, int argc, LPWSTR *argv)
{
    UNREFERENCED_PARAMETER(message);

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return; // Not explorable - "/flag" can be typed in chat to bypass flag hero buttons, so this is needed.
    }
    if (argc <= 1) {
        Instance().FlagHero(0); // "/flag"
        return;
    }
    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);
    float x;
    float y;
    unsigned int n_heros = 0; // Count of heros available
    unsigned int f_hero = 0;  // Hero number to flag
    if (arg1 == L"all") {
        if (argc <= 2) {
            Instance().FlagHero(0); // "/flag all" == "/flag"
            return;
        }
        if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
            Log::Error("Please provide command in format /flag all [x] [y]"); // Not enough args or coords not valid float vals.
            return;
        }
        GW::PartyMgr::FlagAll(GW::GamePos(x, y, 0)); // "/flag all -2913.41 3004.78"
        return;
    }
    const auto heroarray = GW::GameContext::instance()->party->player_party->heroes;
    if (heroarray.valid())
        n_heros = heroarray.size();
    if (n_heros < 1) {
        return; // Player has no heroes, so no need to continue.
    }
    if (arg1 == L"clear") {
        for (unsigned int i = 1; i <= n_heros; ++i) {
            GW::PartyMgr::UnflagHero(i);
        }
        GW::PartyMgr::UnflagAll(); // "/flag clear"
        return;
    }
    if (!GuiUtils::ParseUInt(argv[1], &f_hero) || f_hero < 1 || f_hero > n_heros) {
        Log::Error("Invalid hero number");
        return; // Invalid hero number
    }
    if (argc < 3) {
        Instance().FlagHero(f_hero); // "/flag 5"
        return;
    }
    const std::wstring arg2 = GuiUtils::ToLower(argv[2]);
    if (arg2 == L"clear") {
        GW::PartyMgr::UnflagHero(f_hero); // "/flag 5 clear"
        return;
    }
    if (argc < 4 || !GuiUtils::ParseFloat(argv[2], &x) || !GuiUtils::ParseFloat(argv[3], &y)) {
        Log::Error("Please provide command in format /flag [hero number] [x] [y]"); // Invalid coords
        return;
    }
    GW::PartyMgr::FlagHeroAgent(GW::Agents::GetHeroAgentID(f_hero), GW::GamePos(x, y, 0)); // "/flag 5 -2913.41 3004.78"
}

void Minimap::DrawSettingInternal()
{
    static char const *minimap_modifier_behavior_combo_str = "Disabled\0Draw\0Target\0Move\0Walk\0\0";

    ImVec2 winsize(100.0f, 100.0f);
    ImGuiWindow *window = ImGui::FindWindowByName(Name());
    if (window) {
        winsize = window->Size;
    }
    if (ImGui::DragFloat("Size", &winsize.x, 1.0f, 0.0f, 0.0f, "%.0f")) {
        winsize.y = winsize.x;
        ImGui::SetWindowSize(Name(), winsize);
    }

    ImGui::Text("General");
    static float a = scale;
    if (ImGui::DragFloat("Scale", &a, 0.01f, 0.1f, 10.f))
        scale = a;
    ImGui::Text("You can set the color alpha to 0 to disable any minimap feature.");
    // agent_rendered has its own TreeNodes
    agent_renderer.DrawSettings();
    if (ImGui::TreeNode("Ranges")) {
        range_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Pings and drawings")) {
        pingslines_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("AoE Effects")) {
        effect_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Symbols")) {
        symbols_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Terrain")) {
        pmap_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Custom Markers")) {
        custom_renderer.DrawSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Hero flagging")) {
        ImGui::Checkbox("Show hero flag controls", &hero_flag_controls_show);
        ImGui::Checkbox("Attach to minimap", &hero_flag_window_attach);
        ImGui::ShowHelp("If disabled, you can move/resize the window with 'Unlock Move All'.");
        Colors::DrawSettingHueWheel("Background", &hero_flag_window_background);
        ImGui::TreePop();
    }
    ImGui::Checkbox("Show boss by profession color on minimap", &agent_renderer.boss_colors);
    ImGui::Checkbox("Show hidden NPCs", &agent_renderer.show_hidden_npcs);
    ImGui::ShowHelp("Show NPCs that aren't usually visible on the minimap\ne.g. minipets, invisible NPCs");
    ImGui::Checkbox("Allow mouse click-through", &mouse_clickthrough);
    ImGui::ShowHelp("Toolbox minimap will not capture mouse events");
    if (mouse_clickthrough) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    ImGui::Checkbox("Allow mouse click-through in outposts", &mouse_clickthrough_in_outpost);
    ImGui::ShowHelp("Toolbox minimap will not capture mouse events when in an outpost");
    ImGui::Text("Hold + Click modifiers");
    ImGui::ShowHelp("Define behaviour of holding keyboard keys and clicking the minimap.\n"
                    "Draw: ping and draw on the compass.\n"
                    "Target: click to target agent.\n"
                    "Move: move the minimap outside of compass range.\n"
                    "Walk: start walking character to selected location.\n");
    ImGui::Combo("None", reinterpret_cast<int *>(&key_none_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Control", reinterpret_cast<int *>(&key_ctrl_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Shift", reinterpret_cast<int *>(&key_shift_behavior), minimap_modifier_behavior_combo_str);
    ImGui::Combo("Alt", reinterpret_cast<int *>(&key_alt_behavior), minimap_modifier_behavior_combo_str);
    if (mouse_clickthrough) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
    ImGui::Checkbox("Reduce agent ping spam", &pingslines_renderer.reduce_ping_spam);
    ImGui::ShowHelp("Additional pings on the same agents will increase the duration of the existing ping, rather than create a new one.");
    ImGui::Checkbox("Map Rotation", &rotate_minimap);
    ImGui::ShowHelp("Map rotation on (e.g. Compass), or off (e.g. Mission Map).");
    ImGui::Checkbox("Circular", &circular_map);
    ImGui::ShowHelp("Whether the map should be circular like the compass or a square (default).");
}

void Minimap::LoadSettings(CSimpleIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    scale = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(scale), 1.0));
    hero_flag_controls_show = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), true);
    hero_flag_window_attach = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), true);
    hero_flag_window_background = Colors::Load(ini, Name(), "hero_flag_controls_background", ImColor(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
    mouse_clickthrough = ini->GetBoolValue(Name(), VAR_NAME(mouse_clickthrough), false);
    mouse_clickthrough_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_outpost), mouse_clickthrough_in_outpost);
    rotate_minimap = ini->GetBoolValue(Name(), VAR_NAME(rotate_minimap), true);
    circular_map = ini->GetBoolValue(Name(), VAR_NAME(circular_map), true);
    key_none_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_none_behavior), 1));
    key_ctrl_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_ctrl_behavior), 2));
    key_shift_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_shift_behavior), 3));
    key_alt_behavior = static_cast<MinimapModifierBehaviour>(ini->GetLongValue(Name(), VAR_NAME(key_alt_behavior), 4));
    pingslines_renderer.reduce_ping_spam = ini->GetBoolValue(Name(), VAR_NAME(reduce_ping_spam), false);
    range_renderer.LoadSettings(ini, Name());
    pmap_renderer.LoadSettings(ini, Name());
    agent_renderer.LoadSettings(ini, Name());
    pingslines_renderer.LoadSettings(ini, Name());
    symbols_renderer.LoadSettings(ini, Name());
    custom_renderer.LoadSettings(ini, Name());
    effect_renderer.LoadSettings(ini, Name());
}

void Minimap::SaveSettings(CSimpleIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    ini->SetDoubleValue(Name(), VAR_NAME(scale), scale);
    ini->SetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), hero_flag_controls_show);
    ini->SetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), hero_flag_window_attach);
    Colors::Save(ini, Name(), VAR_NAME(hero_flag_window_background), hero_flag_window_background);
    ini->SetBoolValue(Name(), VAR_NAME(mouse_clickthrough), mouse_clickthrough);
    ini->SetBoolValue(Name(), VAR_NAME(mouse_clickthrough_in_outpost), mouse_clickthrough_in_outpost);
    ini->SetLongValue(Name(), VAR_NAME(key_none_behavior), static_cast<long>(key_none_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_ctrl_behavior), static_cast<long>(key_ctrl_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_shift_behavior), static_cast<long>(key_shift_behavior));
    ini->SetLongValue(Name(), VAR_NAME(key_alt_behavior), static_cast<long>(key_alt_behavior));
    ini->SetBoolValue(Name(), VAR_NAME(reduce_ping_spam), pingslines_renderer.reduce_ping_spam);
    ini->SetBoolValue(Name(), VAR_NAME(rotate_minimap), rotate_minimap);
    ini->SetBoolValue(Name(), VAR_NAME(circular_map), circular_map);
    range_renderer.SaveSettings(ini, Name());
    pmap_renderer.SaveSettings(ini, Name());
    agent_renderer.SaveSettings(ini, Name());
    pingslines_renderer.SaveSettings(ini, Name());
    symbols_renderer.SaveSettings(ini, Name());
    custom_renderer.SaveSettings(ini, Name());
    effect_renderer.SaveSettings(ini, Name());
}

void Minimap::GetPlayerHeroes(GW::PartyInfo *party, std::vector<GW::AgentID> &_player_heroes)
{
    _player_heroes.clear();
    if (!party)
        return;
    GW::AgentLiving *player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player)
        return;
    const uint32_t player_id = player->login_number;
    auto heroes = party->heroes;
    _player_heroes.reserve(heroes.size());
    for (GW::HeroPartyMember &hero : heroes) {
        if (hero.owner_player_id == player_id)
            _player_heroes.push_back(hero.agent_id);
    }
}

float Minimap::GetMapRotation() const
{
    return rotate_minimap ? GW::CameraMgr::GetYaw() : static_cast<float>(1.5708);
}

D3DXVECTOR2 Minimap::GetGwinchScale() const
{
    return gwinch_scale;
}

void Minimap::Draw(IDirect3DDevice9 *device)
{
    if (!IsActive())
        return;

    GW::Agent *me = GW::Agents::GetPlayer();
    if (me == nullptr)
        return;

    // Check shadowstep location
    if (shadowstep_location.x != 0.0f || shadowstep_location.y != 0.0f) {
        GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
        if (!effects.valid()) {
            shadowstep_location = GW::Vec2f();
        }
        else {
            bool found = false;
            for (unsigned int i = 0; !found && i < effects.size(); ++i) {
                found = effects[i].skill_id == (DWORD)GW::Constants::SkillID::Shadow_of_Haste || effects[i].skill_id == (DWORD)GW::Constants::SkillID::Shadow_Walk;
            }
            if (!found) {
                shadowstep_location = GW::Vec2f();
            }
        }
    }

    // if not center and want to move, move center towards player
    if ((translation.x != 0 || translation.y != 0) && (me->move_x != 0 || me->move_y != 0) && TIMER_DIFF(last_moved) > ms_before_back) {
        const GW::Vec2f v(translation.x, translation.y);
        const float speed = std::min((TIMER_DIFF(last_moved) - ms_before_back) * acceleration, 500.0f);
        GW::Vec2f d = v;
        d = GW::Normalize(d) * speed;
        if (std::abs(d.x) > std::abs(v.x)) {
            translation = GW::Vec2f(0, 0);
        } else {
            translation -= d;
        }
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))) {
        // window pos are already rounded by imgui, so casting is no big deal
        location.x = static_cast<int>(ImGui::GetWindowPos().x);
        location.y = static_cast<int>(ImGui::GetWindowPos().y);
        size.x = static_cast<int>(ImGui::GetWindowSize().x);
        size.y = static_cast<int>(ImGui::GetWindowSize().y);
        ImGui::GetWindowDrawList()->AddCallback(
            [](const ImDrawList *parent_list, const ImDrawCmd *cmd) -> void {
                UNREFERENCED_PARAMETER(parent_list);

                auto device = static_cast<IDirect3DDevice9 *>(cmd->UserCallbackData);
                GW::Agent *me = GW::Agents::GetPlayer();
                if (me == nullptr)
                    return;

                // Backup the DX9 state
                IDirect3DStateBlock9 *d3d9_state_block = nullptr;
                if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
                    return;

                // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
                device->SetFVF(D3DFVF_CUSTOMVERTEX);
                device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
                device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                device->SetPixelShader(nullptr);
                device->SetVertexShader(nullptr);
                device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                device->SetRenderState(D3DRS_LIGHTING, false);
                device->SetRenderState(D3DRS_ZENABLE, false);
                device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
                device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
                device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                auto FillRect = [&device](const D3DCOLOR color, const int x, const int y, const int w, const int h) {
                    D3DVertex vertices[6] = {{static_cast<float>(x), static_cast<float>(y), 1.0f, color},         {static_cast<float>(x) + w, static_cast<float>(y), 1.0f, color}, {static_cast<float>(x), static_cast<float>(y) + h, 1.0f, color},
                                             {static_cast<float>(x) + w, static_cast<float>(y) + h, 1.0f, color}, {static_cast<float>(x), static_cast<float>(y), 1.0f, color},     {static_cast<float>(x) + w, static_cast<float>(y), 1.0f, color}};
                    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 4, vertices, sizeof(D3DVertex));
                };

                auto FillCircle = [&device](const float x, const float y, const float radius, const Color clr, float resolution = 199.f) {
                    resolution = std::min(resolution, 199.f);
                    D3DVertex vertices[200];
                    for (auto i = 0; i <= resolution; ++i) {
                        vertices[i] = {radius * cos(D3DX_PI * (i / (resolution / 2.f))) + x, y + radius * sin(D3DX_PI * (i / (resolution / 2.f))), 0, clr};
                    }
                    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, static_cast<unsigned int>(ceil(resolution)), vertices, sizeof(D3DVertex));
                };

                D3DCOLOR color = Instance().pmap_renderer.GetBackgroundColor();
                if (!Instance().circular_map) {
                    auto &style = ImGui::GetStyle();
                    RECT clipping;
                    clipping.left = static_cast<long>(cmd->ClipRect.x - style.WindowPadding.x / 2); // > 0 ? cmd->ClipRect.x : 0);
                    clipping.right = static_cast<long>(cmd->ClipRect.z + style.WindowPadding.x / 2 + 1);
                    clipping.top = static_cast<long>(cmd->ClipRect.y);
                    clipping.bottom = static_cast<long>(cmd->ClipRect.w);
                    device->SetScissorRect(&clipping);
                    device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
                    FillRect(color, Instance().location.x, Instance().location.y, Instance().size.x, Instance().size.y); // fill rect with chosen background color
                } else {
                    device->SetRenderState(D3DRS_STENCILENABLE, true); // enable stencil testing
                    device->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
                    device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff);
                    device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0); // clear depth and stencil buffer
                    device->SetRenderState(D3DRS_STENCILREF, 1);
                    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
                    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);                           // write ref value into stencil buffer when passed
                    float radius = static_cast<float>(Instance().size.x / 2.f);                                // rounding error in viewmatrix transformation
                    FillCircle(Instance().location.x + radius, Instance().location.y + radius, radius, color); // draw circle with chosen background color into stencil buffer, fills buffer with 1's

                    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL); // only draw where 1 is in the buffer
                    device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
                    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
                }

                Instance().RenderSetupProjection(device);

                D3DXMATRIX view;
                D3DXMATRIX identity;
                D3DXMatrixIdentity(&identity);
                device->SetTransform(D3DTS_WORLD, &identity);
                device->SetTransform(D3DTS_VIEW, &identity);

                D3DXMATRIX translate_char;
                D3DXMatrixTranslation(&translate_char, -me->pos.x, -me->pos.y, 0);

                D3DXMATRIX rotate_char;
                D3DXMatrixRotationZ(&rotate_char, -Instance().GetMapRotation() + static_cast<float>(M_PI_2));

                D3DXMATRIX scaleM, translationM;
                D3DXMatrixScaling(&scaleM, Instance().scale, Instance().scale, 1.0f);
                D3DXMatrixTranslation(&translationM, Instance().translation.x, Instance().translation.y, 0);

                float gwinch_scale = static_cast<float>(Instance().size.x) / 5000.0f / 2.f * Instance().scale;
                if (gwinch_scale != Instance().gwinch_scale.x) {
                    Instance().range_renderer.Invalidate();
                    Instance().gwinch_scale = {gwinch_scale, gwinch_scale};
                }

                view = translate_char * rotate_char * scaleM * translationM;
                device->SetTransform(D3DTS_VIEW, &view);

                Instance().pmap_renderer.Render(device);

                Instance().custom_renderer.Render(device);

                // move the rings to the char position
                D3DXMatrixTranslation(&translate_char, me->pos.x, me->pos.y, 0);
                device->SetTransform(D3DTS_WORLD, &translate_char);
                Instance().range_renderer.Render(device);
                device->SetTransform(D3DTS_WORLD, &identity);

                if (Instance().translation.x != 0 || Instance().translation.y != 0) {
                    D3DXMATRIX view2 = scaleM;
                    device->SetTransform(D3DTS_VIEW, &view2);
                    Instance().range_renderer.SetDrawCenter(true);
                    Instance().range_renderer.Render(device);
                    Instance().range_renderer.SetDrawCenter(false);
                    device->SetTransform(D3DTS_VIEW, &view);
                }

                Instance().symbols_renderer.Render(device);

                device->SetTransform(D3DTS_WORLD, &identity);
                Instance().agent_renderer.Render(device);

                Instance().effect_renderer.Render(device);

                Instance().pingslines_renderer.Render(device);

                if (Instance().circular_map) {
                    device->SetRenderState(D3DRS_STENCILREF, 0);
                    device->SetRenderState(D3DRS_STENCILWRITEMASK, 0x00000000);
                    device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
                    device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
                    device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
                    device->SetRenderState(D3DRS_STENCILENABLE, false);
                }

                // Restore the DX9 state
                d3d9_state_block->Apply();
                d3d9_state_block->Release();
            },
            static_cast<void *>(device));
    }
    ImGui::End();
    ImGui::PopStyleColor();

    if (hero_flag_controls_show && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        // @Cleanup: Maybe not lambda, but realy adapted in this case.
        auto GetPlayerParty = []() -> GW::PartyInfo * {
            GW::GameContext *gamectx = GW::GameContext::instance();
            if (!(gamectx && gamectx->party))
                return nullptr;
            return gamectx->party->player_party;
        };

        const auto playerparty = GetPlayerParty();
        GetPlayerHeroes(playerparty, player_heroes);
        bool player_has_henchmans = false;
        if (playerparty && playerparty->henchmen.size() && GW::PartyMgr::GetPlayerIsLeader())
            player_has_henchmans = true;

        if (playerparty && (player_has_henchmans || !player_heroes.empty())) {
            if (hero_flag_window_attach) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(location.x), static_cast<float>(location.y + size.y)));
                ImGui::SetNextWindowSize(ImVec2(static_cast<float>(size.x), 40.0f));
            }
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(hero_flag_window_background).Value);
            if (ImGui::Begin("Hero Controls", nullptr, GetWinFlags(hero_flag_window_attach ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove : 0, false))) {
                static const char *flag_txt[] = {"All", "1", "2", "3", "4", "5", "6", "7", "8"};
                GW::Vec3f allflag = GW::GameContext::instance()->world->all_flag;
                const unsigned int num_heroflags = player_heroes.size() + 1;
                const float w_but = (ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemSpacing.x * (num_heroflags)) / (num_heroflags + 1);

                for (unsigned int i = 0; i < num_heroflags; ++i) {
                    if (i > 0)
                        ImGui::SameLine();
                    const bool old_flagging = flagging[i];
                    if (old_flagging)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

                    if (ImGui::Button(flag_txt[i], ImVec2(w_but, 0))) {
                        flagging[i] ^= 1;
                    }
                    if (old_flagging)
                        ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (i == 0)
                            GW::PartyMgr::UnflagAll();
                        else
                            GW::PartyMgr::UnflagHero(i);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear", ImVec2(-1, 0))) {
                    GW::PartyMgr::UnflagAll();
                    for (unsigned int i = 1; i < num_heroflags; ++i) {
                        GW::PartyMgr::UnflagHero(i);
                    }
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }
    }
}

GW::Vec2f Minimap::InterfaceToWorldPoint(Vec2i pos) const
{
    GW::Agent *me = GW::Agents::GetPlayer();
    if (me == nullptr)
        return GW::Vec2f(0, 0);

    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert viewport projection
    v.x = v.x - location.x;
    v.y = location.y - v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = (2.0f * v.x / size.x - 1.0f);
    v.y = (2.0f * v.y / size.x + 1.0f);

    // scale up to [-w, w]
    const float w = 5000.0f;
    v *= w;

    // translate by camera
    v -= translation;

    // scale by camera
    v /= scale;

    // rotate by current camera rotation
    const float angle = Instance().GetMapRotation() - static_cast<float>(M_PI_2);
    const float x1 = v.x * std::cos(angle) - v.y * std::sin(angle);
    const float y1 = v.x * std::sin(angle) + v.y * std::cos(angle);
    v = GW::Vec2f(x1, y1);

    // translate by character position
    v += me->pos;

    return v;
}

GW::Vec2f Minimap::InterfaceToWorldVector(Vec2i pos) const
{
    GW::Vec2f v(static_cast<float>(pos.x), static_cast<float>(pos.y));

    // Invert y direction
    v.y = -v.y;

    // go from [0, width][0, height] to [-1, 1][-1, 1]
    v.x = (2.0f * v.x / size.x);
    v.y = (2.0f * v.y / size.x);

    // scale up to [-w, w]
    const float w = 5000.0f;
    v *= w;

    return v;
}

void Minimap::SelectTarget(GW::Vec2f pos)
{
    GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid())
        return;

    float distance = 600.0f * 600.0f;
    size_t closest = static_cast<size_t>(-1);

    for (size_t i = 0; i < agents.size(); ++i) {
        GW::Agent *agent = agents[i];
        if (agent == nullptr)
            continue;
        GW::AgentLiving *living = agent->GetAsAgentLiving();
        if (living && living->GetIsDead())
            continue;
        if (agent->GetIsItemType())
            continue;
        if (agent->GetIsGadgetType() && agent->GetAsAgentGadget()->gadget_id != 8141)
            continue; // allow locked chests
        if (living && (living->player_number >= 230 && living->player_number <= 346))
            continue; // block all useless minis
        const float newDistance = GW::GetSquareDistance(pos, agents[i]->pos);
        if (distance > newDistance) {
            distance = newDistance;
            closest = i;
        }
    }

    if (closest != static_cast<size_t>(-1)) {
        GW::Agents::ChangeTarget(agents[closest]);
    }
}

bool Minimap::WndProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (is_observing)
        return false;
    if (mouse_clickthrough)
        return Message == WM_LBUTTONDOWN && FlagHeros(lParam);
    if (mouse_clickthrough_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return false;
    switch (Message) {
        case WM_MOUSEMOVE:
            return OnMouseMove(Message, wParam, lParam);
        case WM_LBUTTONDOWN:
            return OnMouseDown(Message, wParam, lParam);
        case WM_MOUSEWHEEL:
            return OnMouseWheel(Message, wParam, lParam);
        case WM_LBUTTONDBLCLK:
            return OnMouseDblClick(Message, wParam, lParam);
        case WM_LBUTTONUP:
            return OnMouseUp(Message, wParam, lParam);
        default:
            return false;
    }
}
bool Minimap::FlagHeros(LPARAM lParam)
{
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;

    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    bool flagged = false;
    if (flagging[0]) {
        flagging[0] = false;
        GW::PartyMgr::FlagAll(GW::GamePos(worldpos));
        flagged = true;
    }
    for (size_t i = 1; i < 9; ++i) {
        if (flagging[i]) {
            flagging[i] = false;
            flagged = true;
            GW::PartyMgr::FlagHeroAgent(player_heroes[i - 1], GW::GamePos(worldpos));
        }
    }
    return flagged;
}
bool Minimap::OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    if (!IsActive())
        return false;
    if (FlagHeros(lParam))
        return true;
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;

    mousedown = true;
    const GW::Vec2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(worldpos);
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Walk)) {
        GW::Agents::Move(worldpos);
        pingslines_renderer.AddMouseClickPing(worldpos);
        return true;
    }

    drag_start.x = x;
    drag_start.y = y;

    if (IsKeyDown(MinimapModifierBehaviour::Move))
        return true;

    if (!lock_move)
        return true;

    pingslines_renderer.OnMouseDown(worldpos.x, worldpos.y);

    return true;
}

bool Minimap::OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    if (!IsActive())
        return false;

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    if (!IsInside(x, y))
        return false;

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    return true;
}

bool Minimap::OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    if (!mousedown)
        return false;

    mousedown = false;

    return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    if (!mousedown)
        return false;

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    // if (!IsInside(x, y)) return false;

    if (IsKeyDown(MinimapModifierBehaviour::Target)) {
        SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
        return true;
    }

    if (IsKeyDown(MinimapModifierBehaviour::Move)) {
        const Vec2i diff = Vec2i(x - drag_start.x, y - drag_start.y);
        translation += InterfaceToWorldVector(diff);
        drag_start = Vec2i(x, y);
        last_moved = TIMER_INIT();
        return true;
    }

    const GW::Vec2f v = InterfaceToWorldPoint(Vec2i(x, y));
    return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Message);
    UNREFERENCED_PARAMETER(lParam);
    if (!IsActive())
        return false;

    // Mouse wheel x and y are in absolute coords, not window coords! (Windows why...)
    const ImVec2 mouse = ImGui::GetMousePos();
    if (!IsInside(static_cast<int>(mouse.x), static_cast<int>(mouse.y)))
        return false;

    const int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

    if (IsKeyDown(MinimapModifierBehaviour::Move)) {
        const float delta = zDelta > 0 ? 1.024f : 0.9765625f;
        scale *= delta;
        return true;
    }

    return false;
}

bool Minimap::IsInside(int x, int y) const
{
    // if outside square, return false
    if (x < location.x)
        return false;
    if (x > location.x + size.x)
        return false;
    if (y < location.y)
        return false;
    if (y > location.y + size.y)
        return false;

    // if centered, use radar range
    if (translation.x == 0 && translation.y == 0) {
        const GW::Vec2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
        GW::Agent *me = GW::Agents::GetPlayer();
        if (!me)
            return false;
        const float sqrdst = GW::GetSquareDistance(me->pos, gamepos);
        return sqrdst < GW::Constants::SqrRange::Compass;
    }
    return true;
}
bool Minimap::IsActive() const
{
    return visible && !loading && GW::Map::GetIsMapLoaded() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetPlayerId() != 0;
}

void Minimap::RenderSetupProjection(IDirect3DDevice9 *device) const
{
    D3DVIEWPORT9 viewport;
    device->GetViewport(&viewport);

    const float w = 5000.0f * 2;
    // IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
    const D3DXMATRIX ortho_matrix(2 / w, 0, 0, 0, 0, 2 / w, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    //// note: manually craft the projection to viewport instead of using
    //// SetViewport to allow target regions outside the viewport
    //// e.g. negative x/y for slightly offscreen map
    const float xscale = static_cast<float>(size.x) / viewport.Width;
    const float yscale = static_cast<float>(size.x) / viewport.Height;
    const float xtrans = static_cast<float>(location.x * 2 + size.x) / viewport.Width - 1.0f;
    const float ytrans = -static_cast<float>(location.y * 2 + size.x) / viewport.Height + 1.0f;
    ////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
    const D3DXMATRIX viewport_matrix(xscale, 0, 0, 0, 0, yscale, 0, 0, 0, 0, 1, 0, xtrans, ytrans, 0, 1);

    D3DXMATRIX proj = ortho_matrix * viewport_matrix;

    device->SetTransform(D3DTS_PROJECTION, &proj);
}

bool Minimap::IsKeyDown(const MinimapModifierBehaviour mmb) const
{
    return (key_none_behavior == mmb and not ImGui::IsKeyDown(VK_CONTROL) and not ImGui::IsKeyDown(VK_SHIFT) and not ImGui::IsKeyDown(VK_MENU)) or (key_ctrl_behavior == mmb and ImGui::IsKeyDown(VK_CONTROL)) or (key_shift_behavior == mmb and ImGui::IsKeyDown(VK_SHIFT)) or
           (key_alt_behavior == mmb and ImGui::IsKeyDown(VK_MENU));
}
