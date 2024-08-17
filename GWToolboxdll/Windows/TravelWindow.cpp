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
#include <Modules/Resources.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/TravelWindowConstants.h>
#include <Utils/TextUtils.h>

namespace {
    bool search_in_english = true;

    std::string SanitiseForSearch(const std::wstring& in)
    {
        using namespace TextUtils;
        std::string sanitised = ToLower(RemovePunctuation(WStringToString(in)));
        // Remove "the " from front of entered string
        const size_t found = sanitised.rfind("the ");
        if (found == 0) {
            sanitised.replace(found, 4, "");
        }
        return sanitised;
    }

    // Maps map id to a searchable char array via Name()
    struct SearchableArea {
    protected:
        char* name = nullptr;
        GuiUtils::EncString* enc_name = nullptr;

    public:
        GW::Constants::MapID map_id = GW::Constants::MapID::None;

        SearchableArea(GW::Constants::MapID _map_id)
            : map_id(_map_id)
        {
            const auto map_info = GW::Map::GetMapInfo(map_id);
            if (!map_info) {
                name = new char[1];
            }
            else {
                enc_name = new GuiUtils::EncString(map_info->name_id);
                if (search_in_english)
                    enc_name->language(GW::Constants::Language::English);
                enc_name->wstring();
            }
        }

        ~SearchableArea()
        {
            delete enc_name;
            delete[] name;
        }

        const char* Name()
        {
            if (name) {
                return name;
            }
            if (!enc_name) {
                name = new char[1];
                return name;
            }
            if (enc_name->IsDecoding())
                return nullptr;
            // Sanitise for searching my removing punctuation etc
            const auto sanitised = SanitiseForSearch(enc_name->wstring());
            name = new char[sanitised.length() + 1];
            strcpy(name, sanitised.c_str());
            delete enc_name;
            enc_name = nullptr;
            return name;
        }
    };

    // List of explorables with decoded area names, used for searching via chat command
    std::vector<SearchableArea*> searchable_explorable_areas{};

    enum class FetchedMapNames : uint8_t {
        Pending,
        Decoding,
        Ready
    };

    FetchedMapNames fetched_searchable_explorable_areas = FetchedMapNames::Pending;

    // List of explorables with decoded area names, used for searching via chat command
    std::vector<SearchableArea*> searchable_outposts{};

    FetchedMapNames fetched_searchable_outposts = FetchedMapNames::Pending;

    TravelWindow& Instance()
    {
        return TravelWindow::Instance();
    }

    bool ImInPresearing() { return GW::Map::GetIsMapLoaded() && GW::Map::GetCurrentMapInfo()->region == GW::Region_Presearing; }

    bool IsInGH()
    {
        const auto* p = GW::GuildMgr::GetPlayerGuild();
        return p && p == GW::GuildMgr::GetCurrentGH();
    }

    bool IsLuxon()
    {
        GW::GuildContext* c = GW::GetGuildContext();
        return c && c->player_guild_index && c->guilds[c->player_guild_index]->faction;
    }

