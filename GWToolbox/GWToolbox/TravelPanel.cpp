#include "TravelPanel.h"

#include <string>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

#include "MainWindow.h"
#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"


using namespace OSHGui;

void TravelPanel::BuildUI() {

	ComboBox* combo = new TravelCombo(this);
	combo->SetMaxShowItems(10);
	combo->SetText("Travel To...");
	for (int i = 0; i < n_outposts; ++i) {
		combo->AddItem(IndexToOutpostName(i));
	}
	combo->SetSize(SizeI(GetWidth() - 2 * Padding, GuiUtils::BUTTON_HEIGHT));
	combo->SetLocation(PointI(Padding, Padding));
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo](Control*) {
		if (combo->GetSelectedIndex() < 0) return;
		GW::Constants::MapID id = IndexToOutpostID(combo->GetSelectedIndex());
		GW::Map().Travel(id, district_, district_number_);
		combo->SetText("Travel To...");
		combo->SetSelectedIndex(-1);
	});
	AddControl(combo);

	ComboBox* district = new ComboBox(this);
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
	district->SetSize(SizeI(GetWidth() - 2 * Padding, GuiUtils::BUTTON_HEIGHT));
	district->SetLocation(PointI(Padding, district->GetBottom() + Padding));
	district->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, district](Control*) {
		UpdateDistrict(district->GetSelectedIndex());
	});
	district->SetSelectedIndex(0);
	AddControl(district);

	using namespace GW::Constants;
	AddTravelButton("ToA", 0, 2, MapID::Temple_of_the_Ages);
	AddTravelButton("DoA", 1, 2, MapID::Domain_of_Anguish);
	AddTravelButton("Kamadan", 0, 3, MapID::Kamadan_Jewel_of_Istan_outpost);
	AddTravelButton("Embark", 1, 3, MapID::Embark_Beach);
	AddTravelButton("Vlox's", 0, 4, MapID::Vloxs_Falls);
	AddTravelButton("Gadd's", 1, 4, MapID::Gadds_Encampment_outpost);
	AddTravelButton("Urgoz", 0, 5, MapID::Urgozs_Warren);
	AddTravelButton("Deep", 1, 5, MapID::The_Deep);

	for (int i = 0; i < 3; ++i) {
		std::string key = std::string("Travel") + std::to_string(i);
		int index = Config::IniRead(MainWindow::IniSection(), key.c_str(), 0l);
		ComboBox* fav_combo = new TravelCombo(this);
		fav_combo->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::BUTTON_HEIGHT));
		fav_combo->SetLocation(PointI(Padding, Padding + (GuiUtils::BUTTON_HEIGHT + Padding) * (i + 6)));
		for (int j = 0; j < n_outposts; ++j) {
			fav_combo->AddItem(IndexToOutpostName(j));
		}
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			Config::IniWrite(MainWindow::IniSection(),
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* fav_button = new Button(this);
		fav_button->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4), GuiUtils::BUTTON_HEIGHT));
		fav_button->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 3), fav_combo->GetTop()));
		fav_button->SetText("Go");
		fav_button->GetClickEvent() += ClickEventHandler([this, i](Control*) {
			TravelFavorite(i);
		});
		AddControl(fav_button);

		combo_boxes_[i] = fav_combo;
	}
}

void TravelPanel::TravelFavorite(int fav_idx) {
	if (fav_idx >= 0 && fav_idx < 3) {
		int outpost_idx = combo_boxes_[fav_idx]->GetSelectedIndex();
		GW::Map().Travel(IndexToOutpostID(outpost_idx), district_, district_number_);
	}
}

TravelPanel::TravelCombo::TravelCombo(OSHGui::Control* parent)
	: OSHGui::ComboBox(parent) {
	listBox_->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	listBox_->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
}


void TravelPanel::AddTravelButton(std::string text, int grid_x, int grid_y, GW::Constants::MapID map_id) {
	Button* button = new Button(this);
	button->SetText(text);
	button->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 2), GuiUtils::BUTTON_HEIGHT));
	button->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 2, grid_x), 
		Padding + (GuiUtils::BUTTON_HEIGHT + Padding) * grid_y));
	button->GetClickEvent() += ClickEventHandler([this, map_id](Control*) {
		GW::Map().Travel(map_id, district_, district_number_);
	});
	AddControl(button);
}

