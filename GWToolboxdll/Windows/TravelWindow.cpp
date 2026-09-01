#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>

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
#include <Widgets/CartographerWidget.h>
#include <Modules/Resources.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/TravelWindowConstants.h>
#include <Constants/MapAdjacency.h>
#include <Utils/TextUtils.h>
#include <GWCA/Managers/QuestMgr.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/WorldMapWidget.h>

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    TravelWindow::Settings settings;

    std::string SanitiseForSearch(const std::wstring& in)
    {
        using namespace TextUtils;
        std::string sanitised = ToLower(RemovePunctuation(WStringToString(in)));
        // 从输入字符串开头移除 "the "
        const size_t found = sanitised.rfind("the ");
        if (found == 0) {
            sanitised.replace(found, 4, "");
        }
        return sanitised;
    }

    // 将地图 ID 映射到可通过 Name() 搜索的字符数组
    struct SearchableArea {
    protected:
        char* name = nullptr;
        std::unique_ptr<GuiUtils::EncString> enc_name;

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
                enc_name = std::make_unique<GuiUtils::EncString>(map_info->name_id);
                if (settings.search_in_english)
                    enc_name->language(GW::Constants::Language::English);
                enc_name->wstring();
            }
        }

        ~SearchableArea()
        {
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
            const auto sanitised = SanitiseForSearch(enc_name->wstring());
            name = new char[sanitised.length() + 1];
            strcpy(name, sanitised.c_str());
            enc_name.reset();
            return name;
        }
    };

    enum class FetchedMapNames : uint8_t {
        Pending,
        Decoding,
        Ready
    };

    // 按世界划分的可搜索旅行目的地的拥有列表
    std::vector<SearchableArea*> pre_searchable_areas{};
    std::vector<SearchableArea*> post_searchable_areas{};
    // 指向与玩家当前世界匹配的两个列表之一
    std::vector<SearchableArea*>* visible_searchable_areas = nullptr;

    FetchedMapNames fetched_searchable_areas = FetchedMapNames::Pending;

    TravelWindow& Instance()
    {
        return TravelWindow::Instance();
    }

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



    bool IsValidOutpost(const GW::Constants::MapID map_id)
    {
        if (map_id == GW::Constants::MapID::Gate_of_Anguish_elite_mission)
            return false;
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!GW::Map::HasMapDisplayInfo(map_info) || GW::Map::IsExcludedMapInfo(map_info))
            return false;
        switch (map_info->type) {
            case GW::RegionType::City:
            case GW::RegionType::Challenge:
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

    std::vector<TravelWindow::AliasEntry> user_aliases{};

    void PopulateDefaultAliases()
    {
        user_aliases.clear();
        for (const auto& [key, val] : default_outpost_aliases) {
            TravelWindow::AliasEntry entry{};
            entry.alias = key;
            entry.map_id = val.map_id;
            entry.district = val.district;
            entry.district_number = val.district_number;
            user_aliases.push_back(entry);
        }
        std::ranges::sort(user_aliases, [](const TravelWindow::AliasEntry& a, const TravelWindow::AliasEntry& b) {
            return a.alias < b.alias;
        });
    }

    int DistrictToAliasIndex(const GW::Constants::District d)
    {
        for (size_t i = 0; i < alias_district_ids.size(); i++) {
            if (alias_district_ids[i] == d)
                return static_cast<int>(i);
        }
        return 0;
    }

    struct UIErrorMessage {
        int error_index;
        wchar_t* message;
    };

    MapStruct pending_map_travel;

    constexpr auto messages_to_hook = {
        GW::UI::UIMessage::kErrorMessage,
        GW::UI::UIMessage::kMapChange,
        GW::UI::UIMessage::kTravel
    };
    GW::HookEntry OnUIMessage_HookEntry;


    const std::vector<TravelWindow::UserDestEntry> default_user_destinations = {
        {GW::Constants::MapID::Temple_of_the_Ages},
        {GW::Constants::MapID::Domain_of_Anguish},
        {GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost},
        {GW::Constants::MapID::Embark_Beach},
        {GW::Constants::MapID::Vloxs_Falls},
        {GW::Constants::MapID::Gadds_Encampment_outpost},
        {GW::Constants::MapID::Urgozs_Warren},
        {GW::Constants::MapID::The_Deep},
    };

    // ==== 用户定义的旅行目的地（在主窗口中显示为两列按钮） ====
    std::vector<TravelWindow::UserDestEntry> user_destinations{};

    void PopulateDefaultDestinations()
    {
        user_destinations = default_user_destinations;
    }

    // ==== 滚动到前哨站 ====
    GW::Constants::MapID scroll_to_outpost_id = GW::Constants::MapID::None;   // 我们想要到达哪个前哨站？
    GW::Constants::MapID scroll_from_outpost_id = GW::Constants::MapID::None; // 我们从哪个前哨站出发？

    bool map_travel_countdown_started = false;

    bool to_minimize = false;

    IDirect3DTexture9** scroll_texture = nullptr;

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
                if (!(settings.retry_map_travel && pending_map_travel.map_id != GW::Constants::MapID::None)) {
                    break;
                }
                const auto msg = static_cast<UIErrorMessage*>(wparam);
                if (msg && msg->message && *msg->message == 0xb25) {
                    // 旅行失败，但我们想重试
                    // 注意：0xb25 = "该区域已满。请选择其他区域。"
                    status->blocked = true;
                    SendUIMessage(GW::UI::UIMessage::kTravel, &pending_map_travel);
                }
            }
            break;
        }
    }

    // ==== 辅助函数 ====
    GW::Constants::MapID IndexToOutpostID(const int index)
    {
        if (visible_searchable_areas && static_cast<size_t>(index) < visible_searchable_areas->size()) {
            return (*visible_searchable_areas)[index]->map_id;
        }
        return GW::Constants::MapID::Great_Temple_of_Balthazar_outpost;
    }

    int OutpostIDToIndex(GW::Constants::MapID map_id)
    {
        if (!visible_searchable_areas) return -1;
        for (size_t i = 0, size = visible_searchable_areas->size(); i < size; i++) {
            if ((*visible_searchable_areas)[i]->map_id == map_id)
                return i;
        }
        return -1;
    }

    bool ParseOutpost(const std::wstring& s, GW::Constants::MapID& outpost, GW::Constants::District& district, const uint32_t&)
    {
        // 按地图 ID，例如 "/tp 77" 到 House zu Heltzer
        uint32_t map_id = 0;
        if (TextUtils::ParseUInt(s.c_str(), &map_id)) {
            return outpost = static_cast<GW::Constants::MapID>(map_id), true;
        }

        // 按完整前哨站名称（无标点），例如 "/tp GrEaT TemplE oF BalthaZAR"
        std::string compare = SanitiseForSearch(s);

        // 快捷词，例如 "/tp doa" 到 Domain of Anguish
        const std::string first_word = compare.substr(0, compare.find(' '));
        for (const auto& entry : user_aliases) {
            if (first_word == entry.alias) {
                outpost = entry.map_id;
                if (entry.district != GW::Constants::District::Current)
                    district = entry.district;
                return true;
            }
        }

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
                    continue; // 用户输入的字符串比此前哨站名称长
                }
                if (strncmp(map_names[i], compare, searchStringLength) != 0) {
                    continue; // 不匹配
                }
                if (thisMapLength == searchStringLength) {
                    return map_ids[i]; // 精确匹配，退出
                }
                if (!bestMatchMapName || strcmp(map_names[i], bestMatchMapName) < 0) {
                    bestMatchMapID = map_ids[i];
                    bestMatchMapName = map_names[i];
                }
            }
            return bestMatchMapID;
        };
        auto FindMatchingMapVec = [](const char* compare, std::vector<SearchableArea*>& maps) -> GW::Constants::MapID {
            const char* bestMatchMapName = nullptr;
            auto bestMatchMapID = GW::Constants::MapID::None;
            bool bestMatchIsOutpost = false;

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
                    continue; // 用户输入的字符串比此前哨站名称长
                }
                if (strncmp(map.Name(), compare, searchStringLength) != 0) {
                    continue; // 不匹配
                }
                if (thisMapLength == searchStringLength) {
                    return map.map_id; // 精确匹配，退出
                }
                const bool thisIsOutpost = IsValidOutpost(map.map_id);
                if (!bestMatchMapName || (thisIsOutpost && !bestMatchIsOutpost) ||
                    (thisIsOutpost == bestMatchIsOutpost && strcmp(map.Name(), bestMatchMapName) < 0)) {
                    bestMatchMapID = map.map_id;
                    bestMatchMapName = map.Name();
                    bestMatchIsOutpost = thisIsOutpost;
                }
            }
            return bestMatchMapID;
        };
        auto best_match_map_id = GW::Constants::MapID::None;
        if (fetched_searchable_areas == FetchedMapNames::Ready && visible_searchable_areas) {
            best_match_map_id = FindMatchingMapVec(compare.c_str(), *visible_searchable_areas);
        }

        if (best_match_map_id != GW::Constants::MapID::None) {
            return outpost = best_match_map_id, true; // 精确匹配
        }
        return false;
    }

    bool ParseDistrict(const std::wstring& s, GW::Constants::District& district, uint32_t& number)
    {
        std::string compare = TextUtils::ToLower(TextUtils::RemovePunctuation(TextUtils::WStringToString(s)));
        const std::string first_word = compare.substr(0, compare.find(' '));

        static constexpr ctll::fixed_string district_regex = "([a-z]{2,3})(\\d+)?";
        if (auto m = ctre::match<district_regex>(first_word)) {
            const auto& shorthand_outpost = shorthand_district_names.find(m.get<1>().to_string());
            if (shorthand_outpost == shorthand_district_names.end()) {
                return false;
            }
            district = shorthand_outpost->second.district;
            if (m.size() > 2 && !TextUtils::ParseUInt(m.get<2>().to_string().c_str(), &number)) {
                number = 0;
            }
            return true;
        }
        return false;
    }

    void CHAT_CMD_FUNC(CmdTP)
    {
        if (argc == 1) {
            Log::Error("[错误] 请提供参数");
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
        if (argOutpost == L"gh") {
            if (IsInGH()) {
                GW::GuildMgr::LeaveGH();
            }
            else {
                GW::GuildMgr::TravelGH();
            }
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
        if (argOutpost == L"outpost") {
            instance.TravelNearest(TravelWindow::GetNearestOutpostToPlayer());
            return;
        }
        if (argOutpost == L"nick" || argOutpost == L"nicholas") {
            const auto nick = DailyQuests::GetNicholasTheTraveller();
            instance.TravelNearest(nick.quest->map_id);
            return;
        }
        if (argOutpost == L"quest") {
            const auto quest = GW::QuestMgr::GetActiveQuest();
            if (quest && quest->map_to != GW::Constants::MapID::None) {
                instance.TravelNearest(quest->map_to);
            }
            return;
        }
        if (argOutpost == L"carto") {
            GW::Vec2f target_wm;
            if (!CartographerWidget::GetCurrentTargetWorldPos(target_wm)) {
                Log::Error("[Error] The cartographer helper has no current target");
                return;
            }
            instance.TravelNearest(WorldMapWidget::GetMapIdForLocation(target_wm));
            return;
        }
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
            Log::Error("[错误] 未识别收藏");
            return;
        }
        for (auto i = 2; i < argc - 1; i++) {
            // 前哨站名称可以是 "/tp" 之后的任何内容，但在区域之前，例如 "/tp house zu heltzer ae1"
            argOutpost.append(L" ");
            argOutpost.append(TextUtils::ToLower(argv[i]));
        }
        const bool isValidDistrict = ParseDistrict(argDistrict, district, district_number);
        if (isValidDistrict && argc == 2) {
            // 例如 "/tp ae1"
            instance.Travel(outpost, district, district_number); // 注意：ParseDistrict 通过引用设置 district 和 district_number 变量。
            return;
        }
        if (!isValidDistrict && argc > 2) {
            // 例如 "/tp house zu heltzer"
            argOutpost.append(L" ");
            argOutpost.append(argDistrict);
        }
        if (ParseOutpost(argOutpost, outpost, district, district_number)) {
            const wchar_t first_char_of_last_arg = *argv[argc - 1];
            switch (outpost) {
                case GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost:
                case GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost:
                    if (first_char_of_last_arg == 'l') // - 例如 /tp viz local
                    {
                        outpost = GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost;
                    }
                    else if (first_char_of_last_arg == 'f') {
                        outpost = GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost;
                    }
                    break;
                case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
                case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
                    if (first_char_of_last_arg == 'l') // - 例如 /tp fa lux
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
                    if (first_char_of_last_arg == 'l') // - 例如 /tp jq lux
                    {
                        outpost = GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost;
                    }
                    else if (first_char_of_last_arg == 'k') {
                        outpost = GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost;
                    }
                    else {
                        outpost = IsLuxon() ? GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost : GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost;
                    }
                    break;
                default:
                    break;
            }
            instance.Travel(outpost, district, district_number); // 注意：ParseOutpost 通过引用设置 outpost、district 和 district_number 变量。
            return;
        }
        Log::Error("[错误] 未识别前哨站 '%ls'", argOutpost.c_str());
    }

    const char* GetMapName(GW::Constants::MapID map_id)
    {
        if (map_id < GW::Constants::MapID::None || map_id > GW::Constants::MapID::Count)
            return "...";
        return Resources::GetMapName(map_id)->string().c_str();
    }

    bool outpost_name_array_getter(void* /* _data */, int idx, const char** out_text)
    {
        if (!visible_searchable_areas || idx < 0 || static_cast<size_t>(idx) >= visible_searchable_areas->size()) {
            return false;
        }

        *out_text = GetMapName((*visible_searchable_areas)[idx]->map_id);
        return true;
    }

    void BuildSearchableAreas(std::vector<SearchableArea*>& vec, const std::function<bool(GW::Constants::MapID, const GW::AreaInfo*)>& cmp)
    {
        for (const auto ptr : vec) {
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
            // 既然这个已解码，检查可搜索区域中是否已有同名项，例如节日前哨站
            if (searchable_area_indeces_by_name.contains(it->Name())) {
                auto* existing = searchable_area_indeces_by_name[it->Name()];
                // 优先选择前哨站而非非前哨站（例如 The Eternal Grove 前哨站 vs 探索区域）
                if (IsValidOutpost(it->map_id) && !IsValidOutpost(existing->map_id)) {
                    auto existing_pos = std::ranges::find(vec, existing);
                    vec.erase(existing_pos);
                    delete existing;
                } else {
                    vec.erase(vec.begin() + i);
                    delete it;
                }
                return CheckSearchableAreasDecoded(vec);
            }
            searchable_area_indeces_by_name[it->Name()] = it;
        }
        std::ranges::sort(vec, [](const SearchableArea* lhs, const SearchableArea* rhs) {
            return strcmp(GetMapName(lhs->map_id), GetMapName(rhs->map_id)) < 0;
        });
        return true;
    }
}

void TravelWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
    scroll_texture = Resources::GetItemImage(L"Passage Scroll to the Deep");
    district = GW::Constants::District::Current;
    district_number = 0;

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"tp", &CmdTP);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"to", &CmdTP);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"travel", &CmdTP);

    for (const auto message_id : messages_to_hook) {
        RegisterUIMessageCallback(&OnUIMessage_HookEntry, message_id, OnUIMessage);
    }
}

void TravelWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (auto& s : pre_searchable_areas) delete s;
    pre_searchable_areas.clear();
    for (auto& s : post_searchable_areas) delete s;
    post_searchable_areas.clear();
    visible_searchable_areas = nullptr;
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void TravelWindow::TravelButton(const GW::Constants::MapID mapid, const int x_idx, const GW::Constants::District dest_district, const uint32_t dest_district_number) const
{
    const auto text = GetMapName(mapid);
    if (!(text && *text))
        return;
    if (x_idx != 0) {
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    }
    const float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemInnerSpacing.x) / 2 - 2.f * ImGui::GetStyle().WindowPadding.x;
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
        Instance().Travel(mapid, dest_district, dest_district_number);
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
        if (GW::Map::IsPreSearing()) {
            TravelButton(GW::Constants::MapID::Ascalon_City_pre_searing, 0);
            TravelButton(GW::Constants::MapID::Ashford_Abbey_outpost, 1);
            TravelButton(GW::Constants::MapID::Foibles_Fair_outpost, 0);
            TravelButton(GW::Constants::MapID::Fort_Ranik_pre_Searing_outpost, 1);
            TravelButton(GW::Constants::MapID::The_Barradin_Estate_outpost, 0);
            TravelButton(GW::Constants::MapID::Piken_Square_pre_Searing_outpost, 1);
        }
        else {
            ImGui::PushItemWidth(-1.0f);
            static int travelto_index = -1;
            if (ImGui::MyCombo("###travelto", "旅行到...", &travelto_index, outpost_name_array_getter, nullptr, visible_searchable_areas ? visible_searchable_areas->size() : 0)) {
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
                        // 美服 1
                        district_number = 1;
                    }
                }
            }
            ImGui::PopItemWidth();

            size_t render_idx = 0;
            for (const auto& dest : user_destinations) {
                if (dest.map_id == GW::Constants::MapID::None)
                    continue;
                const auto effective_district = dest.district != GW::Constants::District::Current ? dest.district : district;
                const auto effective_district_number = dest.district != GW::Constants::District::Current ? dest.district_number : district_number;
                TravelButton(dest.map_id, static_cast<int>(render_idx % 2), effective_district, effective_district_number);
                render_idx++;
            }
            if (settings.show_zaishen_buttons) {
                const float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemInnerSpacing.x) / 2 - 2.f * ImGui::GetStyle().WindowPadding.x;
                if (ImGui::Button("扎伊圣悬赏", {w, 0})) {
                    GW::Chat::SendChat('/', "tp zb");
                }
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("扎伊圣任务", {w, 0})) {
                    GW::Chat::SendChat('/', "tp zm");
                }
                if (ImGui::Button("扎伊圣征服", {w, 0})) {
                    GW::Chat::SendChat('/', "tp zv");
                }
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("扎伊圣战斗", {w, 0})) {
                    GW::Chat::SendChat('/', "tp zc");
                }
            }
        }
        if (pending_map_travel.map_id != GW::Constants::MapID::None && IsValidOutpost(pending_map_travel.map_id)) {
            const auto abort_str = std::format("停止重试前往 {}", GetMapName(pending_map_travel.map_id));
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
        ScrollToOutpost(scroll_to_outpost_id); // 我们正在滚动到前哨站的过程中
    }

    // 启动时构建两个独立的可搜索列表（前传和后传）
    switch (fetched_searchable_areas) {
        case FetchedMapNames::Pending: {
            BuildSearchableAreas(pre_searchable_areas, [](const GW::Constants::MapID map_id, const GW::AreaInfo* map) {
                if (!map || !map->name_id) return false;
                return GW::Map::IsPreSearing(map_id) && IsValidOutpost(map_id);
            });
            BuildSearchableAreas(post_searchable_areas, [](const GW::Constants::MapID map_id, const GW::AreaInfo* map) {
                if (!map || !map->name_id || GW::Map::IsPreSearing(map_id)) return false;
                if (IsValidOutpost(map_id)) return true;
                if (map->GetIsOnWorldMap() && map->type == GW::RegionType::ExplorableZone) return true;
                if (map->type == GW::RegionType::Dungeon && !MapAdjacency::GetNeighbors(map_id).empty()) return true;
                return false;
            });
            fetched_searchable_areas = FetchedMapNames::Decoding;
        }
        break;
        case FetchedMapNames::Decoding: {
            if (CheckSearchableAreasDecoded(pre_searchable_areas) && CheckSearchableAreasDecoded(post_searchable_areas)) {
                fetched_searchable_areas = FetchedMapNames::Ready;
            }
        }
        break;
    }

    // 当玩家在前传和后传之间切换时切换指针
    if (fetched_searchable_areas == FetchedMapNames::Ready) {
        auto* next = GW::Map::IsPreSearing() ? &pre_searchable_areas : &post_searchable_areas;
        if (visible_searchable_areas != next)
            visible_searchable_areas = next;
    }
}

