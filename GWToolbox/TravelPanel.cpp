#include "TravelPanel.h"
#include "../API/APIMain.h"
#include <string>

using namespace OSHGui;
using namespace GWAPI;
using namespace std;

TravelPanel::TravelPanel() {
}

void TravelPanel::BuildUI() {
	SetSize(WIDTH, HEIGHT);

	using namespace GwConstants;
	AddTravelButton("ToA", 0, 0, MapID::Temple_of_the_Ages);
	AddTravelButton("DoA", 1, 0, MapID::Domain_of_Anguish);
	AddTravelButton("Kamadan", 0, 1, MapID::Kamadan_Jewel_of_Istan_outpost);
	AddTravelButton("Embark", 1, 1, MapID::Embark_Beach);
	AddTravelButton("Vlox's", 0, 2, MapID::Vloxs_Falls);
	AddTravelButton("Gadd's", 1, 2, MapID::Gadds_Encampment_outpost);
	AddTravelButton("Urgoz", 0, 3, MapID::Urgozs_Warren);
	AddTravelButton("Deep", 1, 3, MapID::The_Deep);

	ComboBox* combo = new ComboBox();
	combo->SetMaxShowItems(14);
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
	combo->SetLocation(DefaultBorderPadding, GetHeight() - DefaultBorderPadding - combo->GetHeight());
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {
		UpdateDistrict(combo->GetSelectedIndex());
	});
	combo->SetSelectedIndex(0);
	AddControl(combo);
}

void TravelPanel::AddTravelButton(string text, int grid_x, int grid_y, GwConstants::MapID map_id) {
	Button* button = new Button();
	button->SetText(text);
	button->SetSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	button->SetLocation(SPACE + (BUTTON_WIDTH + SPACE) * grid_x, 
		SPACE + (BUTTON_HEIGHT + SPACE) * grid_y);
	button->GetClickEvent() += ClickEventHandler([this, map_id](Control*) {
		GWAPIMgr::GetInstance()->Map->Travel(map_id, district(), region(), language());
	});
	AddControl(button);
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