    bool IsAlreadyInOutpost(const GW::Constants::MapID outpost_id, const GW::Constants::District _district, const uint32_t _district_number = 0)
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
               && GW::Map::GetMapID() == outpost_id
               && GW::Map::RegionFromDistrict(_district) == GW::Map::GetRegion()
               && GW::Map::LanguageFromDistrict(_district) == GW::Map::GetLanguage()
               && (!_district_number || _district_number == static_cast<uint32_t>(GW::Map::GetDistrict()));
    }

    bool IsPreSearing(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        return map_info && map_info->region == GW::Region::Region_Presearing;
    }

    bool IsValidOutpost(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !map_info->icon_start_x)
            return false;
        if ((map_info->flags & 0x5000000) == 0x5000000)
            return false; // e.g. "wrong" augury rock is map 119, no NPCs
        if ((map_info->flags & 0x80000000) == 0x80000000)
            return false; // e.g. Debug map
        switch (map_info->type) {
            case GW::RegionType::City:
            case GW::RegionType::CompetitiveMission:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::EliteMission:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::Outpost:
                break;
            default:
                return false;
        }
        return true;
    }

    struct MapStruct {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::ServerRegion region_id = GW::Constants::ServerRegion::International;
        GW::Constants::Language language_id = GW::Constants::Language::English;
        uint32_t district_number = 0;
    };

    struct UIErrorMessage {
        int error_index;
        wchar_t* message;
    };

    bool retry_map_travel = false;
    MapStruct pending_map_travel;

    constexpr auto messages_to_hook = {
        GW::UI::UIMessage::kErrorMessage,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kTravel
    };
    GW::HookEntry OnUIMessage_HookEntry;


    // ==== Favorites ====
    std::vector<GW::Constants::MapID> favourites{};

    // ==== options ====
    bool close_on_travel = false;
    bool collapse_on_travel = false;

    // ==== scroll to outpost ====
    GW::Constants::MapID scroll_to_outpost_id = GW::Constants::MapID::None;   // Which outpost do we want to end up in?
    GW::Constants::MapID scroll_from_outpost_id = GW::Constants::MapID::None; // Which outpost did we start from?

    bool map_travel_countdown_started = false;

    bool to_minimize = false;

    IDirect3DTexture9** scroll_texture = nullptr;

    /* Not used, but good to keep for reference!
    enum error_message_ids {
    error_B29 = 52,
    error_B30,
    error_B31,
    error_B32,
    error_B33,
    error_B34,
    error_B35,
    error_B36,
    error_B37,
    error_B38
    };
    enum error_message_trans_codes {
    error_B29 = 0xB29, // The target party has members who do not meet this mission's level requirements.
    error_B30, // You may not enter that outpost
    error_B31, // Your party leader must be a member of this guild. An officer from this guild must also be in the party.
    error_B32, // You must be the leader of your party to do that.
    error_B33, // You must be a member of a party to do that.
    error_B34, // Your party is already waiting to go somewhere else.
    error_B35, // Your party is already in that guild hall.
    error_B36, // Your party is already in that district.
    error_B37, // Your party is already in the active district.
    error_B38, // The merged party would be too large.
    };  */

    void OnUIMessage(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kMapChange: {
                pending_map_travel.map_id = GW::Constants::MapID::None;
            }
            break;
            case GW::UI::UIMessage::kTravel: {
                const auto t = static_cast<MapStruct*>(wparam);
                if (t && t != &pending_map_travel) {
                    pending_map_travel = *t;
                }
            }
            break;
            case GW::UI::UIMessage::kErrorMessage: {
                if (!(retry_map_travel && pending_map_travel.map_id != GW::Constants::MapID::None)) {
                    break;
                }
                const auto msg = static_cast<UIErrorMessage*>(wparam);
                if (msg && msg->message && *msg->message == 0xb25) {
                    // Travel failed, but we want to retry
                    // NB: 0xb25 = "That district is full. Please select another."
                    status->blocked = true;
                    SendUIMessage(GW::UI::UIMessage::kTravel, &pending_map_travel);
                }
            }
            break;
        }
    }

    // ==== Helpers ====
    GW::Constants::MapID IndexToOutpostID(const int index)
    {
        if (static_cast<size_t>(index) < searchable_outposts.size()) {
            return searchable_outposts[index]->map_id;
        }
        return GW::Constants::MapID::Great_Temple_of_Balthazar_outpost;
    }

    int OutpostIDToIndex(GW::Constants::MapID map_id)
    {
        for (size_t i = 0, size = searchable_outposts.size(); i < size; i++) {
            if (searchable_outposts[i]->map_id == map_id)
                return i;
        }
        return -1;
    }

    bool ParseOutpost(const std::wstring& s, GW::Constants::MapID& outpost, GW::Constants::District& district, const uint32_t&)
    {
        // By Map ID e.g. "/tp 77" for house zu heltzer
        uint32_t map_id = 0;
        if (TextUtils::ParseUInt(s.c_str(), &map_id)) {
            return outpost = static_cast<GW::Constants::MapID>(map_id), true;
        }

        // By full outpost name (without punctuation) e.g. "/tp GrEaT TemplE oF BalthaZAR"
        std::string compare = SanitiseForSearch(s);

        // Shortcut words e.g "/tp doa" for domain of anguish
        const std::string first_word = compare.substr(0, compare.find(' '));
        const auto& shorthand_outpost = shorthand_outpost_names.find(first_word);
        if (shorthand_outpost != shorthand_outpost_names.end()) {
            const OutpostAlias& outpost_info = shorthand_outpost->second;
            outpost = outpost_info.map_id;
            if (outpost_info.district != GW::Constants::District::Current) {
                district = outpost_info.district;
            }
            return true;
        }

        // Helper function
        auto FindMatchingMap = [](const char* compare, const char* const* map_names, const GW::Constants::MapID* map_ids, const size_t map_count) -> GW::Constants::MapID {
            const char* bestMatchMapName = nullptr;
            auto bestMatchMapID = GW::Constants::MapID::None;

            const auto searchStringLength = compare ? strlen(compare) : 0;
            if (!searchStringLength) {
                return bestMatchMapID;
            }
            for (size_t i = 0; i < map_count; i++) {
                const auto thisMapLength = strlen(map_names[i]);
                if (searchStringLength > thisMapLength) {
                    continue; // String entered by user is longer than this outpost name.
                }
                if (strncmp(map_names[i], compare, searchStringLength) != 0) {
                    continue; // No match
                }
                if (thisMapLength == searchStringLength) {
                    return map_ids[i]; // Exact match, break.
                }
                if (!bestMatchMapName || strcmp(map_names[i], bestMatchMapName) < 0) {
                    bestMatchMapID = map_ids[i];
                    bestMatchMapName = map_names[i];
                }
            }
            return bestMatchMapID;
        };
        // Helper function
        auto FindMatchingMapVec = [](const char* compare, std::vector<SearchableArea*>& maps) -> GW::Constants::MapID {
            const char* bestMatchMapName = nullptr;
            auto bestMatchMapID = GW::Constants::MapID::None;

            const auto searchStringLength = compare ? strlen(compare) : 0;
            if (!searchStringLength) {
                return bestMatchMapID;
            }
            for (auto it : maps) {
                auto& map = *it;
                if (!map.Name())
                    continue;
                const auto thisMapLength = strlen(map.Name());
                if (searchStringLength > thisMapLength) {
                    continue; // String entered by user is longer than this outpost name.
                }
                if (strncmp(map.Name(), compare, searchStringLength) != 0) {
                    continue; // No match
                }
                if (thisMapLength == searchStringLength) {
                    return map.map_id; // Exact match, break.
                }
                if (!bestMatchMapName || strcmp(map.Name(), bestMatchMapName) < 0) {
                    bestMatchMapID = map.map_id;
                    bestMatchMapName = map.Name();
                }
            }
            return bestMatchMapID;
        };
        auto best_match_map_id = GW::Constants::MapID::None;
        if (ImInPresearing()) {
            best_match_map_id = FindMatchingMap(compare.c_str(), presearing_map_names.data(), presearing_map_ids.data(), presearing_map_ids.size());
        }
        else {
            if (best_match_map_id == GW::Constants::MapID::None && fetched_searchable_outposts == FetchedMapNames::Ready) {
                // find explorable area matching this, and then find nearest unlocked outpost.
                best_match_map_id = FindMatchingMapVec(compare.c_str(), searchable_outposts);
            }
            if (best_match_map_id == GW::Constants::MapID::None && fetched_searchable_explorable_areas == FetchedMapNames::Ready) {
                // find explorable area matching this, and then find nearest unlocked outpost.
                best_match_map_id = FindMatchingMapVec(compare.c_str(), searchable_explorable_areas);
                if (best_match_map_id != GW::Constants::MapID::None) {
                    best_match_map_id = Instance().GetNearestOutpost(best_match_map_id);
                }
            }
        }

        if (best_match_map_id != GW::Constants::MapID::None) {
            return outpost = best_match_map_id, true; // Exact match
        }
        return false;
    }

    bool ParseDistrict(const std::wstring& s, GW::Constants::District& district, uint32_t& number)
    {
        std::string compare = TextUtils::ToLower(TextUtils::RemovePunctuation(TextUtils::WStringToString(s)));
        const std::string first_word = compare.substr(0, compare.find(' '));

        static const std::regex district_regex("([a-z]{2,3})(\\d)?");
        std::smatch m;
        if (!std::regex_search(first_word, m, district_regex)) {
            return false;
        }
        // Shortcut words e.g "/tp ae" for american english
        const auto& shorthand_outpost = shorthand_district_names.find(m[1].str());
        if (shorthand_outpost == shorthand_district_names.end()) {
            return false;
        }
        district = shorthand_outpost->second.district;
        if (m.size() > 2 && !TextUtils::ParseUInt(m[2].str().c_str(), &number)) {
            number = 0;
        }

        return true;
    }

    void CHAT_CMD_FUNC(CmdTP)
    {
        // zero argument error
        if (argc == 1) {
            Log::Error("[Error] Please provide an argument");
            return;
        }
        GW::Constants::MapID outpost = GW::Map::GetMapID();
        auto district = GW::Constants::District::Current;
        uint32_t district_number = 0;

        std::wstring argOutpost = TextUtils::ToLower(argv[1]);
        const std::wstring argDistrict = TextUtils::ToLower(argv[argc - 1]);
        if (argOutpost == L"stop") {
            pending_map_travel.map_id = GW::Constants::MapID::None;
            return;
        }
        // Guild hall
        if (argOutpost == L"gh") {
            if (IsInGH()) {
                GW::GuildMgr::LeaveGH();
            }
            else {
                GW::GuildMgr::TravelGH();
            }
            return;
        }

        if (argOutpost == L"nick" || argOutpost == L"nicholas") {
            const auto nick = DailyQuests::GetNicholasTheTraveller();
            Instance().TravelNearest(nick->map_id);
            return;
        }
        if (argOutpost == L"zv") {
            GW::Chat::SendChat('/', L"zv travel");
            return;
        }
        if (argOutpost == L"zm") {
            GW::Chat::SendChat('/', L"zm travel");
            return;
        }
        if (argOutpost == L"zb") {
            GW::Chat::SendChat('/', L"zb travel");
            return;
        }

        TravelWindow& instance = Instance();
        if (argOutpost.size() > 2 && argOutpost.compare(0, 3, L"fav", 3) == 0) {
            const std::wstring fav_s_num = argOutpost.substr(3, std::wstring::npos);
            if (fav_s_num.empty()) {
                instance.TravelFavorite(0);
                return;
            }
            uint32_t fav_num;
            if (TextUtils::ParseUInt(fav_s_num.c_str(), &fav_num) && fav_num > 0) {
                instance.TravelFavorite(fav_num - 1);
                return;
            }
            Log::Error("[Error] Did not recognize favourite");
            return;
        }
        for (auto i = 2; i < argc - 1; i++) {
            // Outpost name can be anything after "/tp" but before the district e.g. "/tp house zu heltzer ae1"
            argOutpost.append(L" ");
            argOutpost.append(TextUtils::ToLower(argv[i]));
        }
        const bool isValidDistrict = ParseDistrict(argDistrict, district, district_number);
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
            const wchar_t first_char_of_last_arg = *argv[argc - 1];
            switch (outpost) {
                case GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost:
                case GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost:
                    if (first_char_of_last_arg == 'l') // - e.g. /tp viz local
                    {
                        outpost = GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost;
                    }
                    else if (first_char_of_last_arg == 'f') {
                        outpost = GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost;
                    }
                    break;
                case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
                case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
                    if (first_char_of_last_arg == 'l') // - e.g. /tp fa lux
                    {
                        outpost = GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost;
                    }
                    else if (first_char_of_last_arg == 'k') {
                        outpost = GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                    }
                    else {
                        outpost = IsLuxon() ? GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost : GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                    }
                    break;
                case GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost:
                case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
                    if (first_char_of_last_arg == 'l') // - e.g. /tp jq lux
                    {
                        outpost = GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost;
                    }
                    else if (first_char_of_last_arg == 'k') {
                        outpost = GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost;
                    }
                    else {
                        outpost = IsLuxon() ? GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost : GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost;
                    }
                    break;
                default:
                    break;
            }
            instance.Travel(outpost, district, district_number); // NOTE: ParseOutpost sets outpost, district and district_number vars by reference.
            return;
        }
        Log::Error("[Error] Did not recognize outpost '%ls'", argOutpost.c_str());
    }

    const char* GetMapName(GW::Constants::MapID map_id)
    {
        if (map_id < GW::Constants::MapID::None || map_id > GW::Constants::MapID::Count)
            return "...";
        return Resources::GetMapName(map_id)->string().c_str();
    }

    bool outpost_name_array_getter(void* /* _data */, int idx, const char** out_text)
    {
        if (idx < 0 || static_cast<size_t>(idx) > searchable_outposts.size()) {
            return false;
        }

        *out_text = GetMapName(searchable_outposts[idx]->map_id);
        return true;
    }

    void BuildSearchableAreas(std::vector<SearchableArea*>& vec, std::function<bool(GW::Constants::MapID, const GW::AreaInfo*)> cmp)
    {
        for (auto& ptr : vec) {
            delete ptr;
        }
        vec.clear();
        std::unordered_map<uint32_t, bool> added_map_name_ids;
        for (auto i = 1u; i < static_cast<uint32_t>(GW::Constants::MapID::Count); i++) {
            const auto& map_id = static_cast<GW::Constants::MapID>(i);
            const GW::AreaInfo* map = GW::Map::GetMapInfo(map_id);
            if (!cmp(map_id, map))
                continue;
            vec.push_back(new SearchableArea(map_id));
        }
    }

    bool CheckSearchableAreasDecoded(std::vector<SearchableArea*>& vec)
    {
        std::unordered_map<std::string, SearchableArea*> searchable_area_indeces_by_name;
        for (size_t i = 0, size = vec.size(); i < size; i++) {
            const auto it = vec[i];
            if (!it->Name())
                return false;
            // Now that this one has decoded, check that theres nothing already in the searchable areas that have the same name, e.g. festival outposts
            if (searchable_area_indeces_by_name.contains(it->Name())) {
                vec.erase(vec.begin() + i);
                delete it;
                return CheckSearchableAreasDecoded(vec);
            }
            searchable_area_indeces_by_name[it->Name()] = it;
        }
        std::ranges::sort(vec, [](SearchableArea* lhs, SearchableArea* rhs) {
            return strcmp(GetMapName(lhs->map_id), GetMapName(rhs->map_id)) < 0;
        });
        return true;
    }
}