bool GetMapLabelPos(const GW::AreaInfo* map, GW::Vec2f* out)
{
    if (!map) return false;
    *out = {static_cast<float>(map->x), static_cast<float>(map->y)};
    if (!out->x) {
        out->x = static_cast<float>(map->icon_start_x + (map->icon_end_x - map->icon_start_x) / 2);
        out->y = static_cast<float>(map->icon_start_y + (map->icon_end_y - map->icon_start_y) / 2);
    }
    if (!out->x) {
        out->x = static_cast<float>(map->icon_start_x_dupe + (map->icon_end_x_dupe - map->icon_start_x_dupe) / 2);
        out->y = static_cast<float>(map->icon_start_y_dupe + (map->icon_end_y_dupe - map->icon_start_y_dupe) / 2);
    }
    return out->x != 0.f;
}

GW::Constants::MapID TravelWindow::GetNearestOutpostToLocation(const GW::AreaInfo* origin, const GW::Vec2f& world_map_pos) {
    if (!origin) return GW::Constants::MapID::None;
    float nearest_distance = std::numeric_limits<float>::max();
    auto nearest_map_id = GW::Constants::MapID::None;
    GW::Vec2f map_pos;
    for (size_t i = 0; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        const auto& map_id = static_cast<GW::Constants::MapID>(i);
        if (!GW::Map::GetIsMapUnlocked(map_id)) continue;
        if (!(IsValidOutpost(map_id) && GW::Map::GetMapInfo(map_id)->GetIsOnWorldMap())) continue;
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!(map_info && map_info->continent == origin->continent && map_info->campaign == origin->campaign)) continue;
        // if ((map_info->flags & 0x5000000) != 0)
        //    continue; // 例如“错误”的 Augury Rock 是地图 119，没有 NPC
        if (!GetMapLabelPos(map_info, &map_pos)) continue;
        const float dist = GetDistance(world_map_pos, map_pos);
        if (dist < nearest_distance) {
            nearest_distance = dist;
            nearest_map_id = static_cast<GW::Constants::MapID>(i);
        }
    }
    return nearest_map_id;
}

