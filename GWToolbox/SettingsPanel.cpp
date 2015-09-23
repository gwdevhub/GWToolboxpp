#include "SettingsPanel.h"
#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"
#include "HealthWindow.h"
#include "TimerWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"

using namespace OSHGui;

SettingsPanel::SettingsPanel() {
}

void SettingsPanel::BuildUI() {
	SetSize(250, 300);
	int width = 250;
	int item_width = width - 2 * DefaultBorderPadding;
	int item_height = 25;

	GWToolbox* tb = GWToolbox::instance();
	Config* config = tb->config();

	Label* version = new Label();
	version->SetText("GWToolbox++ 0.1 (Alpha)");
	version->SetLocation(GetWidth() / 2 - version->GetWidth() / 2, DefaultBorderPadding);
	AddControl(version);

	Label* authors = new Label();
	authors->SetText("by Has and KAOS");
	authors->SetLocation(GetWidth() / 2 - authors->GetWidth() / 2, version->GetBottom() + DefaultBorderPadding);
	AddControl(authors);

	CheckBox* tabsleft = new CheckBox();
	tabsleft->SetText("Open tabs on the left");
	tabsleft->SetLocation(DefaultBorderPadding, authors->GetBottom() + DefaultBorderPadding * 3);
	tabsleft->SetSize(item_width, item_height);
	tabsleft->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyTabsLeft(), false));
	tabsleft->GetCheckedChangedEvent() += CheckedChangedEventHandler([tabsleft](Control*) {
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyTabsLeft(), tabsleft->GetChecked());
		GWToolbox::instance()->main_window()->SetPanelPositions(tabsleft->GetChecked());
	});
	AddControl(tabsleft);

	CheckBox* freeze = new CheckBox();
	freeze->SetText("Freeze info widget positions");
	freeze->SetLocation(DefaultBorderPadding, tabsleft->GetBottom() + DefaultBorderPadding);
	freeze->SetSize(item_width, item_height);
	freeze->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyFreeze(), false));
	freeze->GetCheckedChangedEvent() += CheckedChangedEventHandler([freeze](Control*) {
		bool b = freeze->GetChecked();
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyFreeze(), b);
		GWToolbox::instance()->timer_window()->SetFreeze(b);
		GWToolbox::instance()->bonds_window()->SetFreze(b);
		GWToolbox::instance()->health_window()->SetFreeze(b);
		GWToolbox::instance()->distance_window()->SetFreeze(b);
	});
	AddControl(freeze);

	CheckBox* hidetarget = new CheckBox();
	hidetarget->SetText("Hide target windows");
	hidetarget->SetLocation(DefaultBorderPadding, freeze->GetBottom() + DefaultBorderPadding);
	hidetarget->SetSize(item_width, item_height);
	hidetarget->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyHideTarget(), false));
	hidetarget->GetCheckedChangedEvent() += CheckedChangedEventHandler([hidetarget](Control*) {
		bool hide = hidetarget->GetChecked();
		GWToolbox::instance()->health_window()->SetHideTarget(hide);
		GWToolbox::instance()->distance_window()->SetHideTarget(hide);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyHideTarget(), hide);
	});
	AddControl(hidetarget);

	CheckBox* minimizealtpos = new CheckBox();
	minimizealtpos->SetText("Minmize to Alt Position");
	minimizealtpos->SetLocation(DefaultBorderPadding, hidetarget->GetBottom() + DefaultBorderPadding);
	minimizealtpos->SetSize(item_width, item_height);
	minimizealtpos->SetChecked(config->iniReadBool(MainWindow::IniSection(),
		MainWindow::IniKeyMinAltPos(), false));
	minimizealtpos->GetCheckedChangedEvent() += CheckedChangedEventHandler([minimizealtpos](Control*) {
		bool enabled = minimizealtpos->GetChecked();
		GWToolbox::instance()->main_window()->setUseMinimizedAltPos(enabled);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyMinAltPos(), enabled);
	});
	AddControl(minimizealtpos);


	Button* folder = new Button();
	folder->SetText("Open Settings Folder");
	folder->SetSize(item_width, item_height);
	folder->SetLocation(DefaultBorderPadding, GetHeight() - DefaultBorderPadding - folder->GetHeight());
	folder->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteA(NULL, "open", GuiUtils::getSettingsFolderA().c_str(), NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(folder);

	Button* website = new Button();
	website->SetText("Open GWToolbox++ Website");
	website->SetSize(item_width, item_height);
	website->SetLocation(DefaultBorderPadding, folder->GetTop() - DefaultBorderPadding - website->GetHeight());
	website->GetClickEvent() += ClickEventHandler([](Control*) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteA(NULL, "open", "http://fbgmguild.com/GWToolboxpp/", NULL, NULL, SW_SHOWNORMAL);
	});
	AddControl(website);
}
