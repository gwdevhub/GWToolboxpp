#include "TravelPanel.h"
#include "../API/APIMain.h"
#include "MainWindow.h"
#include "GWToolbox.h"
#include "Config.h"
#include <string>

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

	SetSize(WIDTH, HEIGHT);

	ComboBox* combo = new TravelCombo();
	combo->SetMaxShowItems(10);
	combo->SetText("Travel To...");
	for (int i = 0; i < n_outposts; ++i) {
		combo->AddItem(IndexToOutpostName(i));
	}
	combo->SetSize(GetWidth() - 2 * SPACE, BUTTON_HEIGHT);
	combo->SetLocation(SPACE, SPACE);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {
		if (combo->GetSelectedIndex() < 0) return;
		GwConstants::MapID id = IndexToOutpostID(combo->GetSelectedIndex());
		GWAPIMgr::instance()->Map()->Travel(id, this->district(), this->region(), this->language());
		combo->SetText("Travel To...");
		combo->SetSelectedIndex(-1);
	});
	AddControl(combo);

	ComboBox* district = new ComboBox();
	district->SetMaxShowItems(14);
	district->AddItem("Current District");
	district->AddItem("International");
	district->AddItem("American");
	district->AddItem("American District 1");
	district->AddItem("Europe English");
	district->AddItem("Europe French");
	district->AddItem("Europe German");
	district->AddItem("Europe Italian");
	district->AddItem("Europe Spanish");
	district->AddItem("Europe Polish");
	district->AddItem("Europe Russian");
	district->AddItem("Asian Korean");
	district->AddItem("Asia Chinese");
	district->AddItem("Asia Japanese");
	district->SetSize(GetWidth() - 2 * SPACE, BUTTON_HEIGHT);
	district->SetLocation(SPACE, district->GetBottom() + SPACE);
	district->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, district](Control*) {
		UpdateDistrict(district->GetSelectedIndex());
	});
	district->SetSelectedIndex(0);
	AddControl(district);

	using namespace GwConstants;
	AddTravelButton("ToA", 0, 2, MapID::Temple_of_the_Ages);
	AddTravelButton("DoA", 1, 2, MapID::Domain_of_Anguish);
	AddTravelButton("Kamadan", 0, 3, MapID::Kamadan_Jewel_of_Istan_outpost);
	AddTravelButton("Embark", 1, 3, MapID::Embark_Beach);
	AddTravelButton("Vlox's", 0, 4, MapID::Vloxs_Falls);
	AddTravelButton("Gadd's", 1, 4, MapID::Gadds_Encampment_outpost);
	AddTravelButton("Urgoz", 0, 5, MapID::Urgozs_Warren);
	AddTravelButton("Deep", 1, 5, MapID::The_Deep);

	for (int i = 0; i < 3; ++i) {
		wstring key = wstring(L"Travel") + to_wstring(i);
		int index = GWToolbox::instance()->config()->iniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new TravelCombo();
		fav_combo->SetSize((WIDTH - 3 * SPACE) * 3 / 4, BUTTON_HEIGHT);
		fav_combo->SetLocation(DefaultBorderPadding, SPACE * 2 + (BUTTON_HEIGHT + SPACE) * (i + 6));
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
		fav_button->SetSize((WIDTH - 3 * SPACE) / 4, BUTTON_HEIGHT);
		fav_button->SetLocation(fav_combo->GetRight() + DefaultBorderPadding, fav_combo->GetTop());
		fav_button->SetText("Go");
		fav_button->GetClickEvent() += ClickEventHandler([this, fav_combo](Control*) {
			int index = fav_combo->GetSelectedIndex();
			GWAPIMgr::instance()->Map()->Travel(IndexToOutpostID(index),
				this->district(), this->region(), this->language());
		});
		AddControl(fav_button);
	}
}

TravelPanel::TravelCombo::TravelCombo() {
	listBox_->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	listBox_->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
}

DWORD TravelPanel::region() {
	if (current_district_) {
		return GWAPIMgr::instance()->Map()->GetRegion();
	} else {
		return region_;
	}
}

DWORD TravelPanel::language() {
	if (current_district_) {
		return GWAPIMgr::instance()->Map()->GetLanguage();
	} else {
		return language_;
	}
}


void TravelPanel::AddTravelButton(string text, int grid_x, int grid_y, GwConstants::MapID map_id) {
	Button* button = new Button();
	button->SetText(text);
	button->SetSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	button->SetLocation(SPACE + (BUTTON_WIDTH + SPACE) * grid_x, 
		SPACE * 2 + (BUTTON_HEIGHT + SPACE) * grid_y);
	button->GetClickEvent() += ClickEventHandler([this, map_id](Control*) {
		GWAPIMgr::instance()->Map()->Travel(map_id, district(), region(), language());
	});
	AddControl(button);
}

