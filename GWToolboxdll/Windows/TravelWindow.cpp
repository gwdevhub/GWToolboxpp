#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/GuildContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Resources.h>
#include <Windows/TravelWindow.h>

#define N_OUTPOSTS 180
#define N_DISTRICTS 14

namespace {
    bool outpost_name_array_getter(void* data, int idx, const char** out_text);

    bool ImInPresearing() { return GW::Map::GetCurrentMapInfo()->region == GW::Region_Presearing; }

    bool outpost_name_array_getter(void *data, int idx, const char **out_text)
    {
        UNREFERENCED_PARAMETER(data);
        switch (idx) {
            case 0:
                *out_text = "Abaddon's Gate";
                break;
            case 1:
                *out_text = "Abaddon's Mouth";
                break;
            case 2:
                *out_text = "Altrumm Ruins";
                break;
            case 3:
                *out_text = "Amatz Basin";
                break;
            case 4:
                *out_text = "Amnoon Oasis, The";
                break;
            case 5:
                *out_text = "Arborstone";
                break;
            case 6:
                *out_text = "Ascalon City";
                break;
            case 7:
                *out_text = "Aspenwood Gate (Kurzick)";
                break;
            case 8:
                *out_text = "Aspenwood Gate (Luxon)";
                break;
            case 9:
                *out_text = "Astralarium, The";
                break;
            case 10:
                *out_text = "Augury Rock";
                break;
            case 11:
                *out_text = "Aurios Mines, The";
                break;
            case 12:
                *out_text = "Aurora Glade";
                break;
            case 13:
                *out_text = "Bai Paasu Reach";
                break;
            case 14:
                *out_text = "Basalt Grotto";
                break;
            case 15:
                *out_text = "Beacon's Perch";
                break;
            case 16:
                *out_text = "Beetletun";
                break;
            case 17:
                *out_text = "Beknur Harbor";
                break;
            case 18:
                *out_text = "Bergen Hot Springs";
                break;
            case 19:
                *out_text = "Blacktide Den";
                break;
            case 20:
                *out_text = "Bloodstone Fen";
                break;
            case 21:
                *out_text = "Bone Palace";
                break;
            case 22:
                *out_text = "Boreal Station";
                break;
            case 23:
                *out_text = "Boreas Seabed";
                break;
            case 24:
                *out_text = "Borlis Pass";
                break;
            case 25:
                *out_text = "Brauer Academy";
                break;
            case 26:
                *out_text = "Breaker Hollow";
                break;
            case 27:
                *out_text = "Camp Hojanu";
                break;
            case 28:
                *out_text = "Camp Rankor";
                break;
            case 29:
                *out_text = "Cavalon";
                break;
            case 30:
                *out_text = "Central Transfer Chamber";
                break;
            case 31:
                *out_text = "Chahbek Village";
                break;
            case 32:
                *out_text = "Champion's Dawn";
                break;
            case 33:
                *out_text = "Chantry of Secrets";
                break;
            case 34:
                *out_text = "Codex Arena";
                break;
            case 35:
                *out_text = "Consulate Docks";
                break;
            case 36:
                *out_text = "Copperhammer Mines";
                break;
            case 37:
                *out_text = "D'Alessio Seaboard";
                break;
            case 38:
                *out_text = "Dajkah Inlet";
                break;
            case 39:
                *out_text = "Dasha Vestibule";
                break;
            case 40:
                *out_text = "Deep, The";
                break;
            case 41:
                *out_text = "Deldrimor War Camp";
                break;
            case 42:
                *out_text = "Destiny's Gorge";
                break;
            case 43:
                *out_text = "Divinity Coast";
                break;
            case 44:
                *out_text = "Doomlore Shrine";
                break;
            case 45:
                *out_text = "Dragon's Lair, The";
                break;
            case 46:
                *out_text = "Dragon's Throat";
                break;
            case 47:
                *out_text = "Droknar's Forge";
                break;
            case 48:
                *out_text = "Druid's Overlook";
                break;
            case 49:
                *out_text = "Dunes of Despair";
                break;
            case 50:
                *out_text = "Durheim Archives";
                break;
            case 51:
                *out_text = "Dzagonur Bastion";
                break;
            case 52:
                *out_text = "Elona Reach";
                break;
            case 53:
                *out_text = "Embark Beach";
                break;
            case 54:
                *out_text = "Ember Light Camp";
                break;
            case 55:
                *out_text = "Eredon Terrace";
                break;
            case 56:
                *out_text = "Eternal Grove, The";
                break;
            case 57:
                *out_text = "Eye of the North";
                break;
            case 58:
                *out_text = "Fishermen's Haven";
                break;
            case 59:
                *out_text = "Fort Aspenwood (Kurzick)";
                break;
            case 60:
                *out_text = "Fort Aspenwood (Luxon)";
                break;
            case 61:
                *out_text = "Fort Ranik";
                break;
            case 62:
                *out_text = "Frontier Gate";
                break;
            case 63:
                *out_text = "Frost Gate, The";
                break;
            case 64:
                *out_text = "Gadd's Encampment";
                break;
            case 65:
                *out_text = "Gate of Anguish";
                break;
            case 66:
                *out_text = "Gate of Desolation";
                break;
            case 67:
                *out_text = "Gate of Fear";
                break;
            case 68:
                *out_text = "Gate of Madness";
                break;
            case 69:
                *out_text = "Gate of Pain";
                break;
            case 70:
                *out_text = "Gate of Secrets";
                break;
            case 71:
                *out_text = "Gate of the Nightfallen Lands";
                break;
            case 72:
                *out_text = "Gate of Torment";
                break;
            case 73:
                *out_text = "Gates of Kryta";
                break;
            case 74:
                *out_text = "Grand Court of Sebelkeh";
                break;
            case 75:
                *out_text = "Granite Citadel, The";
                break;
            case 76:
                *out_text = "Great Northern Wall, The";
                break;
            case 77:
                *out_text = "Great Temple of Balthazar";
                break;
            case 78:
                *out_text = "Grendich Courthouse";
                break;
            case 79:
                *out_text = "Gunnar's Hold";
                break;
            case 80:
                *out_text = "Gyala Hatchery";
                break;
            case 81:
                *out_text = "Harvest Temple";
                break;
            case 82:
                *out_text = "Hell's Precipice";
                break;
            case 83:
                *out_text = "Henge of Denravi";
                break;
            case 84:
                *out_text = "Heroes' Ascent";
                break;
            case 85:
                *out_text = "Heroes' Audience";
                break;
            case 86:
                *out_text = "Honur Hill";
                break;
            case 87:
                *out_text = "House zu Heltzer";
                break;
            case 88:
                *out_text = "Ice Caves of Sorrow";
                break;
            case 89:
                *out_text = "Ice Tooth Cave";
                break;
            case 90:
                *out_text = "Imperial Sanctum";
                break;
            case 91:
                *out_text = "Iron Mines of Moladune";
                break;
            case 92:
                *out_text = "Jade Flats (Kurzick)";
                break;
            case 93:
                *out_text = "Jade Flats (Luxon)";
                break;
            case 94:
                *out_text = "Jade Quarry (Kurzick), The";
                break;
            case 95:
                *out_text = "Jade Quarry (Luxon), The";
                break;
            case 96:
                *out_text = "Jennur's Horde";
                break;
            case 97:
                *out_text = "Jokanur Diggings";
                break;
            case 98:
                *out_text = "Kaineng Center";
                break;
            case 99:
                *out_text = "Kamadan, Jewel of Istan";
                break;
            case 100:
                *out_text = "Kodash Bazaar, The";
                break;
            case 101:
                *out_text = "Kodlonu Hamlet";
                break;
            case 102:
                *out_text = "Kodonur Crossroads";
                break;
            case 103:
                *out_text = "Lair of the Forgotten";
                break;
            case 104:
                *out_text = "Leviathan Pits";
                break;
            case 105:
                *out_text = "Lion's Arch";
                break;
            case 106:
                *out_text = "Longeye's Ledge";
                break;
            case 107:
                *out_text = "Lutgardis Conservatory";
                break;
            case 108:
                *out_text = "Maatu Keep";
                break;
            case 109:
                *out_text = "Maguuma Stade";
                break;
            case 110:
                *out_text = "Marhan's Grotto";
                break;
            case 111:
                *out_text = "Marketplace, The";
                break;
            case 112:
                *out_text = "Mihanu Township";
                break;
            case 113:
                *out_text = "Minister Cho's Estate";
                break;
            case 114:
                *out_text = "Moddok Crevice";
                break;
            case 115:
                *out_text = "Mouth of Torment, The";
                break;
            case 116:
                *out_text = "Nahpui Quarter";
                break;
            case 117:
                *out_text = "Nolani Academy";
                break;
            case 118:
                *out_text = "Nundu Bay";
                break;
            case 119:
                *out_text = "Olafstead";
                break;
            case 120:
                *out_text = "Piken Square";
                break;
            case 121:
                *out_text = "Pogahn Passage";
                break;
            case 122:
                *out_text = "Port Sledge";
                break;
            case 123:
                *out_text = "Quarrel Falls";
                break;
            case 124:
                *out_text = "Raisu Palace";
                break;
            case 125:
                *out_text = "Ran Musu Gardens";
                break;
            case 126:
                *out_text = "Random Arenas";
                break;
            case 127:
                *out_text = "Rata Sum";
                break;
            case 128:
                *out_text = "Remains of Sahlahja";
                break;
            case 129:
                *out_text = "Rilohn Refuge";
                break;
            case 130:
                *out_text = "Ring of Fire";
                break;
            case 131:
                *out_text = "Riverside Province";
                break;
            case 132:
                *out_text = "Ruins of Morah";
                break;
            case 133:
                *out_text = "Ruins of Surmia";
                break;
            case 134:
                *out_text = "Saint Anjeka's Shrine";
                break;
            case 135:
                *out_text = "Sanctum Cay";
                break;
            case 136:
                *out_text = "Sardelac Sanitarium";
                break;
            case 137:
                *out_text = "Seafarer's Rest";
                break;
            case 138:
                *out_text = "Seeker's Passage";
                break;
            case 139:
                *out_text = "Seitung Harbor";
                break;
            case 140:
                *out_text = "Senji's Corner";
                break;
            case 141:
                *out_text = "Serenity Temple";
                break;
            case 142:
                *out_text = "Shadow Nexus, The";
                break;
            case 143:
                *out_text = "Shing Jea Arena";
                break;
            case 144:
                *out_text = "Shing Jea Monastery";
                break;
            case 145:
                *out_text = "Sifhalla";
                break;
            case 146:
                *out_text = "Sunjiang District";
                break;
            case 147:
                *out_text = "Sunspear Arena";
                break;
            case 148:
                *out_text = "Sunspear Great Hall";
                break;
            case 149:
                *out_text = "Sunspear Sanctuary";
                break;
            case 150:
                *out_text = "Tahnnakai Temple";
                break;
            case 151:
                *out_text = "Tanglewood Copse";
                break;
            case 152:
                *out_text = "Tarnished Haven";
                break;
            case 153:
                *out_text = "Temple of the Ages";
                break;
            case 154:
                *out_text = "Thirsty River";
                break;
            case 155:
                *out_text = "Thunderhead Keep";
                break;
            case 156:
                *out_text = "Tihark Orchard";
                break;
            case 157:
                *out_text = "Tomb of the Primeval Kings";
                break;
            case 158:
                *out_text = "Tsumei Village";
                break;
            case 159:
                *out_text = "Umbral Grotto";
                break;
            case 160:
                *out_text = "Unwaking Waters (Kurzick)";
                break;
            case 161:
                *out_text = "Unwaking Waters (Luxon)";
                break;
            case 162:
                *out_text = "Urgoz's Warren";
                break;
            case 163:
                *out_text = "Vasburg Armory";
                break;
            case 164:
                *out_text = "Venta Cemetery";
                break;
            case 165:
                *out_text = "Ventari's Refuge";
                break;
            case 166:
                *out_text = "Vizunah Square (Foreign)";
                break;
            case 167:
                *out_text = "Vizunah Square (Local)";
                break;
            case 168:
                *out_text = "Vlox's Falls";
                break;
            case 169:
                *out_text = "Wehhan Terraces";
                break;
            case 170:
                *out_text = "Wilds, The";
                break;
            case 171:
                *out_text = "Yahnur Market";
                break;
            case 172:
                *out_text = "Yak's Bend";
                break;
            case 173:
                *out_text = "Yohlon Haven";
                break;
            case 174:
                *out_text = "Zaishen Challenge";
                break;
            case 175:
                *out_text = "Zaishen Elite";
                break;
            case 176:
                *out_text = "Zaishen Menagerie";
                break;
            case 177:
                *out_text = "Zen Daijun";
                break;
            case 178:
                *out_text = "Zin Ku Corridor";
                break;
            case 179:
                *out_text = "Zos Shivros Channel";
                break;
            default:
                *out_text = "";
        }
        return true;
    }
    static bool IsInGH()
    {
        auto* p = GW::GuildMgr::GetPlayerGuild();
        return p && p == GW::GuildMgr::GetCurrentGH();
    }
    static bool IsLuxon()
    {
        GW::GuildContext* c = GW::GetGuildContext();
        return c && c->player_guild_index && c->guilds[c->player_guild_index]->faction;
    }
    static bool IsAlreadyInOutpost(GW::Constants::MapID outpost_id, GW::Constants::District _district, uint32_t _district_number = 0)
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
            && GW::Map::GetMapID() == outpost_id
            && TravelWindow::RegionFromDistrict(_district) == GW::Map::GetRegion()
            && TravelWindow::LanguageFromDistrict(_district) == GW::Map::GetLanguage()
            && (!_district_number || _district_number == static_cast<uint32_t>(GW::Map::GetDistrict()));
    }


    struct MapStruct {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        int region_id = 0;
        int language_id = 0;
        uint32_t district_number = 0;
    };

    struct UIErrorMessage {
        int error_index;
        wchar_t* message;
    };

    bool retry_map_travel = false;
    MapStruct pending_map_travel;

    GW::UI::UIMessage messages_to_hook[] = {
        GW::UI::UIMessage::kErrorMessage,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kTravel
    };
    GW::HookEntry OnUIMessage_HookEntry;
    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kMapChange: {
            pending_map_travel.map_id = GW::Constants::MapID::None;
        } break;
        case GW::UI::UIMessage::kTravel: {
            MapStruct* t = (MapStruct*)wparam;
            if (t && t != &pending_map_travel) 
                pending_map_travel = *t;
        } break;
        case GW::UI::UIMessage::kErrorMessage: {
            if (!(retry_map_travel && pending_map_travel.map_id != GW::Constants::MapID::None))
                break;
            UIErrorMessage* msg = (UIErrorMessage*)wparam;
            if (msg && msg->message && *msg->message == 0xb25) {
                // Travel failed, but we want to retry
                // NB: 0xb25 = "That district is full. Please select another."
                status->blocked = true;
                GW::UI::SendUIMessage(GW::UI::UIMessage::kTravel, (void*)&pending_map_travel);
            }
        } break;
        }
    }
}