GW::Constants::MapID TravelWindow::GetNearestOutpostToPlayer()
{
    const auto my_pos = GW::PlayerMgr::GetPlayerPosition();
    if (!my_pos) return GW::Constants::MapID::None;
    GW::Vec2f world_map_pos;
    if (!WorldMapWidget::GamePosToWorldMap(*my_pos, world_map_pos)) return GW::Constants::MapID::None;
    return GetNearestOutpostToLocation(GW::Map::GetMapInfo(), world_map_pos);
}


GW::Constants::MapID TravelWindow::GetNearestOutpost(const GW::Constants::MapID map_to)
{
    // 如果 map_to 本身是有效且已解锁的前哨站，直接返回
    if (IsValidOutpost(map_to) && GW::Map::GetIsMapUnlocked(map_to))
        return map_to;

    using MapID = GW::Constants::MapID;
    std::vector<MapID> queue;
    std::vector<uint32_t> depth(static_cast<size_t>(MapID::Count), UINT32_MAX);

    queue.push_back(map_to);
    depth[static_cast<size_t>(map_to)] = 0;

    // 获取目标的世界地图位置用于平局判定
    const GW::AreaInfo* origin_info = GW::Map::GetMapInfo(map_to);
    GW::Vec2f origin_pos{};
    const bool has_origin_pos = origin_info && GetMapLabelPos(origin_info, &origin_pos);

    MapID best = MapID::None;
    uint32_t best_depth = UINT32_MAX;
    uint32_t best_party_size = 0;
    float best_distance = std::numeric_limits<float>::max();

    for (size_t head = 0; head < queue.size(); head++) {
        const auto current = queue[head];
        const auto current_depth = depth[static_cast<size_t>(current)];

        // 一旦超过已找到的最佳前哨站深度，停止探索
        if (best != MapID::None && current_depth > best_depth)
            break;

        if (current != map_to && IsValidOutpost(current) && GW::Map::GetIsMapUnlocked(current)) {
            const auto* cur_info = GW::Map::GetMapInfo(current);
            const uint32_t party_size = cur_info ? cur_info->max_party_size : 0;

            // 平局判定：优先选择队伍容量更大的，然后是欧几里得距离（同大陆），
            // 如果没有世界地图位置则按邻接数组顺序
            float dist = std::numeric_limits<float>::max();
            if (has_origin_pos && cur_info && cur_info->continent == origin_info->continent) {
                GW::Vec2f outpost_pos;
                if (GetMapLabelPos(cur_info, &outpost_pos))
                    dist = GetDistance(origin_pos, outpost_pos);
            }

            const bool is_better = best == MapID::None
                || (current_depth == best_depth && party_size > best_party_size)
                || (current_depth == best_depth && party_size == best_party_size && dist < best_distance);

            if (is_better) {
                best = current;
                best_depth = current_depth;
                best_party_size = party_size;
                best_distance = dist;
            }
        }

        for (const auto neighbor : MapAdjacency::GetNeighbors(current)) {
            const auto idx = static_cast<size_t>(neighbor);
            if (idx < depth.size() && depth[idx] == UINT32_MAX) {
                depth[idx] = current_depth + 1;
                queue.push_back(neighbor);
            }
        }
    }

    if (best != MapID::None)
        return best;

    // 如果通过邻接图无法到达，回退到世界地图上的欧几里得距离
    const GW::AreaInfo* this_map = GW::Map::GetMapInfo(map_to);
    if (!this_map) return MapID::None;

    GW::Vec2f world_map_location;
    if (!GetMapLabelPos(this_map, &world_map_location))
        return MapID::None;
    return GetNearestOutpostToLocation(this_map, world_map_location);
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
        return; // 地图加载中，因此我们不再等待旅行计时开始或结束。
    }
    if (IsWaitingForMapTravel()) {
        map_travel_countdown_started = true;
        return; // 当前在旅行倒计时中。等待倒计时完成或被取消。
    }
    if (map_travel_countdown_started) {
        pending_map_travel.map_id = GW::Constants::MapID::None;
        map_travel_countdown_started = false;
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return; // 我们正在等待倒计时，但它被取消了。
    }
    if (pending_map_travel.map_id != GW::Constants::MapID::None) {
        return; // 检查得太快；仍在等待地图旅行或其倒计时。
    }

    const GW::Constants::MapID map_id = GW::Map::GetMapID();
    if (scroll_to_outpost_id == GW::Constants::MapID::None) {
        scroll_to_outpost_id = outpost_id;
        scroll_from_outpost_id = map_id;
    }
    if (scroll_to_outpost_id != outpost_id) {
        return; // 已在前往另一个前哨站
    }
    if (map_id == outpost_id) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        if (!IsAlreadyInOutpost(outpost_id, _district, _district_number)) {
            GW::Map::Travel(outpost_id, _district, _district_number);
        }
        return; // 已在此前哨站。调用 GW::Map::Travel 以防区域不同。
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
            Log::Error("滚动目标前哨站无效");
            return;
    }
    if (!is_ready_to_scroll && scroll_from_outpost_id != map_id) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return; // 不在可滚动的前哨站中，但也不在起始前哨站 — 用户已决定前往其他地方。
    }

    const GW::Item* scroll_to_use = GW::Items::GetItemByModelId(
        scroll_model_id,
        static_cast<int>(GW::Constants::Bag::Backpack),
        static_cast<int>(GW::Constants::Bag::Storage_14));
    if (!scroll_to_use) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        Log::Error("背包中未找到旅行卷轴");
        return; // 未找到卷轴。
    }
    if (is_ready_to_scroll) {
        scroll_to_outpost_id = GW::Constants::MapID::None;
        GW::Items::UseItem(scroll_to_use);
        return; // 完成。
    }
    // 前往启程海滩。
    if (!Travel(GW::Constants::MapID::Embark_Beach, _district, _district_number)) {
        // 移动到滚动前哨站失败
        scroll_to_outpost_id = GW::Constants::MapID::None;
        return;
    }
}

