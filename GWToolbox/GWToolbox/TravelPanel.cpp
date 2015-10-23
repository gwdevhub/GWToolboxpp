#include "TravelPanel.h"

#include <string>

#include "GWCA\APIMain.h"

#include "MainWindow.h"
#include "GWToolbox.h"
#include "Config.h"


using namespace OSHGui;
using namespace GWAPI;
using namespace std;

TravelPanel::TravelPanel() {
}

void TravelPanel::BuildUI() {
	current_district_ = true;
	region_ = 0;
	district_ = 0;
	language_ = 0;

	ComboBox* combo = new TravelCombo();
	combo->SetMaxShowItems(10);
	combo->SetText(L"Travel To...");
	for (int i = 0; i < n_outposts; ++i) {
		combo->AddItem(IndexToOutpostName(i));
	}
	combo->SetSize(GetWidth() - 2 * DefaultBorderPadding, BUTTON_HEIGHT);
	combo->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {
		if (combo->GetSelectedIndex() < 0) return;
		GwConstants::MapID id = IndexToOutpostID(combo->GetSelectedIndex());
		GWCA::Api().Map().Travel(id, this->district(), this->region(), this->language());
		combo->SetText(L"Travel To...");
		combo->SetSelectedIndex(-1);
	});
	AddControl(combo);

	ComboBox* district = new ComboBox();
	district->SetMaxShowItems(14);
	district->AddItem(L"Current District");
	district->AddItem(L"International");
	district->AddItem(L"American");
	district->AddItem(L"American District 1");
	district->AddItem(L"Europe English");
	district->AddItem(L"Europe French");
	district->AddItem(L"Europe German");
	district->AddItem(L"Europe Italian");
	district->AddItem(L"Europe Spanish");
	district->AddItem(L"Europe Polish");
	district->AddItem(L"Europe Russian");
	district->AddItem(L"Asian Korean");
	district->AddItem(L"Asia Chinese");
	district->AddItem(L"Asia Japanese");
	district->SetSize(GetWidth() - 2 * DefaultBorderPadding, BUTTON_HEIGHT);
	district->SetLocation(DefaultBorderPadding, district->GetBottom() + DefaultBorderPadding);
	district->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, district](Control*) {
		UpdateDistrict(district->GetSelectedIndex());
	});
	district->SetSelectedIndex(0);
	AddControl(district);

	using namespace GwConstants;
	AddTravelButton(L"ToA", 0, 2, MapID::Temple_of_the_Ages);
	AddTravelButton(L"DoA", 1, 2, MapID::Domain_of_Anguish);
	AddTravelButton(L"Kamadan", 0, 3, MapID::Kamadan_Jewel_of_Istan_outpost);
	AddTravelButton(L"Embark", 1, 3, MapID::Embark_Beach);
	AddTravelButton(L"Vlox's", 0, 4, MapID::Vloxs_Falls);
	AddTravelButton(L"Gadd's", 1, 4, MapID::Gadds_Encampment_outpost);
	AddTravelButton(L"Urgoz", 0, 5, MapID::Urgozs_Warren);
	AddTravelButton(L"Deep", 1, 5, MapID::The_Deep);

	for (int i = 0; i < 3; ++i) {
		wstring key = wstring(L"Travel") + to_wstring(i);
		int index = GWToolbox::instance()->config()->iniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new TravelCombo();
		fav_combo->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4, 3), BUTTON_HEIGHT);
		fav_combo->SetLocation(DefaultBorderPadding, 
			DefaultBorderPadding + (BUTTON_HEIGHT + DefaultBorderPadding) * (i + 6));
		for (int i = 0; i < n_outposts; ++i) {
			fav_combo->AddItem(IndexToOutpostName(i));
		}
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			GWToolbox::instance()->config()->iniWriteLong(MainWindow::IniSection(), 
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* fav_button = new Button();
		fav_button->SetSize(GuiUtils::ComputeWidth(GetWidth(), 4), BUTTON_HEIGHT);
		fav_button->SetLocation(GuiUtils::ComputeX(GetWidth(), 4, 3), fav_combo->GetTop());
		fav_button->SetText(L"Go");
		fav_button->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWCA::Api().Map().Travel(IndexToOutpostID(index),
				this->district(), this->region(), this->language());
		});
		AddControl(fav_button);
	}
}

