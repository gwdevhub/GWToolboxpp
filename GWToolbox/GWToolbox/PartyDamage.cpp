#include "PartyDamage.h"

#include <GWCA\GWCA.h>
#include <sstream>

#include "GuiUtils.h"
#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;

PartyDamage::PartyDamage() {

	total = 0;
	send_timer = TBTimer::init();

	GWCA::Api().StoC().AddGameServerEvent<StoC::P151>(
		std::bind(&PartyDamage::DamagePacketCallback, this, std::placeholders::_1));

	GWCA::Api().StoC().AddGameServerEvent<StoC::P230>(
		std::bind(&PartyDamage::MapLoadedCallback, this, std::placeholders::_1));

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyX(), 400);
	int y = config.IniReadLong(PartyDamage::IniSection(), PartyDamage::IniKeyY(), 100);
	SetLocation(x, y);

	line_height_ = GuiUtils::GetPartyHealthbarHeight();
	SetSize(Drawing::SizeI(ABS_WIDTH + PERC_WIDTH, line_height_ * MAX_PLAYERS));

	if (!Application::Instance().GetTheme().ContainsColorTheme(PartyDamage::ThemeKey())) {
		Drawing::Theme::ControlTheme ctheme(default_forecolor, default_backcolor);
		Drawing::Theme::ControlTheme barctheme(default_forebarcolor, default_backbarcolor);
		Drawing::Theme& theme = Application::Instance().GetTheme();
		theme.SetControlColorTheme(PartyDamage::ThemeKey(), ctheme);
		theme.SetControlColorTheme(PartyDamage::ThemeBarsKey(), barctheme);
	}

	Drawing::Theme::ControlTheme theme = Application::Instance()
		.GetTheme().GetControlColorTheme(PartyDamage::ThemeKey());
	Drawing::Theme::ControlTheme bartheme = Application::Instance()
		.GetTheme().GetControlColorTheme(PartyDamage::ThemeBarsKey());

	SetBackColor(theme.BackColor);
	labelcolor = theme.ForeColor;

	int offsetX = 2;
	int offsetY = 2;
	float fontsize = 9.0f;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i].damage= 0;
		damage[i].recent_damage = 0;
		damage[i].last_damage = TBTimer::init();

		bar[i] = new Panel();
		bar[i]->SetSize(WIDTH, line_height_);
		bar[i]->SetLocation(0, i * line_height_);
		bar[i]->SetBackColor(bartheme.BackColor);
		AddControl(bar[i]);

		recent[i] = new Panel();
		recent[i]->SetSize(WIDTH, RECENT_HEIGHT);
		recent[i]->SetLocation(0, (i + 1) * line_height_ - RECENT_HEIGHT);
		recent[i]->SetBackColor(bartheme.ForeColor);
		AddControl(recent[i]);

		absolute[i] = new DragButton();
		absolute[i]->SetText(L"0 %");
		absolute[i]->SetSize(ABS_WIDTH, line_height_ - RECENT_HEIGHT / 2);
		absolute[i]->SetLocation(0, i * line_height_);
		absolute[i]->SetFont(GuiUtils::getTBFont(fontsize, true));
		absolute[i]->SetBackColor(Drawing::Color::Empty());
		absolute[i]->SetForeColor(theme.ForeColor);
		absolute[i]->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
			SaveLocation();
		});
		AddControl(absolute[i]);

		percent[i] = new DragButton();
		percent[i]->SetText(L"");
		percent[i]->SetSize(PERC_WIDTH, line_height_ - RECENT_HEIGHT / 2);
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
	ShowWindow(show);

	SetFreeze(config.IniReadBool(MainWindow::IniSection(), MainWindow::IniKeyFreeze(), false));

	std::shared_ptr<PartyDamage> self = std::shared_ptr<PartyDamage>(this);
	Form::Show(self);
}

void PartyDamage::MapLoadedCallback(GWAPI::StoC::P230* packet) {
	switch (GWCA::Api().Map().GetInstanceType()) {
	case GwConstants::InstanceType::Outpost:
		in_explorable = false;
		break;
	case GwConstants::InstanceType::Explorable:
		if (!in_explorable) {
			in_explorable = true;
			Reset();
		}
		break;
	case GwConstants::InstanceType::Loading:
	default:
		break;
	}
}

