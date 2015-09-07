#include "TimerWindow.h"

#include <sstream>
#include <string>

#include "GWToolbox.h"
#include "Config.h"
#include "../API/APIMain.h"
#include "GuiUtils.h"

using namespace OSHGui;

TimerWindow::TimerWindow() {
	current_time_ = 0;

	Config* config = GWToolbox::instance()->config();
	int x = config->iniReadLong(TimerWindow::IniSection(), TimerWindow::IniKeyX(), 100);
	int y = config->iniReadLong(TimerWindow::IniSection(), TimerWindow::IniKeyY(), 100);

	SetLocation(x, y);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	
	shadow_ = new TimerLabel();
	int offsetX= 2; 
	int offsetY = 2;
	shadow_->SetText("Temp");
	shadow_->SetAnchor(AnchorStyles::Left);
	shadow_->SetSize(GetSize());
	shadow_->SetLocation(offsetX, offsetY);
	shadow_->SetFont(GuiUtils::getTBFont(26.0f, true));
	shadow_->SetBackColor(Drawing::Color::Empty());
	shadow_->SetForeColor(Drawing::Color::Black());
	shadow_->SetEnabled(false);
	AddControl(shadow_);

	timer_ = new TimerLabel();
	timer_->SetText("Temp");
	shadow_->SetAnchor(AnchorStyles::Left);
	timer_->SetSize(GetSize());
	timer_->SetLocation(0, 0);
	timer_->SetFont(GuiUtils::getTBFont(26.0f, true));
	timer_->SetBackColor(Drawing::Color::Empty());
	timer_->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(timer_);
}

void TimerWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(TimerWindow::IniSection(), TimerWindow::IniKeyX(), x);
	config->iniWriteLong(TimerWindow::IniSection(), TimerWindow::IniKeyY(), y);
}

void TimerWindow::UpdateLabel() {
	long uptime = GWAPIMgr::GetInstance()->Map->GetInstanceTime();
	long time = uptime / 1000;
	if (time != current_time_) {
		current_time_ = time;

		int seconds = time % 60;
		int minutes = (int)(time / 60) % 60;
		int hours = (int)(time / (60 * 60));

		stringstream ss;
		ss.fill('0');
		ss << hours;
		ss << ":";
		ss.width(2);
		ss << minutes;
		ss.width(1);
		ss << ":";
		ss.width(2);
		ss << seconds;
		
		timer_->SetText(ss.str());
		timer_->Invalidate();

		shadow_->SetText(ss.str());
		shadow_->Invalidate();
	}
	
}