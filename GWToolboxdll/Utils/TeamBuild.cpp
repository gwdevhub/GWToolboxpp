#include "stdafx.h"

#include <Utils/TeamBuild.h>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Logger.h>
#include <Modules/Resources.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <Windows/PconsWindow.h>
#include "TeamBuildEncoder.h"
#include <GWCA/Managers/PartyMgr.h>
#include "ToolboxUtils.h"

namespace {

    IDirect3DTexture9** skill_toggle_sprite = nullptr;

    const Color build_edit_pcon_enabled_color = Colors::ARGB(102, 0, 255, 0);

    using GW::Constants::HeroID;

    // ----------------------------------------------------------------
    // Async build-load infrastructure
    // ----------------------------------------------------------------

    struct PendingBuildLoad {
        Build build;
        enum Stage : uint8_t { AddHero, WaitForHero, Finished } stage = AddHero;
        size_t party_hero_index = 0xFFFFFFFF;
        clock_t started = 0;

        bool Process();
    };

    std::vector<PendingBuildLoad> pending_build_loads;
    std::queue<std::wstring>      send_queue;
    clock_t send_timer   = 0;
    clock_t kickall_timer = 0;

    size_t GetPlayerHeroCount()
    {
        size_t ret = 0;
        const GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        if (!party_info) return ret;
        const GW::HeroPartyMemberArray& heroes = party_info->heroes;
        if (!heroes.valid()) return ret;
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!me) return ret;
        const uint32_t my_id = me->login_number;
        for (size_t i = 0; i < heroes.size(); i++) {
            if (heroes[i].owner_player_id == my_id) ret++;
        }
        return ret;
    }

    const GW::HeroFlag* GetHeroFlagInfo(const uint32_t hero_id)
    {
        const GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->world) return nullptr;
        for (const GW::HeroFlag& flag : g->world->hero_flags) {
            if (flag.hero_id == hero_id) return &flag;
        }
        return nullptr;
    }

    GW::HeroPartyMember* GetPartyHeroByID(const HeroID hero_id, size_t* out_hero_index)
    {
        GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        if (!party_info) return nullptr;
        GW::HeroPartyMemberArray& heroes = party_info->heroes;
        if (!heroes.valid()) return nullptr;
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!me) return nullptr;
        const uint32_t my_id = me->login_number;
        for (size_t i = 0; i < heroes.size(); i++) {
            if (heroes[i].owner_player_id == my_id && heroes[i].hero_id == hero_id) {
                if (out_hero_index) *out_hero_index = i + 1;
                return &heroes[i];
            }
        }
        return nullptr;
    }

    void LoadPcons(const Build& build)
    {
        if (build.pcons.empty()) return;
        PconsWindow* pcw = &PconsWindow::Instance();
        std::vector<Pcon*> loaded, not_visible;
        for (auto* pcon : pcw->pcons) {
            const bool enable = build.pcons.contains(pcon->ini);
            if (enable) {
                if (!pcon->IsVisible()) { not_visible.push_back(pcon); continue; }
                pcon->SetEnabled(false);
                loaded.push_back(pcon);
            }
            pcon->SetEnabled(enable);
        }
        if (!loaded.empty()) {
            std::string s;
            size_t i = 0;
            for (const auto* p : loaded) { if (i++) s += ", "; s += p->abbrev; }
            Log::Flash("Pcons loaded: %s", s.c_str());
        }
        if (!not_visible.empty()) {
            std::string s;
            size_t i = 0;
            for (const auto* p : not_visible) { if (i++) s += ", "; s += p->abbrev; }
            Log::Warning("Pcons not loaded (not visible in Pcons window): %s", s.c_str());
        }
    }

    bool PendingBuildLoad::Process()
    {
        if (build.IsPlayerBuild()) {
            if (!build.code.empty())
                GW::SkillbarMgr::LoadSkillTemplate(build.code.c_str());
            LoadPcons(build);
            return true;
        }

        // Hero build: async — add hero then wait for it to appear in party.
        if (!started) started = TIMER_INIT();
        if (TIMER_DIFF(started) > 1000) return true; // timeout

        switch (stage) {
            case AddHero:
                if (!ToolboxUtils::IsHeroUnlocked(build.hero_id)) 
                    return true;
                GW::PartyMgr::AddHero(build.hero_id);
                stage = WaitForHero;
                [[fallthrough]];
            case WaitForHero: {
                const GW::HeroPartyMember* hero = GetPartyHeroByID(build.hero_id, &party_hero_index);
                if (!hero) break;
                const GW::HeroFlag* flag = GetHeroFlagInfo(build.hero_id);
                if (!flag) break;
                if (!build.code.empty())
                    GW::SkillbarMgr::LoadSkillTemplate(hero->agent_id, build.code.c_str());
                for (uint32_t k = 0; k < 8; k++)
                    GW::PartyMgr::SetHeroSkillDisabled(hero->agent_id, k, ((build.disabled_skills >> k) & 1) != 0);
                GW::UI::SendUIMessage(build.show_panel
                    ? GW::UI::UIMessage::kShowHeroPanel
                    : GW::UI::UIMessage::kHideHeroPanel,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(build.hero_id)));
                const auto behavior = static_cast<GW::HeroBehavior>(build.behavior);
                if (behavior <= GW::HeroBehavior::AvoidCombat && flag->hero_behavior != behavior)
                    GW::PartyMgr::SetHeroBehavior(hero->agent_id, behavior);
                stage = Finished;
                [[fallthrough]];
            }
            case Finished:
                return true;
        }
        return false;
    }

    // ----------------------------------------------------------------

    // Hero IDs in the same order as the GW client hero panel.
    // Razah appears after mesmers because most players without mercenaries have it set as mesmer.
    constexpr std::array HeroIndexToID = {
        HeroID::NoHero,
        HeroID::Goren,
        HeroID::Koss,
        HeroID::Jora,
        HeroID::AcolyteJin,
        HeroID::MargridTheSly,
        HeroID::PyreFierceshot,
        HeroID::Tahlkora,
        HeroID::Dunkoro,
        HeroID::Ogden,
        HeroID::MasterOfWhispers,
        HeroID::Olias,
        HeroID::Livia,
        HeroID::Norgu,
        HeroID::Razah,
        HeroID::Gwen,
        HeroID::AcolyteSousuke,
        HeroID::ZhedShadowhoof,
        HeroID::Vekk,
        HeroID::Zenmai,
        HeroID::Anton,
        HeroID::Miku,
        HeroID::Xandra,
        HeroID::ZeiRi,
        HeroID::GeneralMorgahn,
        HeroID::KeiranThackeray,
        HeroID::Hayda,
        HeroID::Melonni,
        HeroID::MOX,
        HeroID::Kahmu,
        HeroID::Merc1,
        HeroID::Merc2,
        HeroID::Merc3,
        HeroID::Merc4,
        HeroID::Merc5,
        HeroID::Merc6,
        HeroID::Merc7,
        HeroID::Merc8,
        HeroID::Devona,
        HeroID::GhostOfAlthea
    };

    // Returns hero IDs sorted by name; re-sorts each frame until all names are decoded.
    const std::vector<HeroID>& SortedHeroIDs()
    {
        static std::vector<HeroID> sorted;
        static bool is_sorted = false;
        if (!is_sorted) {
            sorted.clear();
            for (const auto id : HeroIndexToID) {
                if (id != HeroID::NoHero) sorted.push_back(id);
            }
            bool all_decoded = true;
            for (const auto id : sorted) {
                if (Resources::GetHeroName(id)->string().empty()) {
                    all_decoded = false;
                    break;
                }
            }
            std::ranges::sort(sorted, [](const HeroID a, const HeroID b) {
                return _stricmp(Resources::GetHeroName(a)->string().c_str(),
                                Resources::GetHeroName(b)->string().c_str()) < 0;
            });
            is_sorted = all_decoded;
        }
        return sorted;
    }

    void DefaultView(const Build& build)
    {
        GW::GameThread::Enqueue([code = build.code, name = build.name] {
            GW::UI::ChatTemplate t{};
            auto code_ws = TextUtils::StringToWString(code);
            t.code.m_buffer = code_ws.data();
            t.code.m_size = t.code.m_capacity = code_ws.size() + 1;
            auto name_ws = TextUtils::StringToWString(name);
            t.name = name_ws.data();
            GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenTemplate, &t);
        });
    }

} // namespace