void PartyDamage::DamagePacketCallback(GWAPI::StoC::P151* packet) {

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

	long dmg;
	if (target->MaxHP > 0 && target->MaxHP < 100000) {
		dmg = std::lround(-packet->value * target->MaxHP);
		hp_map[target->PlayerNumber] = target->MaxHP;
	} else {
		auto it = hp_map.find(target->PlayerNumber);
		if (it == hp_map.end()) {
			// max hp not found, approximate with hp/lvl formula
			dmg = std::lround(-packet->value * target->Level * 20 + 100);
		} else {
			long maxhp = it->second;
			dmg = std::lround(-packet->value * it->second);
		}
	}

	int index = cause->PlayerNumber - 1;

	if (damage[index].damage == 0) {
		damage[index].agent_id = packet->cause_id;
		damage[index].name = GWCA::Api().Agents().GetPlayerNameByLoginNumber(cause->LoginNumber);
		damage[index].primary = static_cast<GwConstants::Profession>(cause->Primary);
		damage[index].secondary = static_cast<GwConstants::Profession>(cause->Secondary);
	}

	damage[index].damage += dmg;
	total += dmg;

	if (isVisible_) {
		damage[index].recent_damage += dmg;
		damage[index].last_damage = TBTimer::init();
	}
}

void PartyDamage::MainRoutine() {
	if (!send_queue.empty() && TBTimer::diff(send_timer) > 600) {
		send_timer = TBTimer::init();
		GWCA api;
		if (api().Map().GetInstanceType() != GwConstants::InstanceType::Loading
			&& api().Agents().GetPlayer()) {
			api().Chat().SendChat(send_queue.front().c_str(), L'#');
			send_queue.pop();
		}
	}
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
			bar[i]->SetVisible(visible);
			recent[i]->SetVisible(visible);
		}
	}

	// reset recent if needed
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (TBTimer::diff(damage[i].last_damage) > RECENT_MAX_TIME) {
			damage[i].recent_damage = 0;
		}
	}

	long max_recent = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max_recent < damage[i].recent_damage) {
			max_recent = damage[i].recent_damage;
		}
	}

	long max = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (max < damage[i].damage) {
			max = damage[i].damage;
		}
	}

	const int BUF_SIZE = 10;
	wchar_t buff[BUF_SIZE];
	for (int i = 0; i < party_size_; ++i) {
		if (damage[i].damage < 1000) {
			swprintf_s(buff, BUF_SIZE, L"%d", damage[i].damage);
		} else if (damage[i].damage < 1000 * 10) {
			swprintf_s(buff, BUF_SIZE, L"%.2f k", (float)damage[i].damage / 1000);
		} else if (damage[i].damage < 1000 * 1000) {
			swprintf_s(buff, BUF_SIZE, L"%.1f k", (float)damage[i].damage / 1000);
		} else {
			swprintf_s(buff, BUF_SIZE, L"%.1f mil", (float)damage[i].damage / 1000000);
		}
		absolute[i]->SetText(buff);

		float perc_of_total = GetPercentageOfTotal(damage[i].damage);
		swprintf_s(buff, BUF_SIZE, L"%.1f %%", perc_of_total);
		percent[i]->SetText(buff);

		float part_of_max = 0;
		if (max > 0) {
			part_of_max = (float)(damage[i].damage) / max;
		}
		bar[i]->SetSize(std::lround(WIDTH * part_of_max), line_height_);
		bar[i]->SetLocation(std::lround(WIDTH * (1 - part_of_max)), i * line_height_);

		float part_of_recent = 0;
		if (max_recent > 0) {
			part_of_recent = (float)(damage[i].recent_damage) / max_recent;
		}
		recent[i]->SetSize(std::lround(WIDTH * part_of_recent), RECENT_HEIGHT);
		recent[i]->SetLocation(std::lround(WIDTH * (1 - part_of_recent)),
			(i + 1) * line_height_ - RECENT_HEIGHT);

		Drawing::Color inactive = labelcolor - Drawing::Color(0.0f, 0.3f, 0.3f, 0.3f);
		if (damage[i].damage == 0 
			|| api().Map().GetInstanceType() == GwConstants::InstanceType::Outpost
			|| api().Agents().GetAgentByID(damage[i].agent_id) == nullptr) {

			absolute[i]->SetForeColor(inactive);
			percent[i]->SetForeColor(inactive);
		} else {
			absolute[i]->SetForeColor(labelcolor);
			percent[i]->SetForeColor(labelcolor);
		}
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

float PartyDamage::GetPartOfTotal(long dmg) {
	if (total == 0) return 0;
	return (float)dmg / total;
}

void PartyDamage::WriteChat() {
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (damage[i].damage > 0) {
			std::wstringstream ss;
			ss << GwConstants::to_wstring(damage[i].primary);
			ss << L"/";
			ss << GwConstants::to_wstring(damage[i].secondary);
			ss << L" ";
			ss << damage[i].name;
			ss << L" (";
			ss << GetPercentageOfTotal(damage[i].damage);
			ss << L" %) ";
			ss << damage[i].damage;
			send_queue.push(ss.str());
		}
	}
	send_queue.push(L"Total (100%) " + std::to_wstring(total));
}

void PartyDamage::Reset() {
	total = 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		damage[i].Reset();
	}
}

void PartyDamage::SetFreeze(bool b) {
	containerPanel_->SetEnabled(!b);
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		absolute[i]->SetEnabled(!b);
		percent[i]->SetEnabled(!b);
	}
}