void TravelPanel::UpdateDistrict(int gui_index) {
	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::instance();
	current_district_ = false;
	region_ = api->Map()->GetRegion();
	district_ = 0;
	language_ = api->Map()->GetLanguage();
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

string TravelPanel::IndexToOutpostName(int index) {
	switch (index) {
	case 0: return string("Abaddon's Gate");
	case 1: return string("Abaddon's Mouth");
	case 2: return string("Altrumm Ruins");
	case 3: return string("Amatz Basin");
	case 4: return string("Arborstone");
	case 5: return string("Ascalon City");
	case 6: return string("Ascalon City (pre-searing)");
	case 7: return string("Ashford Abbey (pre-searing)");
	case 8: return string("Aspenwood Gate (Kurzick)");
	case 9: return string("Aspenwood Gate (Luxon)");
	case 10: return string("Augury Rock");
	case 11: return string("Aurora Glade");
	case 12: return string("Bai Paasu Reach");
	case 13: return string("Basalt Grotto");
	case 14: return string("Beacon's Perch");
	case 15: return string("Beetletun");
	case 16: return string("Beknur Harbor");
	case 17: return string("Bergen Hot Springs");
	case 18: return string("Blacktide Den");
	case 19: return string("Bloodstone Fen");
	case 20: return string("Bone Palace");
	case 21: return string("Boreal Station");
	case 22: return string("Boreas Seabed");
	case 23: return string("Borlis Pass");
	case 24: return string("Brauer Academy");
	case 25: return string("Breaker Hollow");
	case 26: return string("Camp Hojanu");
	case 27: return string("Camp Rankor");
	case 28: return string("Cavalon");
	case 29: return string("Central Transfer Chamber");
	case 30: return string("Chahbek Village");
	case 31: return string("Champion's Dawn");
	case 32: return string("Chantry of Secrets");
	case 33: return string("Codex Arena");
	case 34: return string("Consulate Docks");
	case 35: return string("Copperhammer Mines");
	case 36: return string("D'Alessio Seaboard");
	case 37: return string("Dajkah Inlet");
	case 38: return string("Dasha Vestibule");
	case 39: return string("Deldrimor War Camp");
	case 40: return string("Destiny's Gorge");
	case 41: return string("Divinity Coast");
	case 42: return string("Doomlore Shrine");
	case 43: return string("Dragon's Throat");
	case 44: return string("Droknar's Forge");
	case 45: return string("Druid's Overlook");
	case 46: return string("Dunes of Despair");
	case 47: return string("Durheim Archives");
	case 48: return string("Dzagonur Bastion");
	case 49: return string("Elona Reach");
	case 50: return string("Embark Beach");
	case 51: return string("Ember Light Camp");
	case 52: return string("Eredon Terrace");
	case 53: return string("Eye of the North");
	case 54: return string("Fishermen's Haven");
	case 55: return string("Foible's Fair (pre-searing)");
	case 56: return string("Fort Aspenwood (Kurzick)");
	case 57: return string("Fort Aspenwood (Luxon)");
	case 58: return string("Fort Ranik");
	case 59: return string("Fort Ranik (pre-searing)");
	case 60: return string("Frontier Gate");
	case 61: return string("Gadd's Encampment");
	case 62: return string("Gate of Anguish");
	case 63: return string("Gate of Desolation");
	case 64: return string("Gate of Fear");
	case 65: return string("Gate of Madness");
	case 66: return string("Gate of Pain");
	case 67: return string("Gate of Secrets");
	case 68: return string("Gate of the Nightfallen Lands");
	case 69: return string("Gate of Torment");
	case 70: return string("Gates of Kryta");
	case 71: return string("Grand Court of Sebelkeh");
	case 72: return string("Great Temple of Balthazar");
	case 73: return string("Grendich Courthouse");
	case 74: return string("Gunnar's Hold");
	case 75: return string("Gyala Hatchery");
	case 76: return string("Harvest Temple");
	case 77: return string("Hell's Precipice");
	case 78: return string("Henge of Denravi");
	case 79: return string("Heroes' Ascent");
	case 80: return string("Heroes' Audience");
	case 81: return string("Honur Hill");
	case 82: return string("House zu Heltzer");
	case 83: return string("Ice Caves of Sorrow");
	case 84: return string("Ice Tooth Cave");
	case 85: return string("Imperial Sanctum");
	case 86: return string("Iron Mines of Moladune");
	case 87: return string("Jade Flats (Kurzick)");
	case 88: return string("Jade Flats (Luxon)");
	case 89: return string("Jennur's Horde");
	case 90: return string("Jokanur Diggings");
	case 91: return string("Kaineng Center");
	case 92: return string("Kamadan, Jewel of Istan");
	case 93: return string("Kodlonu Hamlet");
	case 94: return string("Kodonur Crossroads");
	case 95: return string("Lair of the Forgotten");
	case 96: return string("Leviathan Pits");
	case 97: return string("Lion's Arch");
	case 98: return string("Longeye's Ledge");
	case 99: return string("Lutgardis Conservatory");
	case 100: return string("Maatu Keep");
	case 101: return string("Maguuma Stade");
	case 102: return string("Marhan's Grotto");
	case 103: return string("Mihanu Township");
	case 104: return string("Minister Cho's Estate");
	case 105: return string("Moddok Crevice");
	case 106: return string("Nahpui Quarter");
	case 107: return string("Nolani Academy");
	case 108: return string("Nundu Bay");
	case 109: return string("Olafstead");
	case 110: return string("Piken Square");
	case 111: return string("Pogahn Passage");
	case 112: return string("Port Sledge");
	case 113: return string("Quarrel Falls");
	case 114: return string("Raisu Palace");
	case 115: return string("Ran Musu Gardens");
	case 116: return string("Random Arenas");
	case 117: return string("Rata Sum");
	case 118: return string("Remains of Sahlahja");
	case 119: return string("Rilohn Refuge");
	case 120: return string("Ring of Fire");
	case 121: return string("Riverside Province");
	case 122: return string("Ruins of Morah");
	case 123: return string("Ruins of Surmia");
	case 124: return string("Saint Anjeka's Shrine");
	case 125: return string("Sanctum Cay");
	case 126: return string("Sardelac Sanitarium");
	case 127: return string("Seafarer's Rest");
	case 128: return string("Seeker's Passage");
	case 129: return string("Seitung Harbor");
	case 130: return string("Senji's Corner");
	case 131: return string("Serenity Temple");
	case 132: return string("Shing Jea Arena");
	case 133: return string("Shing Jea Monastery");
	case 134: return string("Sifhalla");
	case 135: return string("Sunjiang District");
	case 136: return string("Sunspear Arena");
	case 137: return string("Sunspear Great Hall");
	case 138: return string("Sunspear Sanctuary");
	case 139: return string("Tahnnakai Temple");
	case 140: return string("Tanglewood Copse");
	case 141: return string("Tarnished Haven");
	case 142: return string("Temple of the Ages");
	case 143: return string("The Amnoon Oasis");
	case 144: return string("The Astralarium");
	case 145: return string("The Aurios Mines");
	case 146: return string("The Barradin Estate (pre-searing)");
	case 147: return string("The Deep");
	case 148: return string("The Dragon's Lair");
	case 149: return string("The Eternal Grove");
	case 150: return string("The Frost Gate");
	case 151: return string("The Granite Citadel");
	case 152: return string("The Great Northern Wall");
	case 153: return string("The Jade Quarry (Kurzick)");
	case 154: return string("The Jade Quarry (Luxon)");
	case 155: return string("The Kodash Bazaar");
	case 156: return string("The Marketplace");
	case 157: return string("The Mouth of Torment");
	case 158: return string("The Shadow Nexus");
	case 159: return string("The Wilds");
	case 160: return string("Thirsty River");
	case 161: return string("Thunderhead Keep");
	case 162: return string("Tihark Orchard");
	case 163: return string("Tomb of the Primeval Kings");
	case 164: return string("Tsumei Village");
	case 165: return string("Umbral Grotto");
	case 166: return string("Unwaking Waters (Kurzick)");
	case 167: return string("Unwaking Waters (Luxon)");
	case 168: return string("Urgoz's Warren");
	case 169: return string("Vasburg Armory");
	case 170: return string("Venta Cemetery");
	case 171: return string("Ventari's Refuge");
	case 172: return string("Vizunah Square (Foreign Quarter)");
	case 173: return string("Vizunah Square (Local Quarter)");
	case 174: return string("Vlox's Falls");
	case 175: return string("Wehhan Terraces");
	case 176: return string("Yahnur Market");
	case 177: return string("Yak's Bend");
	case 178: return string("Yohlon Haven");
	case 179: return string("Zaishen Challenge");
	case 180: return string("Zaishen Elite");
	case 181: return string("Zaishen Menagerie");
	case 182: return string("Zen Daijun");
	case 183: return string("Zin Ku Corridor");
	case 184: return string("Zos Shivros Channel");
	default: return string("a bad map");
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