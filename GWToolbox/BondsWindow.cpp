#include "BondsWindow.h"

#include <sstream>
#include <string>

#include "OSHGui.hpp"

#include "GWToolbox.h"
#include "Config.h"
#include "../API/APIMain.h"
#include "GuiUtils.h"


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

	bool show = config->iniReadBool(BondsWindow::IniSection(), BondsWindow::IniKeyShow(), false);
	Show(show);

	std::shared_ptr<BondsWindow> self = std::shared_ptr<BondsWindow>(this);
	Form::Show(self);
}

BondsWindow::BondsMonitor::BondsMonitor() {
	isFocusable_ = true;
	SetEnabled(true);
	hovered_player = -1;
	hovered_bond = -1;
	party_size = n_players; // initialize at max, upcate will take care of shrinking as needed.
	pressed = false;
	freezed = GWToolbox::instance()->config()->iniReadBool(MainWindow::IniSection(),
		MainWindow::IniKeyFreeze(), false);

	for (int i = 0; i < n_players; ++i) {
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
	for (int i = 0; i < n_players; ++i) {
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
	if (!freezed) DragButton::OnMouseDown(mouse);
	pressed = true;
	Invalidate();
}
void BondsWindow::BondsMonitor::OnMouseMove(const OSHGui::MouseMessage &mouse) {
	if (!freezed) DragButton::OnMouseMove(mouse);
	int player = GetPlayer(mouse.GetLocation().Y);
	int bond = GetBond(mouse.GetLocation().X);
	if (hovered_player != player || hovered_bond != bond) {
		hovered_player = player;
		hovered_bond = bond;
		pressed = false;
		Invalidate();
	}
}
void BondsWindow::BondsMonitor::OnMouseUp(const OSHGui::MouseMessage &mouse) {
	if (!freezed) DragButton::OnMouseUp(mouse);
	if (pressed) {
		int player = GetPlayer(mouse.GetLocation().Y);
		int bond = GetBond(mouse.GetLocation().X);

		if (player >= 0 && player < n_players
			&& bond >= 0 && bond < n_bonds) {
			
			DropUseBuff(bond, player);
		}
	}

	pressed = false;
	Invalidate();
}

void BondsWindow::BondsMonitor::DropUseBuff(int bond, int player) {
	GWAPIMgr* api = GWAPIMgr::instance();
	if (pics[player][bond]->GetVisible()) {
		if (buff_id[player][bond] > 0) {
			api->Effects()->DropBuff(buff_id[player][bond]);
		}
	} else {
		// cast bond on player
		GwConstants::SkillID buff;
		switch (static_cast<Bond>(bond)) {
		case Bond::Balth: buff = GwConstants::SkillID::Balthazars_Spirit; break;
		case Bond::Life: buff = GwConstants::SkillID::Life_Bond; break;
		case Bond::Prot: buff = GwConstants::SkillID::Protective_Bond; break;
		}

		int target = api->Agents()->GetAgentIdByLoginNumber(player + 1);
		if (target <= 0) return;

		int slot = api->Skillbar()->getSkillSlot(buff);
		if (slot <= 0) return;
		if (api->Skillbar()->GetPlayerSkillbar().Skills[slot].Recharge != 0) return;

		api->Skillbar()->UseSkill(slot, target);
	}
}

void BondsWindow::BondsMonitor::OnMouseLeave(const OSHGui::MouseMessage &mouse) {
	DragButton::OnMouseLeave(mouse);
	hovered_player = -1;
	hovered_bond = -1;
	Invalidate();
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

	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::instance();

	int size = api->Agents()->GetPartySize();
	if (size > n_players) size = n_players;
	if (party_size != size) {
		party_size = size;
		SetSize(n_bonds * IMG_SIZE, party_size * IMG_SIZE);
		parent_->SetSize(GetSize());
	}

	bool show[n_players][n_bonds];
	for (int i = 0; i < n_players; ++i) {
		for (int j = 0; j < n_bonds; ++j) {
			show[i][j] = false;
		}
	}

	AgentEffectsArray effects = api->Effects()->GetPartyEffectArray();
	if (effects.valid()) {
		BuffArray buffs = effects[0].Buffs;
		AgentArray agents = api->Agents()->GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				int player = -1;
				DWORD target_id = buffs[i].TargetAgentId;
				if (target_id < agents.size() && agents[target_id]) {
					player = agents[target_id]->PlayerNumber;
					if (player == 0 || player > n_players) continue;
					--player;	// player numbers are from 1 to partysize in party list
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
		}
	}

	for (int i = 0; i < n_players; ++i) {
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