// ============================================================
// Build
// ============================================================

Build::Build(std::string_view n, std::string_view c,
             GW::Constants::HeroID hero_id_, int show_panel_,
             uint32_t behavior_, uint8_t disabled_skills_)
    : name(n)
    , code(c)
    , hero_id(hero_id_)
    , behavior(behavior_)
    , show_panel(show_panel_ != 0)
    , disabled_skills(disabled_skills_)
{
}

std::string Build::GetFallbackBuildName() const
{
    if (code.empty()) return {};
    GW::SkillbarMgr::SkillTemplate st{};
    if (!GW::SkillbarMgr::DecodeSkillTemplate(st, code.c_str())) return {};
    for (const auto skill_id : st.skills) {
        if (skill_id == GW::Constants::SkillID::No_Skill) continue;
        const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill || !skill->IsElite()) continue;
        return Resources::GetSkillName(skill_id)->string();
    }
    return {};
}

const GW::SkillbarMgr::SkillTemplate* Build::Decode()
{
    if (!IsDecoded() && !GW::SkillbarMgr::DecodeSkillTemplate(skill_template_, code.c_str())) {
        skill_template_.primary = skill_template_.secondary = GW::Constants::Profession::None;
    }
    return IsDecoded() ? &skill_template_ : nullptr;
}