void TravelPanel::UpdateDistrict(int gui_index) {
	district_number_ = 0;
	switch (gui_index) {
	case 0: district_ = GW::Constants::District::Current; break;
	case 1: district_ = GW::Constants::District::International; break;
	case 2: district_ = GW::Constants::District::American; break;
	case 3: // American District 1
		district_ = GW::Constants::District::American;
		district_number_ = 1;
		break;
	case 4: district_ = GW::Constants::District::EuropeEnglish; break;
	case 5: district_ = GW::Constants::District::EuropeFrench; break;
	case 6: district_ = GW::Constants::District::EuropeGerman; break;
	case 7: district_ = GW::Constants::District::EuropeItalian; break;
	case 8: district_ = GW::Constants::District::EuropeSpanish; break;
	case 9: district_ = GW::Constants::District::EuropePolish; break;
	case 10: district_ = GW::Constants::District::EuropeRussian; break;
	case 11: district_ = GW::Constants::District::AsiaKorean; break;
	case 12: district_ = GW::Constants::District::AsiaChinese; break;
	case 13: district_ = GW::Constants::District::AsiaJapanese; break;
	default:
		break;
	}
}

std::string TravelPanel::IndexToOutpostName(int index) {
	switch (index) {
	case 0: return std::string("Abaddon's Gate");
	case 1: return std::string("Abaddon's Mouth");
	case 2: return std::string("Altrumm Ruins");
	case 3: return std::string("Amatz Basin");
	case 4: return std::string("Amnoon Oasis, The");
	case 5: return std::string("Arborstone");
	case 6: return std::string("Ascalon City");
	case 7: return std::string("Aspenwood Gate (Kurzick)");
	case 8: return std::string("Aspenwood Gate (Luxon)");
	case 9: return std::string("Astralarium, The");
	case 10: return std::string("Augury Rock");
	case 11: return std::string("Aurios Mines, The");
	case 12: return std::string("Aurora Glade");
	case 13: return std::string("Bai Paasu Reach");
	case 14: return std::string("Basalt Grotto");
	case 15: return std::string("Beacon's Perch");
	case 16: return std::string("Beetletun");
	case 17: return std::string("Beknur Harbor");
	case 18: return std::string("Bergen Hot Springs");
	case 19: return std::string("Blacktide Den");
	case 20: return std::string("Bloodstone Fen");
	case 21: return std::string("Bone Palace");
	case 22: return std::string("Boreal Station");
	case 23: return std::string("Boreas Seabed");
	case 24: return std::string("Borlis Pass");
	case 25: return std::string("Brauer Academy");
	case 26: return std::string("Breaker Hollow");
	case 27: return std::string("Camp Hojanu");
	case 28: return std::string("Camp Rankor");
	case 29: return std::string("Cavalon");
	case 30: return std::string("Central Transfer Chamber");
	case 31: return std::string("Chahbek Village");
	case 32: return std::string("Champion's Dawn");
	case 33: return std::string("Chantry of Secrets");
	case 34: return std::string("Codex Arena");
	case 35: return std::string("Consulate Docks");
	case 36: return std::string("Copperhammer Mines");
	case 37: return std::string("D'Alessio Seaboard");
	case 38: return std::string("Dajkah Inlet");
	case 39: return std::string("Dasha Vestibule");
	case 40: return std::string("Deep, The");
	case 41: return std::string("Deldrimor War Camp");
	case 42: return std::string("Destiny's Gorge");
	case 43: return std::string("Divinity Coast");
	case 44: return std::string("Doomlore Shrine");
	case 45: return std::string("Dragon's Lair, The");
	case 46: return std::string("Dragon's Throat");
	case 47: return std::string("Droknar's Forge");
	case 48: return std::string("Druid's Overlook");
	case 49: return std::string("Dunes of Despair");
	case 50: return std::string("Durheim Archives");
	case 51: return std::string("Dzagonur Bastion");
	case 52: return std::string("Elona Reach");
	case 53: return std::string("Embark Beach");
	case 54: return std::string("Ember Light Camp");
	case 55: return std::string("Eredon Terrace");
	case 56: return std::string("Eternal Grove, The");
	case 57: return std::string("Eye of the North");
	case 58: return std::string("Fishermen's Haven");
	case 59: return std::string("Fort Aspenwood (Kurzick)");
	case 60: return std::string("Fort Aspenwood (Luxon)");
	case 61: return std::string("Fort Ranik");
	case 62: return std::string("Frontier Gate");
	case 63: return std::string("Frost Gate, The");
	case 64: return std::string("Gadd's Encampment");
	case 65: return std::string("Gate of Anguish");
	case 66: return std::string("Gate of Desolation");
	case 67: return std::string("Gate of Fear");
	case 68: return std::string("Gate of Madness");
	case 69: return std::string("Gate of Pain");
	case 70: return std::string("Gate of Secrets");
	case 71: return std::string("Gate of the Nightfallen Lands");
	case 72: return std::string("Gate of Torment");
	case 73: return std::string("Gates of Kryta");
	case 74: return std::string("Grand Court of Sebelkeh");
	case 75: return std::string("Granite Citadel, The");
	case 76: return std::string("Great Northern Wall, The");
	case 77: return std::string("Great Temple of Balthazar");
	case 78: return std::string("Grendich Courthouse");
	case 79: return std::string("Gunnar's Hold");
	case 80: return std::string("Gyala Hatchery");
	case 81: return std::string("Harvest Temple");
	case 82: return std::string("Hell's Precipice");
	case 83: return std::string("Henge of Denravi");
	case 84: return std::string("Heroes' Ascent");
	case 85: return std::string("Heroes' Audience");
	case 86: return std::string("Honur Hill");
	case 87: return std::string("House zu Heltzer");
	case 88: return std::string("Ice Caves of Sorrow");
	case 89: return std::string("Ice Tooth Cave");
	case 90: return std::string("Imperial Sanctum");
	case 91: return std::string("Iron Mines of Moladune");
	case 92: return std::string("Jade Flats (Kurzick)");
	case 93: return std::string("Jade Flats (Luxon)");
	case 94: return std::string("Jade Quarry (Kurzick), The");
	case 95: return std::string("Jade Quarry (Luxon), The");
	case 96: return std::string("Jennur's Horde");
	case 97: return std::string("Jokanur Diggings");
	case 98: return std::string("Kaineng Center");
	case 99: return std::string("Kamadan, Jewel of Istan");
	case 100: return std::string("Kodash Bazaar, The");
	case 101: return std::string("Kodlonu Hamlet");
	case 102: return std::string("Kodonur Crossroads");
	case 103: return std::string("Lair of the Forgotten");
	case 104: return std::string("Leviathan Pits");
	case 105: return std::string("Lion's Arch");
	case 106: return std::string("Longeye's Ledge");
	case 107: return std::string("Lutgardis Conservatory");
	case 108: return std::string("Maatu Keep");
	case 109: return std::string("Maguuma Stade");
	case 110: return std::string("Marhan's Grotto");
	case 111: return std::string("Marketplace, The");
	case 112: return std::string("Mihanu Township");
	case 113: return std::string("Minister Cho's Estate");
	case 114: return std::string("Moddok Crevice");
	case 115: return std::string("Mouth of Torment, The");
	case 116: return std::string("Nahpui Quarter");
	case 117: return std::string("Nolani Academy");
	case 118: return std::string("Nundu Bay");
	case 119: return std::string("Olafstead");
	case 120: return std::string("Piken Square");
	case 121: return std::string("Pogahn Passage");
	case 122: return std::string("Port Sledge");
	case 123: return std::string("Quarrel Falls");
	case 124: return std::string("Raisu Palace");
	case 125: return std::string("Ran Musu Gardens");
	case 126: return std::string("Random Arenas");
	case 127: return std::string("Rata Sum");
	case 128: return std::string("Remains of Sahlahja");
	case 129: return std::string("Rilohn Refuge");
	case 130: return std::string("Ring of Fire");
	case 131: return std::string("Riverside Province");
	case 132: return std::string("Ruins of Morah");
	case 133: return std::string("Ruins of Surmia");
	case 134: return std::string("Saint Anjeka's Shrine");
	case 135: return std::string("Sanctum Cay");
	case 136: return std::string("Sardelac Sanitarium");
	case 137: return std::string("Seafarer's Rest");
	case 138: return std::string("Seeker's Passage");
	case 139: return std::string("Seitung Harbor");
	case 140: return std::string("Senji's Corner");
	case 141: return std::string("Serenity Temple");
	case 142: return std::string("Shadow Nexus, The");
	case 143: return std::string("Shing Jea Arena");
	case 144: return std::string("Shing Jea Monastery");
	case 145: return std::string("Sifhalla");
	case 146: return std::string("Sunjiang District");
	case 147: return std::string("Sunspear Arena");
	case 148: return std::string("Sunspear Great Hall");
	case 149: return std::string("Sunspear Sanctuary");
	case 150: return std::string("Tahnnakai Temple");
	case 151: return std::string("Tanglewood Copse");
	case 152: return std::string("Tarnished Haven");
	case 153: return std::string("Temple of the Ages");
	case 154: return std::string("Thirsty River");
	case 155: return std::string("Thunderhead Keep");
	case 156: return std::string("Tihark Orchard");
	case 157: return std::string("Tomb of the Primeval Kings");
	case 158: return std::string("Tsumei Village");
	case 159: return std::string("Umbral Grotto");
	case 160: return std::string("Unwaking Waters (Kurzick)");
	case 161: return std::string("Unwaking Waters (Luxon)");
	case 162: return std::string("Urgoz's Warren");
	case 163: return std::string("Vasburg Armory");
	case 164: return std::string("Venta Cemetery");
	case 165: return std::string("Ventari's Refuge");
	case 166: return std::string("Vizunah Square (Foreign Quarter)");
	case 167: return std::string("Vizunah Square (Local Quarter)");
	case 168: return std::string("Vlox's Falls");
	case 169: return std::string("Wehhan Terraces");
	case 170: return std::string("Wilds, The");
	case 171: return std::string("Yahnur Market");
	case 172: return std::string("Yak's Bend");
	case 173: return std::string("Yohlon Haven");
	case 174: return std::string("Zaishen Challenge");
	case 175: return std::string("Zaishen Elite");
	case 176: return std::string("Zaishen Menagerie");
	case 177: return std::string("Zen Daijun");
	case 178: return std::string("Zin Ku Corridor");
	case 179: return std::string("Zos Shivros Channel");
	default: return std::string("a bad map");
	}
}

