#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Widgets/AlcoholWidget.h>
#include <Windows/HotkeysWindow.h>
#include <Windows/MainWindow.h>
#include <Windows/PconsWindow.h>

using namespace GW::Constants;

bool Pcon::map_has_effects_array = false;

PconsWindow::PconsWindow() {
    const float s = 64.0f; // all icons are 64x64

    pcons.push_back(new PconCons("Essence of Celerity", "Essence", "essence", L"Essence of Celerity", IDB_Pcons_Essence,
        ImVec2(5 / s, 10 / s), ImVec2(46 / s, 51 / s),
        ItemID::ConsEssence, SkillID::Essence_of_Celerity_item_effect, 5));

    pcons.push_back(new PconCons("Grail of Might", "Grail", "grail", L"Grail of Might", IDB_Pcons_Grail,
        ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s),
        ItemID::ConsGrail, SkillID::Grail_of_Might_item_effect, 5));

    pcons.push_back(new PconCons("Armor of Salvation", "Armor", "armor", L"Armor of Salvation", IDB_Pcons_Armor,
        ImVec2(0 / s, 2 / s), ImVec2(56 / s, 58 / s),
        ItemID::ConsArmor, SkillID::Armor_of_Salvation_item_effect, 5));

    pcons.push_back(new PconGeneric("Red Rock Candy", "Red Rock", "redrock", L"Red Rock Candy", IDB_Pcons_RedRock,
        ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
        ItemID::RRC, SkillID::Red_Rock_Candy_Rush, 5));

    pcons.push_back(new PconGeneric("Blue Rock Candy", "Blue Rock", "bluerock", L"Blue Rock Candy", IDB_Pcons_BlueRock,
        ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
        ItemID::BRC, SkillID::Blue_Rock_Candy_Rush, 10));

    pcons.push_back(new PconGeneric("Green Rock Candy", "Green Rock", "greenrock", L"Green Rock Candy", IDB_Pcons_GreenRock,
        ImVec2(0 / s, 4 / s), ImVec2(52 / s, 56 / s),
        ItemID::GRC, SkillID::Green_Rock_Candy_Rush, 15));

    pcons.push_back(new PconGeneric("Golden Egg", "Egg", "egg", L"Golden Egg", IDB_Pcons_Egg,
        ImVec2(1 / s, 8 / s), ImVec2(48 / s, 55 / s),
        ItemID::Eggs, SkillID::Golden_Egg_skill, 20));

    pcons.push_back(new PconGeneric("Candy Apple", "Apple", "apple", L"Candy Apple", IDB_Pcons_Apple,
        ImVec2(0 / s, 7 / s), ImVec2(50 / s, 57 / s),
        ItemID::Apples, SkillID::Candy_Apple_skill, 10));

    pcons.push_back(new PconGeneric("Candy Corn", "Corn", "corn", L"Candy Corn", IDB_Pcons_Corn,
        ImVec2(5 / s, 10 / s), ImVec2(48 / s, 53 / s),
        ItemID::Corns, SkillID::Candy_Corn_skill, 10));

    pcons.push_back(new PconGeneric("Birthday Cupcake", "Cupcake", "cupcake", L"Birthday Cupcake", IDB_Pcons_Cupcake,
        ImVec2(1 / s, 5 / s), ImVec2(51 / s, 55 / s),
        ItemID::Cupcakes, SkillID::Birthday_Cupcake_skill, 10));

    pcons.push_back(new PconGeneric("Slice of Pumpkin Pie", "Pie", "pie", L"Slice of Pumpkin Pie", IDB_Pcons_Pie,
        ImVec2(0 / s, 7 / s), ImVec2(52 / s, 59 / s),
        ItemID::Pies, SkillID::Pie_Induced_Ecstasy, 10));

    pcons.push_back(new PconGeneric("War Supplies", "War Supply", "warsupply", L"War Supplies", IDB_Pcons_WarSupplies,
        ImVec2(0 / s, 0 / s), ImVec2(63 / s, 63 / s),
        ItemID::Warsupplies, SkillID::Well_Supplied, 20));

    pcons.push_back(pcon_alcohol = new PconAlcohol("Alcohol", "Alcohol", "alcohol", L"Dwarven Ale", IDB_Pcons_Ale,
        ImVec2(-5 / s, 1 / s), ImVec2(57 / s, 63 / s),
        10));

    pcons.push_back(new PconLunar("Lunar Fortunes", "Lunars", "lunars", L"Lunar Fortune", IDB_Pcons_Lunar,
        ImVec2(1 / s, 4 / s), ImVec2(56 / s, 59 / s),
        10));

    pcons.push_back(new PconCity("City speedboost", "City IMS", "city", L"Sugary Blue Drink", IDB_Pcons_BlueDrink,
        ImVec2(0 / s, 1 / s), ImVec2(61 / s, 62 / s),
        20));

    pcons.push_back(new PconGeneric("Drake Kabob", "Kabob", "kabob", L"Drake Kabob", IDB_Pcons_Kabob,
        ImVec2(0 / s, 0 / s), ImVec2(64 / s, 64 / s),
        ItemID::Kabobs, SkillID::Drake_Skin, 10));

    pcons.push_back(new PconGeneric("Bowl of Skalefin Soup", "Soup", "soup", L"Bowl of Skalefin Soup", IDB_Pcons_Soup,
        ImVec2(2 / s, 5 / s), ImVec2(51 / s, 54 / s),
        ItemID::SkalefinSoup, SkillID::Skale_Vigor, 10));

    pcons.push_back(new PconGeneric("Pahnai Salad", "Salad", "salad", L"Pahnai Salad", IDB_Pcons_Salad,
        ImVec2(0 / s, 5 / s), ImVec2(49 / s, 54 / s),
        ItemID::PahnaiSalad, SkillID::Pahnai_Salad_item_effect, 10));

    pcons.push_back(new PconGeneric(L"Scroll of Hunter's Insight", 5976, SkillID::Hunters_Insight, 20));
    pcons.back()->visible = false; // sets the default, LoadSettings will overwrite this

    // Refillers
    pcons.push_back(new PconRefiller("Scroll of Resurrection", "Scroll", "resscroll", L"Scroll of Resurrection", IDB_Mat_ResScroll,
        ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s), ItemID::ResScrolls, 5));
    pcons.push_back(new PconRefiller("Powerstone of Courage", "Pstone", "pstone", L"Powerstone of Courage", IDB_Mat_Powerstone,
        ImVec2(5 / s, 12 / s), ImVec2(49 / s, 56 / s), ItemID::Powerstone, 5));

    // Summoning Stones
    // @Cleanup: Add these items to GWCA or something
    pcons.push_back(new PconRefiller(L"Tengu Support Flare",30209));
    pcons.push_back(new PconRefiller(L"Imperial Guard Reinforcement Order", 30210));
    pcons.push_back(new PconRefiller(L"Chitinous Summoning Stone", 30959));
    pcons.push_back(new PconRefiller(L"Amber Summoning Stone", 30961));
    pcons.push_back(new PconRefiller(L"Arctic Summoning Stone", 30962));
    //pcons.push_back(new PconRefiller(L"Automaton Summoning Stone", ????));
    pcons.push_back(new PconRefiller(L"Celestial Summoning Stone", 34176));
    pcons.push_back(new PconRefiller(L"Mystical Summoning Stone", 30960));
    pcons.push_back(new PconRefiller(L"Demonic Summoning Stone", 30963));
    pcons.push_back(new PconRefiller(L"Gelatinous Summoning Stone", 30964));
    pcons.push_back(new PconRefiller(L"Fossilized Summoning Stone", 30965));
    pcons.push_back(new PconRefiller(L"Jadeite Summoning Stone", 30966));
    pcons.push_back(new PconRefiller(L"Mischievous Summoning Stone", 31022));
    pcons.push_back(new PconRefiller(L"Frosty Summoning Stone", 31023));
    pcons.push_back(new PconRefiller(L"Mercantile Summoning Stone", 31154));
    pcons.push_back(new PconRefiller(L"Mysterious Summoning Stone", 31155));
    pcons.push_back(new PconRefiller(L"Zaishen Summoning Stone", 31156));
    pcons.push_back(new PconRefiller(L"Ghastly Summoning Stone", 32557));
    pcons.push_back(new PconRefiller(L"Shining Blade Warhorn", 35126));
}
void PconsWindow::Initialize() {
    ToolboxWindow::Initialize();
    AlcoholWidget::Instance().Initialize(); // Pcons depend on alcohol widget to track current drunk level.
    Resources::Instance().LoadTexture(&button_texture, Resources::GetPath(L"img/icons", L"cupcake.png"), IDB_Icon_Cupcake);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentSetPlayer>(&AgentSetPlayer_Entry,
    [](GW::HookStatus *, GW::Packet::StoC::AgentSetPlayer *pak) -> void {
        Pcon::player_id = pak->unk1;
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AddExternalBond>(&AddExternalBond_Entry,&OnAddExternalBond);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PostProcess>(&PostProcess_Entry,&OnPostProcessEffect);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&GenericValue_Entry,&OnGenericValue);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&AgentState_Entry,&OnAgentState);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry,&OnSpeechBubble);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, &OnObjectiveDone);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::VanquishComplete>(&VanquishComplete_Entry, &OnVanquishComplete);
    GW::Chat::CreateCommand(L"pcons", &CmdPcons);
}
void PconsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    for (Pcon *pcon : pcons) {
        pcon->Terminate();
    }
}
void PconsWindow::OnAddExternalBond(GW::HookStatus *status, GW::Packet::StoC::AddExternalBond *pak)
{
    if (PconAlcohol::suppress_lunar_skills &&
        pak->caster_id == GW::Agents::GetPlayerId() && pak->receiver_id == 0 &&
        (pak->skill_id == (DWORD)GW::Constants::SkillID::Spiritual_Possession ||
         pak->skill_id == (DWORD)GW::Constants::SkillID::Lucky_Aura)) {
        // printf("blocked skill %d\n", pak->skill_id);
        status->blocked = true;
    }
}
void PconsWindow::OnPostProcessEffect(GW::HookStatus *status,
                                      GW::Packet::StoC::PostProcess *pak)
{
    PconAlcohol::alcohol_level = pak->level;
    PconsWindow &instance = Instance();
    //printf("Level = %d, tint = %d\n", pak->level, pak->tint);
    if (instance.enabled)
        instance.pcon_alcohol->Update();
    status->blocked = PconAlcohol::suppress_drunk_effect;
}
void PconsWindow::OnGenericValue(GW::HookStatus *, GW::Packet::StoC::GenericValue *pak) {
    if (PconAlcohol::suppress_drunk_emotes
        && pak->agent_id == GW::Agents::GetPlayerId()
        && pak->value_id == 22) {

        if (pak->value == 0x33E807E5) pak->value = 0; // kneel
        if (pak->value == 0x313AC9D1) pak->value = 0; // bored
        if (pak->value == 0x3033596A) pak->value = 0; // moan
        if (pak->value == 0x305A7EF2) pak->value = 0; // flex
        if (pak->value == 0x74999B06) pak->value = 0; // fistshake
        if (pak->value == 0x30446E61) pak->value = 0; // roar
    }
}
void PconsWindow::OnAgentState(GW::HookStatus *, GW::Packet::StoC::AgentState *pak)
{
    if (PconAlcohol::suppress_drunk_emotes &&
        pak->agent_id == GW::Agents::GetPlayerId() && pak->state & 0x2000) {
        pak->state ^= 0x2000;
    }
}
void PconsWindow::OnSpeechBubble(GW::HookStatus *status, GW::Packet::StoC::SpeechBubble *pak) {
    if (!PconAlcohol::suppress_drunk_text) return;
    bool blocked = status->blocked;
    status->blocked = true;

    wchar_t* m = pak->message;
    if (m[0] == 0x8CA && m[1] == 0xA4F7 && m[2] == 0xF552 && m[3] == 0xA32) return; // i love you man!
    if (m[0] == 0x8CB && m[1] == 0xE20B && m[2] == 0x9835 && m[3] == 0x4C75) return; // I'm the king of the world!
    if (m[0] == 0x8CC && m[1] == 0xFA4D && m[2] == 0xF068 && m[3] == 0x393) return; // I think I need to sit down
    if (m[0] == 0x8CD && m[1] == 0xF2C2 && m[2] == 0xBBAD && m[3] == 0x1EAD) return; // I think I'm gonna be sick
    if (m[0] == 0x8CE && m[1] == 0x85E5 && m[2] == 0xF726 && m[3] == 0x68B1) return; // Oh no, not again
    if (m[0] == 0x8CF && m[1] == 0xEDD3 && m[2] == 0xF2B9 && m[3] == 0x3F34) return; // It's spinning...
    if (m[0] == 0x8D0 && m[1] == 0xF056 && m[2] == 0xE7AD && m[3] == 0x7EE6) return; // Everyone stop shouting!
    if (m[0] == 0x8101 && m[1] == 0x6671 && m[2] == 0xCBF8 && m[3] == 0xE717) return; // "BE GONE!"
    if (m[0] == 0x8101 && m[1] == 0x6672 && m[2] == 0xB0D6 && m[3] == 0xCE2F) return; // "Soon you will all be crushed."
    if (m[0] == 0x8101 && m[1] == 0x6673 && m[2] == 0xDAA5 && m[3] == 0xD0A1) return; // "You are no match for my almighty power."
    if (m[0] == 0x8101 && m[1] == 0x6674 && m[2] == 0x8BF9 && m[3] == 0x8C19) return; // "Such fools to think you can attack me here. Come closer so you can see the face of your doom!"
    if (m[0] == 0x8101 && m[1] == 0x6675 && m[2] == 0x996D && m[3] == 0x87BA) return; // "No one can stop me, let alone you puny mortals!"
    if (m[0] == 0x8101 && m[1] == 0x6676 && m[2] == 0xBAFA && m[3] == 0x8E15) return; // "You are messing with affairs that are beyond your comprehension. Leave now and I may let you live!"
    if (m[0] == 0x8101 && m[1] == 0x6677 && m[2] == 0xA186 && m[3] == 0xF84C) return; // "His blood has returned me to my mortal body."
    if (m[0] == 0x8101 && m[1] == 0x6678 && m[2] == 0xD2ED && m[3] == 0xE693) return; // "I have returned!"
    if (m[0] == 0x8101 && m[1] == 0x6679 && m[2] == 0xA546 && m[3] == 0xF24A) return; // "Abaddon will feast on your eyes!"
    if (m[0] == 0x8101 && m[1] == 0x667A && m[2] == 0xB477 && m[3] == 0xA79A) return; // "Abaddon's sword has been drawn. He sends me back to you with tokens of renewed power!"
    if (m[0] == 0x8101 && m[1] == 0x667B && m[2] == 0x8FBB && m[3] == 0xC739) return; // "Are you the Keymaster?"
    if (m[0] == 0x8101 && m[1] == 0x667C && m[2] == 0xFE50 && m[3] == 0xC173) return; // "Human sacrifice. Dogs and cats living together. Mass hysteria!"
    if (m[0] == 0x8101 && m[1] == 0x667D && m[2] == 0xBBC6 && m[3] == 0xAC9E) return; // "Take me now, subcreature."'
    if (m[0] == 0x8101 && m[1] == 0x667E && m[2] == 0xCD71 && m[3] == 0xDEE3) return; // "We must prepare for the coming of Banjo the Clown, God of Puppets."
    if (m[0] == 0x8101 && m[1] == 0x667F && m[2] == 0xE823 && m[3] == 0x9435) return; // "This house is clean."
    if (m[0] == 0x8101 && m[1] == 0x6680 && m[2] == 0x82FC && m[3] == 0xDCEC) return;
    if (m[0] == 0x8101 && m[1] == 0x6681 && m[2] == 0xC86C && m[3] == 0xB975) return; // "Mommy? Where are you? I can't find you. I can't. I'm afraid of the light, mommy. I'm afraid of the light."
    if (m[0] == 0x8101 && m[1] == 0x6682 && m[2] == 0xE586 && m[3] == 0x9311) return; // "Get away from my baby!"
    if (m[0] == 0x8101 && m[1] == 0x6683 && m[2] == 0xA949 && m[3] == 0xE643) return; // "This house has many hearts."'
    if (m[0] == 0x8101 && m[1] == 0x6684 && m[2] == 0xB765 && m[3] == 0x93F1) return; // "As a boy I spent much time in these lands."
    if (m[0] == 0x8101 && m[1] == 0x6685 && m[2] == 0xEDE0 && m[3] == 0xAF1D) return; // "I see dead people."
    if (m[0] == 0x8101 && m[1] == 0x6686 && m[2] == 0xD356 && m[3] == 0xDC69) return; // "Do you like my fish balloon? Can you hear it singing to you...?"
    if (m[0] == 0x8101 && m[1] == 0x6687 && m[2] == 0xEA3C && m[3] == 0x96F0) return; // "4...Itchy...Tasty..."
    if (m[0] == 0x8101 && m[1] == 0x6688 && m[2] == 0xCBDD && m[3] == 0xB1CF) return; // "Gracious me, was I raving? Please forgive me. I'm mad."
    if (m[0] == 0x8101 && m[1] == 0x6689 && m[2] == 0xE770 && m[3] == 0xEEA4) return; // "Keep away. The sow is mine."
    if (m[0] == 0x8101 && m[1] == 0x668A && m[2] == 0x885F && m[3] == 0xE61D) return; // "All is well. I'm not insane."
    if (m[0] == 0x8101 && m[1] == 0x668B && m[2] == 0xCCDD && m[3] == 0x88AA) return; // "I like how they've decorated this place. The talking lights are a nice touch."
    if (m[0] == 0x8101 && m[1] == 0x668C && m[2] == 0x8873 && m[3] == 0x9A16) return; // "There's a reason there's a festival ticket in my ear. I'm trying to lure the evil spirits out of my head."
    if (m[0] == 0x8101 && m[1] == 0x668D && m[2] == 0xAF68 && m[3] == 0xF84A) return; // "And this is where I met the Lich. He told me to burn things."
    if (m[0] == 0x8101 && m[1] == 0x668E && m[2] == 0xFE43 && m[3] == 0x9CB3) return; // "When I grow up, I want to be a principal or a caterpillar."
    if (m[0] == 0x8101 && m[1] == 0x668F && m[2] == 0xDAFF && m[3] == 0x903E) return; // "Oh boy, sleep! That's where I'm a Luxon."
    if (m[0] == 0x8101 && m[1] == 0x6690 && m[2] == 0xA1F5 && m[3] == 0xD15F) return; // "My cat's breath smells like cat food."
    if (m[0] == 0x8101 && m[1] == 0x6691 && m[2] == 0xAE54 && m[3] == 0x8EC6) return; // "My cat's name is Mittens."
    if (m[0] == 0x8101 && m[1] == 0x6692 && m[2] == 0xDFBB && m[3] == 0xD674) return; // "Then the healer told me that BOTH my eyes were lazy. And that's why it was the best summer ever!"
    if (m[0] == 0x8101 && m[1] == 0x6693 && m[2] == 0xAC9F && m[3] == 0xDCBE) return; // "Go, banana!"
    if (m[0] == 0x8101 && m[1] == 0x6694 && m[2] == 0x9ACA && m[3] == 0xC746) return; // "It's a trick. Get an axe."
    if (m[0] == 0x8101 && m[1] == 0x6695 && m[2] == 0x8ED8 && m[3] == 0xD572) return; // "Klaatu...barada...necktie?"
    if (m[0] == 0x8101 && m[1] == 0x6696 && m[2] == 0xE883 && m[3] == 0xFED7) return; // "You're disgusting, but I love you!"
    if (m[0] == 0x8101 && m[1] == 0x68BA && m[2] == 0xA875 && m[3] == 0xA785) return; // "Cross over, children. All are welcome. All welcome. Go into the light. There is peace and serenity in the light."
    //printf("m[0] == 0x%X && m[1] == 0x%X && m[2] == 0x%X && m[3] == 0x%X\n", m[0], m[1], m[2], m[3]);

    status->blocked = blocked;
    return;
}
void PconsWindow::OnObjectiveDone(GW::HookStatus *,GW::Packet::StoC::ObjectiveDone *packet)
{
    PconsWindow &instance = Instance();
    instance.objectives_complete.push_back(packet->objective_id);
    instance.CheckObjectivesCompleteAutoDisable();
}
void PconsWindow::OnVanquishComplete(GW::HookStatus *,GW::Packet::StoC::VanquishComplete *)
{
    PconsWindow &instance = Instance();
    if (!instance.disable_cons_on_vanquish_completion || !instance.enabled)
        return;
    instance.SetEnabled(false);
    Log::Info("Cons auto-disabled on completion");
}
void PconsWindow::CmdPcons(const wchar_t*, int argc, LPWSTR* argv) {
    if (argc <= 1) {
        Instance().ToggleEnable();
    } else if (argc >= 2) {
        std::wstring arg1 = GuiUtils::ToLower(argv[1]);
        if (arg1 != L"on" && arg1 != L"off" && arg1 != L"toggle" && arg1 != L"refill") {
            Log::Error("Invalid argument '%ls', please use /pcons [on|off|toggle] [pcon]", argv[1]);
        }
        if (argc == 2) {
            if (arg1 == L"on") {
                Instance().SetEnabled(true);
            } else if (arg1 == L"off") {
                Instance().SetEnabled(false);
            } else if (arg1 == L"toggle") {
                Instance().ToggleEnable();
            } else if (arg1 == L"refill") {
                Instance().Refill();
            }
        } else {
            std::wstring argPcon = GuiUtils::ToLower(argv[2]);
            for (int i = 3; i < argc; i++) {
                argPcon.append(L" ");
                argPcon.append(GuiUtils::ToLower(argv[i]));
            }
            std::vector<Pcon*> pcons = PconsWindow::Instance().pcons;
            std::string compare = GuiUtils::WStringToString(argPcon);
            unsigned int compareLength = compare.length();
            Pcon* bestMatch = nullptr;
            unsigned int bestMatchLength = 0;
            for (size_t i = 0; i < pcons.size(); ++i) {
                Pcon* pcon = pcons[i];
                std::string pconName(pcon->chat);
                std::string pconNameSanitized = GuiUtils::ToLower(pconName);
                unsigned int pconNameLength = pconNameSanitized.length();
                if (compareLength > pconNameLength) continue;
                if (pconNameSanitized.rfind(compare) == std::string::npos) continue;
                if (bestMatchLength < pconNameLength) {
                    bestMatchLength = pconNameLength;
                    bestMatch = pcon;
                    if (bestMatchLength == pconNameLength) {
                        break;
                    }
                }
            }
            if (bestMatch == nullptr) {
                Log::Error("Could not find pcon %ls", argPcon.c_str());
                return;
            }
            if (arg1 == L"on") {
                bestMatch->SetEnabled(true);
            } else if (arg1 == L"off") {
                bestMatch->SetEnabled(false);
            } else if (arg1 == L"toggle") {
                bestMatch->Toggle();
            } else if (arg1 == L"refill") {
                bestMatch->Refill();
            }
        }
    }
}