bool Build::IsDecoded() const
{
    return !(skill_template_.primary == GW::Constants::Profession::None &&
             skill_template_.secondary == GW::Constants::Profession::None);
}

const GW::Constants::SkillID* Build::Skills()
{
    return Decode() ? skill_template_.skills : nullptr;
}

void Build::ResetDecodeCache()
{
    skill_template_.primary = skill_template_.secondary = GW::Constants::Profession::None;
}

void Build::View() const {
    GW::GameThread::Enqueue([code = code, name = name] {
        GW::UI::ChatTemplate t = {0};

        auto code_ws = TextUtils::StringToWString(code);
        t.code.m_buffer = code_ws.data();
        t.code.m_size = t.code.m_capacity = code_ws.size() + 1;

        auto name_ws = TextUtils::StringToWString(name);
        t.name = name_ws.data();
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenTemplate, &t);
    });
}
void Build::Send() const
{
    if (code.empty() && name.empty()) return;
    auto gen_name = name;
    if (hero_id != GW::Constants::HeroID::NoHero) {
        if (gen_name.empty())
            gen_name = Resources::GetHeroName(hero_id)->string();
        else
            gen_name = std::format("{} ({})", name, Resources::GetHeroName(hero_id)->string());
    }
    if (gen_name.empty()) {
        gen_name = GetFallbackBuildName();
    }
    const auto msg = code.empty() ? name : std::format("[{};{}]", gen_name, code);
    EnqueueSend(msg);
}

void Build::Copy() const
{
    if (code.empty() && name.empty()) return;
    auto gen_name = name;
    if (hero_id != GW::Constants::HeroID::NoHero) {
        if (gen_name.empty())
            gen_name = Resources::GetHeroName(hero_id)->string();
        else
            gen_name = std::format("{} ({})", name, Resources::GetHeroName(hero_id)->string());
    }
    const auto msg = code.empty() ? name : std::format("[{};{}]", gen_name, code);
    ImGui::SetClipboardText(msg.c_str());
    Log::Flash("Build code copied to clipboard");
}

void Build::EnqueueSend(std::string msg)
{
    send_queue.push(TextUtils::StringToWString(msg));
}