bool TravelWindow::TravelNearest(const GW::Constants::MapID map_id)
{
    const auto outpost = GetNearestOutpost(map_id);
    if (outpost == GW::Constants::MapID::None) {
        Log::ErrorW(L"[错误] 未能找到 %s 附近的已解锁前哨站", Resources::GetMapName(map_id)->wstring().c_str());
        return false;
    }
    return Travel(outpost);
}

bool TravelWindow::Travel(const GW::Constants::MapID map_id, const GW::Constants::District _district /*= 0*/, const uint32_t _district_number)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || map_id == GW::Constants::MapID::None) {
        return false;
    }
    // 将非前哨站地图（探索区域、地城）解析为通过邻接图可到达的最近前哨站
    if (!IsValidOutpost(map_id)) {
        const auto nearest = GetNearestOutpost(map_id);
        if (nearest == GW::Constants::MapID::None) return false;
        return Travel(nearest, _district, _district_number);
    }
    if (!GW::Map::GetIsMapUnlocked(map_id)) {
        const GW::AreaInfo* map = GW::Map::GetMapInfo(map_id);
        wchar_t map_name_buf[8];
        constexpr wchar_t err_message_buf[256] = L"[错误] 你的角色未解锁该地图";
        if (map && map->name_id && GW::UI::UInt32ToEncStr(map->name_id, map_name_buf, 8)) {
            Log::ErrorW(L"[错误] 你的角色未解锁 \x1\x2%s\x2\x108\x107", map_name_buf);
        }
        else {
            Log::ErrorW(err_message_buf);
        }
        return false;
    }
    if (IsAlreadyInOutpost(map_id, _district, _district_number)) {
        Log::Error("[错误] 你已在此前哨站中");
        return true;
    }

    if (settings.collapse_on_travel) {
        to_minimize = true;
    }

    if (settings.close_on_travel) {
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
}

