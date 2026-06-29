#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/Minimap.h>

#include <ImGuiAddons.h>

namespace {
    constexpr float BTN_WIDTH = 20.0f;
}

void RangeRenderer::LoadDefaultCircles()
{
    circles_ = {
        {"Compass",         GW::Constants::Range::Compass,        1.f, 0xFF666611, true,  false},
        {"Spirit Extended", GW::Constants::Range::SpiritExtended, 1.f, 0xFF337733, true,  false},
        {"Spirit",          GW::Constants::Range::Spirit,         1.f, 0xFF337733, true,  false},
        {"Cast",            GW::Constants::Range::Spellcast,      1.f, 0xFF117777, true,  false},
        {"Aggro",           GW::Constants::Range::Earshot,        1.f, 0xFF994444, true,  false},
    };
}

void RangeRenderer::LoadDefaults()
{
    LoadDefaultCircles();
    color_range_hos = 0xFF881188;
    color_range_chain_aggro = 0x00994444;
    color_range_res_aggro = 0x64D6D6D6;
    color_range_shadowstep_aggro = Colors::ARGB(200, 128, 0, 128);
    Invalidate();
}

void RangeRenderer::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "color_range_hos", reinterpret_cast<Colors::SettingColor*>(&color_range_hos));
    SettingsRegistry::RegisterField(module, "color_range_chain_aggro", reinterpret_cast<Colors::SettingColor*>(&color_range_chain_aggro));
    SettingsRegistry::RegisterField(module, "color_range_res_aggro", reinterpret_cast<Colors::SettingColor*>(&color_range_res_aggro));
    SettingsRegistry::RegisterField(module, "color_range_shadowstep_aggro", reinterpret_cast<Colors::SettingColor*>(&color_range_shadowstep_aggro));
}

void RangeRenderer::LoadSettings(const SettingsDoc& doc, const ToolboxIni* ini, const char* section)
{
    LoadDefaultCircles();

    if (!doc.Get(section, "range_circles", circles_) && ini) {
        const bool has_new_format = ini->GetValue(section, "range_circle_count", nullptr) != nullptr;
        if (has_new_format) {
            const auto range_circle_count = ini->GetLongValue(section, "range_circle_count", 0);
            circles_.clear();
            for (long i = 0; i < range_circle_count; i++) {
                char key[64];
                RangeCircle c;
                snprintf(key, sizeof(key), "range_circle_%ld_label", i);
                c.label = ini->GetValue(section, key, "");
                snprintf(key, sizeof(key), "range_circle_%ld_radius", i);
                c.radius = static_cast<float>(ini->GetDoubleValue(section, key, c.radius));
                snprintf(key, sizeof(key), "range_circle_%ld_thickness", i);
                c.line_thickness = static_cast<float>(ini->GetDoubleValue(section, key, c.line_thickness));
                snprintf(key, sizeof(key), "range_circle_%ld_color", i);
                c.color = Colors::Load(ini, section, key, c.color);
                snprintf(key, sizeof(key), "range_circle_%ld_visible", i);
                c.visible = ini->GetBoolValue(section, key, c.visible);
                snprintf(key, sizeof(key), "range_circle_%ld_on_target", i);
                c.on_target = ini->GetBoolValue(section, key, c.on_target);
                circles_.push_back(c);
            }
        }
        else {
            // Migrate colors from the old per-name format
            if (circles_.size() >= 5) {
                circles_[0].color = Colors::Load(ini, section, "color_range_compass", circles_[0].color);
                circles_[1].color = Colors::Load(ini, section, "color_range_spirit_extended", circles_[1].color);
                circles_[2].color = Colors::Load(ini, section, "color_range_spirit", circles_[2].color);
                circles_[3].color = Colors::Load(ini, section, "color_range_cast", circles_[3].color);
                circles_[4].color = Colors::Load(ini, section, "color_range_aggro", circles_[4].color);
                const auto old_thickness = static_cast<float>(ini->GetDoubleValue(section, "range_line_thickness", 1.0));
                for (auto& c : circles_) {
                    c.line_thickness = old_thickness;
                }
            }
        }
    }

    Invalidate();
}

void RangeRenderer::SaveSettings(SettingsDoc& doc, const char* section) const
{
    doc.Set(section, "range_circles", circles_);
}

