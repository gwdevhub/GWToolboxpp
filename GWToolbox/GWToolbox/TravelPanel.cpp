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
	combo->SetText(L"Travel To...");
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
		combo->SetText(L"Travel To...");
		combo->SetSelectedIndex(-1);
	});
	AddControl(combo);

	ComboBox* district = new ComboBox(this);
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
	district->SetSize(SizeI(GetWidth() - 2 * Padding, GuiUtils::BUTTON_HEIGHT));
	district->SetLocation(PointI(Padding, district->GetBottom() + Padding));
	district->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, district](Control*) {
		UpdateDistrict(district->GetSelectedIndex());
	});
	district->SetSelectedIndex(0);
	AddControl(district);

	using namespace GW::Constants;
	AddTravelButton(L"ToA", 0, 2, MapID::Temple_of_the_Ages);
	AddTravelButton(L"DoA", 1, 2, MapID::Domain_of_Anguish);
	AddTravelButton(L"Kamadan", 0, 3, MapID::Kamadan_Jewel_of_Istan_outpost);
	AddTravelButton(L"Embark", 1, 3, MapID::Embark_Beach);
	AddTravelButton(L"Vlox's", 0, 4, MapID::Vloxs_Falls);
	AddTravelButton(L"Gadd's", 1, 4, MapID::Gadds_Encampment_outpost);
	AddTravelButton(L"Urgoz", 0, 5, MapID::Urgozs_Warren);
	AddTravelButton(L"Deep", 1, 5, MapID::The_Deep);

	for (int i = 0; i < 3; ++i) {
		std::wstring key = std::wstring(L"Travel") + std::to_wstring(i);
		int index = Config::IniReadLong(MainWindow::IniSection(), key.c_str(), 0);
		ComboBox* fav_combo = new TravelCombo(this);
		fav_combo->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4, 3), GuiUtils::BUTTON_HEIGHT));
		fav_combo->SetLocation(PointI(Padding, Padding + (GuiUtils::BUTTON_HEIGHT + Padding) * (i + 6)));
		for (int j = 0; j < n_outposts; ++j) {
			fav_combo->AddItem(IndexToOutpostName(j));
		}
		fav_combo->SetSelectedIndex(index);
		fav_combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
			[fav_combo, key](Control*) {
			Config::IniWriteLong(MainWindow::IniSection(),
				key.c_str(), fav_combo->GetSelectedIndex());
		});
		AddControl(fav_combo);

		Button* fav_button = new Button(this);
		fav_button->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 4), GuiUtils::BUTTON_HEIGHT));
		fav_button->SetLocation(PointI(GuiUtils::ComputeX(GetWidth(), 4, 3), fav_combo->GetTop()));
		fav_button->SetText(L"Go");
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


