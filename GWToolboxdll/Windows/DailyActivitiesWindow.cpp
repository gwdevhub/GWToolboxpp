#include "stdafx.h"

#include <Logger.h>

#include <Windows/DailyActivitiesWindow.h>

// Can put a background for double bounty weekend
// Can highlight interesting bosses
// Over with mouse should print reward of the quest (especially with bonus)
// Some combinaison to open the wiki
// Small view
// When something is highlighted, print it in the chat when tb is openned

static const uint32_t SECONDS_PER_DAY_U32 = 24 * 60 * 60;
static const double   SECONDS_PER_DAY_DOUBLE = 24.0 * 60 * 60;
static const char* zbounties[66] = {
    "Droajam, Mage of the Sands",
    "Royen Beastkeeper",
    "Eldritch Ettin",
    "Vengeful Aatxe",
    "Fronis Irontoe",
    "Urgoz",
    "Fenrir",
    "Selvetarm",
    "Mohby Windbeak",
    "Charged Blackness",
    "Rotscale",
    "Zoldark the Unholy",
    "Korshek the Immolated",
    "Myish, Lady of the Lake",
    "Frostmaw the Kinslayer",
    "Kunvie Firewing",
    "Z'him Monns",
    "The Greater Darkness",
    "TPS Regulator Golem",
    "Plague of Destruction",
    "The Darknesses",
    "Admiral Kantoh",
    "Borrguus Blisterbark",
    "Forgewight",
    "Baubao Wavewrath",
    "Joffs the Mitigator",
    "Rragar Maneater",
    "Chung, the Attuned",
    "Lord Jadoth",
    "Nulfastu, Earthbound",
    "The Iron Forgeman",
    "Magmus",
    "Mobrin, Lord of the Marsh",
    "Jarimiya the Unmerciful",
    "Duncan the Black",
    "Quansong Spiritspeak",
    "The Stygian Underlords",
    "Fozzy Yeoryios",
    "The Black Beast of Arrgh",
    "Arachni",
    "The Four Horsemen",
    "Remnant of Antiquities",
    "Arbor Earthcall",
    "Prismatic Ooze",
    "Lord Khobay",
    "Jedeh the Mighty",
    "Ssuns, Blessed of Dwayna",
    "Justiciar Thommis",
    "Harn and Maxine Coldstone",
    "Pywatt the Swift",
    "Fendi Nin",
    "Mungri Magicbox",
    "Priest of Menzies",
    "Ilsundur, Lord of Fire",
    "Kepkhet Marrowfeast",
    "Commander Wahli",
    "Kanaxai",
    "Khabuus",
    "Molotov Rocktail",
    "The Stygian Lords",
    "Dragon Lich",
    "Havok Soulwail",
    "Ghial the Bone Dancer",
    "Murakai, Lady of the Night",
    "Rand Stormweaver",
    "Verata",
};

static const char* zmissions[69] = {
    "Sunjiang District",
    "Elona Reach",
    "Gate of Pain",
    "Blood Washes Blood",
    "Bloodstone Fen",
    "Jennur's Horde",
    "Gyala Hatchery",
    "Abaddon's Gate",
    "The Frost Gate",
    "Augury Rock",
    "Grand Court of Sebelkeh",
    "Ice Caves of Sorrow",
    "Raisu Palace",
    "Gate of Desolation",
    "Thirsty River",
    "Blacktide Den",
    "Against the Charr",
    "Abaddon's Mouth",
    "Nundu Bay",
    "Divinity Coast",
    "Zen Daijun",
    "Pogahn Passage",
    "Tahnnakai Temple",
    "The Great Northern Wall",
    "Dasha Vestibule",
    "The Wilds",
    "Unwaking Waters",
    "Chahbek Village",
    "Aurora Glade",
    "A Time for Heroes",
    "Consulate Docks",
    "Ring of Fire",
    "Nahpui Quarter",
    "The Dragon's Lair",
    "Dzagonur Bastion",
    "D'Alessio Seaboard",
    "Assault on the Stronghold",
    "The Eternal Grove",
    "Sanctum Cay",
    "Rilohn Refuge",
    "Warband of Brothers",
    "Borlis Pass",
    "Imperial Sanctum",
    "Moddok Crevice",
    "Nolani Academy",
    "Destruction's Depths",
    "Venta Cemetery",
    "Fort Ranik",
    "A Gate Too Far",
    "Minister Cho's Estate",
    "Thunderhead Keep",
    "Tihark Orchard",
    "Finding the Bloodstone",
    "Dunes of Despair",
    "Vizunah Square",
    "Jokanur Diggings",
    "Iron Mines of Moladune",
    "Kodonur Crossroads",
    "G.O.L.E.M.",
    "Arborstone",
    "Gates of Kryta",
    "Gate of Madness",
    "The Elusive Golemancer",
    "Riverside Province",
    "Boreas Seabed",
    "Ruins of Morah",
    "Hell's Precipice",
    "Ruins of Surmia",
    "Curse of the Nornbear",
};