bool PconsWindow::DrawTabButton(IDirect3DDevice9* device, bool show_icon, bool show_text, bool center_align_text) {
    bool clicked = ToolboxWindow::DrawTabButton(device, show_icon, show_text, center_align_text);

    ImGui::PushStyleColor(ImGuiCol_Text, enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (ImGui::Button(enabled ? "Enabled###pconstoggle" : "Disabled###pconstoggle",
        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        ToggleEnable();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    return clicked;
}
void PconsWindow::Draw(IDirect3DDevice9* device) {
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();
    if (show_enable_button) {
        ImGui::PushStyleColor(ImGuiCol_Text, enabled ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
        if (ImGui::Button(enabled ? "Enabled###pconstoggle" : "Disabled###pconstoggle",
            ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ToggleEnable();
        }
        ImGui::PopStyleColor();
    }
    int j = 0;
    for (unsigned int i = 0; i < pcons.size(); ++i) {
        if (pcons[i]->IsVisible()) {
            if (j++ % items_per_row > 0) {
                ImGui::SameLine(0,2.0f);
            }
            pcons[i]->Draw(device);
        }
    }

    if(instance_type == GW::Constants::InstanceType::Explorable && show_auto_disable_pcons_tickbox) {
        if (j && j % items_per_row > 0)
            ImGui::NewLine();
        if (!current_objectives_to_check.empty()) {
            ImGui::Checkbox("Off @ end", &disable_cons_on_objective_completion);
            ImGui::ShowHelp(disable_cons_on_objective_completion_hint);
        }
        if (!(current_final_room_location == GW::Vec2f(0, 0))) {
            ImGui::Checkbox("Off @ boss", &disable_cons_in_final_room);
            ImGui::ShowHelp(disable_cons_in_final_room_hint);
        }
        if (in_vanquishable_area) {
            ImGui::Checkbox("Off @ end", &disable_cons_on_vanquish_completion);
            ImGui::ShowHelp(disable_cons_on_vanquish_completion_hint);
        }
    }
    ImGui::End();

}
void PconsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (instance_type != GW::Map::GetInstanceType() || map_id != GW::Map::GetMapID())
        MapChanged(); // Map changed.
    if (!player && instance_type == GW::Constants::InstanceType::Explorable)
        player = GW::Agents::GetPlayer(); // Won't be immediately able to get player ptr on map load, so put here.
    if (!Pcon::map_has_effects_array) {
        Pcon::map_has_effects_array = GW::Effects::GetPlayerEffectsArray() != nullptr;
    }
    in_vanquishable_area = GW::Map::GetFoesToKill() != 0;
    CheckBossRangeAutoDisable();
    for (Pcon* pcon : pcons) {
        pcon->Update();
    }
}
void PconsWindow::MapChanged() {
    elite_area_check_timer = TIMER_INIT();
    map_id = GW::Map::GetMapID();
    Pcon::map_has_effects_array = false;
    if(instance_type != GW::Constants::InstanceType::Loading)
        previous_instance_type = instance_type;
    instance_type = GW::Map::GetInstanceType();
    // If we've just come from an explorable area then disable pcons.
    if (disable_pcons_on_map_change && previous_instance_type == GW::Constants::InstanceType::Explorable)
        SetEnabled(false);

    player = nullptr;
    elite_area_disable_triggered = false;
    // Find out which objectives we need to complete for this map.
    auto map_objectives_it = objectives_to_complete_by_map_id.find(map_id);
    if (map_objectives_it != objectives_to_complete_by_map_id.end()) {
        objectives_complete.clear();
        current_objectives_to_check = map_objectives_it->second;
    }
    else {
        current_objectives_to_check.clear();
    }
    // Find out if we need to check for boss range for this map.
    auto map_location_it = final_room_location_by_map_id.find(map_id);
    if (map_location_it != final_room_location_by_map_id.end()) {
        current_final_room_location = map_location_it->second;
    }
    else {
        current_final_room_location = GW::Vec2f(0, 0);
    }


}
void PconsWindow::Refill(bool do_refill) {
    for (Pcon* pcon : pcons) {
        pcon->Refill(do_refill && pcon->IsEnabled());
    }
}
bool PconsWindow::GetEnabled() {
    return enabled;
}
bool PconsWindow::SetEnabled(bool b) {
    if (enabled == b) return enabled; // Do nothing - already enabled/disabled.
    enabled = b;
    Refill(enabled);
    switch (GW::Map::GetInstanceType()) {
    case GW::Constants::InstanceType::Outpost:
        if(tick_with_pcons)
            GW::PartyMgr::Tick(enabled);
    case GW::Constants::InstanceType::Explorable: {
        if (HotkeysWindow::Instance().current_hotkey &&
            !HotkeysWindow::Instance()
                 .current_hotkey->show_message_in_emote_channel)
            break; // Selected hotkey doesn't allow a message.
        ImGuiWindow *main =
            ImGui::FindWindowByName(MainWindow::Instance().Name());
        ImGuiWindow *pcon = ImGui::FindWindowByName(Name());
        if ((pcon == nullptr || pcon->Collapsed || !visible) &&
            (main == nullptr || main->Collapsed ||
             !MainWindow::Instance().visible)) {
            Log::Info("Pcons %s", enabled ? "enabled" : "disabled");
        }
        break;
    }
    default:
        break;
    }
    return enabled;
}

void PconsWindow::RegisterSettingsContent() {
    ToolboxUIElement::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "Game Settings",
        nullptr,
        [this](const std::string*, bool is_showing) {
            if (!is_showing) return;
            DrawLunarsAndAlcoholSettings();
        }, 1.1f);
}

void PconsWindow::DrawLunarsAndAlcoholSettings() {
    ImGui::Text("Lunars and Alcohol");
    ImGui::Text("Current drunk level: %d", Pcon::alcohol_level);
    ImGui::StartSpacedElements(380.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Suppress lunar and drunk post-processing effects", &Pcon::suppress_drunk_effect);
    ImGui::ShowHelp("Will actually disable any *change*, so make sure you're not drunk already when enabling this!");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Suppress lunar and drunk text", &Pcon::suppress_drunk_text);
    ImGui::ShowHelp("Will hide drunk and lunars messages on top of your and other characters");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Suppress drunk emotes", &Pcon::suppress_drunk_emotes);
    ImGui::ShowHelp("Important:\n"
        "This feature is experimental and might crash your game.\n"
        "Using level 1 alcohol instead of this is recommended for preventing drunk emotes.\n"
        "This will prevent kneel, bored, moan, flex, fistshake and roar.\n");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Hide Spiritual Possession and Lucky Aura", &Pcon::suppress_lunar_skills);
    ImGui::ShowHelp("Will hide the skills in your effect monitor");
}

void PconsWindow::CheckObjectivesCompleteAutoDisable() {
    if (!enabled || elite_area_disable_triggered || instance_type != GW::Constants::InstanceType::Explorable) {
        return;     // Pcons disabled, auto disable already triggered, or not in explorable area.
    }
    if (!disable_cons_on_objective_completion || objectives_complete.empty() || current_objectives_to_check.empty()) {
        return; // No objectives complete, or no objectives to check for this map.
    }
    bool objective_complete = false;
    for (size_t i = 0; i < current_objectives_to_check.size(); i++) {
        objective_complete = false;
        for (size_t j = 0; j < objectives_complete.size() && !objective_complete; j++) {
            objective_complete = current_objectives_to_check.at(i) == objectives_complete.at(j);
        }
        if (!objective_complete)    return; // Not all objectives complete.
    }
    if (objective_complete) {
        elite_area_disable_triggered = true;
        SetEnabled(false);
        Log::Info("Cons auto-disabled on completion");
    }
}

void PconsWindow::CheckBossRangeAutoDisable() { // Trigger Elite area auto disable if applicable
    if (!enabled || elite_area_disable_triggered || instance_type != GW::Constants::InstanceType::Explorable) {
        return;     // Pcons disabled, auto disable already triggered, or not in explorable area.
    }
    if (!disable_cons_in_final_room || current_final_room_location == GW::Vec2f(0, 0) || !player || TIMER_DIFF(elite_area_check_timer) < 1000) {
        return;     // No boss location to check for this map, player ptr not loaded, or checked recently already.
    }
    elite_area_check_timer = TIMER_INIT();
    float d = GetDistance(GW::Vec2f(player->pos), current_final_room_location);
    if (d > 0 && d <= GW::Constants::Range::Spirit) {
        elite_area_disable_triggered = true;
        SetEnabled(false);
        Log::Info("Cons auto-disabled in range of boss");
    }
}

void PconsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

    for (Pcon* pcon : pcons) {
        pcon->LoadSettings(ini, Name());
    }

    tick_with_pcons = ini->GetBoolValue(Name(), VAR_NAME(tick_with_pcons), true);
    items_per_row = ini->GetLongValue(Name(), VAR_NAME(items_per_row), 3);
    Pcon::pcons_delay = ini->GetLongValue(Name(), VAR_NAME(pcons_delay), 5000);
    Pcon::lunar_delay = ini->GetLongValue(Name(), VAR_NAME(lunar_delay), 500);
    Pcon::size = (float)ini->GetDoubleValue(Name(), "pconsize", 46.0);
    Pcon::disable_when_not_found = ini->GetBoolValue(Name(), VAR_NAME(disable_when_not_found), true);
    Pcon::enabled_bg_color = Colors::Load(ini, Name(), VAR_NAME(enabled_bg_color), Pcon::enabled_bg_color);
    show_enable_button = ini->GetBoolValue(Name(), VAR_NAME(show_enable_button), show_enable_button);
    Pcon::suppress_drunk_effect = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_effect), false);
    Pcon::suppress_drunk_text = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_text), false);
    Pcon::suppress_drunk_emotes = ini->GetBoolValue(Name(), VAR_NAME(suppress_drunk_emotes), false);
    Pcon::suppress_lunar_skills = ini->GetBoolValue(Name(), VAR_NAME(suppress_lunar_skills), false);
    Pcon::pcons_by_character = ini->GetBoolValue(Name(), VAR_NAME(pcons_by_character), Pcon::pcons_by_character);
    Pcon::refill_if_below_threshold = ini->GetBoolValue(Name(), VAR_NAME(refill_if_below_threshold), false);
    show_auto_refill_pcons_tickbox = ini->GetBoolValue(Name(), VAR_NAME(show_auto_refill_pcons_tickbox), show_auto_refill_pcons_tickbox);
    show_auto_disable_pcons_tickbox = ini->GetBoolValue(Name(), VAR_NAME(show_auto_disable_pcons_tickbox), show_auto_disable_pcons_tickbox);

    show_storage_quantity = ini->GetBoolValue(Name(), VAR_NAME(show_storage_quantity), show_storage_quantity);
    shift_click_toggles_category = ini->GetBoolValue(Name(), VAR_NAME(shift_click_toggles_category), shift_click_toggles_category);

    disable_pcons_on_map_change = ini->GetBoolValue(Name(), VAR_NAME(disable_pcons_on_map_change), disable_pcons_on_map_change);
    disable_cons_on_vanquish_completion = ini->GetBoolValue(Name(), VAR_NAME(disable_cons_on_vanquish_completion), disable_cons_on_vanquish_completion);
    disable_cons_in_final_room = ini->GetBoolValue(Name(), VAR_NAME(disable_cons_in_final_room), disable_cons_in_final_room);
    disable_cons_on_objective_completion = ini->GetBoolValue(Name(), VAR_NAME(disable_cons_on_objective_completion), disable_cons_on_objective_completion);
}

void PconsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);

    for (Pcon* pcon : pcons) {
        pcon->SaveSettings(ini, Name());
    }

    ini->SetBoolValue(Name(), VAR_NAME(tick_with_pcons), tick_with_pcons);
    ini->SetLongValue(Name(), VAR_NAME(items_per_row), items_per_row);
    ini->SetLongValue(Name(), VAR_NAME(pcons_delay), Pcon::pcons_delay);
    ini->SetLongValue(Name(), VAR_NAME(lunar_delay), Pcon::lunar_delay);
    ini->SetDoubleValue(Name(), "pconsize", Pcon::size);
    ini->SetBoolValue(Name(), VAR_NAME(disable_when_not_found), Pcon::disable_when_not_found);
    Colors::Save(ini, Name(), VAR_NAME(enabled_bg_color), Pcon::enabled_bg_color);
    ini->SetBoolValue(Name(), VAR_NAME(show_enable_button), show_enable_button);

    ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_effect), Pcon::suppress_drunk_effect);
    ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_text), Pcon::suppress_drunk_text);
    ini->SetBoolValue(Name(), VAR_NAME(suppress_drunk_emotes), Pcon::suppress_drunk_emotes);
    ini->SetBoolValue(Name(), VAR_NAME(suppress_lunar_skills), Pcon::suppress_lunar_skills);
    ini->SetBoolValue(Name(), VAR_NAME(pcons_by_character), Pcon::pcons_by_character);
    ini->SetBoolValue(Name(), VAR_NAME(hide_city_pcons_in_explorable_areas), Pcon::hide_city_pcons_in_explorable_areas);

    ini->SetBoolValue(Name(), VAR_NAME(refill_if_below_threshold), Pcon::refill_if_below_threshold);
    ini->SetBoolValue(Name(), VAR_NAME(show_auto_refill_pcons_tickbox), show_auto_refill_pcons_tickbox);
    ini->SetBoolValue(Name(), VAR_NAME(show_auto_disable_pcons_tickbox), show_auto_disable_pcons_tickbox);
    ini->SetBoolValue(Name(), VAR_NAME(shift_click_toggles_category), shift_click_toggles_category);
    ini->SetBoolValue(Name(), VAR_NAME(show_storage_quantity), show_storage_quantity);

    ini->SetBoolValue(Name(), VAR_NAME(disable_pcons_on_map_change), disable_pcons_on_map_change);
    ini->SetBoolValue(Name(), VAR_NAME(disable_cons_on_vanquish_completion), disable_cons_on_vanquish_completion);
    ini->SetBoolValue(Name(), VAR_NAME(disable_cons_in_final_room), disable_cons_in_final_room);
    ini->SetBoolValue(Name(), VAR_NAME(disable_cons_on_objective_completion), disable_cons_on_objective_completion);
}

