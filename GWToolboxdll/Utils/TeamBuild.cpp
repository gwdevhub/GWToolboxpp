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
#include <GWCA/Managers/PartyMgr.h>
#include <Logger.h>
#include <Modules/Resources.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <Windows/BuildsWindow.h>
#include <Windows/PconsWindow.h>
#include <Windows/RerollWindow.h>
#include "TeamBuildEncoder.h"
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
    std::queue<std::wstring> send_queue;
    std::string pending_reroll_build_code;
    clock_t send_timer = 0;
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
                if (!pcon->IsVisible()) {
                    not_visible.push_back(pcon);
                    continue;
                }
                pcon->SetEnabled(false);
                loaded.push_back(pcon);
            }
            pcon->SetEnabled(enable);
        }
        if (!loaded.empty()) {
            std::string s;
            size_t i = 0;
            for (const auto* p : loaded) {
                if (i++) s += ", ";
                s += p->abbrev;
            }
            Log::Flash("Pcons loaded: %s", s.c_str());
        }
        if (!not_visible.empty()) {
            std::string s;
            size_t i = 0;
            for (const auto* p : not_visible) {
                if (i++) s += ", ";
                s += p->abbrev;
            }
            Log::Warning("Pcons not loaded (not visible in Pcons window): %s", s.c_str());
        }
    }

    bool PendingBuildLoad::Process()
    {
        if (build.IsPlayerBuild()) {
            if (!build.code.empty()) GW::SkillbarMgr::LoadSkillTemplate(build.code.c_str());
            LoadPcons(build);
            return true;
        }

        // Hero build: async — add hero then wait for it to appear in party.
        if (!started) started = TIMER_INIT();
        if (TIMER_DIFF(started) > 1000) return true; // timeout

        switch (stage) {
            case AddHero:
                if (!ToolboxUtils::IsHeroUnlocked(build.hero_id)) return true;
                if (!GW::PartyMgr::AddHero(build.hero_id)) {
                    Log::Warning("Failed to add hero %d", build.hero_id);
                    return true;
                }
                stage = WaitForHero;
                [[fallthrough]];
            case WaitForHero: {
                const GW::HeroPartyMember* hero = GetPartyHeroByID(build.hero_id, &party_hero_index);
                if (!hero) break;
                const GW::HeroFlag* flag = GetHeroFlagInfo(build.hero_id);
                if (!flag) break;
                if (!build.code.empty()) GW::SkillbarMgr::LoadSkillTemplate(hero->agent_id, build.code.c_str());
                for (uint32_t k = 0; k < 8; k++)
                    GW::PartyMgr::SetHeroSkillDisabled(hero->agent_id, k, ((build.disabled_skills >> k) & 1) != 0);
                GW::UI::SendUIMessage(build.show_panel ? GW::UI::UIMessage::kShowHeroPanel : GW::UI::UIMessage::kHideHeroPanel, reinterpret_cast<void*>(static_cast<uintptr_t>(build.hero_id)));
                const auto behavior = static_cast<GW::HeroBehavior>(build.behavior);
                if (behavior <= GW::HeroBehavior::AvoidCombat && flag->hero_behavior != behavior) GW::PartyMgr::SetHeroBehavior(hero->agent_id, behavior);
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
                return _stricmp(Resources::GetHeroName(a)->string().c_str(), Resources::GetHeroName(b)->string().c_str()) < 0;
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

Build::Build(std::string_view n, std::string_view c, GW::Constants::HeroID hero_id_, int show_panel_, uint32_t behavior_, uint8_t disabled_skills_)
    : name(n), code(c), hero_id(hero_id_), behavior(behavior_), show_panel(show_panel_ != 0), disabled_skills(disabled_skills_)
{}

std::string Build::GetFallbackBuildName()
{
    if (code.empty()) return {};
    const auto decoded = Decode();
    if (!decoded) return {};
    for (const auto skill_id : decoded->skills) {
        if (skill_id == GW::Constants::SkillID::No_Skill) continue;
        const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (!skill || !skill->IsElite()) continue;
        return Resources::GetSkillName(skill_id)->string();
    }
    return {};
}

const GW::SkillbarMgr::SkillTemplate* Build::Decode()
{
    if (!decode_attempted_) {
        decode_attempted_ = true;
        GW::SkillbarMgr::DecodeSkillTemplate(skill_template_, code.c_str());
    }
    return IsDecoded() ? &skill_template_ : nullptr;
}

bool Build::IsDecoded() const
{
    return decode_attempted_ && !(skill_template_.primary == GW::Constants::Profession::None && skill_template_.secondary == GW::Constants::Profession::None);
}

void Build::ResetDecodeCache()
{
    decode_attempted_ = false;
    skill_template_.primary = skill_template_.secondary = GW::Constants::Profession::None;
}
const GW::Constants::SkillID* Build::Skills()
{
    return Decode() ? skill_template_.skills : nullptr;
}

void Build::View() const
{
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
std::string Build::DisplayName()
{
    auto gen_name = name;
    if (gen_name.empty()) {
        gen_name = GetFallbackBuildName();
    }
    if (hero_id != GW::Constants::HeroID::NoHero) {
        if (gen_name.empty())
            gen_name = Resources::GetHeroName(hero_id)->string();
        else
            gen_name = std::format("{} ({})", name, Resources::GetHeroName(hero_id)->string());
    }
    return gen_name;
}

void Build::Send()
{
    constexpr size_t kMaxLen = 120;
    const auto gen = DisplayName();

    std::string msg;
    if (code.empty()) {
        msg = gen.substr(0, kMaxLen);
    }
    else {
        // "[{gen};{code}]" — overhead is 3 chars: '[', ';', ']'
        constexpr size_t kOverhead = 3;
        const size_t nameMax = kMaxLen > code.size() + kOverhead ? kMaxLen - code.size() - kOverhead : 0;
        msg = std::format("[{};{}]", gen.substr(0, nameMax), code);
    }

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

void Build::RerollAndLoad(const wchar_t* character_name) const
{
    if (!character_name || code.empty()) return;
    // Reroll to the matching-profession character, returning to the same map and rejoining the same
    // party so the player ends up back with their team after switching characters. Only queue the
    // template to load once the reroll has actually started, so the build is applied to the new
    // character rather than the current one (see Build::Update).
    constexpr bool same_map = true;
    constexpr bool same_party = true;
    if (RerollWindow::Instance().Reroll(character_name, same_map, same_party)) {
        pending_reroll_build_code = code;
    }
}


void Build::Load() const
{
    if (code.empty() && hero_id == GW::Constants::HeroID::NoHero) return;
    pending_build_loads.push_back({*this});
}

void Build::Update()
{
    const auto instance_type = GW::Map::GetInstanceType();

    if (!pending_reroll_build_code.empty() && !RerollWindow::IsRerolling()) {
        // The reroll has ended (completed, declined, or failed). Only apply the queued build if we
        // actually landed on a character whose profession can use it; otherwise discard it, so a
        // cancelled reroll doesn't load the template onto the wrong character.
        if (const GW::AgentLiving* me = GW::Agents::GetControlledCharacter()) {
            GW::SkillbarMgr::SkillTemplate st{};
            const auto player_profession = static_cast<GW::Constants::Profession>(me->primary);
            if (GW::SkillbarMgr::DecodeSkillTemplate(st, pending_reroll_build_code.c_str())
                && st.primary == player_profession) {
                const Build pending("", pending_reroll_build_code);
                pending.Load();
            }
            pending_reroll_build_code.clear();
        }
    }

    if (instance_type == GW::Constants::InstanceType::Loading) {
        pending_build_loads.clear();
        while (!send_queue.empty())
            send_queue.pop();
        kickall_timer = 0;
        return;
    }

    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        if (GW::Agents::GetControlledCharacter()) {
            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
            send_timer = TIMER_INIT();
        }
    }

    if (kickall_timer) {
        if (TIMER_DIFF(kickall_timer) > 500 || instance_type != GW::Constants::InstanceType::Outpost || !GetPlayerHeroCount()) {
            kickall_timer = 0;
        }
        return;
    }

    for (size_t i = 0; i < pending_build_loads.size(); i++) {
        auto& pending = pending_build_loads[i];
        if (!pending.build.IsPlayerBuild() && instance_type != GW::Constants::InstanceType::Outpost) {
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

TeamBuild::TeamBuild(std::string_view n, std::string_view id) : name(n), ui_id(id.empty() ? std::to_string(++s_cur_ui_id) : std::string(id)) {}

TeamBuild::TeamBuild(const TeamBuild& other)
    : edit_open(other.edit_open), focus_next_frame(other.focus_next_frame), mode(other.mode), show_numbers(other.show_numbers), has_hero_slots(other.has_hero_slots), name(other.name), group(other.group), ui_id(std::to_string(++s_cur_ui_id)),
      builds(other.builds)
{}

TeamBuild& TeamBuild::operator=(const TeamBuild& other)
{
    if (this != &other) {
        edit_open = other.edit_open;
        focus_next_frame = other.focus_next_frame;
        mode = other.mode;
        show_numbers = other.show_numbers;
        has_hero_slots = other.has_hero_slots;
        name = other.name;
        group = other.group;
        ui_id = std::to_string(++s_cur_ui_id);
        builds = other.builds;
    }
    return *this;
}

void TeamBuild::SetSkillToggleSprite(IDirect3DTexture9** sprite)
{
    skill_toggle_sprite = sprite;
}

const std::wstring& TeamBuild::GetEncoded() const
{
    if (!encoded_cache_.has_value()) encoded_cache_ = TeamBuildEncoder::TeamBuildToEncoded(*this);
    return *encoded_cache_;
}

void TeamBuild::ResetEncodedCache() const
{
    encoded_cache_.reset();
}

void TeamBuild::Send(bool one_by_one)
{
    if (!name.empty()) Build::EnqueueSend(name);
    if (one_by_one) {
        for (auto& build : builds)
            build.Send();
    }
    else {
        const auto& encoded = GetEncoded();
        if (!encoded.empty()) send_queue.push(std::format(L"[TB;{}]", encoded));
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
TeamBuild TeamBuild::Duplicate()
{
    TeamBuild copy = *this;
    copy.name += " (Copy)";
    return std::move(copy);
}

void TeamBuild::DrawTooltip()
{
    for (auto& build : builds) {
        const bool has_hero = build.hero_id != GW::Constants::HeroID::NoHero;
        const bool has_name = !build.name.empty();
        const auto decoded = build.Decode();

        if (has_hero_slots) {
            if (!has_hero && !has_name && !decoded) continue;
        }
        else {
            if (!has_name && !decoded) continue;
        }

        ImGui::Spacing();

        if (has_hero_slots) {
            const auto hero_name = has_hero ? Resources::GetHeroName(build.hero_id)->string() : std::string("Player");
            const auto display_name = has_name ? build.name : build.GetFallbackBuildName();
            const auto full_name = std::format("{} ({})", display_name, hero_name);
            ImGui::TextUnformatted(full_name.c_str());
        }
        else {
            ImGui::TextUnformatted(build.name.c_str());
        }

        GW::SkillbarMgr::SkillTemplate st{};
        if (decoded) {
            GuiUtils::DrawSkillbar(decoded, false);
        }
        else {
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "No Build Defined");
        }

        ImGui::Spacing();
    }
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
        if (!GW::PartyMgr::KickAllHeroes()) {
            Log::Warning("Failed to kick all heroes");
            return;
        }
        kickall_timer = TIMER_INIT();
    }
    if (mode > 0) {
        GW::PartyMgr::SetHardMode(mode == 2) || (Log::Warning("Failed to set hard mode"), true);
    }
    for (const auto& build : builds) {
        build.Load();
    }
}

// ------------------------------------------------------------
// Player-builds layout (BuildsWindow style)
// ------------------------------------------------------------
void TeamBuild::DrawPlayerBuildsContent(bool& builds_changed, bool editable)
{
    const float font_scale = ImGui::FontScale();
    const auto row_height = ImGui::CalcTextSize(" ").y * 2.f;
    const auto icon_btn_size = ImVec2(row_height, row_height);
    const float spacing = 4.f * font_scale;
    const auto min_row_width = row_height * 15.f;
    // 4 visible icon buttons when editable: chat, load/reroll, edit, dropdown
    // 3 when read-only: chat, load/reroll, dropdown (no edit toggle)
    const size_t icon_btns = editable ? 4 : 3;

    const auto* me = GW::Agents::GetControlledCharacter();
    const auto player_profession = me ? static_cast<GW::Constants::Profession>(me->primary) : GW::Constants::Profession::None;

    bool tmp = builds_changed;
    builds_changed = false;

    for (size_t j = 0; j < builds.size(); j++) {
        Build& build = builds[j];
        ImGui::PushID(static_cast<int>(j));

        const bool editing = editable && editing_build_idx_ == static_cast<int>(j);

        // ---- Row: number + name (editable in edit mode) + icon buttons ----
        ImGui::Text("#%zu", j + 1);
        ImGui::SameLine(0);
        ImGui::Indent();

        const auto btns_start = min_row_width + ImGui::GetIndent() - (icon_btns * (icon_btn_size.x + spacing));
        const float name_width = btns_start - spacing - ImGui::GetIndent();

        if (editing) {
            ImGui::PushItemWidth(name_width);
            ImGui::InputText("###name", build.name, 128);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Build name/label");
            }

            ImGui::PushItemWidth(name_width);
            if (ImGui::InputText("###code", build.code, 128)) {
                build.ResetDecodeCache();
                ResetEncodedCache();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([&build]() {
                    ImGui::TextUnformatted("Build code");
                    if (const auto decoded = build.Decode()) {
                        ImGui::Spacing();
                        GuiUtils::DrawSkillbar(build.Decode());
                    }
                });
            }
        }
        else {
            // Static display — click to show skillbar tooltip
            ImGui::PushItemWidth(name_width);
            const auto& disp = !build.name.empty() ? build.name : build.GetFallbackBuildName();
            ImGui::TextUnformatted(disp.c_str());
            if (ImGui::IsItemHovered() && !build.code.empty()) {
                ImGui::SetTooltip([&build]() {
                    GuiUtils::DrawSkillbar(build.Decode(), false);
                });
            }
            ImGui::PopItemWidth();

            if (const auto decoded = build.Decode()) {
                GuiUtils::DrawSkillbar(decoded, false);
            }
            else {
                const auto width = (row_height * 8) + row_height / 2.f;
                ImGui::Dummy({width, row_height});
            }
        }


        // --- Send to chat ---
        ImGui::SameLine(btns_start);
        if (GuiUtils::IconButton("##chat", GuiUtils::GwButtonIcon::ChatIcon, icon_btn_size)) build.Send();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send to team chat");

        // --- Load / Reroll ---
        ImGui::SameLine(0, spacing);
        {
            const auto skillbar = build.Decode();
            bool can_load = false;
            const wchar_t* reroll_to = 0;
            GW::Constants::Profession build_prof = GW::Constants::Profession::None;
            if (!skillbar) {
                ImGui::Dummy(icon_btn_size);
            }
            else {
                if (build.IsPlayerBuild()) {
                    build_prof = skillbar->primary;
                    can_load = player_profession != GW::Constants::Profession::None && build_prof == player_profession;
                    if (!can_load && build_prof != GW::Constants::Profession::None) reroll_to = RerollWindow::FindAvailableCharForProfession(build_prof);
                }
                else {
                    can_load = ToolboxUtils::IsHeroUnlocked(build.hero_id);
                }
                if (can_load) {
                    if (GuiUtils::IconButton("##load", GuiUtils::GwButtonIcon::LoadFromTemplate, icon_btn_size)) build.Load();
                }
                else if (reroll_to) {
                    if (GuiUtils::IconButtonConfirm("##reroll", GuiUtils::GwButtonIcon::ManageTemplates, icon_btn_size)) build.RerollAndLoad(reroll_to);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(std::format("Reroll to {} and load build", TextUtils::WStringToString(reroll_to)).c_str());
                }
                else {
                    ImGui::Dummy(icon_btn_size);
                }
            }
        }

        // --- Edit toggle ---
        ImGui::SameLine(0, spacing);
        if (editable) {
            if (editing) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(ICON_FA_EDIT "##edit", icon_btn_size)) editing_build_idx_ = editing ? -1 : static_cast<int>(j);
            if (editing) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(editing ? "Stop editing" : "Edit build");
        }

        // --- Dropdown (view, copy, delete) ---
        ImGui::SameLine(0, spacing);
        if (ImGui::Button(ICON_FA_ELLIPSIS_V, icon_btn_size)) ImGui::OpenPopup("##build_menu");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("More options");

        if (ImGui::BeginPopup("##build_menu")) {
            if (ImGui::MenuItem(ICON_FA_EYE "  View build")) build.View();
            if (ImGui::MenuItem(ICON_FA_COPY "  Copy build code")) build.Copy();
            if (editable) {
                ImGui::Separator();
                bool delete_confirmed = false;
                if (ImGui::ConfirmButton(ICON_FA_TRASH "  Delete build", &delete_confirmed, "Delete Build\n\nAre you sure?\nThis operation cannot be undone.")) {
                    if (editing_build_idx_ == static_cast<int>(j))
                        editing_build_idx_ = -1;
                    else if (editing_build_idx_ > static_cast<int>(j))
                        editing_build_idx_--;
                    builds.erase(builds.begin() + static_cast<ptrdiff_t>(j));
                    ResetEncodedCache();
                    builds_changed = true;
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::EndPopup();
        }

        // ---- Expanded edit panel (below the row) ----
        if (editing) {
            ImGui::TextUnformatted("Pcons:");
            ImGui::ShowHelp("Enable or disable pcons when this build is loaded");
            const auto& pcons = PconsWindow::Instance().pcons;
            const float skill_h = ImGui::CalcTextSize(" ").y * 2.f;
            ImGui::StartSpacedElements(skill_h + ImGui::GetStyle().ItemSpacing.x);
            size_t i = 0;
            for (const auto pcon : pcons) {
                ImGui::PushID(i++);
                const bool active = build.pcons.contains(pcon->ini);
                ImGui::NextSpacedElement();
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, build_edit_pcon_enabled_color);
                if (ImGui::IconButton("", *pcon->GetTexture(), {skill_h, skill_h})) {
                    if (!active)
                        build.pcons.emplace(pcon->ini);
                    else
                        build.pcons.erase(pcon->ini);
                }
                if (active) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(pcon->chat.c_str());
                ImGui::PopID();
            }
        }
        ImGui::Unindent();
        ImGui::PopID();
        if (builds_changed) break;
    }

    builds_changed |= tmp;

    ImGui::Spacing();

    if (editable) {
        if (ImGui::Checkbox("Show numbers", &show_numbers)) builds_changed = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Prefix build names with their index when sending to chat");

        ImGui::SameLine();
        const float add_btn_width = 140.f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - add_btn_width);
        if (ImGui::Button("Add Build", ImVec2(add_btn_width, 0))) {
            builds.emplace_back("", "");
            ResetEncodedCache();
            editing_build_idx_ = static_cast<int>(builds.size()) - 1;
            builds_changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add another build row");
    }
}
// ------------------------------------------------------------
// Hero-builds layout (HeroBuildsWindow style)
// ------------------------------------------------------------
void TeamBuild::DrawHeroBuildsContent(bool& builds_changed, bool editable)
{
    const float font_scale = ImGui::FontScale();
    const auto row_height = ImGui::CalcTextSize(" ").y * 2.f;
    const auto icon_btn_size = ImVec2(row_height, row_height);
    const float spacing = 4.f * font_scale;
    const auto min_row_width = row_height * 15.f;
    // 4 visible icon buttons when editable: send, load, edit, dropdown
    // 3 when read-only: send, load, dropdown (no edit toggle)
    const size_t icon_btns = editable ? 4 : 3;

    const auto* me = GW::Agents::GetControlledCharacter();
    const auto player_profession = me ? static_cast<GW::Constants::Profession>(me->primary) : GW::Constants::Profession::None;

    bool tmp = builds_changed;
    builds_changed = false;

    size_t player_idx = builds.size();
    for (size_t j = 0; j < builds.size(); ++j) {
        if (builds[j].hero_id == GW::Constants::HeroID::NoHero) {
            player_idx = j;
            break;
        }
    }

    uint32_t hero_count = 1;

    for (size_t j = 0; j < builds.size(); j++) {
        Build& build = builds[j];
        ImGui::PushID(static_cast<int>(j));

        const bool editing = editable && editing_build_idx_ == static_cast<int>(j);
        const bool is_player = j == player_idx;

        // ---- Row label ----
        if (is_player)
            ImGui::Text("P");
        else
            ImGui::Text("#%u", hero_count++);

        ImGui::SameLine(0);
        ImGui::Indent();

        const auto btns_start = min_row_width + ImGui::GetIndent() - (icon_btns * (icon_btn_size.x + spacing));
        const float name_width = btns_start - spacing - ImGui::GetIndent();

        // ---- Name + code (editable in edit mode) ----
        if (editing) {
            ImGui::PushItemWidth(name_width);
            ImGui::InputText("###name", build.name, 128);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Build name/label");

            ImGui::PushItemWidth(name_width);
            if (ImGui::InputText("###code", build.code, 128)) {
                build.ResetDecodeCache();
                ResetEncodedCache();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([&build]() {
                    ImGui::TextUnformatted("Build code");
                    if (const auto decoded = build.Decode()) {
                        ImGui::Spacing();
                        GuiUtils::DrawSkillbar(decoded);
                    }
                });
            }
        }
        else {
            ImGui::TextUnformatted(build.DisplayName().c_str());
            if (ImGui::IsItemHovered() && !build.code.empty()) {
                ImGui::SetTooltip([&build]() {
                    GuiUtils::DrawSkillbar(build.Decode(), false);
                });
            }

            if (const auto decoded = build.Decode()) {
                GuiUtils::DrawSkillbar(decoded, false);
            }
            else {
                const float width = (row_height * 8) + row_height / 2.f;
                ImGui::Dummy({width, row_height});
            }
        }

        // --- Send to chat ---
        ImGui::SameLine(btns_start);
        if (GuiUtils::IconButton("##chat", GuiUtils::GwButtonIcon::ChatIcon, icon_btn_size)) build.Send();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send to team chat");

        // --- Load / Reroll ---
        ImGui::SameLine(0, spacing);
        {
            const auto skillbar = build.Decode();
            bool can_load = false;
            const wchar_t* reroll_to = nullptr;
            GW::Constants::Profession build_prof = GW::Constants::Profession::None;

            if (!skillbar) {
                ImGui::Dummy(icon_btn_size);
            }
            else if (is_player) {
                build_prof = skillbar->primary;
                can_load = player_profession != GW::Constants::Profession::None && build_prof == player_profession;
                if (!can_load && build_prof != GW::Constants::Profession::None) reroll_to = RerollWindow::FindAvailableCharForProfession(build_prof);

                if (can_load) {
                    if (GuiUtils::IconButton("##load", GuiUtils::GwButtonIcon::LoadFromTemplate, icon_btn_size)) build.Load();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load build");
                }
                else if (reroll_to) {
                    if (GuiUtils::IconButtonConfirm("##reroll", GuiUtils::GwButtonIcon::ManageTemplates, icon_btn_size)) build.RerollAndLoad(reroll_to);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(std::format("Reroll to {} and load build", TextUtils::WStringToString(reroll_to)).c_str());
                }
                else {
                    ImGui::Dummy(icon_btn_size);
                }
            }
            else {
                can_load = ToolboxUtils::IsHeroUnlocked(build.hero_id);
                if (can_load) {
                    if (GuiUtils::IconButton("Load##load", GuiUtils::GwButtonIcon::LoadFromTemplate, icon_btn_size)) build.Load();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load build on hero");
                }
                else {
                    ImGui::Dummy(icon_btn_size);
                }
            }
        }

        // --- Edit toggle ---
        ImGui::SameLine(0, spacing);
        if (editable) {
            if (editing) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(ICON_FA_EDIT "##edit", icon_btn_size)) editing_build_idx_ = editing ? -1 : static_cast<int>(j);
            if (editing) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(editing ? "Stop editing" : "Edit build");
        }

        // --- Dropdown (view, copy, move, delete) ---
        ImGui::SameLine(0, spacing);
        if (ImGui::Button(ICON_FA_ELLIPSIS_V, icon_btn_size)) ImGui::OpenPopup("##build_menu");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("More options");

        if (ImGui::BeginPopup("##build_menu")) {
            if (ImGui::MenuItem(ICON_FA_EYE "  View build")) build.View();
            if (ImGui::MenuItem(ICON_FA_COPY "  Copy build code")) build.Copy();
            if (editable) {

                if (!is_player) {
                    const bool prev_is_player = j > 0 && j - 1 == player_idx;
                    const bool next_is_player = j + 1 < builds.size() && j + 1 == player_idx;
                    const bool can_move_up = j > 0 && !prev_is_player;
                    const bool can_move_down = j + 1 < builds.size() && !next_is_player;
                    ImGui::Separator();
                    if (!can_move_up) ImGui::BeginDisabled();
                    if (ImGui::MenuItem(ICON_FA_ARROW_UP "  Move up")) {
                        std::swap(builds[j - 1], builds[j]);
                        ResetEncodedCache();
                        builds_changed = true;
                        ImGui::EndPopup();
                        ImGui::Unindent();
                        ImGui::PopID();
                        break;
                    }
                    if (!can_move_up) ImGui::EndDisabled();
                    if (!can_move_down) ImGui::BeginDisabled();
                    if (ImGui::MenuItem(ICON_FA_ARROW_DOWN "  Move down")) {
                        std::swap(builds[j], builds[j + 1]);
                        ResetEncodedCache();
                        builds_changed = true;
                        ImGui::EndPopup();
                        ImGui::Unindent();
                        ImGui::PopID();
                        break;
                    }
                    if (!can_move_down) ImGui::EndDisabled();
                }

                ImGui::Separator();
                bool delete_confirmed = false;
                if (ImGui::ConfirmButton(ICON_FA_TRASH "  Delete build", &delete_confirmed, "Delete Build\n\nAre you sure?\nThis operation cannot be undone.")) {
                    if (editing_build_idx_ == static_cast<int>(j))
                        editing_build_idx_ = -1;
                    else if (editing_build_idx_ > static_cast<int>(j))
                        editing_build_idx_--;
                    builds.erase(builds.begin() + static_cast<ptrdiff_t>(j));
                    ResetEncodedCache();
                    builds_changed = true;
                    ImGui::EndPopup();
                    ImGui::Unindent();
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::EndPopup();
        }

        // ---- Expanded edit section ----
        if (editing) {
            if (!is_player) {
                // Hero selector
                const auto& sorted_heroes = SortedHeroIDs();
                const auto hero_it = std::ranges::find(sorted_heroes, build.hero_id);
                int combo_idx = hero_it != sorted_heroes.end() ? static_cast<int>(std::distance(sorted_heroes.begin(), hero_it)) : -1;
                ImGui::PushItemWidth(name_width);
                if (ImGui::MyCombo(
                        "###heroid", "Choose Hero", &combo_idx,
                        [](void*, const int idx, const char** out_text) -> bool {
                            const auto& heroes = SortedHeroIDs();
                            if (idx < 0 || idx >= static_cast<int>(heroes.size())) return false;
                            *out_text = Resources::GetHeroName(heroes[idx])->string().c_str();
                            return true;
                        },
                        nullptr, static_cast<int>(sorted_heroes.size())
                    )) {
                    build.hero_id = (combo_idx >= 0 && combo_idx < static_cast<int>(sorted_heroes.size())) ? sorted_heroes[combo_idx] : HeroID::NoHero;
                    ResetEncodedCache();
                    builds_changed = true;
                }
                ImGui::PopItemWidth();

                // Show panel toggle
                ImGui::SameLine(0, spacing);
                const auto* panel_icon = reinterpret_cast<const char*>(build.show_panel ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
                if (ImGui::Button(panel_icon, icon_btn_size)) {
                    build.show_panel = !build.show_panel;
                    builds_changed = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(build.show_panel ? "Hero panel: Show" : "Hero panel: Hide");

                // Behavior toggle
                ImGui::SameLine(0, spacing);
                const char* behavior_icon = reinterpret_cast<const char*>(ICON_FA_SHIELD_ALT);
                const char* behavior_tooltip = "Hero behaviour: Guard";
                switch (build.behavior) {
                    case 0:
                        behavior_icon = reinterpret_cast<const char*>(ICON_FA_FIST_RAISED);
                        behavior_tooltip = "Hero behaviour: Fight";
                        break;
                    case 2:
                        behavior_icon = reinterpret_cast<const char*>(ICON_FA_DOVE);
                        behavior_tooltip = "Hero behaviour: Avoid Combat";
                        break;
                }
                if (ImGui::Button(behavior_icon, icon_btn_size)) {
                    if (++build.behavior > 2) build.behavior = 0;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(behavior_tooltip);

                // Disabled skills
                ImGui::SameLine(0, spacing);
                int enabled_count = 8;
                for (int k = 0; k < 8; k++)
                    if ((build.disabled_skills >> k) & 1) enabled_count--;
                char skills_label[8];
                snprintf(skills_label, sizeof(skills_label), "%d/8", enabled_count);
                if (ImGui::Button(skills_label, icon_btn_size)) ImGui::OpenPopup("SkillsPopup");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle which skills are disabled when loading");

                if (ImGui::BeginPopup("SkillsPopup")) {
                    const auto decoded = build.Decode();
                    constexpr float skill_px = 48.0f;
                    for (int k = 0; k < 8; k++) {
                        if (k > 0) ImGui::SameLine(0, 0);
                        const bool is_disabled = (build.disabled_skills >> k) & 1;
                        const auto skill_id = decoded ? decoded->skills[k] : GW::Constants::SkillID::No_Skill;
                        auto* skill_tex = *Resources::GetSkillImage(skill_id);
                        ImGui::PushID(k);
                        const ImVec2 pos = ImGui::GetCursorScreenPos();
                        if (ImGui::InvisibleButton("##skill_slot", ImVec2(skill_px, skill_px))) {
                            if (is_disabled)
                                build.disabled_skills &= static_cast<uint8_t>(~(1u << k));
                            else
                                build.disabled_skills |= static_cast<uint8_t>(1u << k);
                        }
                        const ImVec2 p_max(pos.x + skill_px, pos.y + skill_px);
                        auto* dl = ImGui::GetWindowDrawList();
                        if (skill_id != GW::Constants::SkillID::No_Skill && skill_tex)
                            dl->AddImage(reinterpret_cast<ImTextureID>(skill_tex), pos, p_max);
                        else
                            dl->AddRectFilled(pos, p_max, IM_COL32(50, 50, 50, 255));
                        if (is_disabled) {
                            dl->AddRectFilled(pos, p_max, IM_COL32(0, 0, 0, 140));
                            if (skill_toggle_sprite && *skill_toggle_sprite) dl->AddImage(reinterpret_cast<ImTextureID>(*skill_toggle_sprite), pos, p_max, ImVec2(0.f, 0.5f), ImVec2(0.5f, 1.f));
                        }
                        if (ImGui::IsItemHovered()) dl->AddRect(pos, p_max, IM_COL32(255, 255, 255, 200), 0.f, 0, 2.f);
                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }
            }

            // Pcons (player slot only)
            if (is_player) {
                ImGui::TextUnformatted("Pcons:");
                ImGui::ShowHelp("Enable or disable pcons when this build is loaded");
                const auto& pcons = PconsWindow::Instance().pcons;
                const float skill_h = ImGui::CalcTextSize(" ").y * 2.f;
                ImGui::StartSpacedElements(skill_h + ImGui::GetStyle().ItemSpacing.x);
                size_t i = 0;
                for (const auto pcon : pcons) {
                    ImGui::PushID(i++);
                    const bool active = build.pcons.contains(pcon->ini);
                    ImGui::NextSpacedElement();
                    if (active) ImGui::PushStyleColor(ImGuiCol_Button, build_edit_pcon_enabled_color);
                    if (ImGui::IconButton("", *pcon->GetTexture(), {skill_h, skill_h})) {
                        if (!active)
                            build.pcons.emplace(pcon->ini);
                        else
                            build.pcons.erase(pcon->ini);
                    }
                    if (active) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(pcon->chat.c_str());
                    ImGui::PopID();
                }
            }
        }

        ImGui::Unindent();
        if (editing) ImGui::Spacing();
        ImGui::PopID();
        if (builds_changed) break;
    }

    builds_changed |= tmp;

    ImGui::Spacing();
}

// ------------------------------------------------------------
// DrawEditWindow
// ------------------------------------------------------------

bool TeamBuild::DrawEditWindow(size_t index, std::vector<TeamBuild>& all_builds, bool& builds_changed)
{
    const auto winname = std::format("{}###teambuild_{}", name, ui_id);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
    if (focus_next_frame) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowCollapsed(false);
        focus_next_frame = false;
    }

    if (!ImGui::Begin(winname.c_str(), &edit_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return true;
    }

    if (has_hero_slots) {
        builds_changed |= ImGui::InputText("Hero Build Name", name, 128);
        builds_changed |= ImGui::InputText("Group", group, 128);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Assign to a group. Builds sharing a group name are shown together under a collapsible header.");
        }
        DrawHeroBuildsContent(builds_changed);
    }
    else {
        ImGui::PushItemWidth(-120.f);
        builds_changed |= ImGui::InputText("Build Name", name, 128);
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
    if (ImGui::Button("Duplicate")) {
        auto cpy = Duplicate();
        cpy.has_hero_slots = has_hero_slots;
        cpy.edit_open = true;
        edit_open = false;
        all_builds.push_back(std::move(cpy));
        builds_changed = true;
        ImGui::End();
        return false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Duplicate Teambuild");
    }
    bool deleted = false;
    if (ImGui::ConfirmButton("Delete", &deleted, "Delete Teambuild?\n\nAre you sure?\nThis operation cannot be undone.\n\n")) {
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
    if (ImGui::ConfirmButton("Send all builds in chat", &send_all_confirming_, "Send All Builds to Chat\n\nThis will send each build as a separate message in team chat.\nAre you sure?")) {
        this->Send(true);
        send_all_confirming_ = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Send all builds as individual skill template links in team chat.\nNon-toolbox users will be able to click builds one by one.");
    }

    ImGui::End();
    return true;
}

// ------------------------------------------------------------
// DrawDetachedWindow
// ------------------------------------------------------------
void TeamBuild::DrawDetachedWindow(std::vector<TeamBuild>& hero_builds, bool& builds_changed)
{
    if (!edit_open) return;
    const auto winname = std::format("{}###detached_{}", name, ui_id);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    if (focus_next_frame) {
        ImGui::SetNextWindowCollapsed(false);
        ImGui::SetNextWindowFocus();
        focus_next_frame = false;
    }
    if (!ImGui::Begin(winname.c_str(), &edit_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (has_hero_slots)
        DrawHeroBuildsContent(builds_changed, false);
    else
        DrawPlayerBuildsContent(builds_changed, false);

    // ---- Bottom buttons (detached-specific) ----
    if (has_hero_slots) {
        if (ImGui::Button("Load All")) Load();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load all builds onto your heroes");
        ImGui::SameLine();
    }
    if (ImGui::Button("Add to My Builds")) {
        TeamBuild copy = *this;
        copy.edit_open = false;
        if (copy.has_hero_slots) {
            hero_builds.push_back(std::move(copy));
            builds_changed = true;
        }
        else {
            BuildsWindow::Instance().AddTeambuild(std::move(copy));
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(has_hero_slots ? "Save this teambuild to your Hero Builds list" : "Save this teambuild to your Builds list");

    ImGui::SameLine();
    if (ImGui::Button("Close")) edit_open = false;

    ImGui::End();
}