void TravelWindow::Initialize() {
    ToolboxWindow::Initialize();
    scroll_texture = Resources::GetItemImage(L"Passage Scroll to the Deep");
    district = GW::Constants::District::Current;
    district_number = 0;

    GW::Chat::CreateCommand(L"tp", &CmdTP);
    GW::Chat::CreateCommand(L"to", &CmdTP);
    GW::Chat::CreateCommand(L"travel", &CmdTP);

    for (auto message_id : messages_to_hook) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, message_id, OnUIMessage);
    }
    
}
void TravelWindow::Terminate() {
    ToolboxWindow::Terminate();
    for (const auto it : searchable_explorable_areas) {
        delete[] it;
    }
    searchable_explorable_areas.clear();
    for (auto message_id : messages_to_hook) {
        (message_id);
        GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
    }
}

void TravelWindow::TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid) {
    if (x_idx != 0) ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemInnerSpacing.x) / 2 - ImGui::GetStyle().WindowPadding.x;
    bool clicked = false;
    switch (mapid) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::Urgozs_Warren:
            clicked |= ImGui::IconButton(text, (ImTextureID) *scroll_texture, ImVec2(w, 0));
            break;
        default:
            clicked |= ImGui::Button(text, ImVec2(w, 0));
            break;
    }
    if (clicked) {
        Travel(mapid, district, district_number);
        if (close_on_travel) visible = false;
    }
}

