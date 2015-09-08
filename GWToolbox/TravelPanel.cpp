#include "TravelPanel.h"
#include "../API/APIMain.h"

using namespace OSHGui;
using namespace GWAPI;

TravelPanel::TravelPanel() {
}

void TravelPanel::BuildUI() {
	SetSize(WIDTH, HEIGHT);

	ComboBox* combo = new ComboBox();
	combo->AddItem("Current District");
	combo->AddItem("International");
	combo->AddItem("American");
	combo->AddItem("American District 1");
	combo->AddItem("Europe English");
	combo->AddItem("Europe French");
	combo->AddItem("Europe German");
	combo->AddItem("Europe Italian");
	combo->AddItem("Europe Spanish");
	combo->AddItem("Europe Polish");
	combo->AddItem("Europe Russian");
	combo->AddItem("Asian Korean");
	combo->AddItem("Asia Chinese");
	combo->AddItem("Asia Japanese");
	combo->SetSize(GetWidth() - 2 * SPACE, BUTTON_HEIGHT);
	combo->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {
		UpdateDistrict(combo->GetSelectedIndex());
	});
	combo->SetSelectedIndex(0);
	AddControl(combo);

	Button* toa = new Button();
	toa->SetText("ToA");
	toa->SetSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	toa->SetLocation(SPACE, combo->GetBottom() + SPACE);
	toa->GetClickEvent() += ClickEventHandler([this](Control*) {
		GWAPIMgr::GetInstance()->Map->Travel(
			GwConstants::MapID::ToA, district(), region(), language());
	});
	AddControl(toa);

	Button* doa = new Button();
	doa->SetText("DoA");
	doa->SetSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	doa->SetLocation(toa->GetRight() + SPACE, toa->GetTop());
	toa->GetClickEvent() += ClickEventHandler([this](Control*) {
		GWAPIMgr::GetInstance()->Map->Travel(
			GwConstants::MapID::DoA, district(), region(), language());
	});
	AddControl(doa);

}

void TravelPanel::UpdateDistrict(int gui_index) {
	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();
	region_ = api->Map->GetRegion();
	district_ = 0;
	language_ = api->Map->GetLanguage();
	switch (gui_index) {
	case 0: // Current District
		break;
	case 1: // International
		region_ = -2;
		break;
	case 2: // American
		region_ = 0;
		break;
	case 3: // American District 1
		region_ = 0;
		district_ = 1;
		break;
	case 4: // Europe English
		region_ = 2;
		language_ = 0;
		break;
	case 5: // Europe French
		region_ = 2;
		language_ = 2;
		break;
	case 6: // Europe German
		region_ = 2;
		language_ = 3;
		break;
	case 7: // Europe Italian
		region_ = 2;
		language_ = 4;
		break;
	case 8: // Europe Spanish
		region_ = 2;
		language_ = 5;
		break;
	case 9: // Europe Polish
		region_ = 2;
		language_ = 9;
		break;
	case 10: // Europe Russian
		region_ = 2;
		language_ = 10;
		break;
	case 11: // Asian Korean
		region_ = 1;
		language_ = 0;
		break;
	case 12: // Asia Chinese
		region_ = 3;
		language_ = 0;
		break;
	case 13: // Asia Japanese
		region_ = 4;
		language_ = 0;
		break;
	default:
		break;
	}
}