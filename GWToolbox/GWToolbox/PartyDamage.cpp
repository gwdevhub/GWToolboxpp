#include "PartyDamage.h"

#include <GWCA\GWCA.h>

#include "GuiUtils.h"
#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;

PartyDamage::PartyDamage() {

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

		// get target agent
		GW::Agent* target = GWCA::Api().Agents().GetAgentByID(packet->target_id);
		if (target == nullptr) return;
		if (target->LoginNumber != 0) return; // ignore player-inflicted damage
											  // such as Life bond or sacrifice

		if (target->MaxHP > 0 && target->MaxHP < 100000) {
			long dmg = std::lround(packet->value * target->MaxHP);
			printf("[%d] %d -> %d\n", cause->PlayerNumber, dmg, packet->target_id);
		} else {
			printf("[%d] %f %% -> %d\n", cause->PlayerNumber,
				packet->value, packet->target_id);
		}
	});

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyX(), 400);
	int y = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyY(), 100);
	SetLocation(x, y);

	int line_height = GuiUtils::GetPartyHealthbarHeight();
	SetSize(Drawing::SizeI(ABS_WIDTH + PERC_WIDTH, line_height * MAX_PLAYERS));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(BondsWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	int offsetX = 2;
	int offsetY = 2;
	float fontsize = 8.0f;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		absolute[i] = new DragButton();
		absolute[i]->SetText(L"100%");
		absolute[i]->SetSize(ABS_WIDTH, line_height);
		absolute[i]->SetLocation(0, i * line_height);
		absolute[i]->SetFont(GuiUtils::getTBFont(fontsize, true));
		absolute[i]->SetBackColor(Drawing::Color::Empty());
		absolute[i]->SetForeColor(theme.ForeColor);
		absolute[i]->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
			SaveLocation();
		});
		AddControl(absolute[i]);

		percent[i] = new DragButton();
		percent[i]->SetText(L"42.0k");
		percent[i]->SetSize(PERC_WIDTH, line_height);
		percent[i]->SetLocation(ABS_WIDTH, i * line_height);
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

}

void PartyDamage::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(PartyDamage::IniSection(), PartyDamage::IniKeyX(), x);
	config.IniWriteLong(PartyDamage::IniSection(), PartyDamage::IniKeyY(), y);
}