void TravelWindow::Draw(IDirect3DDevice9* pDevice)
{
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (ImInPresearing()) {
            TravelButton("Ascalon City", 0, GW::Constants::MapID::Ascalon_City_pre_searing);
            TravelButton("Ashford Abbey", 1, GW::Constants::MapID::Ashford_Abbey_outpost);
            TravelButton("Foible's Fair", 0, GW::Constants::MapID::Foibles_Fair_outpost);
            TravelButton("Fort Ranik", 1, GW::Constants::MapID::Fort_Ranik_pre_Searing_outpost);
            TravelButton("The Barradin Estate", 0, GW::Constants::MapID::The_Barradin_Estate_outpost);
        } else {
            ImGui::PushItemWidth(-1.0f);
            static int travelto_index = -1;
            if (ImGui::MyCombo(
                    "travelto", "Travel To...", &travelto_index, outpost_name_array_getter, nullptr, N_OUTPOSTS)) {
                GW::Constants::MapID id = IndexToOutpostID(travelto_index);
                Travel(id, district, district_number);
                travelto_index = -1;
                if (close_on_travel) visible = false;
            }

            static int district_index = 0;
            static const char* const district_words[] = {
                "Current District",
                "International",
                "American",
                "American District 1",
                "Europe English",
                "Europe French",
                "Europe German",
                "Europe Italian",
                "Europe Spanish",
                "Europe Polish",
                "Europe Russian",
                "Asian Korean",
                "Asia Chinese",
                "Asia Japanese",
            };
            if (ImGui::Combo("###district", &district_index, district_words, N_DISTRICTS)) {
                district_number = 0;
                switch (district_index) {
                    case 0: district = GW::Constants::District::Current; break;
                    case 1: district = GW::Constants::District::International; break;
                    case 2: district = GW::Constants::District::American; break;
                    case 3: // American District 1
                        district = GW::Constants::District::American;
                        district_number = 1;
                        break;
                    case 4: district = GW::Constants::District::EuropeEnglish; break;
                    case 5: district = GW::Constants::District::EuropeFrench; break;
                    case 6: district = GW::Constants::District::EuropeGerman; break;
                    case 7: district = GW::Constants::District::EuropeItalian; break;
                    case 8: district = GW::Constants::District::EuropeSpanish; break;
                    case 9: district = GW::Constants::District::EuropePolish; break;
                    case 10: district = GW::Constants::District::EuropeRussian; break;
                    case 11: district = GW::Constants::District::AsiaKorean; break;
                    case 12: district = GW::Constants::District::AsiaChinese; break;
                    case 13: district = GW::Constants::District::AsiaJapanese; break;
                    default: break;
                }
            }
            ImGui::PopItemWidth();

            TravelButton("ToA", 0, GW::Constants::MapID::Temple_of_the_Ages);
            TravelButton("DoA", 1, GW::Constants::MapID::Domain_of_Anguish);
            TravelButton("Kamadan", 0, GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost);
            TravelButton("Embark", 1, GW::Constants::MapID::Embark_Beach);
            TravelButton("Vlox's", 0, GW::Constants::MapID::Vloxs_Falls);
            TravelButton("Gadd's", 1, GW::Constants::MapID::Gadds_Encampment_outpost);
            TravelButton("Urgoz", 0, GW::Constants::MapID::Urgozs_Warren);
            TravelButton("Deep", 1, GW::Constants::MapID::The_Deep);

            for (int i = 0; i < fav_count; ++i) {
                ImGui::PushID(i);
                ImGui::PushItemWidth(-40.0f - ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::MyCombo("", "Select a favorite", &fav_index[static_cast<size_t>(i)], outpost_name_array_getter,
                    nullptr, N_OUTPOSTS);
                ImGui::PopItemWidth();
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Go", ImVec2(40.0f, 0))) {
                    TravelFavorite(static_cast<size_t>(i));
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

void TravelWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (scroll_to_outpost_id != GW::Constants::MapID::None) {
        ScrollToOutpost(scroll_to_outpost_id); // We're in the process of scrolling to an outpost
    }
    // Dynamically generate a list of all explorable areas that the game has rather than storing another massive const array.
    switch (fetched_searchable_explorable_areas) {
    case Pending:
        for (uint32_t i = 0; i < static_cast<uint32_t>(GW::Constants::MapID::Count); i++) {
            GW::AreaInfo* map = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(i));
            if (!map || !map->name_id || !map->GetIsOnWorldMap() || map->type != GW::RegionType::ExplorableZone)
                continue;
            searchable_explorable_area_ids.push_back(static_cast<GW::Constants::MapID>(i));
            GuiUtils::EncString* s = new GuiUtils::EncString(map->name_id);
            s->string(); // Trigger decode
            searchable_explorable_areas_decode.push_back(s);
        }
        fetched_searchable_explorable_areas = Decoding;
        break;
    case Decoding: {
        bool ok = true;
        for (size_t i = 0; i < searchable_explorable_areas_decode.size() && ok; i++) {
            ok = !searchable_explorable_areas_decode[i]->string().empty();
        }
        if (ok)
            fetched_searchable_explorable_areas = Decoded;
    } break;
    case Decoded:
        for (size_t i = 0; i < searchable_explorable_areas_decode.size(); i++) {
            std::string sanitised = GuiUtils::ToLower(GuiUtils::RemovePunctuation(searchable_explorable_areas_decode[i]->string()));
            char* out = new char[sanitised.length() + 1]; // NB: Delete this char* in destructor
            strcpy(out, sanitised.c_str());
            delete searchable_explorable_areas_decode[i];
            searchable_explorable_areas.push_back(out);
        }
        searchable_explorable_areas_decode.clear();
        fetched_searchable_explorable_areas = Ready;
        break;
    }

}
GW::Constants::MapID TravelWindow::GetNearestOutpost(GW::Constants::MapID map_to) {
    GW::AreaInfo* this_map = GW::Map::GetMapInfo(map_to);
    GW::AreaInfo* nearest = nullptr;
    GW::AreaInfo* map_info = nullptr;
    float nearest_distance = FLT_MAX;
    GW::Constants::MapID nearest_map_id = GW::Constants::MapID::None;

    auto get_pos = [](GW::AreaInfo* map) {
        GW::Vec2f pos = { (float)map->x,(float)map->y };
        if (!pos.x) {
            pos.x = (float)(map->icon_start_x + (map->icon_end_x - map->icon_start_x) / 2);
            pos.y = (float)(map->icon_start_y + (map->icon_end_y - map->icon_start_y) / 2);
        }
        if (!pos.x) {
            pos.x = (float)(map->icon_start_x_dupe + (map->icon_end_x_dupe - map->icon_start_x_dupe) / 2);
            pos.y = (float)(map->icon_start_y_dupe + (map->icon_end_y_dupe - map->icon_start_y_dupe) / 2);
        }
        return pos;
    };

    GW::Vec2f this_pos = get_pos(this_map);
    if (!this_pos.x)
        this_pos = { (float)this_map->icon_start_x,(float)this_map->icon_start_y };
    for (size_t i = 0; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        map_info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(i));
        if (!map_info || !map_info->thumbnail_id || !map_info->GetIsOnWorldMap())
            continue;
        if (map_info->campaign != this_map->campaign || map_info->region == GW::Region_Presearing)
            continue;
        switch (map_info->type) {
        case GW::RegionType::City:
        case GW::RegionType::CompetitiveMission:
        case GW::RegionType::CooperativeMission:
        case GW::RegionType::EliteMission:
        case GW::RegionType::MissionOutpost:
        case GW::RegionType::Outpost:
            break;
        default:
            continue;
        }
        //if ((map_info->flags & 0x5000000) != 0)
         //   continue; // e.g. "wrong" augury rock is map 119, no NPCs
        if (!GW::Map::GetIsMapUnlocked(static_cast<GW::Constants::MapID>(i)))
            continue;
        float dist = GW::GetDistance(this_pos, get_pos(map_info));
        if (dist < nearest_distance) {
            nearest_distance = dist;
            nearest = map_info;
            nearest_map_id = static_cast<GW::Constants::MapID>(i);
        }
    }
    return nearest_map_id;
}

bool TravelWindow::IsWaitingForMapTravel() {
    return GW::GetGameContext()->party != nullptr && (GW::GetGameContext()->party->flag & 0x8) > 0;
}

void TravelWindow::ScrollToOutpost(GW::Constants::MapID outpost_id, GW::Constants::District _district, uint32_t _district_number) {
    if (!GW::Map::GetIsMapLoaded() || (!GW::PartyMgr::GetIsPartyLoaded() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)) {
        map_travel_countdown_started = false;
        pending_map_travel = false;
        return; // Map loading, so we're no longer waiting for travel timer to start or finish.
    }
    if (IsWaitingForMapTravel()) {
        map_travel_countdown_started = true;
        return; // Currently in travel countdown. Wait until the countdown is complete or cancelled.
    }
    if (map_travel_countdown_started) {
        pending_map_travel = false;
        map_travel_countdown_started = false;
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return; // We were waiting for countdown, but it was cancelled.
    }
    if (pending_map_travel) {
        return; // Checking too soon; still waiting for either a map travel or a countdown for it.
    }

    GW::Constants::MapID map_id = GW::Map::GetMapID();
    if (scroll_to_outpost_id == GW::Constants::MapID::None) {
        scroll_to_outpost_id = outpost_id;
        scroll_from_outpost_id = map_id;
    }
    if (scroll_to_outpost_id != outpost_id) return; // Already travelling to another outpost
    if (map_id == outpost_id) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        if (!IsAlreadyInOutpost(outpost_id, _district, _district_number))
            UITravel(outpost_id, _district, _district_number);
        return; // Already at this outpost. Called GW::Map::Travel just in case district is different.
    }

    uint32_t scroll_model_id = 0;
    bool is_ready_to_scroll = map_id == GW::Constants::MapID::Embark_Beach;
    switch (scroll_to_outpost_id) {
    case GW::Constants::MapID::The_Deep:
        scroll_model_id = 22279;
        is_ready_to_scroll |= map_id == GW::Constants::MapID::Cavalon_outpost;
        break;
    case GW::Constants::MapID::Urgozs_Warren:
        scroll_model_id = 3256;
        is_ready_to_scroll |= map_id == GW::Constants::MapID::House_zu_Heltzer_outpost;
        break;
    default:
        Log::Error("Invalid outpost for scrolling");
        return;
    }
    if (!is_ready_to_scroll && scroll_from_outpost_id != map_id) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return; // Not in scrollable outpost, but we're not in the outpost we started from either - user has decided to travel somewhere else.
    }

    GW::Item* scroll_to_use = GW::Items::GetItemByModelId(
        scroll_model_id,
        static_cast<int>(GW::Constants::Bag::Backpack),
        static_cast<int>(GW::Constants::Bag::Storage_14));
    if (!scroll_to_use) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        Log::Error("No scroll found in inventory for travel");
        return; // No scroll found.
    }
    if (is_ready_to_scroll) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        GW::Items::UseItem(scroll_to_use);
        return; // Done.
    }
    pending_map_travel = Travel(GW::Constants::MapID::Embark_Beach, _district, _district_number);
    //GW::Map::Travel(GW::Constants::MapID::Embark_Beach, district, district_number); // Travel to embark.
}