void TravelPanel::AddTravelButton(std::wstring text, int grid_x, int grid_y, GW::Constants::MapID map_id) {
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

std::wstring TravelPanel::IndexToOutpostName(int index) {
	switch (index) {
	case 0: return std::wstring(L"Abaddon's Gate");
	case 1: return std::wstring(L"Abaddon's Mouth");
	case 2: return std::wstring(L"Altrumm Ruins");
	case 3: return std::wstring(L"Amatz Basin");
	case 4: return std::wstring(L"Amnoon Oasis, The");
	case 5: return std::wstring(L"Arborstone");
	case 6: return std::wstring(L"Ascalon City");
	case 7: return std::wstring(L"Aspenwood Gate (Kurzick)");
	case 8: return std::wstring(L"Aspenwood Gate (Luxon)");
	case 9: return std::wstring(L"Astralarium, The");
	case 10: return std::wstring(L"Augury Rock");
	case 11: return std::wstring(L"Aurios Mines, The");
	case 12: return std::wstring(L"Aurora Glade");
	case 13: return std::wstring(L"Bai Paasu Reach");
	case 14: return std::wstring(L"Basalt Grotto");
	case 15: return std::wstring(L"Beacon's Perch");
	case 16: return std::wstring(L"Beetletun");
	case 17: return std::wstring(L"Beknur Harbor");
	case 18: return std::wstring(L"Bergen Hot Springs");
	case 19: return std::wstring(L"Blacktide Den");
	case 20: return std::wstring(L"Bloodstone Fen");
	case 21: return std::wstring(L"Bone Palace");
	case 22: return std::wstring(L"Boreal Station");
	case 23: return std::wstring(L"Boreas Seabed");
	case 24: return std::wstring(L"Borlis Pass");
	case 25: return std::wstring(L"Brauer Academy");
	case 26: return std::wstring(L"Breaker Hollow");
	case 27: return std::wstring(L"Camp Hojanu");
	case 28: return std::wstring(L"Camp Rankor");
	case 29: return std::wstring(L"Cavalon");
	case 30: return std::wstring(L"Central Transfer Chamber");
	case 31: return std::wstring(L"Chahbek Village");
	case 32: return std::wstring(L"Champion's Dawn");
	case 33: return std::wstring(L"Chantry of Secrets");
	case 34: return std::wstring(L"Codex Arena");
	case 35: return std::wstring(L"Consulate Docks");
	case 36: return std::wstring(L"Copperhammer Mines");
	case 37: return std::wstring(L"D'Alessio Seaboard");
	case 38: return std::wstring(L"Dajkah Inlet");
	case 39: return std::wstring(L"Dasha Vestibule");
	case 40: return std::wstring(L"Deep, The");
	case 41: return std::wstring(L"Deldrimor War Camp");
	case 42: return std::wstring(L"Destiny's Gorge");
	case 43: return std::wstring(L"Divinity Coast");
	case 44: return std::wstring(L"Doomlore Shrine");
	case 45: return std::wstring(L"Dragon's Lair, The");
	case 46: return std::wstring(L"Dragon's Throat");
	case 47: return std::wstring(L"Droknar's Forge");
	case 48: return std::wstring(L"Druid's Overlook");
	case 49: return std::wstring(L"Dunes of Despair");
	case 50: return std::wstring(L"Durheim Archives");
	case 51: return std::wstring(L"Dzagonur Bastion");
	case 52: return std::wstring(L"Elona Reach");
	case 53: return std::wstring(L"Embark Beach");
	case 54: return std::wstring(L"Ember Light Camp");
	case 55: return std::wstring(L"Eredon Terrace");
	case 56: return std::wstring(L"Eternal Grove, The");
	case 57: return std::wstring(L"Eye of the North");
	case 58: return std::wstring(L"Fishermen's Haven");
	case 59: return std::wstring(L"Fort Aspenwood (Kurzick)");
	case 60: return std::wstring(L"Fort Aspenwood (Luxon)");
	case 61: return std::wstring(L"Fort Ranik");
	case 62: return std::wstring(L"Frontier Gate");
	case 63: return std::wstring(L"Frost Gate, The");
	case 64: return std::wstring(L"Gadd's Encampment");
	case 65: return std::wstring(L"Gate of Anguish");
	case 66: return std::wstring(L"Gate of Desolation");
	case 67: return std::wstring(L"Gate of Fear");
	case 68: return std::wstring(L"Gate of Madness");
	case 69: return std::wstring(L"Gate of Pain");
	case 70: return std::wstring(L"Gate of Secrets");
	case 71: return std::wstring(L"Gate of the Nightfallen Lands");
	case 72: return std::wstring(L"Gate of Torment");
	case 73: return std::wstring(L"Gates of Kryta");
	case 74: return std::wstring(L"Grand Court of Sebelkeh");
	case 75: return std::wstring(L"Granite Citadel, The");
	case 76: return std::wstring(L"Great Northern Wall, The");
	case 77: return std::wstring(L"Great Temple of Balthazar");
	case 78: return std::wstring(L"Grendich Courthouse");
	case 79: return std::wstring(L"Gunnar's Hold");
	case 80: return std::wstring(L"Gyala Hatchery");
	case 81: return std::wstring(L"Harvest Temple");
	case 82: return std::wstring(L"Hell's Precipice");
	case 83: return std::wstring(L"Henge of Denravi");
	case 84: return std::wstring(L"Heroes' Ascent");
	case 85: return std::wstring(L"Heroes' Audience");
	case 86: return std::wstring(L"Honur Hill");
	case 87: return std::wstring(L"House zu Heltzer");
	case 88: return std::wstring(L"Ice Caves of Sorrow");
	case 89: return std::wstring(L"Ice Tooth Cave");
	case 90: return std::wstring(L"Imperial Sanctum");
	case 91: return std::wstring(L"Iron Mines of Moladune");
	case 92: return std::wstring(L"Jade Flats (Kurzick)");
	case 93: return std::wstring(L"Jade Flats (Luxon)");
	case 94: return std::wstring(L"Jade Quarry (Kurzick), The");
	case 95: return std::wstring(L"Jade Quarry (Luxon), The");
	case 96: return std::wstring(L"Jennur's Horde");
	case 97: return std::wstring(L"Jokanur Diggings");
	case 98: return std::wstring(L"Kaineng Center");
	case 99: return std::wstring(L"Kamadan, Jewel of Istan");
	case 100: return std::wstring(L"Kodash Bazaar, The");
	case 101: return std::wstring(L"Kodlonu Hamlet");
	case 102: return std::wstring(L"Kodonur Crossroads");
	case 103: return std::wstring(L"Lair of the Forgotten");
	case 104: return std::wstring(L"Leviathan Pits");
	case 105: return std::wstring(L"Lion's Arch");
	case 106: return std::wstring(L"Longeye's Ledge");
	case 107: return std::wstring(L"Lutgardis Conservatory");
	case 108: return std::wstring(L"Maatu Keep");
	case 109: return std::wstring(L"Maguuma Stade");
	case 110: return std::wstring(L"Marhan's Grotto");
	case 111: return std::wstring(L"Marketplace, The");
	case 112: return std::wstring(L"Mihanu Township");
	case 113: return std::wstring(L"Minister Cho's Estate");
	case 114: return std::wstring(L"Moddok Crevice");
	case 115: return std::wstring(L"Mouth of Torment, The");
	case 116: return std::wstring(L"Nahpui Quarter");
	case 117: return std::wstring(L"Nolani Academy");
	case 118: return std::wstring(L"Nundu Bay");
	case 119: return std::wstring(L"Olafstead");
	case 120: return std::wstring(L"Piken Square");
	case 121: return std::wstring(L"Pogahn Passage");
	case 122: return std::wstring(L"Port Sledge");
	case 123: return std::wstring(L"Quarrel Falls");
	case 124: return std::wstring(L"Raisu Palace");
	case 125: return std::wstring(L"Ran Musu Gardens");
	case 126: return std::wstring(L"Random Arenas");
	case 127: return std::wstring(L"Rata Sum");
	case 128: return std::wstring(L"Remains of Sahlahja");
	case 129: return std::wstring(L"Rilohn Refuge");
	case 130: return std::wstring(L"Ring of Fire");
	case 131: return std::wstring(L"Riverside Province");
	case 132: return std::wstring(L"Ruins of Morah");
	case 133: return std::wstring(L"Ruins of Surmia");
	case 134: return std::wstring(L"Saint Anjeka's Shrine");
	case 135: return std::wstring(L"Sanctum Cay");
	case 136: return std::wstring(L"Sardelac Sanitarium");
	case 137: return std::wstring(L"Seafarer's Rest");
	case 138: return std::wstring(L"Seeker's Passage");
	case 139: return std::wstring(L"Seitung Harbor");
	case 140: return std::wstring(L"Senji's Corner");
	case 141: return std::wstring(L"Serenity Temple");
	case 142: return std::wstring(L"Shadow Nexus, The");
	case 143: return std::wstring(L"Shing Jea Arena");
	case 144: return std::wstring(L"Shing Jea Monastery");
	case 145: return std::wstring(L"Sifhalla");
	case 146: return std::wstring(L"Sunjiang District");
	case 147: return std::wstring(L"Sunspear Arena");
	case 148: return std::wstring(L"Sunspear Great Hall");
	case 149: return std::wstring(L"Sunspear Sanctuary");
	case 150: return std::wstring(L"Tahnnakai Temple");
	case 151: return std::wstring(L"Tanglewood Copse");
	case 152: return std::wstring(L"Tarnished Haven");
	case 153: return std::wstring(L"Temple of the Ages");
	case 154: return std::wstring(L"Thirsty River");
	case 155: return std::wstring(L"Thunderhead Keep");
	case 156: return std::wstring(L"Tihark Orchard");
	case 157: return std::wstring(L"Tomb of the Primeval Kings");
	case 158: return std::wstring(L"Tsumei Village");
	case 159: return std::wstring(L"Umbral Grotto");
	case 160: return std::wstring(L"Unwaking Waters (Kurzick)");
	case 161: return std::wstring(L"Unwaking Waters (Luxon)");
	case 162: return std::wstring(L"Urgoz's Warren");
	case 163: return std::wstring(L"Vasburg Armory");
	case 164: return std::wstring(L"Venta Cemetery");
	case 165: return std::wstring(L"Ventari's Refuge");
	case 166: return std::wstring(L"Vizunah Square (Foreign Quarter)");
	case 167: return std::wstring(L"Vizunah Square (Local Quarter)");
	case 168: return std::wstring(L"Vlox's Falls");
	case 169: return std::wstring(L"Wehhan Terraces");
	case 170: return std::wstring(L"Wilds, The");
	case 171: return std::wstring(L"Yahnur Market");
	case 172: return std::wstring(L"Yak's Bend");
	case 173: return std::wstring(L"Yohlon Haven");
	case 174: return std::wstring(L"Zaishen Challenge");
	case 175: return std::wstring(L"Zaishen Elite");
	case 176: return std::wstring(L"Zaishen Menagerie");
	case 177: return std::wstring(L"Zen Daijun");
	case 178: return std::wstring(L"Zin Ku Corridor");
	case 179: return std::wstring(L"Zos Shivros Channel");
	default: return std::wstring(L"a bad map");
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
