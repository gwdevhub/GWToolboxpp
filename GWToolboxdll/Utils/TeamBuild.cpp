#include "stdafx.h"

#include <Utils/TeamBuild.h>

#include <GWCA/Constants/Skills.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <Windows/PconsWindow.h>

namespace {

    IDirect3DTexture9** skill_toggle_sprite = nullptr;

    const Color build_edit_pcon_enabled_color = Colors::ARGB(102, 0, 255, 0);

    using GW::Constants::HeroID;

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

// ------------------------------------------------------------
// Player-builds layout (BuildsWindow style)
// ------------------------------------------------------------

void TeamBuild::DrawPlayerBuildsContent(
    bool& builds_changed,
    const BuildAction& on_load,
    const BuildAction& on_send,
    const BuildAction& on_view)
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
        const float btn_offset = ImGui::GetContentRegionAvail().x - del_width - btn_width * 3 - spacing * 3;

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
                if (on_send) on_send(*this, j);
            }
            else {
                if (on_view) on_view(*this, j);
                else         DefaultView(build);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ImGui::GetIO().KeyCtrl
                ? "Click to send to team chat"
                : "Click to view build. Ctrl + Click to send to chat.");
        }

        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
            if (on_load) on_load(*this, j);
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
        bool delete_confirmed = false;
        if (ImGui::ConfirmButton(ICON_FA_TRASH, &delete_confirmed,
                "Delete Build\n\nAre you sure you want to delete this build?\nThis operation cannot be undone.")) {
            if (editing_build_idx_ == static_cast<int>(j))       editing_build_idx_ = -1;
            else if (editing_build_idx_ > static_cast<int>(j))   editing_build_idx_--;
            builds.erase(builds.begin() + static_cast<ptrdiff_t>(j));
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
    bool& builds_changed,
    const BuildAction& on_load,
    const BuildAction& on_send,
    const BuildAction& on_view)
{
    const float btn_width      = 50.0f * ImGui::FontScale();
    const float icon_btn_width = btn_width / 1.75f;
    const float panel_width    = btn_width + 12.0f;
    const float item_spacing   = ImGui::GetStyle().ItemInnerSpacing.x;
    const float text_item_width =
        (ImGui::GetContentRegionAvail().x - btn_width - btn_width - btn_width
         - panel_width - icon_btn_width - item_spacing * 4) / 3.f;

    float offset = btn_width;
    ImGui::SetCursorPosX(offset);
    ImGui::Text("Name");
    ImGui::SameLine(offset += text_item_width + item_spacing);
    ImGui::Text("Template");

    for (size_t j = 0; j < builds.size(); ++j) {
        offset = btn_width;
        Build& build = builds[j];
        ImGui::PushID(static_cast<int>(j));

        if (j == 0) ImGui::Text("P");
        else        ImGui::Text("H#%zu", j);

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
            builds_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip([&build]() {
                GuiUtils::DrawSkillbar(build.code.c_str());
            });
        }

        ImGui::SameLine(offset += text_item_width + item_spacing);

        if (j == 0) {
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
                if (on_send) on_send(*this, j);
            }
            else {
                if (on_view) on_view(*this, j);
                else         DefaultView(build);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ImGui::GetIO().KeyCtrl
                ? "Click to send to team chat"
                : "Click to view build. Ctrl + Click to send to chat.");
        }

        ImGui::SameLine(offset += btn_width + item_spacing);
        if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
            if (on_load) on_load(*this, j);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(j == 0 ? "Load Build on Player" : "Load Build on Hero");
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
    bool& builds_changed,
    const BuildAction& on_load,
    const BuildAction& on_send,
    const BuildAction& on_view)
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
        DrawHeroBuildsContent(builds_changed, on_load, on_send, on_view);
    }
    else {
        ImGui::PushItemWidth(-120.f);
        builds_changed |= ImGui::InputText("Build Name", name);
        ImGui::PopItemWidth();
        DrawPlayerBuildsContent(builds_changed, on_load, on_send, on_view);
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

        // "Send to chat" is a teambuild-level action that requires encoding.
        // Call on_send with SIZE_MAX as a sentinel to indicate the whole teambuild.
        if (on_send) {
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Send to chat")) {
                on_send(*this, SIZE_MAX);
            }
        }
    }

    ImGui::End();
    return true;
}