bool TravelWindow::Travel(GW::Constants::MapID MapID, GW::Constants::District _district /*= 0*/, uint32_t _district_number) {
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        return false;
    if (!GW::Map::GetIsMapUnlocked(MapID)) {
        const GW::AreaInfo* map = GW::Map::GetMapInfo(MapID);
        wchar_t map_name_buf[8];
        wchar_t err_message_buf[256] = L"[Error] Your character does not have that map unlocked";
        if (map && map->name_id && GW::UI::UInt32ToEncStr(map->name_id, map_name_buf, 8))
            Log::ErrorW(L"[Error] Your character does not have \x1\x2%s\x2\x108\x107 unlocked", map_name_buf);
        else
            Log::ErrorW(err_message_buf);
        return false;
    }
    if (IsAlreadyInOutpost(MapID, _district, _district_number)) {
        Log::Error("[Error] You are already in the outpost");
        return false;
    }
    switch (MapID) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::Urgozs_Warren:
            ScrollToOutpost(MapID, _district, _district_number);
            break;
        default:
            UITravel(MapID, _district, _district_number);
            break;
    }
    return true;
    //return GW::Map::Travel(MapID, District, district_number);
}
void TravelWindow::UITravel(GW::Constants::MapID MapID, GW::Constants::District _district /*= 0*/, uint32_t _district_number) {

    MapStruct* t = new MapStruct();
    t->map_id = MapID;
    t->district_number = _district_number;
    t->region_id = RegionFromDistrict(_district);
    t->language_id = LanguageFromDistrict(_district);

    GW::GameThread::Enqueue([t] {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kTravel, (void*)t);
        delete t;
    });
}