static const char* zcombats[28] = {
    "Codex Arena",
    "Fort Aspenwood",
    "Jade Quarry",
    "Random Arena",
    "Codex Arena",
    "Guild Versus Guild",
    "Jade Quarry",
    "Alliance Battles",
    "Heroes' Ascent",
    "Random Arena",
    "Fort Aspenwood",
    "Jade Quarry",
    "Random Arena",
    "Fort Aspenwood",
    "Heroes' Ascent",
    "Alliance Battles",
    "Guild Versus Guild",
    "Codex Arena",
    "Random Arena",
    "Fort Aspenwood",
    "Alliance Battles",
    "Jade Quarry",
    "Codex Arena",
    "Heroes' Ascent",
    "Guild Versus Guild",
    "Alliance Battles",
    "Heroes' Ascent",
    "Guild Versus Guild",
};

static const char* zvanquishes[136] = {
    "Norrhart Domains",
    "Pockmark Flats",
    "Tahnnakai Temple",
    "Vehjin Mines",
    "Poisoned Outcrops",
    "Prophet's Path",
    "The Eternal Grove",
    "Tasca's Demise",
    "Resplendent Makuun",
    "Reed Bog",
    "Unwaking Waters",
    "Stingray Strand",
    "Sunward Marches",
    "Regent Valley",
    "Wajjun Bazaar",
    "Yatendi Canyons",
    "Twin Serpent Lakes",
    "Sage Lands",
    "Xaquang Skyway",
    "Zehlon Reach",
    "Tangle Root",
    "Silverwood",
    "Zen Daijun",
    "The Arid Sea",
    "Nahpui Quarter",
    "Skyward Reach",
    "The Scar",
    "The Black Curtain",
    "Panjiang Peninsula",
    "Snake Dance",
    "Traveler's Vale",
    "The Breach",
    "Lahtenda Bog",
    "Spearhead Peak",
    "Mount Qinkai",
    "Marga Coast",
    "Melandru's Hope",
    "The Falls",
    "Joko's Domain",
    "Vulture Drifts",
    "Wilderness of Bahdza",
    "Talmark Wilderness",
    "Vehtendi Valley",
    "Talus Chute",
    "Mineral Springs",
    "Anvil Rock",
    "Arborstone",
    "Witman's Folly",
    "Arkjok Ward",
    "Ascalon Foothills",
    "Bahdok Caverns",
    "Cursed Lands",
    "Alcazia Tangle",
    "Archipelagos",
    "Eastern Frontier",
    "Dejarin Estate",
    "Watchtower Coast",
    "Arbor Bay",
    "Barbarous Shore",
    "Deldrimor Bowl",
    "Boreas Seabed",
    "Cliffs of Dohjok",
    "Diessa Lowlands",
    "Bukdek Byway",
    "Bjora Marches",
    "Crystal Overlook",
    "Diviner's Ascent",
    "Dalada Uplands",
    "Drazach Thicket",
    "Fahranur, the First City",
    "Dragon's Gullet",
    "Ferndale",
    "Forum Highlands",
    "Dreadnought's Drift",
    "Drakkar Lake",
    "Dry Top",
    "Tears of the Fallen",
    "Gyala Hatchery",
    "Ettin's Back",
    "Gandara, the Moon Fortress",
    "Grothmar Wardowns",
    "Flame Temple Corridor",
    "Haiju Lagoon",
    "Frozen Forest",
    "Garden of Seborhin",
    "Grenth's Footprint",
    "Jaya Bluffs",
    "Holdings of Chokhin",
    "Ice Cliff Chasms",
    "Griffon's Mouth",
    "Kinya Province",
    "Issnur Isles",
    "Jaga Moraine",
    "Ice Floe",
    "Maishang Hills",
    "Jahai Bluffs",
    "Riven Earth",
    "Icedome",
    "Minister Cho's Estate",
    "Mehtani Keys",
    "Sacnoth Valley",
    "Iron Horse Mine",
    "Morostav Trail",
    "Plains of Jarin",
    "Sparkfly Swamp",
    "Kessex Peak",
    "Mourning Veil Falls",
    "The Alkali Pan",
    "Varajar Fells",
    "Lornar's Pass",
    "Pongmei Valley",
    "The Floodplain of Mahnkelon",
    "Verdant Cascades",
    "Majesty's Rest",
    "Raisu Palace",
    "The Hidden City of Ahdashim",
    "Rhea's Crater",
    "Mamnoon Lagoon",
    "Shadow's Passage",
    "The Mirror of Lyss",
    "Saoshang Trail",
    "Nebo Terrace",
    "Shenzun Tunnels",
    "The Ruptured Heart",
    "Salt Flats",
    "North Kryta Province",
    "Silent Surf",
    "The Shattered Ravines",
    "Scoundrel's Rise",
    "Old Ascalon",
    "Sunjiang District",
    "The Sulfurous Wastes",
    "Magus Stones",
    "Perdition Rock",
    "Sunqua Vale",
    "Turai's Procession",
};