TravelPanel::TravelCombo::TravelCombo() 
	: OSHGui::ComboBox() {
	listBox_->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	listBox_->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
}

DWORD TravelPanel::region() {
	if (current_district_) {
		return GWCA::Api().Map().GetRegion();
	} else {
		return region_;
	}
}

DWORD TravelPanel::language() {
	if (current_district_) {
		return GWCA::Api().Map().GetLanguage();
	} else {
		return language_;
	}
}


void TravelPanel::AddTravelButton(wstring text, int grid_x, int grid_y, GwConstants::MapID map_id) {
	Button* button = new Button();
	button->SetText(text);
	button->SetSize(GuiUtils::ComputeWidth(GetWidth(), 2), BUTTON_HEIGHT);
	button->SetLocation(GuiUtils::ComputeX(GetWidth(), 2, grid_x), 
		DefaultBorderPadding + (BUTTON_HEIGHT + DefaultBorderPadding) * grid_y);
	button->GetClickEvent() += ClickEventHandler([this, map_id](Control*) {
		GWCA::Api().Map().Travel(map_id, district(), region(), language());
	});
	AddControl(button);
}

void TravelPanel::UpdateDistrict(int gui_index) {
	GWCA api;
	current_district_ = false;
	region_ = api().Map().GetRegion();
	district_ = 0;
	language_ = api().Map().GetLanguage();
	switch (gui_index) {
	case 0: // Current District
		current_district_ = true;
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

wstring TravelPanel::IndexToOutpostName(int index) {
	switch (index) {
	case 0: return wstring(L"Abaddon's Gate");
	case 1: return wstring(L"Abaddon's Mouth");
	case 2: return wstring(L"Altrumm Ruins");
	case 3: return wstring(L"Amatz Basin");
	case 4: return wstring(L"Arborstone");
	case 5: return wstring(L"Ascalon City");
	case 6: return wstring(L"Ascalon City (pre-searing)");
	case 7: return wstring(L"Ashford Abbey (pre-searing)");
	case 8: return wstring(L"Aspenwood Gate (Kurzick)");
	case 9: return wstring(L"Aspenwood Gate (Luxon)");
	case 10: return wstring(L"Augury Rock");
	case 11: return wstring(L"Aurora Glade");
	case 12: return wstring(L"Bai Paasu Reach");
	case 13: return wstring(L"Basalt Grotto");
	case 14: return wstring(L"Beacon's Perch");
	case 15: return wstring(L"Beetletun");
	case 16: return wstring(L"Beknur Harbor");
	case 17: return wstring(L"Bergen Hot Springs");
	case 18: return wstring(L"Blacktide Den");
	case 19: return wstring(L"Bloodstone Fen");
	case 20: return wstring(L"Bone Palace");
	case 21: return wstring(L"Boreal Station");
	case 22: return wstring(L"Boreas Seabed");
	case 23: return wstring(L"Borlis Pass");
	case 24: return wstring(L"Brauer Academy");
	case 25: return wstring(L"Breaker Hollow");
	case 26: return wstring(L"Camp Hojanu");
	case 27: return wstring(L"Camp Rankor");
	case 28: return wstring(L"Cavalon");
	case 29: return wstring(L"Central Transfer Chamber");
	case 30: return wstring(L"Chahbek Village");
	case 31: return wstring(L"Champion's Dawn");
	case 32: return wstring(L"Chantry of Secrets");
	case 33: return wstring(L"Codex Arena");
	case 34: return wstring(L"Consulate Docks");
	case 35: return wstring(L"Copperhammer Mines");
	case 36: return wstring(L"D'Alessio Seaboard");
	case 37: return wstring(L"Dajkah Inlet");
	case 38: return wstring(L"Dasha Vestibule");
	case 39: return wstring(L"Deldrimor War Camp");
	case 40: return wstring(L"Destiny's Gorge");
	case 41: return wstring(L"Divinity Coast");
	case 42: return wstring(L"Doomlore Shrine");
	case 43: return wstring(L"Dragon's Throat");
	case 44: return wstring(L"Droknar's Forge");
	case 45: return wstring(L"Druid's Overlook");
	case 46: return wstring(L"Dunes of Despair");
	case 47: return wstring(L"Durheim Archives");
	case 48: return wstring(L"Dzagonur Bastion");
	case 49: return wstring(L"Elona Reach");
	case 50: return wstring(L"Embark Beach");
	case 51: return wstring(L"Ember Light Camp");
	case 52: return wstring(L"Eredon Terrace");
	case 53: return wstring(L"Eye of the North");
	case 54: return wstring(L"Fishermen's Haven");
	case 55: return wstring(L"Foible's Fair (pre-searing)");
	case 56: return wstring(L"Fort Aspenwood (Kurzick)");
	case 57: return wstring(L"Fort Aspenwood (Luxon)");
	case 58: return wstring(L"Fort Ranik");
	case 59: return wstring(L"Fort Ranik (pre-searing)");
	case 60: return wstring(L"Frontier Gate");
	case 61: return wstring(L"Gadd's Encampment");
	case 62: return wstring(L"Gate of Anguish");
	case 63: return wstring(L"Gate of Desolation");
	case 64: return wstring(L"Gate of Fear");
	case 65: return wstring(L"Gate of Madness");
	case 66: return wstring(L"Gate of Pain");
	case 67: return wstring(L"Gate of Secrets");
	case 68: return wstring(L"Gate of the Nightfallen Lands");
	case 69: return wstring(L"Gate of Torment");
	case 70: return wstring(L"Gates of Kryta");
	case 71: return wstring(L"Grand Court of Sebelkeh");
	case 72: return wstring(L"Great Temple of Balthazar");
	case 73: return wstring(L"Grendich Courthouse");
	case 74: return wstring(L"Gunnar's Hold");
	case 75: return wstring(L"Gyala Hatchery");
	case 76: return wstring(L"Harvest Temple");
	case 77: return wstring(L"Hell's Precipice");
	case 78: return wstring(L"Henge of Denravi");
	case 79: return wstring(L"Heroes' Ascent");
	case 80: return wstring(L"Heroes' Audience");
	case 81: return wstring(L"Honur Hill");
	case 82: return wstring(L"House zu Heltzer");
	case 83: return wstring(L"Ice Caves of Sorrow");
	case 84: return wstring(L"Ice Tooth Cave");
	case 85: return wstring(L"Imperial Sanctum");
	case 86: return wstring(L"Iron Mines of Moladune");
	case 87: return wstring(L"Jade Flats (Kurzick)");
	case 88: return wstring(L"Jade Flats (Luxon)");
	case 89: return wstring(L"Jennur's Horde");
	case 90: return wstring(L"Jokanur Diggings");
	case 91: return wstring(L"Kaineng Center");
	case 92: return wstring(L"Kamadan, Jewel of Istan");
	case 93: return wstring(L"Kodlonu Hamlet");
	case 94: return wstring(L"Kodonur Crossroads");
	case 95: return wstring(L"Lair of the Forgotten");
	case 96: return wstring(L"Leviathan Pits");
	case 97: return wstring(L"Lion's Arch");
	case 98: return wstring(L"Longeye's Ledge");
	case 99: return wstring(L"Lutgardis Conservatory");
	case 100: return wstring(L"Maatu Keep");
	case 101: return wstring(L"Maguuma Stade");
	case 102: return wstring(L"Marhan's Grotto");
	case 103: return wstring(L"Mihanu Township");
	case 104: return wstring(L"Minister Cho's Estate");
	case 105: return wstring(L"Moddok Crevice");
	case 106: return wstring(L"Nahpui Quarter");
	case 107: return wstring(L"Nolani Academy");
	case 108: return wstring(L"Nundu Bay");
	case 109: return wstring(L"Olafstead");
	case 110: return wstring(L"Piken Square");
	case 111: return wstring(L"Pogahn Passage");
	case 112: return wstring(L"Port Sledge");
	case 113: return wstring(L"Quarrel Falls");
	case 114: return wstring(L"Raisu Palace");
	case 115: return wstring(L"Ran Musu Gardens");
	case 116: return wstring(L"Random Arenas");
	case 117: return wstring(L"Rata Sum");
	case 118: return wstring(L"Remains of Sahlahja");
	case 119: return wstring(L"Rilohn Refuge");
	case 120: return wstring(L"Ring of Fire");
	case 121: return wstring(L"Riverside Province");
	case 122: return wstring(L"Ruins of Morah");
	case 123: return wstring(L"Ruins of Surmia");
	case 124: return wstring(L"Saint Anjeka's Shrine");
	case 125: return wstring(L"Sanctum Cay");
	case 126: return wstring(L"Sardelac Sanitarium");
	case 127: return wstring(L"Seafarer's Rest");
	case 128: return wstring(L"Seeker's Passage");
	case 129: return wstring(L"Seitung Harbor");
	case 130: return wstring(L"Senji's Corner");
	case 131: return wstring(L"Serenity Temple");
	case 132: return wstring(L"Shing Jea Arena");
	case 133: return wstring(L"Shing Jea Monastery");
	case 134: return wstring(L"Sifhalla");
	case 135: return wstring(L"Sunjiang District");
	case 136: return wstring(L"Sunspear Arena");
	case 137: return wstring(L"Sunspear Great Hall");
	case 138: return wstring(L"Sunspear Sanctuary");
	case 139: return wstring(L"Tahnnakai Temple");
	case 140: return wstring(L"Tanglewood Copse");
	case 141: return wstring(L"Tarnished Haven");
	case 142: return wstring(L"Temple of the Ages");
	case 143: return wstring(L"The Amnoon Oasis");
	case 144: return wstring(L"The Astralarium");
	case 145: return wstring(L"The Aurios Mines");
	case 146: return wstring(L"The Barradin Estate (pre-searing)");
	case 147: return wstring(L"The Deep");
	case 148: return wstring(L"The Dragon's Lair");
	case 149: return wstring(L"The Eternal Grove");
	case 150: return wstring(L"The Frost Gate");
	case 151: return wstring(L"The Granite Citadel");
	case 152: return wstring(L"The Great Northern Wall");
	case 153: return wstring(L"The Jade Quarry (Kurzick)");
	case 154: return wstring(L"The Jade Quarry (Luxon)");
	case 155: return wstring(L"The Kodash Bazaar");
	case 156: return wstring(L"The Marketplace");
	case 157: return wstring(L"The Mouth of Torment");
	case 158: return wstring(L"The Shadow Nexus");
	case 159: return wstring(L"The Wilds");
	case 160: return wstring(L"Thirsty River");
	case 161: return wstring(L"Thunderhead Keep");
	case 162: return wstring(L"Tihark Orchard");
	case 163: return wstring(L"Tomb of the Primeval Kings");
	case 164: return wstring(L"Tsumei Village");
	case 165: return wstring(L"Umbral Grotto");
	case 166: return wstring(L"Unwaking Waters (Kurzick)");
	case 167: return wstring(L"Unwaking Waters (Luxon)");
	case 168: return wstring(L"Urgoz's Warren");
	case 169: return wstring(L"Vasburg Armory");
	case 170: return wstring(L"Venta Cemetery");
	case 171: return wstring(L"Ventari's Refuge");
	case 172: return wstring(L"Vizunah Square (Foreign Quarter)");
	case 173: return wstring(L"Vizunah Square (Local Quarter)");
	case 174: return wstring(L"Vlox's Falls");
	case 175: return wstring(L"Wehhan Terraces");
	case 176: return wstring(L"Yahnur Market");
	case 177: return wstring(L"Yak's Bend");
	case 178: return wstring(L"Yohlon Haven");
	case 179: return wstring(L"Zaishen Challenge");
	case 180: return wstring(L"Zaishen Elite");
	case 181: return wstring(L"Zaishen Menagerie");
	case 182: return wstring(L"Zen Daijun");
	case 183: return wstring(L"Zin Ku Corridor");
	case 184: return wstring(L"Zos Shivros Channel");
	default: return wstring(L"a bad map");
	}
}

GwConstants::MapID TravelPanel::IndexToOutpostID(int index) {
	using namespace GwConstants;
	switch (index) {
	case 0: return MapID::Abaddons_Gate;
	case 1: return MapID::Abaddons_Mouth;
	case 2: return MapID::Altrumm_Ruins;
	case 3: return MapID::Amatz_Basin;
	case 4: return MapID::Arborstone_outpost_mission;
	case 5: return MapID::Ascalon_City_outpost;
	case 6: return MapID::Ascalon_City_pre_searing;
	case 7: return MapID::Ashford_Abbey_outpost;
	case 8: return MapID::Aspenwood_Gate_Kurzick_outpost;
	case 9: return MapID::Aspenwood_Gate_Luxon_outpost;
	case 10: return MapID::Augury_Rock_outpost;
	case 11: return MapID::Aurora_Glade;
	case 12: return MapID::Bai_Paasu_Reach_outpost;
	case 13: return MapID::Basalt_Grotto_outpost;
	case 14: return MapID::Beacons_Perch_outpost;
	case 15: return MapID::Beetletun_outpost;
	case 16: return MapID::Beknur_Harbor_outpost;
	case 17: return MapID::Bergen_Hot_Springs_outpost;
	case 18: return MapID::Blacktide_Den;
	case 19: return MapID::Bloodstone_Fen;
	case 20: return MapID::Bone_Palace_outpost;
	case 21: return MapID::Boreal_Station_outpost;
	case 22: return MapID::Boreas_Seabed_outpost_mission;
	case 23: return MapID::Borlis_Pass;
	case 24: return MapID::Brauer_Academy_outpost;
	case 25: return MapID::Breaker_Hollow_outpost;
	case 26: return MapID::Camp_Hojanu_outpost;
	case 27: return MapID::Camp_Rankor_outpost;
	case 28: return MapID::Cavalon_outpost;
	case 29: return MapID::Central_Transfer_Chamber_outpost;
	case 30: return MapID::Chahbek_Village;
	case 31: return MapID::Champions_Dawn_outpost;
	case 32: return MapID::Chantry_of_Secrets_outpost;
	case 33: return MapID::Codex_Arena_outpost;
	case 34: return MapID::Consulate_Docks;
	case 35: return MapID::Copperhammer_Mines_outpost;
	case 36: return MapID::DAlessio_Seaboard;
	case 37: return MapID::Dajkah_Inlet;
	case 38: return MapID::Dasha_Vestibule;
	case 39: return MapID::Deldrimor_War_Camp_outpost;
	case 40: return MapID::Destinys_Gorge_outpost;
	case 41: return MapID::Divinity_Coast;
	case 42: return MapID::Doomlore_Shrine_outpost;
	case 43: return MapID::Dragons_Throat;
	case 44: return MapID::Droknars_Forge_outpost;
	case 45: return MapID::Druids_Overlook_outpost;
	case 46: return MapID::Dunes_of_Despair;
	case 47: return MapID::Durheim_Archives_outpost;
	case 48: return MapID::Dzagonur_Bastion;
	case 49: return MapID::Elona_Reach;
	case 50: return MapID::Embark_Beach;
	case 51: return MapID::Ember_Light_Camp_outpost;
	case 52: return MapID::Eredon_Terrace_outpost;
	case 53: return MapID::Eye_of_the_North_outpost;
	case 54: return MapID::Fishermens_Haven_outpost;
	case 55: return MapID::Foibles_Fair_outpost;
	case 56: return MapID::Fort_Aspenwood_Kurzick_outpost;
	case 57: return MapID::Fort_Aspenwood_Luxon_outpost;
	case 58: return MapID::Fort_Ranik;
	case 59: return MapID::Fort_Ranik_pre_Searing_outpost;
	case 60: return MapID::Frontier_Gate_outpost;
	case 61: return MapID::Gadds_Encampment_outpost;
	case 62: return MapID::Domain_of_Anguish;
	case 63: return MapID::Gate_of_Desolation;
	case 64: return MapID::Gate_of_Fear_outpost;
	case 65: return MapID::Gate_of_Madness;
	case 66: return MapID::Gate_of_Pain;
	case 67: return MapID::Gate_of_Secrets_outpost;
	case 68: return MapID::Gate_of_the_Nightfallen_Lands_outpost;
	case 69: return MapID::Gate_of_Torment_outpost;
	case 70: return MapID::Gates_of_Kryta;
	case 71: return MapID::Grand_Court_of_Sebelkeh;
	case 72: return MapID::Great_Temple_of_Balthazar_outpost;
	case 73: return MapID::Grendich_Courthouse_outpost;
	case 74: return MapID::Gunnars_Hold_outpost;
	case 75: return MapID::Gyala_Hatchery_outpost_mission;
	case 76: return MapID::Harvest_Temple_outpost;
	case 77: return MapID::Hells_Precipice;
	case 78: return MapID::Henge_of_Denravi_outpost;
	case 79: return MapID::Heroes_Ascent_outpost;
	case 80: return MapID::Heroes_Audience_outpost;
	case 81: return MapID::Honur_Hill_outpost;
	case 82: return MapID::House_zu_Heltzer_outpost;
	case 83: return MapID::Ice_Caves_of_Sorrow;
	case 84: return MapID::Ice_Tooth_Cave_outpost;
	case 85: return MapID::Imperial_Sanctum_outpost_mission;
	case 86: return MapID::Iron_Mines_of_Moladune;
	case 87: return MapID::Jade_Flats_Kurzick_outpost;
	case 88: return MapID::Jade_Flats_Luxon_outpost;
	case 89: return MapID::Jennurs_Horde;
	case 90: return MapID::Jokanur_Diggings;
	case 91: return MapID::Kaineng_Center_outpost;
	case 92: return MapID::Kamadan_Jewel_of_Istan_outpost;
	case 93: return MapID::Kodlonu_Hamlet_outpost;
	case 94: return MapID::Kodonur_Crossroads;
	case 95: return MapID::Lair_of_the_Forgotten_outpost;
	case 96: return MapID::Leviathan_Pits_outpost;
	case 97: return MapID::Lions_Arch_outpost;
	case 98: return MapID::Longeyes_Ledge_outpost;
	case 99: return MapID::Lutgardis_Conservatory_outpost;
	case 100: return MapID::Maatu_Keep_outpost;
	case 101: return MapID::Maguuma_Stade_outpost;
	case 102: return MapID::Marhans_Grotto_outpost;
	case 103: return MapID::Mihanu_Township_outpost;
	case 104: return MapID::Minister_Chos_Estate_outpost_mission;
	case 105: return MapID::Moddok_Crevice;
	case 106: return MapID::Nahpui_Quarter_outpost_mission;
	case 107: return MapID::Nolani_Academy;
	case 108: return MapID::Nundu_Bay;
	case 109: return MapID::Olafstead_outpost;
	case 110: return MapID::Piken_Square_outpost;
	case 111: return MapID::Pogahn_Passage;
	case 112: return MapID::Port_Sledge_outpost;
	case 113: return MapID::Quarrel_Falls_outpost;
	case 114: return MapID::Raisu_Palace_outpost_mission;
	case 115: return MapID::Ran_Musu_Gardens_outpost;
	case 116: return MapID::Random_Arenas_outpost;
	case 117: return MapID::Rata_Sum_outpost;
	case 118: return MapID::Remains_of_Sahlahja;
	case 119: return MapID::Rilohn_Refuge;
	case 120: return MapID::Ring_of_Fire;
	case 121: return MapID::Riverside_Province;
	case 122: return MapID::Ruins_of_Morah;
	case 123: return MapID::Ruins_of_Surmia;
	case 124: return MapID::Saint_Anjekas_Shrine_outpost;
	case 125: return MapID::Sanctum_Cay;
	case 126: return MapID::Sardelac_Sanitarium_outpost;
	case 127: return MapID::Seafarers_Rest_outpost;
	case 128: return MapID::Seekers_Passage_outpost;
	case 129: return MapID::Seitung_Harbor_outpost;
	case 130: return MapID::Senjis_Corner_outpost;
	case 131: return MapID::Serenity_Temple_outpost;
	case 132: return MapID::Shing_Jea_Arena;
	case 133: return MapID::Shing_Jea_Monastery_outpost;
	case 134: return MapID::Sifhalla_outpost;
	case 135: return MapID::Sunjiang_District_outpost_mission;
	case 136: return MapID::Sunspear_Arena;
	case 137: return MapID::Sunspear_Great_Hall_outpost;
	case 138: return MapID::Sunspear_Sanctuary_outpost;
	case 139: return MapID::Tahnnakai_Temple_outpost_mission;
	case 140: return MapID::Tanglewood_Copse_outpost;
	case 141: return MapID::Tarnished_Haven_outpost;
	case 142: return MapID::Temple_of_the_Ages;
	case 143: return MapID::The_Amnoon_Oasis_outpost;
	case 144: return MapID::The_Astralarium_outpost;
	case 145: return MapID::The_Aurios_Mines;
	case 146: return MapID::The_Barradin_Estate_outpost;
	case 147: return MapID::The_Deep;
	case 148: return MapID::The_Dragons_Lair;
	case 149: return MapID::The_Eternal_Grove_outpost_mission;
	case 150: return MapID::The_Frost_Gate;
	case 151: return MapID::The_Granite_Citadel_outpost;
	case 152: return MapID::The_Great_Northern_Wall;
	case 153: return MapID::The_Jade_Quarry_Kurzick_outpost;
	case 154: return MapID::The_Jade_Quarry_Luxon_outpost;
	case 155: return MapID::The_Kodash_Bazaar_outpost;
	case 156: return MapID::The_Marketplace_outpost;
	case 157: return MapID::The_Mouth_of_Torment_outpost;
	case 158: return MapID::The_Shadow_Nexus;
	case 159: return MapID::The_Wilds;
	case 160: return MapID::Thirsty_River;
	case 161: return MapID::Thunderhead_Keep;
	case 162: return MapID::Tihark_Orchard;
	case 163: return MapID::Tomb_of_the_Primeval_Kings;
	case 164: return MapID::Tsumei_Village_outpost;
	case 165: return MapID::Umbral_Grotto_outpost;
	case 166: return MapID::Unwaking_Waters_Kurzick_outpost;
	case 167: return MapID::Unwaking_Waters_Luxon_outpost;
	case 168: return MapID::Urgozs_Warren;
	case 169: return MapID::Vasburg_Armory_outpost;
	case 170: return MapID::Venta_Cemetery;
	case 171: return MapID::Ventaris_Refuge_outpost;
	case 172: return MapID::Vizunah_Square_Foreign_Quarter_outpost;
	case 173: return MapID::Vizunah_Square_Local_Quarter_outpost;
	case 174: return MapID::Vloxs_Falls;
	case 175: return MapID::Wehhan_Terraces_outpost;
	case 176: return MapID::Yahnur_Market_outpost;
	case 177: return MapID::Yaks_Bend_outpost;
	case 178: return MapID::Yohlon_Haven_outpost;
	case 179: return MapID::Zaishen_Challenge_outpost;
	case 180: return MapID::Zaishen_Elite_outpost;
	case 181: return MapID::Zaishen_Menagerie_outpost;
	case 182: return MapID::Zen_Daijun_outpost_mission;
	case 183: return MapID::Zin_Ku_Corridor_outpost;
	case 184: return MapID::Zos_Shivros_Channel;
	default: return MapID::Great_Temple_of_Balthazar_outpost;
	}
}