void TravelWindow::Initialize()
{
    ToolboxWindow::Initialize();
    scroll_texture = Resources::GetItemImage(L"Passage Scroll to the Deep");
    district = GW::Constants::District::Current;
    district_number = 0;

    GW::Chat::CreateCommand(L"tp", &CmdTP);
    GW::Chat::CreateCommand(L"to", &CmdTP);
    GW::Chat::CreateCommand(L"travel", &CmdTP);

    for (const auto message_id : messages_to_hook) {
        RegisterUIMessageCallback(&OnUIMessage_HookEntry, message_id, OnUIMessage);
    }
}

void TravelWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (auto& s : searchable_explorable_areas) {
        delete s;
    }
    searchable_explorable_areas.clear();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void TravelWindow::TravelButton(const GW::Constants::MapID mapid, const int x_idx) const
{
    const auto text = GetMapName(mapid);
    if (!(text && *text))
        return;
    if (x_idx != 0) {
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    }
    const float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemInnerSpacing.x) / 2 - ImGui::GetStyle().WindowPadding.x;
    bool clicked = false;
    switch (mapid) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::Urgozs_Warren:
            clicked |= ImGui::IconButton(text, *scroll_texture, ImVec2(w, 0));
            break;
        default:
            clicked |= ImGui::Button(text, ImVec2(w, 0));
            break;
    }
    if (clicked) {
        Instance().Travel(mapid, district, district_number);
    }
}

void TravelWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (to_minimize) {
        ImGui::SetNextWindowCollapsed(true);
        to_minimize = false;
    }

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (ImInPresearing()) {
            TravelButton(GW::Constants::MapID::Ascalon_City_pre_searing, 0);
            TravelButton(GW::Constants::MapID::Ashford_Abbey_outpost, 1);
            TravelButton(GW::Constants::MapID::Foibles_Fair_outpost, 0);
            TravelButton(GW::Constants::MapID::Fort_Ranik_pre_Searing_outpost, 1);
            TravelButton(GW::Constants::MapID::The_Barradin_Estate_outpost, 0);
        }
        else {
            ImGui::PushItemWidth(-1.0f);
            static int travelto_index = -1;
            if (ImGui::MyCombo("###travelto", "Travel To...", &travelto_index, outpost_name_array_getter, nullptr, searchable_outposts.size())) {
                const auto map_id = IndexToOutpostID(travelto_index);
                Travel(map_id, district, district_number);
                travelto_index = -1;
            }

            static int district_index = 0;
            if (ImGui::Combo("###district", &district_index, district_words.data(), district_words.size())) {
                district_number = 0;
                if (static_cast<size_t>(district_index) < district_ids.size()) {
                    district = district_ids[district_index];
                    if (district_index == 3) {
                        // American 1
                        district_number = 1;
                    }
                }
            }
            ImGui::PopItemWidth();

            TravelButton(GW::Constants::MapID::Temple_of_the_Ages, 0);
            TravelButton(GW::Constants::MapID::Domain_of_Anguish, 1);
            TravelButton(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost, 0);
            TravelButton(GW::Constants::MapID::Embark_Beach, 1);
            TravelButton(GW::Constants::MapID::Vloxs_Falls, 0);
            TravelButton(GW::Constants::MapID::Gadds_Encampment_outpost, 1);
            TravelButton(GW::Constants::MapID::Urgozs_Warren, 0);
            TravelButton(GW::Constants::MapID::The_Deep, 1);

            static int editing = -1;
#
            const auto spacing = ImGui::GetStyle().ItemSpacing.x;
            const auto btn_w = (ImGui::GetIO().FontGlobalScale * 30.f);
            for (size_t i = 0, size = favourites.size(); i < size; i++) {
                ImGui::PushID(i);
                const auto map_id = favourites[i];
                if (editing == static_cast<int>(i) || map_id == GW::Constants::MapID::None) {
                    auto btn_count = 4;
                    if (i == 0 || i + 1 == size)
                        btn_count--;

                    ImGui::PushItemWidth(((btn_w + spacing) * btn_count) * -1);
                    // find the index that matches the map from the array of map ids
                    const auto combo_val = OutpostIDToIndex(map_id);
                    ImGui::MyCombo("", "Select a favorite", (int*)&combo_val, outpost_name_array_getter, nullptr, searchable_outposts.size());
                    favourites[i] = IndexToOutpostID(combo_val);
                    ImGui::PopItemWidth();

                    if (i > 0) {
                        ImGui::SameLine();
                        if (ImGui::ButtonWithHint(ICON_FA_CHEVRON_UP, "Move up", ImVec2(btn_w, 0))) {
                            auto tmp = favourites[i - 1];
                            favourites[i - 1] = map_id;
                            favourites[i] = tmp;
                            editing = i - 1;
                        }
                    }
                    if (i + 1 < size) {
                        ImGui::SameLine();
                        if (ImGui::ButtonWithHint(ICON_FA_CHEVRON_DOWN, "Move down", ImVec2(btn_w, 0))) {
                            auto tmp = favourites[i + 1];
                            favourites[i + 1] = map_id;
                            favourites[i] = tmp;
                            editing = i + 1;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::ButtonWithHint(ICON_FA_TRASH, "Delete", ImVec2(btn_w, 0))) {
                        favourites.erase(favourites.begin() + i);
                        editing = -1;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();
                    if (ImGui::ButtonWithHint(ICON_FA_CHECK, "Done", ImVec2(btn_w, 0))) {
                        editing = -1;
                    }
                }
                else {
                    if (ImGui::Button(GetMapName(map_id), ImVec2((btn_w + spacing) * -1, 0))) {
                        editing = -1;
                        TravelFavorite(i);
                    }
                    ImGui::SameLine();
                    if (ImGui::ButtonWithHint(ICON_FA_PEN, "Edit", ImVec2(btn_w, 0))) {
                        editing = i;
                    }
                }

                ImGui::PopID();
            }
            if (ImGui::Button("Add", ImVec2(btn_w * 2, 0))) {
                favourites.push_back(GW::Constants::MapID::None);
            }
        }
        if (pending_map_travel.map_id != GW::Constants::MapID::None && IsValidOutpost(pending_map_travel.map_id)) {
            const auto abort_str = std::format("Abort retrying travel to {}", GetMapName(pending_map_travel.map_id));
            if (ImGui::Button(abort_str.c_str())) {
                pending_map_travel.map_id = GW::Constants::MapID::None;
            }
        }
    }
    ImGui::End();
}