int TravelWindow::RegionFromDistrict(GW::Constants::District _district) {
    switch (_district) {
    case GW::Constants::District::International: return GW::Constants::Region::International;
    case GW::Constants::District::American: return GW::Constants::Region::America;
    case GW::Constants::District::EuropeEnglish:
    case GW::Constants::District::EuropeFrench:
    case GW::Constants::District::EuropeGerman:
    case GW::Constants::District::EuropeItalian:
    case GW::Constants::District::EuropeSpanish:
    case GW::Constants::District::EuropePolish:
    case GW::Constants::District::EuropeRussian:
        return GW::Constants::Region::Europe;
    case GW::Constants::District::AsiaKorean: return GW::Constants::Region::Korea;
    case GW::Constants::District::AsiaChinese: return GW::Constants::Region::China;
    case GW::Constants::District::AsiaJapanese: return GW::Constants::Region::Japan;
    default:
        break;
    }
    return GW::Map::GetRegion();
}
int TravelWindow::LanguageFromDistrict(GW::Constants::District _district)
{
    switch (_district) {
    case GW::Constants::District::EuropeEnglish: return GW::Constants::EuropeLanguage::English;
    case GW::Constants::District::EuropeFrench: return GW::Constants::EuropeLanguage::French;
    case GW::Constants::District::EuropeGerman: return GW::Constants::EuropeLanguage::German;
    case GW::Constants::District::EuropeItalian: return GW::Constants::EuropeLanguage::Italian;
    case GW::Constants::District::EuropeSpanish: return GW::Constants::EuropeLanguage::Spanish;
    case GW::Constants::District::EuropePolish: return GW::Constants::EuropeLanguage::Polish;
    case GW::Constants::District::EuropeRussian: return GW::Constants::EuropeLanguage::Russian;
    case GW::Constants::District::AsiaKorean:
    case GW::Constants::District::AsiaChinese:
    case GW::Constants::District::AsiaJapanese:
    case GW::Constants::District::International:
    case GW::Constants::District::American:
        return 0;
    default:
        break;
    }
    return GW::Map::GetLanguage();
}