void PconsWindow::DrawSettingInternal() {
    ImGui::Separator();
    ImGui::Text("Functionality:");
    ImGui::StartSpacedElements(275.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Toggle Pcons per character", &Pcon::pcons_by_character);
    ImGui::ShowHelp("Tick to remember pcon enable/disable per character.\nUntick to enable/disable regardless of current character.");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Tick with pcons", &tick_with_pcons);
    ImGui::ShowHelp("Enabling or disabling pcons will also Tick or Untick in party list");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Disable when not found", &Pcon::disable_when_not_found);
    ImGui::ShowHelp("Toolbox will disable a pcon if it is not found in the inventory");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Refill from storage", &Pcon::refill_if_below_threshold);
    ImGui::ShowHelp("Toolbox will refill pcons from storage if below the threshold");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show storage quantity in outpost", &show_storage_quantity);
    ImGui::ShowHelp("Display a number on the bottom of each pcon icon, showing total quantity in storage.\n"
                    "This only displays when in an outpost.");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Shift + Click toggles category", &shift_click_toggles_category);
    ImGui::ShowHelp("If this is ticked, clicking on a pcon while holding shift will enable/disable all of the same category\n"
                    "Categories: Conset, Rock Candies, Kabob+Soup+Salad");
    ImGui::SliderInt("Pcons delay", &Pcon::pcons_delay, 100, 5000, "%d milliseconds");
    ImGui::ShowHelp(
        "After using a pcon, toolbox will not use it again for this amount of time.\n"
        "It is needed to prevent toolbox from using a pcon twice, before it activates.\n"
        "Decrease the value if you have good ping and you die a lot.");
    ImGui::SliderInt("Lunars delay", &Pcon::lunar_delay, 100, 500, "%d milliseconds");
    if (ImGui::TreeNodeEx("Thresholds", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Text("When you have less than this amount:\n-The number in the interface becomes yellow.\n-Warning message is displayed when zoning into outpost.");
        for (Pcon* pcon : pcons) {
            ImGui::SliderInt(pcon->chat.c_str(), &pcon->threshold, 0, 250);
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Interface:");
    ImGui::SliderInt("Items per row", &items_per_row, 1, 18);
    ImGui::DragFloat("Pcon Size", &Pcon::size, 1.0f, 10.0f, 0.0f);
    ImGui::ShowHelp("Size of each Pcon icon in the interface");
    Colors::DrawSettingHueWheel("Enabled-Background", &Pcon::enabled_bg_color);
    if (Pcon::size <= 1.0f) Pcon::size = 1.0f;
    if (ImGui::TreeNodeEx("Visibility", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::StartSpacedElements(300.f);
        ImGui::NextSpacedElement(); ImGui::Checkbox("Show Enable/Disable button", &show_enable_button);
        ImGui::NextSpacedElement(); ImGui::Checkbox("Show auto disable pcons checkboxes", &show_auto_disable_pcons_tickbox);
        ImGui::ShowHelp("Will show a tickbox in the pcons window when in an elite area");
        ImGui::NextSpacedElement(); ImGui::Checkbox("Hide city Pcons in explorable areas", &Pcon::hide_city_pcons_in_explorable_areas);
        ImGui::StartSpacedElements(280.f);
        for (Pcon* pcon : pcons) {
            ImGui::NextSpacedElement(); ImGui::Checkbox(pcon->chat.c_str(), &pcon->visible);
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    DrawLunarsAndAlcoholSettings();
    ImGui::Separator();
    ImGui::Text("Auto-Disabling Pcons");
    ImGui::StartSpacedElements(380.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Auto Disable on Vanquish completion", &disable_cons_on_vanquish_completion);
    ImGui::ShowHelp(disable_cons_on_vanquish_completion_hint);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Auto Disable in final room of Urgoz/Deep", &disable_cons_in_final_room);
    ImGui::ShowHelp(disable_cons_in_final_room_hint);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Auto Disable on final objective completion", &disable_cons_on_objective_completion);
    ImGui::ShowHelp(disable_cons_on_objective_completion_hint);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Disable on map change", &disable_pcons_on_map_change);
    ImGui::ShowHelp("Toolbox will disable pcons when leaving an explorable area");
}