void TravelWindow::Update(const float)
{
    if (scroll_to_outpost_id != GW::Constants::MapID::None) {
        ScrollToOutpost(scroll_to_outpost_id); // We're in the process of scrolling to an outpost
    }

    // Dynamically generate a list of all explorable areas that the game has rather than storing another massive const array.
    switch (fetched_searchable_explorable_areas) {
        case FetchedMapNames::Pending: {
            BuildSearchableAreas(searchable_explorable_areas, [](GW::Constants::MapID, const GW::AreaInfo* map) {
                return map && map->name_id && map->GetIsOnWorldMap() && map->type == GW::RegionType::ExplorableZone;
            });
            fetched_searchable_explorable_areas = FetchedMapNames::Decoding;
        }
        break;
        case FetchedMapNames::Decoding: {
            if (CheckSearchableAreasDecoded(searchable_explorable_areas)) {
                fetched_searchable_explorable_areas = FetchedMapNames::Ready;
            }
        }
        break;
    }

    // Dynamically generate a list of all outposts that the game has rather than storing another massive const array.
    switch (fetched_searchable_outposts) {
        case FetchedMapNames::Pending: {
            BuildSearchableAreas(searchable_outposts, [](GW::Constants::MapID map_id, const GW::AreaInfo*) {
                return IsValidOutpost(map_id) && !IsPreSearing(map_id);
            });
            fetched_searchable_outposts = FetchedMapNames::Decoding;
        }
        break;
        case FetchedMapNames::Decoding: {
            if (CheckSearchableAreasDecoded(searchable_outposts)) {
                fetched_searchable_outposts = FetchedMapNames::Ready;
            }
        }
        break;
    }
}