bool TravelWindow::TravelFavorite(const unsigned int idx)
{
    if (idx >= user_destinations.size()) {
        return false;
    }
    const auto& dest = user_destinations[idx];
    const auto effective_district = dest.district != GW::Constants::District::Current ? dest.district : district;
    const auto effective_district_number = dest.district != GW::Constants::District::Current ? dest.district_number : district_number;
    Travel(dest.map_id, effective_district, effective_district_number);
    return true;
}

void TravelWindow::DrawSettingsInternal()
{
    ImGui::CheckboxWithHelp("旅行时关闭", &settings.close_on_travel, "点击旅行目的地时将关闭旅行窗口");
    ImGui::CheckboxWithHelp("旅行时折叠", &settings.collapse_on_travel, "点击旅行目的地时将折叠旅行窗口");
    ImGui::CheckboxWithHelp("区域满时自动重试", &settings.retry_map_travel, "使用 /tp stop 停止重试。");
    ImGui::CheckboxWithHelp("使用英文地图名称", &settings.search_in_english, "如果取消勾选，/tp 命令将根据当前语言使用本地化地图名称。");
    ImGui::CheckboxWithHelp("显示扎伊圣任务按钮", &settings.show_zaishen_buttons, "在旅行窗口中显示扎伊圣悬赏、任务、征服和战斗旅行按钮。");

    ImGui::Separator();
    ImGui::Text("用户旅行目的地");
    ImGui::ShowHelp("在旅行窗口中显示为半宽按钮的目的地。在此添加、移除或重新排序。使用重置按钮恢复内置默认值。");

    {
        const auto dest_btn_w = ImGui::FontScale() * 30.f;
        const auto dest_spacing = ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::BeginTable("##destinations", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("地图", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("区域", ImGuiTableColumnFlags_WidthFixed, ImGui::FontScale() * 100.f);
            ImGui::TableSetupColumn("区域编号", ImGuiTableColumnFlags_WidthFixed, ImGui::FontScale() * 45.f);
            ImGui::TableSetupColumn("##删除", ImGuiTableColumnFlags_WidthFixed, dest_btn_w);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < user_destinations.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                auto& dest = user_destinations[i];

                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-1);
                auto map_idx = OutpostIDToIndex(dest.map_id);
                if (ImGui::MyCombo("##destmap", "选择地图...", &map_idx, outpost_name_array_getter, nullptr,
                    visible_searchable_areas ? static_cast<int>(visible_searchable_areas->size()) : 0)) {
                    dest.map_id = IndexToOutpostID(map_idx);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);
                auto dist_idx = DistrictToAliasIndex(dest.district);
                if (ImGui::Combo("##destdistrict", &dist_idx, alias_district_names.data(), static_cast<int>(alias_district_names.size()))) {
                    dest.district = alias_district_ids[dist_idx];
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-1);
                auto dist_num = static_cast<int>(dest.district_number);
                if (ImGui::InputInt("##destdistnum", &dist_num, 0)) {
                    dest.district_number = static_cast<uint8_t>(std::max(0, dist_num));
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::ButtonWithHint(ICON_FA_TRASH, "删除目的地", ImVec2(dest_btn_w, 0))) {
                    user_destinations.erase(user_destinations.begin() + i);
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("添加目的地", ImVec2(dest_btn_w * 3, 0))) {
            user_destinations.push_back({});
        }

        ImGui::SameLine(0, dest_spacing);

        static bool dest_reset_confirmed = false;
        if (ImGui::ConfirmButton("重置为默认", &dest_reset_confirmed,
            "重置旅行目的地？\n\n这将把所有目的地恢复为内置默认值。")) {
            PopulateDefaultDestinations();
            dest_reset_confirmed = false;
        }
    }

    ImGui::Separator();
    ImGui::Text("前哨站别名");
    ImGui::ShowHelp("用于 /tp 命令的自定义快捷别名。\n区域和区域编号为可选项。");

    const auto btn_w = ImGui::FontScale() * 30.f;
    const auto spacing = ImGui::GetStyle().ItemSpacing.x;

    bool aliases_changed = false;

    if (ImGui::BeginTable("##aliases", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("别名", ImGuiTableColumnFlags_WidthFixed, ImGui::FontScale() * 70.f);
        ImGui::TableSetupColumn("地图", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("区域", ImGuiTableColumnFlags_WidthFixed, ImGui::FontScale() * 100.f);
        ImGui::TableSetupColumn("区域编号", ImGuiTableColumnFlags_WidthFixed, ImGui::FontScale() * 45.f);
        ImGui::TableSetupColumn("##删除", ImGuiTableColumnFlags_WidthFixed, btn_w);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < user_aliases.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            auto& entry = user_aliases[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##alias", entry.alias, 32))
                aliases_changed = true;

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            auto map_idx = OutpostIDToIndex(entry.map_id);
            if (ImGui::MyCombo("##map", "选择地图...", &map_idx, outpost_name_array_getter, nullptr, visible_searchable_areas ? static_cast<int>(visible_searchable_areas->size()) : 0)) {
                entry.map_id = IndexToOutpostID(map_idx);
                aliases_changed = true;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            auto dist_idx = DistrictToAliasIndex(entry.district);
            if (ImGui::Combo("##district", &dist_idx, alias_district_names.data(), static_cast<int>(alias_district_names.size()))) {
                entry.district = alias_district_ids[dist_idx];
                aliases_changed = true;
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(-1);
            auto dist_num = static_cast<int>(entry.district_number);
            if (ImGui::InputInt("##distnum", &dist_num, 0)) {
                entry.district_number = static_cast<uint8_t>(std::max(0, dist_num));
                aliases_changed = true;
            }

            ImGui::TableSetColumnIndex(4);
            if (ImGui::ButtonWithHint(ICON_FA_TRASH, "删除别名", ImVec2(btn_w, 0))) {
                user_aliases.erase(user_aliases.begin() + i);
                aliases_changed = true;
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("添加别名", ImVec2(btn_w * 3, 0))) {
        user_aliases.push_back({});
    }

    ImGui::SameLine(0, spacing);

    static bool reset_confirmed = false;
    if (ImGui::ConfirmButton("重置为默认", &reset_confirmed,
        "重置前哨站别名？\n\n这将把所有别名恢复为内置默认值。\n自定义别名将丢失。")) {
        PopulateDefaultAliases();
        reset_confirmed = false;
    }

    (void)aliases_changed;
}

void TravelWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);

    user_destinations.clear();
    if (!doc.Get(Name(), "user_destinations", user_destinations)) {
        if (legacy && legacy->GetValue(Name(), "dest_count", nullptr)) {
            const auto dest_count = static_cast<size_t>(legacy->GetLongValue(Name(), "dest_count", 0));
            for (size_t i = 0; i < dest_count; i++) {
                char key[64];
                snprintf(key, _countof(key), "Dest%zu", i);
                const auto map_id = static_cast<GW::Constants::MapID>(legacy->GetLongValue(Name(), key, static_cast<int>(GW::Constants::MapID::None)));
                if (map_id < GW::Constants::MapID::Count && map_id > GW::Constants::MapID::None) {
                    UserDestEntry entry{};
                    entry.map_id = map_id;
                    snprintf(key, _countof(key), "Dest%zu_district", i);
                    entry.district = static_cast<GW::Constants::District>(legacy->GetLongValue(Name(), key, static_cast<int>(GW::Constants::District::Current)));
                    snprintf(key, _countof(key), "Dest%zu_district_num", i);
                    entry.district_number = static_cast<uint8_t>(legacy->GetLongValue(Name(), key, 0));
                    user_destinations.push_back(entry);
                }
            }
        }
        else {
            // 如果存在，从旧的 fav_ 键迁移
            const auto fav_count = legacy ? static_cast<size_t>(legacy->GetLongValue(Name(), "fav_count", 0)) : 0;
            for (size_t i = 0; i < fav_count; i++) {
                char key[32];
                snprintf(key, _countof(key), "Fav%d", i);
                const auto map_id = static_cast<GW::Constants::MapID>(legacy->GetLongValue(Name(), key, static_cast<int>(GW::Constants::MapID::None)));
                if (map_id < GW::Constants::MapID::Count && map_id > GW::Constants::MapID::None)
                    user_destinations.push_back({map_id});
            }
            // 如果仍为空，填充默认值（尊重旧的 show_default_destinations 设置）
            if (user_destinations.empty()) {
                const bool old_show_defaults = legacy ? legacy->GetBoolValue(Name(), "show_default_destinations", true) : true;
                if (old_show_defaults) {
                    PopulateDefaultDestinations();
                }
            }
        }
    }

    user_aliases.clear();
    if (!doc.Get(Name(), "user_aliases", user_aliases) && legacy) {
        const auto alias_count = static_cast<size_t>(legacy->GetLongValue(Name(), "alias_count", 0));
        for (size_t i = 0; i < alias_count; i++) {
            char key[64];
            AliasEntry entry{};
            snprintf(key, sizeof(key), "Alias%zu_key", i);
            const auto* alias_str = legacy->GetValue(Name(), key, nullptr);
            if (!alias_str || !*alias_str)
                continue;
            entry.alias = alias_str;
            snprintf(key, sizeof(key), "Alias%zu_map", i);
            entry.map_id = static_cast<GW::Constants::MapID>(legacy->GetLongValue(Name(), key, static_cast<int>(GW::Constants::MapID::None)));
            snprintf(key, sizeof(key), "Alias%zu_district", i);
            entry.district = static_cast<GW::Constants::District>(legacy->GetLongValue(Name(), key, static_cast<int>(GW::Constants::District::Current)));
            snprintf(key, sizeof(key), "Alias%zu_district_num", i);
            entry.district_number = static_cast<uint8_t>(legacy->GetLongValue(Name(), key, 0));
            user_aliases.push_back(entry);
        }
    }
    if (user_aliases.empty())
        PopulateDefaultAliases();
}

void TravelWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    doc.Set(Name(), "user_destinations", user_destinations);
    doc.Set(Name(), "user_aliases", user_aliases);
}
