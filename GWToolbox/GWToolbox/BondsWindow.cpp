#include "BondsWindow.h"

#include <sstream>
#include <string>

#include <OSHGui\OSHGui.hpp>

#include <GWCA\GWCA.h>
#include <GWCA\EffectMgr.h>
#include <GWCA\SkillbarMgr.h>
#include <GWCA\GWStructures.h>
#include <GWCA\PartyMgr.h>

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"


using namespace OSHGui;

BondsWindow::BondsWindow() {

	int img_size = GuiUtils::GetPartyHealthbarHeight();
	int width = img_size * MAX_BONDS;
	int height = img_size * MAX_PLAYERS;

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(BondsWindow::IniSection(), BondsWindow::IniKeyX(), 400);
	int y = config.IniReadLong(BondsWindow::IniSection(), BondsWindow::IniKeyY(), 100);

	SetLocation(PointI(x, y));
	SetSize(Drawing::SizeI(width, height));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(BondsWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	monitor = new BondsMonitor(this, img_size);
	monitor->SetLocation(PointI(0, 0));
	monitor->SetSize(SizeI(width, height));
	AddControl(monitor);

	std::shared_ptr<BondsWindow> self = std::shared_ptr<BondsWindow>(this);
	Form::Show(self);

	bool show = config.IniReadBool(BondsWindow::IniSection(), BondsWindow::IniKeyShow(), false);
	SetVisible(show);
}

BondsWindow::BondsMonitor::BondsMonitor(OSHGui::Control* parent, int img_size) : 
	DragButton(parent), 
	img_size_(img_size) {

	isFocusable_ = true;
	SetEnabled(true);
	hovered_player = -1;
	hovered_bond = -1;
	party_size = MAX_PLAYERS; // initialize at max, upcate will take care of shrinking as needed.
	pressed = false;

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		for (int j = 0; j < MAX_BONDS; ++j) {
			buff_id[i][j] = 0;
			pics[i][j] = new PictureBox(this);
			pics[i][j]->SetLocation(PointI(j * img_size_ + 1, i *img_size_ + 1));
			pics[i][j]->SetSize(SizeI(img_size_ - 2, img_size_ - 2));
			pics[i][j]->SetStretch(true);
			pics[i][j]->SetVisible(false);
			pics[i][j]->SetEnabled(false);
			pics[i][j]->SetClip(Clipping::OnParent); // so they dont modify clipping
			AddControl(pics[i][j]);
		}
	}
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		pics[i][0]->SetImage(Image::FromFile(GuiUtils::getSubPathA("balthspirit.jpg", "img")));
		pics[i][1]->SetImage(Image::FromFile(GuiUtils::getSubPathA("lifebond.jpg", "img")));
		pics[i][2]->SetImage(Image::FromFile(GuiUtils::getSubPathA("protbond.jpg", "img")));
	}

	GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
}

void BondsWindow::BondsMonitor::Render(OSHGui::Drawing::RenderContext &context) {
	if (!isVisible_) return;

	for (Control* control : controls_) {
		control->Render(context);
	}

	DrawSelf(context);
}

void BondsWindow::BondsMonitor::PopulateGeometry() {
	if (hovered_player != -1 && hovered_bond != -1) {
		Graphics g(*geometry_);
		Drawing::Color c = GetForeColor();
		if (pressed) c = c - Drawing::Color::FromARGB(50, 50, 50, 50);
		g.DrawRectangle(c, img_size_ * hovered_bond, img_size_ * hovered_player, img_size_, img_size_);
	}
}

int BondsWindow::BondsMonitor::GetBond(int xcoord) {
	return (xcoord - absoluteLocation_.X) / img_size_;
}

int BondsWindow::BondsMonitor::GetPlayer(int ycoord) {
	return (ycoord - absoluteLocation_.Y) / img_size_;
}

void BondsWindow::BondsMonitor::OnMouseDown(const OSHGui::MouseMessage &mouse) {
	if (freezed) {
		Control::OnMouseDown(mouse);
	} else {
		DragButton::OnMouseDown(mouse);
	}

	pressed = true;
	Invalidate();
}
void BondsWindow::BondsMonitor::OnMouseMove(const OSHGui::MouseMessage &mouse) {
	if (freezed) {
		Control::OnMouseMove(mouse);
	} else {
		DragButton::OnMouseMove(mouse);
	}

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
	if (freezed) {
		Control::OnMouseUp(mouse);
	} else {
		DragButton::OnMouseUp(mouse);
	}

	if (pressed) {
		int player = GetPlayer(mouse.GetLocation().Y);
		int bond = GetBond(mouse.GetLocation().X);

		if (player >= 0 && player < MAX_PLAYERS
			&& bond >= 0 && bond < MAX_BONDS) {
			
			DropUseBuff(bond, player);
		}
	}

	pressed = false;
	Invalidate();
}

void BondsWindow::BondsMonitor::DropUseBuff(int bond, int player) {
	if (pics[player][bond]->GetVisible()) {
		if (buff_id[player][bond] > 0) {
			GWCA::Effects().DropBuff(buff_id[player][bond]);
		}
	} else {
		// cast bond on player
		GwConstants::SkillID buff;
		switch (static_cast<Bond>(bond)) {
		case Bond::Balth: buff = GwConstants::SkillID::Balthazars_Spirit; break;
		case Bond::Life: buff = GwConstants::SkillID::Life_Bond; break;
		case Bond::Prot: buff = GwConstants::SkillID::Protective_Bond; break;
		}

		int target = GWCA::Agents().GetAgentIdByLoginNumber(player + 1);
		if (target <= 0) return;

		int slot = GWCA::Skillbar().GetSkillSlot(buff);
		if (slot <= 0) return;
		if (GWCA::Skillbar().GetPlayerSkillbar().Skills[slot].Recharge != 0) return;

		GWCA::Skillbar().UseSkill(slot, target);
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
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(BondsWindow::IniSection(), BondsWindow::IniKeyX(), x);
	config.IniWriteLong(BondsWindow::IniSection(), BondsWindow::IniKeyY(), y);
}

void BondsWindow::BondsMonitor::UpdateUI() {
	if (!isVisible_) return;

	int size = GWCA::Party().GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;
	if (party_size != size) {
		party_size = size;
		SetSize(SizeI(MAX_BONDS * img_size_, party_size * img_size_));
		parent_->SetSize(GetSize());
	}

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		for (int j = 0; j < MAX_BONDS; ++j) {
			show[i][j] = false;
		}
	}

	GWCA::GW::AgentEffectsArray effects = GWCA::Effects().GetPartyEffectArray();
	if (effects.valid()) {
		GWCA::GW::BuffArray buffs = effects[0].Buffs;
		GWCA::GW::AgentArray agents = GWCA::Agents().GetAgentArray();

		if (buffs.valid() && agents.valid() && effects.valid()) {
			for (size_t i = 0; i < buffs.size(); ++i) {
				int player = -1;
				DWORD target_id = buffs[i].TargetAgentId;
				if (target_id < agents.size() && agents[target_id]) {
					player = agents[target_id]->PlayerNumber;
					if (player == 0 || player > MAX_PLAYERS) continue;
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

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		for (int j = 0; j < MAX_BONDS; ++j) {
			if (pics[i][j]->GetVisible() != show[i][j]) {
				pics[i][j]->SetVisible(show[i][j]);
			}
		}
	}
}