void Build::Load() const
{
    if (code.empty() && hero_id == GW::Constants::HeroID::NoHero) return;
    pending_build_loads.push_back({*this});
}

void Build::Update()
{
    const auto instance_type = GW::Map::GetInstanceType();

    if (instance_type == GW::Constants::InstanceType::Loading) {
        pending_build_loads.clear();
        while (!send_queue.empty()) send_queue.pop();
        kickall_timer = 0;
        return;
    }

    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        if (GW::Agents::GetControlledCharacter()) {
            GW::Chat::SendChat(send_queue.front().c_str());
            send_queue.pop();
            send_timer = TIMER_INIT();
        }
    }

    if (kickall_timer) {
        if (TIMER_DIFF(kickall_timer) > 500
            || instance_type != GW::Constants::InstanceType::Outpost
            || !GetPlayerHeroCount()) {
            kickall_timer = 0;
        }
        return;
    }

    for (size_t i = 0; i < pending_build_loads.size(); i++) {
        auto& pending = pending_build_loads[i];
        if (!pending.build.IsPlayerBuild()
            && instance_type != GW::Constants::InstanceType::Outpost) {
            pending_build_loads.clear();
            break;
        }
        if (pending.Process()) {
            pending_build_loads.erase(pending_build_loads.begin() + i);
            break;
        }
    }
}

// ============================================================
// TeamBuild
// ============================================================

uint32_t TeamBuild::s_cur_ui_id = 0;

TeamBuild::TeamBuild(std::string_view n, std::string_view id)
    : name(n)
    , ui_id(id.empty() ? std::to_string(++s_cur_ui_id) : std::string(id))
{
}

void TeamBuild::SetSkillToggleSprite(IDirect3DTexture9** sprite)
{
    skill_toggle_sprite = sprite;
}

const std::wstring& TeamBuild::GetEncoded() const
{
    if (!encoded_cache_.has_value())
        encoded_cache_ = TeamBuildEncoder::TeamBuildToEncoded(*this);
    return *encoded_cache_;
}

void TeamBuild::ResetEncodedCache() const
{
    encoded_cache_.reset();
}

void TeamBuild::Send(bool one_by_one) const
{
    if (!name.empty())
        Build::EnqueueSend(name);
    if (one_by_one) {
        for (const auto& build : builds)
            build.Send();
    }
    else {
        const auto& encoded = GetEncoded();
        if (!encoded.empty())
            send_queue.push(std::format(L"[TB;{}]", encoded));
    }
}

void TeamBuild::Copy() const
{
    const auto& encoded = GetEncoded();
    if (encoded.empty()) return;
    const auto msg = TextUtils::WStringToString(std::format(L"[TB;{}]", encoded));
    ImGui::SetClipboardText(msg.c_str());
    Log::Flash("Teambuild code copied to clipboard");
}

bool TeamBuild::ChatCodeTooLong() const
{
    const auto& encoded = GetEncoded();
    // [TB;<encoded>] = 4 + encoded.size() + 1 = encoded.size() + 5; must be < 120
    return !encoded.empty() && encoded.size() + 5 >= 120;
}

void TeamBuild::Load() const
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return;
    }
    if (has_hero_slots) {
        GW::PartyMgr::KickAllHeroes();
        kickall_timer = TIMER_INIT();
    }
    if (mode > 0) {
        GW::PartyMgr::SetHardMode(mode == 2);
    }
    for (const auto& build : builds) {
        build.Load();
    }
}

// ------------------------------------------------------------
// Player-builds layout (BuildsWindow style)
// ------------------------------------------------------------