void DailyActivitiesWindow::Initialize() {
    ToolboxWindow::Initialize();

    cycle_begin = 1244736000ull; // i.e. 06/11/2009 @ 4:00pm (UTC)
}

void DailyActivitiesWindow::DrawLine(
    const char* date,
    const char* mission,
    const char* bounty,
    const char* combat,
    const char* vanquish,
    const char* wanted)
{
    uint32_t space_index = 1;
    static float space_x[] = {0.f, 150.f, 350.f, 550.f, 750.f, 950.f};

    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 size_tmp = ImVec2(win_pos.x + 150, win_pos.y + 50);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::Text("%s", date);
    if (show_mission) {
        draw_list->AddRectFilled(win_pos, size_tmp, IM_COL32(0, 255, 0, 255));
        ImGui::SameLine(space_x[space_index++]);
        ImGui::Text("%s", mission);
    }
    if (show_bounty) {
        ImGui::SameLine(space_x[space_index++]);
        ImGui::Text("%s", bounty);
    }
    if (show_combat) {
        ImGui::SameLine(space_x[space_index++]);
        ImGui::Text("%s", combat);
    }
    if (show_vanquish) {
        ImGui::SameLine(space_x[space_index++]);
        ImGui::Text("%s", vanquish);
    }
    if (show_wanted) {
        ImGui::SameLine(space_x[space_index++]);
        ImGui::Text("%s", wanted);
    }
}

void DailyActivitiesWindow::Draw(IDirect3DDevice9*) {
    if (!visible) return;

    ImColor sCol(102, 187, 238, 255);

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        //ImGui::Checkbox("Highlight SC bounties", &highlight_sc_bounties);
        DrawLine(
            "Date",
            "Zaishen Mission",
            "Zaishen Bounty",
            "Zaishen Combat",
            "Zaishen Vanquish",
            "Shining Blade");
        ImGui::Separator();

        time_t current_time = time(nullptr);
        double diffsec = difftime(current_time, cycle_begin);
        uint32_t day_offset = static_cast<uint32_t>(diffsec / SECONDS_PER_DAY_DOUBLE);

        char date_buffer[32];
        const uint32_t day_before_today = 3;
        time_t current_day_time = current_time - (day_before_today * SECONDS_PER_DAY_U32);
        for (uint32_t draw_day = day_offset - day_before_today; draw_day < day_offset + 50; draw_day++) {
            struct tm* time_utc = gmtime(&current_day_time);
            strftime(date_buffer, 32, "%e %B %G", time_utc);
            DrawLine(
                date_buffer,
                zmissions[draw_day % _countof(zmissions)],
                zbounties[draw_day % _countof(zbounties)],
                zcombats[draw_day % _countof(zcombats)],
                zvanquishes[draw_day % _countof(zvanquishes)],
                "Shining Blade");
            current_day_time += SECONDS_PER_DAY_U32;
        }


        ImGui::TextDisabled("Click on a daily quest to get notified when its coming up. Subscribed quests are highlighted in ");
        ImGui::SameLine();
        ImGui::TextColored(sCol, "blue");
    }
    ImGui::End();
}

void DailyActivitiesWindow::DrawSettingInternal() {
    ImGui::Checkbox("Show Mission", &show_mission);
    ImGui::Checkbox("Show Bounty", &show_bounty);
    ImGui::Checkbox("Show Combat", &show_combat);
    ImGui::Checkbox("Show Vanquish", &show_vanquish);
    ImGui::Checkbox("Show Wanted", &show_wanted);
}

void DailyActivitiesWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);

}

void DailyActivitiesWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
}
