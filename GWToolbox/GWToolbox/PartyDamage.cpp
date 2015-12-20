#include "PartyDamage.h"

#include <GWCA\GWCA.h>

#include "GuiUtils.h"
#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;

PartyDamage::PartyDamage() {

	total = 0;

	GWCA::Api().StoC().AddGameServerEvent<StoC::P151>(
		[this](StoC::P151* packet) {

		// ignore non-damage packets
		switch (packet->type) {
		case StoC::P151_Type::damage:
		case StoC::P151_Type::critical:
		case StoC::P151_Type::armorignoring:
			break;
		default:
			return;
		}

		// ignore heals
		if (packet->value >= 0) return;

		// get cause agent
		GW::Agent* cause = GWCA::Api().Agents().GetAgentByID(packet->cause_id);
		if (cause == nullptr) return;
		if (cause->LoginNumber == 0) return; // ignore NPCs, heroes
		if (cause->PlayerNumber == 0) return; // might as well check for this too
		if (cause->PlayerNumber > MAX_PLAYERS) return;

		// get target agent
		GW::Agent* target = GWCA::Api().Agents().GetAgentByID(packet->target_id);
		if (target == nullptr) return;
		if (target->LoginNumber != 0) return; // ignore player-inflicted damage
											  // such as Life bond or sacrifice

		if (target->MaxHP > 0 && target->MaxHP < 100000) {
			long dmg = std::lround(-packet->value * target->MaxHP);
			//printf("[%d] %d -> %d\n", cause->PlayerNumber, dmg, packet->target_id);
			damage[cause->PlayerNumber - 1] += dmg;
			total += dmg;
		} else {
			//printf("[%d] %f %% -> %d\n", cause->PlayerNumber,
				//packet->value, packet->target_id);
		}
	});

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyX(), 400);
	int y = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyY(), 100);
	SetLocation(x, y);

	line_height_ = GuiUtils::GetPartyHealthbarHeight();
	SetSize(Drawing::SizeI(ABS_WIDTH + PERC_WIDTH, line_height_ * MAX_PLAYERS));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(BondsWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	int offsetX = 2;
	int offsetY = 2;
	float fontsize = 10.0f;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i] = 0;

		absolute[i] = new DragButton();
		absolute[i]->SetText(L"100%");
		absolute[i]->SetSize(ABS_WIDTH, line_height_);
		absolute[i]->SetLocation(0, i * line_height_);
		absolute[i]->SetFont(GuiUtils::getTBFont(fontsize, true));
		absolute[i]->SetBackColor(Drawing::Color::Empty());
		absolute[i]->SetForeColor(theme.ForeColor);
		absolute[i]->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
			SaveLocation();
		});
		AddControl(absolute[i]);

		percent[i] = new DragButton();
		percent[i]->SetText(L"42.0k");
		percent[i]->SetSize(PERC_WIDTH, line_height_);
		percent[i]->SetLocation(ABS_WIDTH, i * line_height_);
		percent[i]->SetFont(GuiUtils::getTBFont(fontsize, true));
		percent[i]->SetBackColor(Drawing::Color::Empty());
		percent[i]->SetForeColor(theme.ForeColor);
		percent[i]->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
			SaveLocation();
		});
		AddControl(percent[i]);
	}

	bool show = config.IniReadBool(PartyDamage::IniSection(), PartyDamage::InikeyShow(), false);
	show = true;
	SetVisible(show);
	containerPanel_->SetVisible(show);
	for (Control* c : controls_) {
		c->SetVisible(show);
	}

	std::shared_ptr<PartyDamage> self = std::shared_ptr<PartyDamage>(this);
	Form::Show(self);
}

void PartyDamage::MainRoutine() {

}

void PartyDamage::UpdateUI() {
	if (!isVisible_) return;

	GWCA api;

	int size = api().Agents().GetPartySize();
	if (size > MAX_PLAYERS) size = MAX_PLAYERS;
	if (party_size_ != size) {
		party_size_ = size;
		SetSize(Drawing::SizeI(WIDTH, party_size_ * line_height_));
		for (int i = 0; i < MAX_PLAYERS; ++i) {
			bool visible = (i < party_size_);
			absolute[i]->SetVisible(visible);
			percent[i]->SetVisible(visible);
		}
	}

	for (int i = 0; i < MAX_PLAYERS; ++i) {
		absolute[i]->SetText(std::to_wstring(damage[i]));
		double part = 0;
		if (total > 0) part = (double)(damage[i]) / total;
		percent[i]->SetText(std::to_wstring(part));
	}
}

void PartyDamage::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(PartyDamage::IniSection(), PartyDamage::IniKeyX(), x);
	config.IniWriteLong(PartyDamage::IniSection(), PartyDamage::IniKeyY(), y);
}