void TeamBuild::DrawPlayerBuildsContent(
    bool& builds_changed)
{
    const float font_scale = ImGui::FontScale();
    const float btn_width  = 50.0f * font_scale;
    const float del_width  = 24.0f * font_scale;
    const float spacing    = ImGui::GetStyle().ItemSpacing.y;

    bool tmp = builds_changed;
    builds_changed = false;

    for (size_t j = 0; j < builds.size(); j++) {
        Build& build = builds[j];
        ImGui::PushID(static_cast<int>(j));

        const bool editing   = editing_build_idx_ == static_cast<int>(j);
        const float btn_offset = ImGui::GetContentRegionAvail().x - del_width * 2 - btn_width * 3 - spacing * 4;

        ImGui::Text("#%zu", j + 1);
        ImGui::PushItemWidth(btn_offset - btn_width - spacing * 2);
        ImGui::SameLine(btn_width, 0);
        if (ImGui::InputText("###name", build.name)) {
            builds_changed = true;
        }
        if (ImGui::IsItemHovered() && !build.name.empty()) {
            ImGui::SetTooltip([&build]() {
                ImGui::TextUnformatted(build.name.c_str());
                GuiUtils::DrawSkillbar(build.code.c_str());
            });
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(btn_offset);
        if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
            if (ImGui::GetIO().KeyCtrl) {
                build.Send();
            }
            else {
                build.View();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ImGui::GetIO().KeyCtrl
                ? "Click to send to team chat"
                : "Click to view build. Ctrl + Click to send to chat.");
        }

        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
            build.Load();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(!build.pcons.empty()
                ? "Click to load build template and pcons"
                : "Click to load build template");
        }

        ImGui::SameLine(0, spacing);
        if (editing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        }
        if (ImGui::Button("Edit", ImVec2(btn_width, 0))) {
            editing_build_idx_ = editing ? -1 : static_cast<int>(j);
        }
        if (editing) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to edit this build");
        }

        ImGui::SameLine(0, spacing);
        if (ImGui::Button(ICON_FA_COPY "###copy", ImVec2(del_width, 0))) {
            build.Copy();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy build code to clipboard");
        }

        ImGui::SameLine(0, spacing);
        bool delete_confirmed = false;
        if (ImGui::ConfirmButton(ICON_FA_TRASH, &delete_confirmed,
                "Delete Build\n\nAre you sure you want to delete this build?\nThis operation cannot be undone.")) {
            if (editing_build_idx_ == static_cast<int>(j))       editing_build_idx_ = -1;
            else if (editing_build_idx_ > static_cast<int>(j))   editing_build_idx_--;
            builds.erase(builds.begin() + static_cast<ptrdiff_t>(j));
            ResetEncodedCache();
            builds_changed = true;
            ImGui::PopID();
            break;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete build");
        }

        if (editing) {
            const float indent = btn_width;
            ImGui::Indent(indent);

            ImGui::TextUnformatted("Build Code:");
            ImGui::SameLine();
            if (ImGui::InputText("###code", build.code)) {
                build.ResetDecodeCache();
                ResetEncodedCache();
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([&build]() {
                    GuiUtils::DrawSkillbar(build.code.c_str());
                });
            }

            // Pcons (player builds only)
            ImGui::TextUnformatted("Pcons:");
            ImGui::ShowHelp("Enable or disable pcons activated in the Pcons window when this build is loaded");
            const auto& pcons = PconsWindow::Instance().pcons;
            const float skill_h = ImGui::CalcTextSize(" ").y * 2.f;
            ImGui::StartSpacedElements(skill_h + ImGui::GetStyle().ItemSpacing.x);
            for (const auto pcon : pcons) {
                ImGui::PushID(pcon);
                const bool active = build.pcons.contains(pcon->ini);
                ImGui::NextSpacedElement();
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, build_edit_pcon_enabled_color);
                if (ImGui::IconButton("", *pcon->GetTexture(), {skill_h, skill_h})) {
                    if (!active) build.pcons.emplace(pcon->ini);
                    else         build.pcons.erase(pcon->ini);
                    builds_changed = true;
                }
                if (active) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(pcon->chat.c_str());
                ImGui::PopID();
            }

            ImGui::Unindent(indent);
        }

        ImGui::PopID();
        if (builds_changed) break;
    }

    builds_changed |= tmp;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Checkbox("Show numbers", &show_numbers)) {
        builds_changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Prefix build names with their index when sending to chat");
    }

    ImGui::SameLine();
    const float add_btn_width = 140.f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - add_btn_width);
    if (ImGui::Button("Add Build", ImVec2(add_btn_width, 0))) {
        builds.emplace_back("", "");
        ResetEncodedCache();
        editing_build_idx_ = static_cast<int>(builds.size()) - 1;
        builds_changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add another build row");
    }
}