GW::Constants::MapID TravelWindow::GetNearestOutpost(const GW::Constants::MapID map_to)
{
    static const auto special_cases = std::map<GW::Constants::MapID, GW::Constants::MapID>{
        {GW::Constants::MapID::Kessex_Peak, GW::Constants::MapID::Temple_of_the_Ages},
    };

    if (special_cases.contains(map_to)) {
        return special_cases.at(map_to);
    }

    const GW::AreaInfo* this_map = GW::Map::GetMapInfo(map_to);
    float nearest_distance = std::numeric_limits<float>::max();
    auto nearest_map_id = GW::Constants::MapID::None;

    auto get_pos = [](const GW::AreaInfo* map) {
        GW::Vec2f pos = {static_cast<float>(map->x), static_cast<float>(map->y)};
        if (!pos.x) {
            pos.x = static_cast<float>(map->icon_start_x + (map->icon_end_x - map->icon_start_x) / 2);
            pos.y = static_cast<float>(map->icon_start_y + (map->icon_end_y - map->icon_start_y) / 2);
        }
        if (!pos.x) {
            pos.x = static_cast<float>(map->icon_start_x_dupe + (map->icon_end_x_dupe - map->icon_start_x_dupe) / 2);
            pos.y = static_cast<float>(map->icon_start_y_dupe + (map->icon_end_y_dupe - map->icon_start_y_dupe) / 2);
        }
        return pos;
    };

    GW::Vec2f this_pos = get_pos(this_map);
    if (!this_pos.x) {
        this_pos = {static_cast<float>(this_map->icon_start_x), static_cast<float>(this_map->icon_start_y)};
    }
    for (size_t i = 0; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        const auto& map_id = static_cast<GW::Constants::MapID>(i);
        if (!GW::Map::GetIsMapUnlocked(map_id))
            continue;
        if (!IsValidOutpost(map_id))
            continue;
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (map_info->campaign != this_map->campaign || map_info->region == GW::Region_Presearing)
            continue;
        //if ((map_info->flags & 0x5000000) != 0)
        //   continue; // e.g. "wrong" augury rock is map 119, no NPCs
        const float dist = GetDistance(this_pos, get_pos(map_info));
        if (dist < nearest_distance) {
            nearest_distance = dist;
            nearest_map_id = static_cast<GW::Constants::MapID>(i);
        }
    }
    return nearest_map_id;
}

