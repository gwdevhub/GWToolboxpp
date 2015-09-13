#include "BondsWindow.h"

#include <sstream>
#include <string>

#include "GWToolbox.h"
#include "Config.h"
#include "../API/APIMain.h"
#include "GuiUtils.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "../include/OSHGui/Misc/Intersection.hpp"

using namespace OSHGui;

BondsWindow::BondsWindow() {

	Config* config = GWToolbox::instance()->config();
	int x = config->iniReadLong(BondsWindow::IniSection(), BondsWindow::IniKeyX(), 400);
	int y = config->iniReadLong(BondsWindow::IniSection(), BondsWindow::IniKeyY(), 100);

	SetLocation(x, y);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	SetEnabled(false);

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(BondsWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	monitor = new BondsMonitor();
	monitor->SetLocation(0, 0);
	monitor->SetSize(WIDTH, HEIGHT);
	AddControl(monitor);

	std::shared_ptr<BondsWindow> self = std::shared_ptr<BondsWindow>(this);
	Form::Show(self);
}

BondsWindow::BondsMonitor::BondsMonitor() {
	isFocusable_ = true;
	SetEnabled(true);
	hovered_player = -1;
	hovered_bond = -1;
	pressed = false;

	for (int i = 0; i < party_size; ++i) {
		for (int j = 0; j < n_bonds; ++j) {
			buff_id[i][j] = 0;
			pics[i][j] = new PictureBox();
			pics[i][j]->SetLocation(j * IMG_SIZE, i *IMG_SIZE);
			pics[i][j]->SetSize(IMG_SIZE, IMG_SIZE);
			pics[i][j]->SetStretch(false);
			pics[i][j]->SetVisible(false);
			pics[i][j]->SetEnabled(false);
			AddSubControl(pics[i][j]);
		}
	}
	for (int i = 0; i < party_size; ++i) {
		pics[i][0]->SetImage(Image::FromFile(GuiUtils::getSubPathA("balthspirit.jpg", "img")));
		pics[i][1]->SetImage(Image::FromFile(GuiUtils::getSubPathA("lifebond.jpg", "img")));
		pics[i][2]->SetImage(Image::FromFile(GuiUtils::getSubPathA("protbond.jpg", "img")));
	}

	GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
}

void BondsWindow::BondsMonitor::DrawSelf(Drawing::RenderContext &context) {
	for (int i = 0; i < party_size; ++i) {
		for (int j = 0; j < n_bonds; ++j) {
			if (pics[i][j]->GetVisible()) {
				pics[i][j]->Render();
			}
		}
	}
	Control::DrawSelf(context);
}

void BondsWindow::BondsMonitor::PopulateGeometry() {
	if (hovered_player != -1 && hovered_bond != -1) {
		Graphics g(*geometry_);
		Drawing::Color c = GetForeColor();
		if (pressed) c = c - Drawing::Color::FromARGB(50, 50, 50, 50);
		g.DrawRectangle(c, (float)IMG_SIZE * hovered_bond, (float)IMG_SIZE * hovered_player, 
			(float)IMG_SIZE, (float)IMG_SIZE);
	}
}

int BondsWindow::BondsMonitor::GetBond(int xcoord) {
	return (xcoord - absoluteLocation_.X) / IMG_SIZE;
}

int BondsWindow::BondsMonitor::GetPlayer(int ycoord) {
	return (ycoord - absoluteLocation_.Y) / IMG_SIZE;
}

void BondsWindow::BondsMonitor::OnMouseDown(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseDown(mouse);
	pressed = true;
	Invalidate();
}
void BondsWindow::BondsMonitor::OnMouseMove(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseMove(mouse);
	int player = GetPlayer(mouse.GetLocation().Y);
	int bond = GetBond(mouse.GetLocation().X);
	if (hovered_player != player || hovered_bond != bond) {
		hovered_player = player;
		hovered_bond = bond;
		Invalidate();
	}
	
}
void BondsWindow::BondsMonitor::OnMouseUp(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseUp(mouse);
	pressed = false;
	Invalidate();
}
void BondsWindow::BondsMonitor::OnMouseLeave(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseLeave(mouse);
	hovered_player = -1;
	hovered_bond = -1;
	Invalidate();
}
void BondsWindow::BondsMonitor::OnMouseClick(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseClick(mouse);
	int player = GetPlayer(mouse.GetLocation().Y);
	int bond = GetBond(mouse.GetLocation().X);
	
	if (player >= 0 && player < party_size
		&& bond >= 0 && bond < n_bonds
		&& pics[player][bond]->GetVisible()) {

		GWAPIMgr::GetInstance()->Effects->DropBuff(buff_id[player][bond]);
	}
}

void BondsWindow::BondsMonitor::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(BondsWindow::IniSection(), BondsWindow::IniKeyX(), x);
	config->iniWriteLong(BondsWindow::IniSection(), BondsWindow::IniKeyY(), y);
}

void BondsWindow::BondsMonitor::UpdateUI() {
	using namespace GW;

	if (!isVisible_) return;

	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();

	AgentEffectsArray effects = api->Effects->GetPartyEffectArray();
	if (!effects.IsValid()) return;

	BuffArray buffs = effects[0].Buffs;
	if (!buffs.IsValid()) return;

	AgentArray agents = api->Agents->GetAgentArray();
	if (!agents.IsValid()) return;

	bool show[party_size][n_bonds];
	for (int i = 0; i < party_size; ++i) {
		for (int j = 0; j < n_bonds; ++j) {
			show[i][j] = false;
		}
	}
	for (size_t i = 0; i < buffs.size(); ++i) {
		int player = -1;
		DWORD target_id = buffs[i].TargetAgentId;
		if (target_id < agents.size() && agents[target_id]) {
			player = agents[target_id]->PlayerNumber;
			if (player == 0 || player > party_size) continue;
			--player;	// player numbers are from 1 to 8 in party list
		}

		int bond = -1;
		switch (static_cast<GwConstants::SkillID>(buffs[i].SkillId)) {
		case GwConstants::SkillID::Balthazars_Spirit: bond = 0; break;
		case GwConstants::SkillID::Life_Bond: bond = 1; break;
		case GwConstants::SkillID::Protective_Bond: bond = 2; break;
		default: break;
		}
		if (bond == -1) continue;
		
		show[player][bond] = true;
		buff_id[player][bond] = buffs[i].BuffId;
	}

	for (int i = 0; i < party_size; ++i) {
		for (int j = 0; j < n_bonds; ++j) {
			if (pics[i][j]->GetVisible() != show[i][j]) {
				pics[i][j]->SetVisible(show[i][j]);
			}
		}
	}
}

void BondsWindow::Show(bool show) {
	SetVisible(show);
	containerPanel_->SetVisible(show);
	monitor->SetVisible(show);
}