// ------------------------------------------------------------
// Hero-builds layout (HeroBuildsWindow style)
// ------------------------------------------------------------

void TeamBuild::DrawHeroBuildsContent(
    bool& builds_changed)
{
    const float btn_width      = 50.0f * ImGui::FontScale();
    const float icon_btn_width = btn_width / 1.75f;
    const float panel_width    = btn_width + 12.0f;
    const float item_spacing   = ImGui::GetStyle().ItemInnerSpacing.x;
    const float text_item_width =
        (ImGui::GetContentRegionAvail().x - btn_width - btn_width - btn_width
         - panel_width - icon_btn_width * 2 - item_spacing * 5) / 3.f;

    float offset = btn_width;
    ImGui::SetCursorPosX(offset);
    ImGui::Text("Name");
    ImGui::SameLine(offset += text_item_width + item_spacing);
    ImGui::Text("Template");

    uint32_t hero_count = 1;
    Build* player = 0;

    for (size_t j = 0; j < builds.size(); ++j) {
        offset = btn_width;
        Build& build = builds[j];
        ImGui::PushID(static_cast<int>(j));

        bool is_player = build.hero_id == GW::Constants::HeroID::NoHero && !player;

        if (is_player) player = &build;

        if (is_player)
            ImGui::Text("P");
        else
            ImGui::Text("H#%zu", hero_count++);

        ImGui::SameLine(offset);
        ImGui::PushItemWidth(text_item_width);
        if (ImGui::InputText("###name", build.name)) {
            builds_changed = true;
        }
        if (ImGui::IsItemHovered() && !build.name.empty()) {
            ImGui::SetTooltip("%s", build.name.c_str());
        }

        ImGui::SameLine(offset += text_item_width + item_spacing);
        if (ImGui::InputText("###code", build.code)) {
            build.ResetDecodeCache();
            ResetEncodedCache();
            builds_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip([&build]() {
                GuiUtils::DrawSkillbar(build.code.c_str());
            });
        }

        ImGui::SameLine(offset += text_item_width + item_spacing);

        if (is_player) {
            // Player slot: no hero controls, no pcons in this layout
            ImGui::TextDisabled("Player");
            ImGui::SameLine(offset += text_item_width + item_spacing + btn_width + 10.0f + icon_btn_width + item_spacing * 2);
            ImGui::PopItemWidth();
        }
        else {
            // Hero slot: hero selector, show_panel, behavior, disabled_skills
            {
                const auto& sorted_heroes = SortedHeroIDs();
                const auto hero_it = std::ranges::find(sorted_heroes, build.hero_id);
                int combo_idx = hero_it != sorted_heroes.end()
                    ? static_cast<int>(std::distance(sorted_heroes.begin(), hero_it)) : -1;
                if (ImGui::MyCombo(
                        "###heroid", "Choose Hero", &combo_idx,
                        [](void*, const int idx, const char** out_text) -> bool {
                            const auto& heroes = SortedHeroIDs();
                            if (idx < 0 || idx >= static_cast<int>(heroes.size())) return false;
                            *out_text = Resources::GetHeroName(heroes[idx])->string().c_str();
                            return true;
                        },
                        nullptr, static_cast<int>(sorted_heroes.size()))) {
                    build.hero_id = (combo_idx >= 0 && combo_idx < static_cast<int>(sorted_heroes.size()))
                        ? sorted_heroes[combo_idx] : HeroID::NoHero;
                    ResetEncodedCache();
                    builds_changed = true;
                }
            }

            ImGui::PopItemWidth();
            ImGui::SameLine(offset += text_item_width + item_spacing);

            // Show-panel toggle
            const auto* panel_icon = reinterpret_cast<const char*>(build.show_panel ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
            if (ImGui::Button(panel_icon, ImVec2(icon_btn_width, 0))) {
                build.show_panel = !build.show_panel;
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(build.show_panel ? "Hero panel: Show" : "Hero panel: Hide");
            }

            ImGui::SameLine(offset += icon_btn_width + item_spacing);

            // Behavior cycle
            const char* behavior_icon    = reinterpret_cast<const char*>(ICON_FA_SHIELD_ALT);
            const char* behavior_tooltip = "Hero behaviour: Guard";
            switch (build.behavior) {
                case 2:
                    behavior_icon    = reinterpret_cast<const char*>(ICON_FA_DOVE);
                    behavior_tooltip = "Hero behaviour: Avoid Combat";
                    break;
                case 0:
                    behavior_icon    = reinterpret_cast<const char*>(ICON_FA_FIST_RAISED);
                    behavior_tooltip = "Hero behaviour: Fight";
                    break;
            }
            if (ImGui::Button(behavior_icon, ImVec2(icon_btn_width, 0))) {
                if (++build.behavior > 2) build.behavior = 0;
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(behavior_tooltip);
            }

            ImGui::SameLine(offset += icon_btn_width + item_spacing);

            // Disabled-skills button + popup
            int enabled_count = 8;
            for (int k = 0; k < 8; k++) {
                if ((build.disabled_skills >> k) & 1) enabled_count--;
            }
            char skills_label[8];
            snprintf(skills_label, sizeof(skills_label), "%d/8", enabled_count);
            if (ImGui::Button(skills_label, ImVec2(icon_btn_width, 0))) {
                ImGui::OpenPopup("SkillsPopup");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to toggle which skills are disabled when loading this build");
            }
            if (ImGui::BeginPopup("SkillsPopup")) {
                GW::SkillbarMgr::SkillTemplate st{};
                const bool decoded = !build.code.empty() && GW::SkillbarMgr::DecodeSkillTemplate(st, build.code.c_str());
                constexpr float skill_px = 48.0f;
                const ImVec2 btn_size(skill_px, skill_px);
                for (int k = 0; k < 8; k++) {
                    if (k > 0) ImGui::SameLine(0, 0);
                    const bool is_disabled = (build.disabled_skills >> k) & 1;
                    const auto skill_id = decoded ? st.skills[k] : GW::Constants::SkillID::No_Skill;
                    auto* skill_tex = *Resources::GetSkillImage(skill_id);
                    ImGui::PushID(k);
                    const ImVec2 pos = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("##skill_slot", btn_size)) {
                        if (is_disabled) build.disabled_skills &= static_cast<uint8_t>(~(1u << k));
                        else             build.disabled_skills |=  static_cast<uint8_t>(1u << k);
                        builds_changed = true;
                    }
                    const bool hovered = ImGui::IsItemHovered();
                    const ImVec2 p_max(pos.x + skill_px, pos.y + skill_px);
                    auto* dl = ImGui::GetWindowDrawList();
                    if (skill_id != GW::Constants::SkillID::No_Skill && skill_tex)
                        dl->AddImage(reinterpret_cast<ImTextureID>(skill_tex), pos, p_max);
                    else
                        dl->AddRectFilled(pos, p_max, IM_COL32(50, 50, 50, 255));
                    if (is_disabled) {
                        dl->AddRectFilled(pos, p_max, IM_COL32(0, 0, 0, 140));
                        if (skill_toggle_sprite && *skill_toggle_sprite) {
                            dl->AddImage(reinterpret_cast<ImTextureID>(*skill_toggle_sprite),
                                pos, p_max, ImVec2(0.0f, 0.5f), ImVec2(0.5f, 1.0f));
                        }
                    }
                    if (hovered) dl->AddRect(pos, p_max, IM_COL32(255, 255, 255, 200), 0.f, 0, 2.f);
                    ImGui::PopID();
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine(offset += icon_btn_width + item_spacing);
        }

        // View / Load buttons (both player and hero rows)
        if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
            if (ImGui::GetIO().KeyCtrl) {
                build.Send();
            }
            else {
                build.View();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ImGui::GetIO().KeyCtrl
                ? "Click to send to team chat"
                : "Click to view build. Ctrl + Click to send to chat.");
        }

        ImGui::SameLine(offset += btn_width + item_spacing);
        if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
            build.Load();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(j == 0 ? "Load Build on Player" : "Load Build on Hero");
        }

        ImGui::SameLine(offset += btn_width + item_spacing);
        if (ImGui::Button(ICON_FA_COPY "###copy", ImVec2(icon_btn_width, 0))) {
            build.Copy();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy build code to clipboard");
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
}

// ------------------------------------------------------------
// DrawEditWindow
// ------------------------------------------------------------

bool TeamBuild::DrawEditWindow(
    size_t index,
    std::vector<TeamBuild>& all_builds,
    bool& builds_changed)
{
    const auto winname = std::format("{}###teambuild_{}", name, ui_id);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
    if (focus_next_frame) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        focus_next_frame = false;
    }

    if (!ImGui::Begin(winname.c_str(), &edit_open)) {
        ImGui::End();
        return true;
    }

    if (has_hero_slots) {
        builds_changed |= ImGui::InputText("Hero Build Name", name);
        builds_changed |= ImGui::InputText("Group", group);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Assign to a group. Builds sharing a group name are shown together under a collapsible header.");
        }
        DrawHeroBuildsContent(builds_changed);
    }
    else {
        ImGui::PushItemWidth(-120.f);
        builds_changed |= ImGui::InputText("Build Name", name);
        ImGui::PopItemWidth();
        DrawPlayerBuildsContent(builds_changed);
    }

    ImGui::Spacing();

    // Teambuild reordering and deletion
    if (ImGui::Button("Up") && index > 0) {
        std::swap(all_builds[index - 1], all_builds[index]);
        builds_changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild up in the list");

    ImGui::SameLine();
    if (ImGui::Button("Down") && index + 1 < all_builds.size()) {
        std::swap(all_builds[index], all_builds[index + 1]);
        builds_changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild down in the list");

    ImGui::SameLine();
    bool deleted = false;
    if (ImGui::ConfirmButton("Delete", &deleted,
            "Delete Teambuild?\n\nAre you sure?\nThis operation cannot be undone.\n\n")) {
        all_builds.erase(all_builds.begin() + static_cast<ptrdiff_t>(index));
        builds_changed = true;
        ImGui::End();
        return false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the teambuild");

    if (has_hero_slots) {
        ImGui::SameLine();
        ImGui::PushItemWidth(110.f);
        constexpr const char* modes[] = {"Don't change", "Normal Mode", "Hard Mode"};
        if (ImGui::Combo("Mode", &mode, modes, 3)) {
            builds_changed = true;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
        if (ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            edit_open = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close this window");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const bool chat_code_too_long = ChatCodeTooLong();
    if (chat_code_too_long) ImGui::BeginDisabled();
    if (ImGui::Button("Send Teambuild code in chat")) {
        this->Send();
    }
    if (chat_code_too_long) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (chat_code_too_long) {
            ImGui::SetTooltip("Teambuild code is too long to send in chat");
        }
        else {
            ImGui::SetTooltip("Send the encoded teambuild link to team chat.\nOther toolbox users can click the chat link without getting spammed.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Teambuild code")) {
        this->Copy();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy the encoded teambuild link to clipboard.\nPaste it anywhere to share your teambuild.");
    }
    ImGui::SameLine();
    if (ImGui::ConfirmButton("Send all builds in chat", &send_all_confirming_,
            "Send All Builds to Chat\n\nThis will send each build as a separate message in team chat.\nAre you sure?")) {
        this->Send(true);
        send_all_confirming_ = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Send all builds as individual skill template links in team chat.\nNon-toolbox users will be able to click builds one by one.");
    }

    ImGui::End();
    return true;
}