bool TravelWindow::TravelFavorite(unsigned int idx) {
    if (idx >= fav_index.size())    return false;
    Travel(IndexToOutpostID(fav_index[idx]), district, district_number);
    if (close_on_travel) visible = false;
    return true;
}

void TravelWindow::DrawSettingInternal() {
    ImGui::Checkbox("Close on travel", &close_on_travel);
    ImGui::ShowHelp("Will close the travel window when clicking on a travel destination");
    ImGui::PushItemWidth(100.0f);
    if (ImGui::InputInt("Number of favorites", &fav_count)) {
        if (fav_count < 0) fav_count = 0;
        if (fav_count > 100) fav_count = 100;
        fav_index.resize(static_cast<size_t>(fav_count), -1);
    }
    ImGui::PopItemWidth();
    ImGui::Checkbox("Automatically retry if the district is full", &retry_map_travel);
}

void TravelWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

    fav_count = ini->GetLongValue(Name(), VAR_NAME(fav_count), 3);
    fav_index.resize(static_cast<size_t>(fav_count), -1);
    for (int i = 0; i < fav_count; ++i) {
        char key[32];
        snprintf(key, 32, "Fav%d", i);
        fav_index[static_cast<size_t>(i)] = ini->GetLongValue(Name(), key, -1);
    }
    LOAD_BOOL(close_on_travel);
    LOAD_BOOL(retry_map_travel);
}

void TravelWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetLongValue(Name(), VAR_NAME(fav_count), fav_count);
    for (int i = 0; i < fav_count; ++i) {
        size_t ui = static_cast<size_t>(i);
        char key[32];
        snprintf(key, 32, "Fav%d", i);
        ini->SetLongValue(Name(), key, fav_index[ui]);
    }
    SAVE_BOOL(close_on_travel);
    SAVE_BOOL(retry_map_travel);
}

GW::Constants::MapID TravelWindow::IndexToOutpostID(int index) {
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
    case 17: return MapID::Beknur_Harbor;
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

void TravelWindow::CmdTP(const wchar_t *message, int argc, LPWSTR *argv)
{
    UNREFERENCED_PARAMETER(message);
    // zero argument error
    if (argc == 1) {
        Log::Error("[Error] Please provide an argument");
        return;
    }
    GW::Constants::MapID outpost = GW::Map::GetMapID();
    GW::Constants::District district = GW::Constants::District::Current;
    uint32_t district_number = 0;

    std::wstring argOutpost = GuiUtils::ToLower(argv[1]);
    std::wstring argDistrict = GuiUtils::ToLower(argv[argc - 1]);
    // Guild hall
    if (argOutpost == L"gh") {
        if (IsInGH())
            GW::GuildMgr::LeaveGH();
        else
            GW::GuildMgr::TravelGH();
        return;
    }
    TravelWindow &instance = Instance();
    if (argOutpost.size() > 2 && argOutpost.compare(0, 3, L"fav", 3) == 0) {
        std::wstring fav_s_num = argOutpost.substr(3, std::wstring::npos);
        if (fav_s_num.empty()) {
            instance.TravelFavorite(0);
            return;
        }
        uint32_t fav_num;
        if (GuiUtils::ParseUInt(fav_s_num.c_str(), &fav_num) && fav_num > 0) {
            instance.TravelFavorite(fav_num - 1);
            return;
        }
        Log::Error("[Error] Did not recognize favourite");
        return;
    }
    for (int i = 2; i < argc - 1; i++) {
        // Outpost name can be anything after "/tp" but before the district e.g. "/tp house zu heltzer ae1"
        argOutpost.append(L" ");
        argOutpost.append(GuiUtils::ToLower(argv[i]));
    }
    bool isValidDistrict = ParseDistrict(argDistrict, district, district_number);
    if (isValidDistrict && argc == 2) {
        // e.g. "/tp ae1"
        instance.Travel(outpost, district, district_number); // NOTE: ParseDistrict sets district and district_number vars by reference.
        return;
    }
    if (!isValidDistrict && argc > 2) {
        // e.g. "/tp house zu heltzer"
        argOutpost.append(L" ");
        argOutpost.append(argDistrict);
    }
    if (ParseOutpost(argOutpost, outpost, district, district_number)) {
        wchar_t first_char_of_last_arg = *argv[argc-1];
        switch (outpost) {
            case GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost:
            case GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost:
                if (first_char_of_last_arg == 'l') // - e.g. /tp viz local
                    outpost = GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost;
                else if (first_char_of_last_arg == 'f')
                    outpost = GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost;
                break;
            case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
            case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
                if (first_char_of_last_arg == 'l') // - e.g. /tp fa lux
                    outpost = GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost;
                else if (first_char_of_last_arg == 'k')
                    outpost = GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                else
                    outpost = IsLuxon() ? GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost : GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                break;
            case GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost:
            case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
                if (first_char_of_last_arg == 'l')  // - e.g. /tp jq lux
                    outpost = GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost;
                else if (first_char_of_last_arg == 'k') 
                    outpost = GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                else
                    outpost = IsLuxon() ? GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost : GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost;
                break;
            default:
                break;
        }
        instance.Travel(outpost, district, district_number); // NOTE: ParseOutpost sets outpost, district and district_number vars by reference.
        return;
    }
    Log::Error("[Error] Did not recognize outpost '%ls'", argOutpost.c_str());
    return;
}
bool TravelWindow::ParseOutpost(const std::wstring &s, GW::Constants::MapID &outpost, GW::Constants::District &district, uint32_t &number)
{
    UNREFERENCED_PARAMETER(number);
    // By Map ID e.g. "/tp 77" for house zu heltzer
    uint32_t map_id = 0;
    if (GuiUtils::ParseUInt(s.c_str(), &map_id))
        return outpost = (GW::Constants::MapID)map_id, true;

    TravelWindow &instance = Instance();

    // By full outpost name (without punctuation) e.g. "/tp GrEaT TemplE oF BalthaZAR"
    std::string compare = GuiUtils::ToLower(GuiUtils::RemovePunctuation(GuiUtils::WStringToString(s)));

    // Shortcut words e.g "/tp doa" for domain of anguish
    std::string first_word = compare.substr(0, compare.find(' '));
    const auto &shorthand_outpost = instance.shorthand_outpost_names.find(first_word);
    if (shorthand_outpost != instance.shorthand_outpost_names.end()) {
        const OutpostAlias &outpost_info = shorthand_outpost->second;
        outpost = outpost_info.map_id;
        if (outpost_info.district != GW::Constants::District::Current)
            district = outpost_info.district;
        return true;
    }

    // Remove "the " from front of entered string
    std::size_t found = compare.rfind("the ");
    if (found == 0)
        compare.replace(found, 4, "");

    // Helper function
    auto FindMatchingMap = [](const char* compare, const char **map_names, const GW::Constants::MapID *map_ids, size_t map_count) -> GW::Constants::MapID {
        const char* bestMatchMapName = nullptr;
        GW::Constants::MapID bestMatchMapID = GW::Constants::MapID::None;

        const auto searchStringLength = compare ? strlen(compare) : 0;
        if (!searchStringLength)
            return bestMatchMapID;
        for (size_t i = 0; i < map_count; i++) {
            const auto thisMapLength = strlen(map_names[i]);
            if (searchStringLength > thisMapLength)
                continue; // String entered by user is longer than this outpost name.
            if (strncmp(map_names[i],compare,searchStringLength) != 0)
                continue; // No match
            if (thisMapLength == searchStringLength)
                return map_ids[i]; // Exact match, break.
            if (!bestMatchMapName || strcmp(map_names[i],bestMatchMapName) < 0) {
                bestMatchMapID = map_ids[i];
                bestMatchMapName = map_names[i];
            }
        }
        return bestMatchMapID;
    };
    GW::Constants::MapID best_match_map_id = GW::Constants::MapID::None;
    if (ImInPresearing()) {
        best_match_map_id = FindMatchingMap(compare.c_str(), instance.presearing_map_names, instance.presearing_map_ids, _countof(instance.presearing_map_ids));
    } else {
        best_match_map_id = FindMatchingMap(compare.c_str(), instance.searchable_map_names, instance.searchable_map_ids, _countof(instance.searchable_map_ids));
        if (best_match_map_id == GW::Constants::MapID::None)
            best_match_map_id = FindMatchingMap(compare.c_str(), instance.searchable_dungeon_names, instance.dungeon_map_ids, _countof(instance.dungeon_map_ids));
        if (best_match_map_id == GW::Constants::MapID::None && instance.fetched_searchable_explorable_areas == Ready) {
            // find explorable area matching this, and then find nearest unlocked outpost.
            best_match_map_id = FindMatchingMap(compare.c_str(), const_cast<const char**>(instance.searchable_explorable_areas.data()), const_cast<const GW::Constants::MapID*>(instance.searchable_explorable_area_ids.data()), instance.searchable_explorable_area_ids.size());
            if(best_match_map_id != GW::Constants::MapID::None)
                best_match_map_id = GetNearestOutpost(best_match_map_id);
        }
    }

    if (best_match_map_id != GW::Constants::MapID::None)
        return outpost = best_match_map_id, true; // Exact match
    return false;
}
bool TravelWindow::ParseDistrict(const std::wstring &s, GW::Constants::District &district, uint32_t &number)
{
    std::string compare = GuiUtils::ToLower(GuiUtils::RemovePunctuation(GuiUtils::WStringToString(s)));
    std::string first_word = compare.substr(0, compare.find(' '));

    const std::regex district_regex("([a-z]{2,3})(\\d)?");
    std::smatch m;
    if (!std::regex_search(first_word, m, district_regex)) {
        return false;
    }
    // Shortcut words e.g "/tp ae" for american english
    TravelWindow& instance = Instance();
    const auto& shorthand_outpost = instance.shorthand_district_names.find(m[1].str());
    if (shorthand_outpost == instance.shorthand_district_names.end()) {
        return false;
    }
    district = shorthand_outpost->second.district;
    if (m.size() > 2 && !GuiUtils::ParseUInt(m[2].str().c_str(), &number)) {
        number = 0;
    }

    return true;
}