void RangeRenderer::DrawSettings()
{
    bool changed = false;

    ImGui::SmallConfirmButton("Restore Defaults##ranges", "Are you sure?", [&](const bool result, void*) {
        if (result) {
            LoadDefaults();
        }
    });

    ImGui::TextDisabled("Radius in gwinches: Aggro=1012, Cast=1248, Spirit=2512, Extended=3500, Compass=5000");
    ImGui::Separator();

    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImGui::PushID("range_circles");
    for (size_t i = 0; i < circles_.size(); i++) {
        auto& c = circles_[i];
        bool row_changed = false;
        ImGui::PushID(static_cast<int>(i));

        row_changed |= ImGui::Checkbox("##visible", &c.visible);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Visible");
        }
        ImGui::SameLine(0.f, spacing);

        ImGui::PushItemWidth(120.f);
        row_changed |= ImGui::InputText("##label", c.label, 127);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Label");
        }
        ImGui::SameLine(0.f, spacing);

        ImGui::PushItemWidth(70.f);
        row_changed |= ImGui::DragFloat("##radius", &c.radius, 10.f, 1.f, 10000.f, "%.0f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Radius (gwinches)");
        }
        ImGui::SameLine(0.f, spacing);

        ImGui::PushItemWidth(60.f);
        constexpr const char* origins[] = {"Player", "Target"};
        int origin_idx = c.on_target ? 1 : 0;
        if (ImGui::Combo("##origin", &origin_idx, origins, 2)) {
            c.on_target = origin_idx == 1;
            row_changed = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Circle origin: centered on Player or current Target");
        }
        ImGui::SameLine(0.f, spacing);

        ImGui::PushItemWidth(50.f);
        row_changed |= ImGui::DragFloat("##thick", &c.line_thickness, 0.1f, 0.1f, 20.f, "%.1fpx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Line thickness (pixels)");
        }
        ImGui::SameLine(0.f, spacing);

        row_changed |= ImGui::ColorButtonPicker("##color", &c.color.value);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Circle color");
        }
        ImGui::SameLine(0.f, spacing);

        if (i > 0) {
            const bool move_up = ImGui::Button(ICON_FA_ARROW_UP, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move up");
            }
            if (move_up) {
                std::swap(circles_[i], circles_[i - 1]);
                row_changed = true;
            }
            ImGui::SameLine(0.f, spacing);
        }
        else {
            ImGui::SameLine(0.f, BTN_WIDTH + spacing * 2);
        }

        if (i < circles_.size() - 1) {
            const bool move_down = ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(BTN_WIDTH, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move down");
            }
            if (move_down) {
                std::swap(circles_[i], circles_[i + 1]);
                row_changed = true;
            }
            ImGui::SameLine(0.f, spacing);
        }
        else {
            ImGui::SameLine(0.f, BTN_WIDTH + spacing * 2);
        }

        const bool remove = ImGui::Button("x##del", ImVec2(BTN_WIDTH, 0));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete");
        }

        ImGui::PopID();
        if (row_changed) {
            changed = true;
        }
        if (remove) {
            circles_.erase(circles_.begin() + static_cast<ptrdiff_t>(i));
            changed = true;
            break;
        }
    }
    ImGui::PopID();

    if (ImGui::Button("Add Circle")) {
        RangeCircle c;
        c.label = std::format("Custom {}", circles_.size() + 1);
        c.radius = GW::Constants::Range::Earshot;
        circles_.push_back(c);
        changed = true;
    }

    ImGui::Separator();
    if (ImGui::TreeNodeEx("Special Range Circles", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::TextDisabled("These circles have special conditional rendering and are not user-configurable.");
        changed |= Colors::DrawSettingHueWheel("HoS range", &color_range_hos);
        changed |= Colors::DrawSettingHueWheel("Chain Aggro range (enemy target)", &color_range_chain_aggro);
        changed |= Colors::DrawSettingHueWheel("Res Aggro range (dead ally target)", &color_range_res_aggro);
        changed |= Colors::DrawSettingHueWheel("Shadow Step range", &color_range_shadowstep_aggro);
        ImGui::TreePop();
    }

    if (changed) {
        Invalidate();
    }
}

void RangeRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLESTRIP;
    checkforhos_ = true;
    havehos_ = false;

    clear();
    // configurable circles + 4 special circles (HoS, chain aggro, res aggro, shadowstep)
    vertices.reserve(circle_points * (circles_.size() + 4) + 6);

    // xdiff/ydiff convert px thickness → game units using current zoom
    auto CreateCircle = [&](const float radius, const DWORD color, const float thickness_px) {
        const float diff = thickness_px / gwinches_per_pixel;
        for (auto i = 0; i <= static_cast<int>(circle_triangles); i += 2) {
            const auto angle = i / static_cast<float>(circle_triangles) * DirectX::XM_2PI;
            vertices.push_back({radius * cos(angle), radius * sin(angle), 0.f, color});
            vertices.push_back({(radius + diff) * cos(angle), (radius + diff) * sin(angle), 0.f, color});
        }
    };

    for (const auto& c : circles_) {
        CreateCircle(c.radius, c.color, c.line_thickness);
    }

    // Special circles (conditionally rendered)
    CreateCircle(360.f, color_range_hos, 1.f);                                  // HoS
    CreateCircle(700.f, color_range_chain_aggro, 1.f);                          // Chain aggro
    CreateCircle(GW::Constants::Range::Earshot, color_range_res_aggro, 1.f);    // Res aggro
    CreateCircle(GW::Constants::Range::Earshot, color_range_shadowstep_aggro, 1.f); // Shadowstep

    // HoS line (2 vertices) + draw_center cross (4 vertices)
    vertices.push_back({260.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({460.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({-150.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({150.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({0.f, -150.f, 0.f, color_range_hos});
    vertices.push_back({0.f, 150.f, 0.f, color_range_hos});

    D3DVertexBuffer::Initialize(device);
}

void RangeRenderer::Render(IDirect3DDevice9* device, float _gwinches_per_pixel)
{
    if (_gwinches_per_pixel != gwinches_per_pixel) {
        Invalidate();
        gwinches_per_pixel = _gwinches_per_pixel;
    }

    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (!buffer) return;

    switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Explorable:
            if (checkforhos_) {
                checkforhos_ = false;
                havehos_ = HaveHos();
            }
            break;
        case GW::Constants::InstanceType::Outpost:
            checkforhos_ = true;
            havehos_ = HaveHos();
            break;
        case GW::Constants::InstanceType::Loading:
            havehos_ = false;
            checkforhos_ = true;
            break;
        default:
            break;
    }

    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));

    const auto DrawCircle = [&](size_t& offset) {
        device->DrawPrimitive(type, offset, circle_triangles);
        offset += circle_points;
    };
    const auto DrawCircleAt = [&](size_t& offset, float x, float y) {
        D3DMATRIX oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        const auto translate = DirectX::XMMatrixTranslation(x, y, 0.f);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
        device->DrawPrimitive(type, offset, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
        offset += circle_points;
    };
    const auto SkipCircle = [&](size_t& offset) {
        offset += circle_points;
    };

    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

    size_t offset = 0;

    // User-configurable circles
    for (const auto& c : circles_) {
        if (!c.visible || (c.color & IM_COL32_A_MASK) == 0) {
            SkipCircle(offset);
        }
        else if (c.on_target && target) {
            DrawCircleAt(offset, target->x, target->y);
        }
        else if (!c.on_target) {
            DrawCircle(offset);
        }
        else {
            SkipCircle(offset);
        }
    }

    // HoS circle (only when player has HoS/Viper's)
    if (havehos_) DrawCircle(offset);
    else SkipCircle(offset);

    // Chain aggro — on enemy target
    if ((color_range_chain_aggro & IM_COL32_A_MASK) && target && target->allegiance == GW::Constants::Allegiance::Enemy) {
        DrawCircleAt(offset, target->x, target->y);
    }
    else {
        SkipCircle(offset);
    }

    // Res aggro — on dead ally in explorable
    if ((color_range_res_aggro & IM_COL32_A_MASK) && target && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && target->allegiance == GW::Constants::Allegiance::Ally_NonAttackable && target->GetIsDead()) {
        DrawCircleAt(offset, target->x, target->y);
    }
    else {
        SkipCircle(offset);
    }

    // Shadowstep aggro
    const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if ((color_range_shadowstep_aggro & IM_COL32_A_MASK) && (shadowstep_location.x != 0.f || shadowstep_location.y != 0.f)) {
        DrawCircleAt(offset, shadowstep_location.x, shadowstep_location.y);
    }
    else {
        SkipCircle(offset);
    }

    // HoS line
    if (havehos_) {
        const auto me = target ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me && me != target && !me->GetIsDead() && !target->GetIsDead() && GetSquareDistance(target->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
            GW::Vec2f v = me->pos - target->pos;
            D3DMATRIX oldworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            const auto rotate = DirectX::XMMatrixRotationZ(std::atan2(v.y, v.x));
            const auto newworld = rotate * XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&oldworld));
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&newworld));
            device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }
    offset += 2;

    if (draw_center_) {
        device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
        offset += 2;
        device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
    }
}

bool RangeRenderer::HaveHos()
{
    GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        checkforhos_ = true;
        return false;
    }

    for (const auto& skill : skillbar->skills) {
        const auto id = static_cast<GW::Constants::SkillID>(skill.skill_id);
        if (id == GW::Constants::SkillID::Heart_of_Shadow) {
            return true;
        }
        if (id == GW::Constants::SkillID::Vipers_Defense) {
            return true;
        }
    }
    return false;
}