GW::Constants::MapID TravelPanel::IndexToOutpostID(int index) {
	using namespace GW::Constants;
	switch (index) {
	case 0: return MapID::Abaddons_Gate;
	case 1: return MapID::Abaddons_Mouth;
	case 2: return MapID::Altrumm_Ruins;
	case 3: return MapID::Amatz_Basin;
	case 4: return MapID::The_Amnoon_Oasis_outpost;
	case 5: return MapID::Arborstone_outpost_mission;
	case 6: return MapID::Ascalon_City_outpost;
	case 7: return MapID::Aspenwood_Gate_Kurzick_outpost;
	case 8: return MapID::Aspenwood_Gate_Luxon_outpost;
	case 9: return MapID::The_Astralarium_outpost;
	case 10: return MapID::Augury_Rock_outpost;
	case 11: return MapID::The_Aurios_Mines;
	case 12: return MapID::Aurora_Glade;
	case 13: return MapID::Bai_Paasu_Reach_outpost;
	case 14: return MapID::Basalt_Grotto_outpost;
	case 15: return MapID::Beacons_Perch_outpost;
	case 16: return MapID::Beetletun_outpost;
	case 17: return MapID::Beknur_Harbor_outpost;
	case 18: return MapID::Bergen_Hot_Springs_outpost;
	case 19: return MapID::Blacktide_Den;
	case 20: return MapID::Bloodstone_Fen;
	case 21: return MapID::Bone_Palace_outpost;
	case 22: return MapID::Boreal_Station_outpost;
	case 23: return MapID::Boreas_Seabed_outpost_mission;
	case 24: return MapID::Borlis_Pass;
	case 25: return MapID::Brauer_Academy_outpost;
	case 26: return MapID::Breaker_Hollow_outpost;
	case 27: return MapID::Camp_Hojanu_outpost;
	case 28: return MapID::Camp_Rankor_outpost;
	case 29: return MapID::Cavalon_outpost;
	case 30: return MapID::Central_Transfer_Chamber_outpost;
	case 31: return MapID::Chahbek_Village;
	case 32: return MapID::Champions_Dawn_outpost;
	case 33: return MapID::Chantry_of_Secrets_outpost;
	case 34: return MapID::Codex_Arena_outpost;
	case 35: return MapID::Consulate_Docks;
	case 36: return MapID::Copperhammer_Mines_outpost;
	case 37: return MapID::DAlessio_Seaboard;
	case 38: return MapID::Dajkah_Inlet;
	case 39: return MapID::Dasha_Vestibule;
	case 40: return MapID::The_Deep;
	case 41: return MapID::Deldrimor_War_Camp_outpost;
	case 42: return MapID::Destinys_Gorge_outpost;
	case 43: return MapID::Divinity_Coast;
	case 44: return MapID::Doomlore_Shrine_outpost;
	case 45: return MapID::The_Dragons_Lair;
	case 46: return MapID::Dragons_Throat;
	case 47: return MapID::Droknars_Forge_outpost;
	case 48: return MapID::Druids_Overlook_outpost;
	case 49: return MapID::Dunes_of_Despair;
	case 50: return MapID::Durheim_Archives_outpost;
	case 51: return MapID::Dzagonur_Bastion;
	case 52: return MapID::Elona_Reach;
	case 53: return MapID::Embark_Beach;
	case 54: return MapID::Ember_Light_Camp_outpost;
	case 55: return MapID::Eredon_Terrace_outpost;
	case 56: return MapID::The_Eternal_Grove_outpost_mission;
	case 57: return MapID::Eye_of_the_North_outpost;
	case 58: return MapID::Fishermens_Haven_outpost;
	case 59: return MapID::Fort_Aspenwood_Kurzick_outpost;
	case 60: return MapID::Fort_Aspenwood_Luxon_outpost;
	case 61: return MapID::Fort_Ranik;
	case 62: return MapID::Frontier_Gate_outpost;
	case 63: return MapID::The_Frost_Gate;
	case 64: return MapID::Gadds_Encampment_outpost;
	case 65: return MapID::Domain_of_Anguish;
	case 66: return MapID::Gate_of_Desolation;
	case 67: return MapID::Gate_of_Fear_outpost;
	case 68: return MapID::Gate_of_Madness;
	case 69: return MapID::Gate_of_Pain;
	case 70: return MapID::Gate_of_Secrets_outpost;
	case 71: return MapID::Gate_of_the_Nightfallen_Lands_outpost;
	case 72: return MapID::Gate_of_Torment_outpost;
	case 73: return MapID::Gates_of_Kryta;
	case 74: return MapID::Grand_Court_of_Sebelkeh;
	case 75: return MapID::The_Granite_Citadel_outpost;
	case 76: return MapID::The_Great_Northern_Wall;
	case 77: return MapID::Great_Temple_of_Balthazar_outpost;
	case 78: return MapID::Grendich_Courthouse_outpost;
	case 79: return MapID::Gunnars_Hold_outpost;
	case 80: return MapID::Gyala_Hatchery_outpost_mission;
	case 81: return MapID::Harvest_Temple_outpost;
	case 82: return MapID::Hells_Precipice;
	case 83: return MapID::Henge_of_Denravi_outpost;
	case 84: return MapID::Heroes_Ascent_outpost;
	case 85: return MapID::Heroes_Audience_outpost;
	case 86: return MapID::Honur_Hill_outpost;
	case 87: return MapID::House_zu_Heltzer_outpost;
	case 88: return MapID::Ice_Caves_of_Sorrow;
	case 89: return MapID::Ice_Tooth_Cave_outpost;
	case 90: return MapID::Imperial_Sanctum_outpost_mission;
	case 91: return MapID::Iron_Mines_of_Moladune;
	case 92: return MapID::Jade_Flats_Kurzick_outpost;
	case 93: return MapID::Jade_Flats_Luxon_outpost;
	case 94: return MapID::The_Jade_Quarry_Kurzick_outpost;
	case 95: return MapID::The_Jade_Quarry_Luxon_outpost;
	case 96: return MapID::Jennurs_Horde;
	case 97: return MapID::Jokanur_Diggings;
	case 98: return MapID::Kaineng_Center_outpost;
	case 99: return MapID::Kamadan_Jewel_of_Istan_outpost;
	case 100: return MapID::The_Kodash_Bazaar_outpost;
	case 101: return MapID::Kodlonu_Hamlet_outpost;
	case 102: return MapID::Kodonur_Crossroads;
	case 103: return MapID::Lair_of_the_Forgotten_outpost;
	case 104: return MapID::Leviathan_Pits_outpost;
	case 105: return MapID::Lions_Arch_outpost;
	case 106: return MapID::Longeyes_Ledge_outpost;
	case 107: return MapID::Lutgardis_Conservatory_outpost;
	case 108: return MapID::Maatu_Keep_outpost;
	case 109: return MapID::Maguuma_Stade_outpost;
	case 110: return MapID::Marhans_Grotto_outpost;
	case 111: return MapID::The_Marketplace_outpost;
	case 112: return MapID::Mihanu_Township_outpost;
	case 113: return MapID::Minister_Chos_Estate_outpost_mission;
	case 114: return MapID::Moddok_Crevice;
	case 115: return MapID::The_Mouth_of_Torment_outpost;
	case 116: return MapID::Nahpui_Quarter_outpost_mission;
	case 117: return MapID::Nolani_Academy;
	case 118: return MapID::Nundu_Bay;
	case 119: return MapID::Olafstead_outpost;
	case 120: return MapID::Piken_Square_outpost;
	case 121: return MapID::Pogahn_Passage;
	case 122: return MapID::Port_Sledge_outpost;
	case 123: return MapID::Quarrel_Falls_outpost;
	case 124: return MapID::Raisu_Palace_outpost_mission;
	case 125: return MapID::Ran_Musu_Gardens_outpost;
	case 126: return MapID::Random_Arenas_outpost;
	case 127: return MapID::Rata_Sum_outpost;
	case 128: return MapID::Remains_of_Sahlahja;
	case 129: return MapID::Rilohn_Refuge;
	case 130: return MapID::Ring_of_Fire;
	case 131: return MapID::Riverside_Province;
	case 132: return MapID::Ruins_of_Morah;
	case 133: return MapID::Ruins_of_Surmia;
	case 134: return MapID::Saint_Anjekas_Shrine_outpost;
	case 135: return MapID::Sanctum_Cay;
	case 136: return MapID::Sardelac_Sanitarium_outpost;
	case 137: return MapID::Seafarers_Rest_outpost;
	case 138: return MapID::Seekers_Passage_outpost;
	case 139: return MapID::Seitung_Harbor_outpost;
	case 140: return MapID::Senjis_Corner_outpost;
	case 141: return MapID::Serenity_Temple_outpost;
	case 142: return MapID::The_Shadow_Nexus;
	case 143: return MapID::Shing_Jea_Arena;
	case 144: return MapID::Shing_Jea_Monastery_outpost;
	case 145: return MapID::Sifhalla_outpost;
	case 146: return MapID::Sunjiang_District_outpost_mission;
	case 147: return MapID::Sunspear_Arena;
	case 148: return MapID::Sunspear_Great_Hall_outpost;
	case 149: return MapID::Sunspear_Sanctuary_outpost;
	case 150: return MapID::Tahnnakai_Temple_outpost_mission;
	case 151: return MapID::Tanglewood_Copse_outpost;
	case 152: return MapID::Tarnished_Haven_outpost;
	case 153: return MapID::Temple_of_the_Ages;
	case 154: return MapID::Thirsty_River;
	case 155: return MapID::Thunderhead_Keep;
	case 156: return MapID::Tihark_Orchard;
	case 157: return MapID::Tomb_of_the_Primeval_Kings;
	case 158: return MapID::Tsumei_Village_outpost;
	case 159: return MapID::Umbral_Grotto_outpost;
	case 160: return MapID::Unwaking_Waters_Kurzick_outpost;
	case 161: return MapID::Unwaking_Waters_Luxon_outpost;
	case 162: return MapID::Urgozs_Warren;
	case 163: return MapID::Vasburg_Armory_outpost;
	case 164: return MapID::Venta_Cemetery;
	case 165: return MapID::Ventaris_Refuge_outpost;
	case 166: return MapID::Vizunah_Square_Foreign_Quarter_outpost;
	case 167: return MapID::Vizunah_Square_Local_Quarter_outpost;
	case 168: return MapID::Vloxs_Falls;
	case 169: return MapID::Wehhan_Terraces_outpost;
	case 170: return MapID::The_Wilds;
	case 171: return MapID::Yahnur_Market_outpost;
	case 172: return MapID::Yaks_Bend_outpost;
	case 173: return MapID::Yohlon_Haven_outpost;
	case 174: return MapID::Zaishen_Challenge_outpost;
	case 175: return MapID::Zaishen_Elite_outpost;
	case 176: return MapID::Zaishen_Menagerie_outpost;
	case 177: return MapID::Zen_Daijun_outpost_mission;
	case 178: return MapID::Zin_Ku_Corridor_outpost;
	case 179: return MapID::Zos_Shivros_Channel;
	default: return MapID::Great_Temple_of_Balthazar_outpost;
	}
}