bool TravelWindow::IsWaitingForMapTravel()
{
    return GW::GetGameContext()->party != nullptr && (GW::GetGameContext()->party->flag & 0x8) > 0;
}

void TravelWindow::ScrollToOutpost(const GW::Constants::MapID outpost_id, const GW::Constants::District _district, const uint32_t _district_number)
{
    if (!GW::Map::GetIsMapLoaded() || (!GW::PartyMgr::GetIsPartyLoaded() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)) {
        map_travel_countdown_started = false;
        pending_map_travel.map_id = GW::Constants::MapID::None;
        return; // Map loading, so we're no longer waiting for travel timer to start or finish.
    }
    if (IsWaitingForMapTravel()) {
        map_travel_countdown_started = true;
        return; // Currently in travel countdown. Wait until the countdown is complete or cancelled.
    }
    if (map_travel_countdown_started) {
        pending_map_travel.map_id = GW::Constants::MapID::None;
        map_travel_countdown_started = false;
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return; // We were waiting for countdown, but it was cancelled.
    }
    if (pending_map_travel.map_id != GW::Constants::MapID::None) {
        return; // Checking too soon; still waiting for either a map travel or a countdown for it.
    }

    const GW::Constants::MapID map_id = GW::Map::GetMapID();
    if (scroll_to_outpost_id == GW::Constants::MapID::None) {
        scroll_to_outpost_id = outpost_id;
        scroll_from_outpost_id = map_id;
    }
    if (scroll_to_outpost_id != outpost_id) {
        return; // Already travelling to another outpost
    }
    if (map_id == outpost_id) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        if (!IsAlreadyInOutpost(outpost_id, _district, _district_number)) {
            GW::Map::Travel(outpost_id, _district, _district_number);
        }
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

    const GW::Item* scroll_to_use = GW::Items::GetItemByModelId(
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
    // Travel to embark.
    if (!Travel(GW::Constants::MapID::Embark_Beach, _district, _district_number)) {
        // Failed to move to outpost for scrolling
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return;
    }
}

bool TravelWindow::TravelNearest(const GW::Constants::MapID map_id)
{
    const auto outpost = GetNearestOutpost(map_id);
    return Travel(outpost);
}

bool TravelWindow::Travel(const GW::Constants::MapID map_id, const GW::Constants::District _district /*= 0*/, const uint32_t _district_number)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return false;
    }
    if (!GW::Map::GetIsMapUnlocked(map_id)) {
        const GW::AreaInfo* map = GW::Map::GetMapInfo(map_id);
        wchar_t map_name_buf[8];
        constexpr wchar_t err_message_buf[256] = L"[Error] Your character does not have that map unlocked";
        if (map && map->name_id && GW::UI::UInt32ToEncStr(map->name_id, map_name_buf, 8)) {
            Log::ErrorW(L"[Error] Your character does not have \x1\x2%s\x2\x108\x107 unlocked", map_name_buf);
        }
        else {
            Log::ErrorW(err_message_buf);
        }
        return false;
    }
    if (IsAlreadyInOutpost(map_id, _district, _district_number)) {
        Log::Error("[Error] You are already in the outpost");
        return true;
    }

    if (collapse_on_travel) {
        to_minimize = true;
    }

    if (close_on_travel) {
        visible = false;
    }

    switch (map_id) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::Urgozs_Warren:
            ScrollToOutpost(map_id, _district, _district_number);
            break;
        default:
            GW::Map::Travel(map_id, _district, _district_number);
            break;
    }
    return true;
    //return GW::Map::Travel(map_id, District, district_number);
}

