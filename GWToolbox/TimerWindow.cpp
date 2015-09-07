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

	timer_ = new DragButton();
	timer_->SetText("TEST");
	timer_->SetSize(GetSize());
	timer_->SetFont(GuiUtils::getTBFont(26.0f, true));
	timer_->SetLocation(0, 0);
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

void TimerWindow::MainRoutine() {
	GWAPIMgr* API = GWAPIMgr::GetInstance();
	long uptime = API->Map->GetInstanceTime();
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
	}
	
}