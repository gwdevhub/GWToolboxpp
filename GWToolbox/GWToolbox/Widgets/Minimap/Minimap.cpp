#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9math.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuiAddons.h>

#include <GuiUtils.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\CameraMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>

#include "logger.h"
#include "Modules\ToolboxSettings.h"

void Minimap::Initialize() {
	ToolboxWidget::Initialize();
	GW::StoC::AddCallback<GW::Packet::StoC::AgentPinged>(
		[&](GW::Packet::StoC::AgentPinged *pak) -> bool {
		if (visible) {
			pingslines_renderer.P046Callback(pak);
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::CompassEvent>(
		[&](GW::Packet::StoC::CompassEvent *pak) -> bool {
		if (visible) {
			pingslines_renderer.P138Callback(pak);
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::GenericValueTarget>(
		[&](GW::Packet::StoC::GenericValueTarget *pak) -> bool {
		if (visible) {
			pingslines_renderer.P153Callback(pak);
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::SkillActivate>(
		[&](GW::Packet::StoC::SkillActivate *pak) -> bool {
		if (visible) {
			pingslines_renderer.P221Callback(pak);
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::InstanceLoadFile>(
		[this](GW::Packet::StoC::InstanceLoadFile *packet) -> bool {
		pmap_renderer.Invalidate();
		loading = false;
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::GameSrvTransfer>(
		[&](GW::Packet::StoC::GameSrvTransfer *pak) -> bool {
		loading = true;
		return false;
	});

	last_moved = TIMER_INIT();

	pmap_renderer.Invalidate();

	GW::Chat::CreateCommand(L"flag",
		[this](const wchar_t *message, int argc, LPWSTR *argv) {
		if (argc <= 1) {
			FlagHero(0);
		} else {
			std::wstring arg0 = GuiUtils::ToLower(argv[1]);
			// partially laziness, and partially safety
			if (arg0 == L"clear") {
				auto heroarray = GW::GameContext::instance()->party->playerparty->heroes;
				unsigned int num_heroflags = 9;
				if (heroarray.valid()) num_heroflags = heroarray.size() + 1;
				GW::PartyMgr::UnflagAll();
				for (unsigned int i = 1; i < num_heroflags; ++i) {
					GW::PartyMgr::UnflagHero(i);
				}
			} else if (arg0 == L"all") {
				FlagHero(0);
			} else if (arg0 == L"1") {
				FlagHero(1);
			} else if (arg0 == L"2") {
				FlagHero(2);
			} else if (arg0 == L"3") {
				FlagHero(3);
			} else if (arg0 == L"4") {
				FlagHero(4);
			} else if (arg0 == L"5") {
				FlagHero(5);
			} else if (arg0 == L"6") {
				FlagHero(6);
			} else if (arg0 == L"7") {
				FlagHero(7);
			}
		}
		return true;
	});
}

void Minimap::DrawSettings() {
	if (ImGui::CollapsingHeader(Name(), ImGuiTreeNodeFlags_AllowItemOverlap)) {
		ImGui::PushID(Name());
		ShowVisibleRadio();
		ImVec2 pos(0, 0);
		ImVec2 size(0, 0);
		if (ImGuiWindow* window = ImGui::FindWindowByName(Name())) {
			pos = window->Pos;
			size = window->Size;
		}
		if (ImGui::DragFloat2("Position", (float*)&pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowPos(Name(), pos);
		}
		ImGui::ShowHelp("You need to show the window for this control to work");
		if (ImGui::DragFloat2("Size", (float*)&size, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowSize(Name(), size);
		}
		ImGui::ShowHelp("You need to show the window for this control to work");
		ImGui::Checkbox("Lock Position", &lock_move);
		ImGui::SameLine();
		ImGui::Checkbox("Lock Size", &lock_size);
		DrawSettingInternal();
		ImGui::PopID();
	} else {
		ShowVisibleRadio();
	}
}

void Minimap::DrawSettingInternal() {
	ImGui::Text("General");
	ImGui::DragFloat("Scale", &scale, 0.01f, 0.1f);
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
		Colors::DrawSetting("Background", &hero_flag_window_background);
		ImGui::TreePop();
	}
    ImGui::Checkbox("Allow mouse click-through", &mouse_clickthrough);
    ImGui::ShowHelp("Toolbox minimap will not capture mouse events");
    if (mouse_clickthrough) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    ImGui::Checkbox("Alt + Click on minimap to move", &alt_click_to_move);
    if (mouse_clickthrough) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
    ImGui::Checkbox("Reduce agent ping spam", &pingslines_renderer.reduce_ping_spam);
    ImGui::ShowHelp("Additional pings on the same agents will increase the duration of the existing ping, rather than create a new one.");
}

void Minimap::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	scale = (float)ini->GetDoubleValue(Name(), VAR_NAME(scale), 1.0);
	hero_flag_controls_show = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), true);
	hero_flag_window_attach = ini->GetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), true);
	hero_flag_window_background = Colors::Load(ini, Name(), "hero_flag_controls_background",
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
    mouse_clickthrough = ini->GetBoolValue(Name(), VAR_NAME(mouse_clickthrough), false);
    alt_click_to_move = ini->GetBoolValue(Name(), VAR_NAME(alt_click_to_move), false);
    pingslines_renderer.reduce_ping_spam = ini->GetBoolValue(Name(), VAR_NAME(reduce_ping_spam), false);
	range_renderer.LoadSettings(ini, Name());
	pmap_renderer.LoadSettings(ini, Name());
	agent_renderer.LoadSettings(ini, Name());
	pingslines_renderer.LoadSettings(ini, Name());
	symbols_renderer.LoadSettings(ini, Name());
	custom_renderer.LoadSettings(ini, Name());
}

void Minimap::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetDoubleValue(Name(), VAR_NAME(scale), scale);
	ini->SetBoolValue(Name(), VAR_NAME(hero_flag_controls_show), hero_flag_controls_show);
	ini->SetBoolValue(Name(), VAR_NAME(hero_flag_window_attach), hero_flag_window_attach);
	Colors::Save(ini, Name(), VAR_NAME(hero_flag_window_background), hero_flag_window_background);
    ini->SetBoolValue(Name(), VAR_NAME(mouse_clickthrough), mouse_clickthrough);
    ini->SetBoolValue(Name(), VAR_NAME(alt_click_to_move), alt_click_to_move);
    ini->SetBoolValue(Name(), VAR_NAME(reduce_ping_spam), pingslines_renderer.reduce_ping_spam);
	range_renderer.SaveSettings(ini, Name());
	pmap_renderer.SaveSettings(ini, Name());
	agent_renderer.SaveSettings(ini, Name());
	pingslines_renderer.SaveSettings(ini, Name());
	symbols_renderer.SaveSettings(ini, Name());
	custom_renderer.SaveSettings(ini, Name());
}

void Minimap::GetPlayerHeroes(GW::PartyInfo *party, std::vector<GW::AgentID>& player_heroes) {
	player_heroes.clear();
	if (!party) return;
	GW::Agent *player = GW::Agents::GetPlayer();
	if (!player) return;
	GW::PlayerID player_id = player->LoginNumber;
	auto heroes = party->heroes;
	player_heroes.reserve(heroes.size());
	for (GW::HeroPartyMember &hero : heroes) {
		if (hero.ownerplayerid == player_id)
			player_heroes.push_back(hero.agentid);
	}
}

void Minimap::Draw(IDirect3DDevice9* device) {
	if (!IsActive()) return;

	GW::Agent* me = GW::Agents::GetPlayer();
	if (me == nullptr) return;

	// if not center and want to move, move center towards player
	if ((translation.x != 0 || translation.y != 0)
		&& (me->MoveX != 0 || me->MoveY != 0)
		&& TIMER_DIFF(last_moved) > ms_before_back) {
		GW::Vector2f v(translation.x, translation.y);
		float speed = std::min((TIMER_DIFF(last_moved) - ms_before_back) * acceleration, 500.0f);
		float n = v.Norm();
		GW::Vector2f d = v.Normalized() * speed;
		if (std::abs(d.x) > std::abs(v.x)) {
			translation = GW::Vector2f(0, 0);
		} else {
			translation -= d;
		}
	}

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(500.0f, 500.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))) {
		// window pos are already rounded by imgui, so casting is no big deal
		location.x = (int)ImGui::GetWindowPos().x;
		location.y = (int)ImGui::GetWindowPos().y;
		size.x = (int)ImGui::GetWindowSize().x;
		size.y = (int)ImGui::GetWindowSize().y;
		ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) -> void {

			IDirect3DDevice9* device = (IDirect3DDevice9*)cmd->UserCallbackData;
			GW::Agent* me = GW::Agents::GetPlayer();
			if (me == nullptr) return;

			// Backup the DX9 state
			IDirect3DStateBlock9* d3d9_state_block = NULL;
			if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
				return;

			// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
			device->SetFVF(D3DFVF_CUSTOMVERTEX);
			device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->SetPixelShader(NULL);
			device->SetVertexShader(NULL);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_LIGHTING, false);
			device->SetRenderState(D3DRS_ZENABLE, false);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
			device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

			ImGuiStyle& style = ImGui::GetStyle();
			RECT old_clipping, clipping;
			device->GetScissorRect(&old_clipping);
			clipping.left = (long)(cmd->ClipRect.x - style.WindowPadding.x / 2); // > 0 ? cmd->ClipRect.x : 0);
			clipping.right = (long)(cmd->ClipRect.z + style.WindowPadding.x / 2 + 1);
			clipping.top = (long)(cmd->ClipRect.y);
			clipping.bottom = (long)(cmd->ClipRect.w);
			device->SetScissorRect(&clipping);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
			Instance().RenderSetupProjection(device);

			D3DXMATRIX view;
			D3DXMATRIX identity;
			D3DXMatrixIdentity(&identity);
			device->SetTransform(D3DTS_WORLD, &identity);
			device->SetTransform(D3DTS_VIEW, &identity);

			D3DXMATRIX translate_char;
			D3DXMatrixTranslation(&translate_char, -me->pos.x, -me->pos.y, 0);

			D3DXMATRIX rotate_char;
			D3DXMatrixRotationZ(&rotate_char, -GW::CameraMgr::GetYaw() + (float)M_PI_2);

			D3DXMATRIX scaleM, translationM;
			D3DXMatrixScaling(&scaleM, Instance().scale, Instance().scale, 1.0f);
			D3DXMatrixTranslation(&translationM, Instance().translation.x, Instance().translation.y, 0);

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

			Instance().pingslines_renderer.Render(device);

			// Restore the DX9 state
			d3d9_state_block->Apply();
			d3d9_state_block->Release();

		}, (void*)device);
	}
	ImGui::End();
	ImGui::PopStyleColor();

	if (hero_flag_controls_show && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
		// @Cleanup: Maybe not lambda, but realy adapted in this case.
		auto GetPlayerParty = [] () -> GW::PartyInfo* {
			GW::GameContext *gamectx = GW::GameContext::instance();
			if (!(gamectx && gamectx->party)) return nullptr;
			return gamectx->party->playerparty;
		};

		auto playerparty = GetPlayerParty();
		GetPlayerHeroes(playerparty, player_heroes);
		bool player_has_henchmans = false;
		if (playerparty && playerparty->henchmen.size() && GW::PartyMgr::GetPlayerIsLeader())
			player_has_henchmans = true;

		if (playerparty && (player_has_henchmans || player_heroes.size())) {
			if (hero_flag_window_attach) {
				ImGui::SetNextWindowPos(ImVec2((float)location.x, (float)(location.y + size.y)));
				ImGui::SetNextWindowSize(ImVec2((float)size.x, 40.0f));
			}
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(hero_flag_window_background).Value);
			if (ImGui::Begin("Hero Controls", nullptr, GetWinFlags(
				hero_flag_window_attach ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove : 0, false))) {
				static const char* flag_txt[] = {
					"All", "1", "2", "3", "4", "5", "6", "7", "8"
				};
				GW::Vector3f allflag = GW::GameContext::instance()->world->all_flag;
				GW::HeroFlagArray& flags = GW::GameContext::instance()->world->hero_flags;
				unsigned int num_heroflags = player_heroes.size() + 1;
				float w_but = (ImGui::GetWindowContentRegionWidth() 
					- ImGui::GetStyle().ItemSpacing.x * (num_heroflags)) / (num_heroflags + 1);

				for (unsigned int i = 0; i < num_heroflags; ++i) {
					if (i > 0) ImGui::SameLine();
					bool old_flagging = flagging[i];
					if (old_flagging) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);

					bool flagged = (i == 0) ?
						(!std::isinf(allflag.x) || !std::isinf(allflag.y)) :
						(flags.valid() && i - 1 < flags.size() && (!std::isinf(flags[i - 1].flag.x) || !std::isinf(flags[i - 1].flag.y)));

					if (ImGui::Button(flag_txt[i], ImVec2(w_but, 0))) {
						flagging[i] ^= 1;
					}
					if (old_flagging) ImGui::PopStyleColor();

					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
						if (i == 0) GW::PartyMgr::UnflagAll();
						else GW::PartyMgr::UnflagHero(i);
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

GW::Vector2f Minimap::InterfaceToWorldPoint(Vec2i pos) const {
	GW::Agent* me = GW::Agents::GetPlayer();
	if (me == nullptr) return GW::Vector2f(0, 0);
	
	GW::Vector2f v((float)pos.x, (float)pos.y);

	// Invert viewport projection
	v.x = v.x - location.x;
	v.y = location.y - v.y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	v.x = (2.0f * v.x / size.x - 1.0f);
	v.y = (2.0f * v.y / size.x + 1.0f);

	// scale up to [-w, w]
	float w = 5000.0f;
	v *= w;

	// translate by camera
	v -= translation;

	// scale by camera
	v /= scale;

	// rotate by current camera rotation
	float angle = GW::CameraMgr::GetYaw() - (float)M_PI_2;
	float x1 = v.x * std::cos(angle) - v.y * std::sin(angle);
	float y1 = v.x * std::sin(angle) + v.y * std::cos(angle);
	v = GW::Vector2f(x1, y1);

	// translate by character position
	v += me->pos;

	return v;
}

GW::Vector2f Minimap::InterfaceToWorldVector(Vec2i pos) const {
	GW::Vector2f v((float)pos.x, (float)pos.y);

	// Invert y direction
	v.y = -v.y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	v.x = (2.0f * v.x / size.x);
	v.y = (2.0f * v.y / size.x);

	// scale up to [-w, w]
	float w = 5000.0f;
	v *= w;

	return v;
}

void Minimap::SelectTarget(GW::Vector2f pos) {
	GW::AgentArray agents = GW::Agents::GetAgentArray();
	if (!agents.valid()) return;

	float distance = 600.0f * 600.0f;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->GetIsCharacterType() && agent->GetIsDead()) continue;
		if (agent->GetIsItemType()) continue;
		if (agent->GetIsGadgetType() && agent->ExtraType != 8141) continue; // allow locked chests
		if (agent->PlayerNumber >= 230 && agent->PlayerNumber <= 346) continue; // block all useless minis
		float newDistance = GW::Agents::GetSqrDistance(pos, agents[i]->pos);
		if (distance > newDistance) {
			distance = newDistance;
			closest = i;
		}
	}

	if (closest > 0) {
		GW::Agents::ChangeTarget(agents[closest]);
	}
}

bool Minimap::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
    if (mouse_clickthrough) return false;
	switch (Message) {
	case WM_MOUSEMOVE: return OnMouseMove(Message, wParam, lParam);
	case WM_LBUTTONDOWN: return OnMouseDown(Message, wParam, lParam);
	case WM_MOUSEWHEEL: return OnMouseWheel(Message, wParam, lParam);
	case WM_LBUTTONDBLCLK: return OnMouseDblClick(Message, wParam, lParam);
	case WM_LBUTTONUP: return OnMouseUp(Message, wParam, lParam);
	default:
		return false;
	}
}
bool Minimap::OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (!IsInside(x, y)) return false;

	mousedown = true;
    GW::Vector2f worldpos = InterfaceToWorldPoint(Vec2i(x, y));

	if (wParam & MK_CONTROL) {
		SelectTarget(worldpos);
		return true;
	}

    if (alt_click_to_move && ImGui::IsKeyDown(VK_MENU)) {
        GW::Agents::Move(worldpos);
        pingslines_renderer.AddMouseClickPing(worldpos);
        return true;
    }

	bool flagged = false;
	if (flagging[0]) {
		flagging[0] = false;
		GW::PartyMgr::FlagAll(GW::GamePos(worldpos));
		flagged = true;
	}
	for (int i = 1; i < 9; ++i) {
		if (flagging[i]) {
			flagging[i] = false;
			flagged = true;
			GW::PartyMgr::FlagHeroAgent(player_heroes[i-1], GW::GamePos(worldpos));
		}
	}
	if (flagged) return true;

	drag_start.x = x;
	drag_start.y = y;

	if (wParam & MK_SHIFT) return true;

	if (!lock_move) return true;

	pingslines_renderer.OnMouseDown(worldpos.x, worldpos.y);

	return true;
}

bool Minimap::OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (!IsInside(x, y)) return false;

	if (wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
		return true;
	}

	return true;
}

bool Minimap::OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	if (!mousedown) return false;

	mousedown = false;

	return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	if (!mousedown) return false;
	
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	//if (!IsInside(x, y)) return false;

	if (wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
		return true;
	}

	if (wParam & MK_SHIFT) {
		Vec2i diff = Vec2i(x - drag_start.x, y - drag_start.y);
		translation += InterfaceToWorldVector(diff);
		drag_start = Vec2i(x, y);
		last_moved = TIMER_INIT();
		return true;
	}

	GW::Vector2f v = InterfaceToWorldPoint(Vec2i(x, y));
	return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

    // Mouse wheel x and y are in absolute coords, not window coords! (Windows why...)
    const ImVec2 mouse = ImGui::GetMousePos();
    if (!IsInside((int)mouse.x, (int)mouse.y)) return false;
	
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

	if (wParam & MK_SHIFT) {
		float delta = zDelta > 0 ? 1.024f : 0.9765625f;
		scale *= delta;
		return true;
	}

	return false;
}

bool Minimap::IsInside(int x, int y) const {
	// if outside square, return false
	if (x < location.x) return false;
	if (x > location.x + size.x) return false;
	if (y < location.y) return false;
	if (y > location.y + size.y) return false;

	// if centered, use radar range
	if (translation.x == 0 && translation.y == 0) {
		GW::Vector2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
		GW::Agent* me = GW::Agents::GetPlayer();
		float sqrdst = GW::Agents::GetSqrDistance(me->pos, gamepos);
		return me && sqrdst < GW::Constants::SqrRange::Compass;
	}
	return true;
}
bool Minimap::IsActive() const {
	return visible
		&& !loading
		&& GW::Map::IsMapLoaded()
		&& GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents::GetPlayerId() != 0;
}

void Minimap::RenderSetupProjection(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	float w = 5000.0f * 2;
	// IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
	D3DXMATRIX ortho_matrix(
		2 / w, 0, 0, 0,
		0, 2 / w, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	//// note: manually craft the projection to viewport instead of using
	//// SetViewport to allow target regions outside the viewport 
	//// e.g. negative x/y for slightly offscreen map
	float xscale = (float)size.x / viewport.Width;
	float yscale = (float)size.x / viewport.Height;
	float xtrans = (float)(location.x * 2 + size.x) / viewport.Width - 1.0f;
	float ytrans = -(float)(location.y * 2 + size.x) / viewport.Height + 1.0f;
	////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
	D3DXMATRIX viewport_matrix(
		xscale, 0, 0, 0,
		0, yscale, 0, 0,
		0, 0, 1, 0,
		xtrans, ytrans, 0, 1
	);

	D3DXMATRIX proj = ortho_matrix * viewport_matrix;

	device->SetTransform(D3DTS_PROJECTION, &proj);
}