bool TravelWindow::TravelFavorite(const unsigned int idx)
{
    if (idx >= favourites.size()) {
        return false;
    }
    Travel(favourites[idx], district, district_number);

    return true;
}

void TravelWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Close on travel", &close_on_travel);
    ImGui::ShowHelp("Will close the travel window when clicking on a travel destination");
    ImGui::Checkbox("Collapse on travel", &collapse_on_travel);
    ImGui::ShowHelp("Will collapse the travel window when clicking on a travel destination");
    ImGui::Checkbox("Automatically retry if the district is full", &retry_map_travel);
    ImGui::ShowHelp("Use /tp stop to stop retrying.");
    ImGui::Checkbox("Use English map names", &search_in_english);
    ImGui::ShowHelp("If this is unchecked, the /tp command will use the localized map names based on your current language.");
}

void TravelWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);


    size_t fav_count = 0;
    LOAD_UINT(fav_count);
    favourites.clear();
    for (size_t i = 0; i < fav_count; i++) {
        char key[32];
        snprintf(key, _countof(key), "Fav%d", i);
        const auto map_id = static_cast<GW::Constants::MapID>(ini->GetLongValue(Name(), key, (int)GW::Constants::MapID::None));
        if (map_id < GW::Constants::MapID::Count && map_id > GW::Constants::MapID::None)
            favourites.push_back(map_id);
    }
    LOAD_BOOL(close_on_travel);
    LOAD_BOOL(collapse_on_travel);
    LOAD_BOOL(retry_map_travel);
    LOAD_BOOL(search_in_english);
}

void TravelWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    size_t fav_count = favourites.size();
    SAVE_UINT(fav_count);
    for (size_t i = 0, size = favourites.size(); i < size; i++) {
        char key[32];
        snprintf(key, _countof(key), "Fav%d", i);
        ini->SetLongValue(Name(), key, static_cast<int>(favourites[i]));
    }
    SAVE_BOOL(close_on_travel);
    SAVE_BOOL(collapse_on_travel);
    SAVE_BOOL(retry_map_travel);
    SAVE_BOOL(search_in_english);
}
