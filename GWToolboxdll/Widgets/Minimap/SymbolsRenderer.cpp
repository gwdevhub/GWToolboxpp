#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/AgentMgr.h>

#include <ImGuiAddons.h>

#include <Color.h>

#include <GWCA/Managers/QuestMgr.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/Minimap/SymbolsRenderer.h>
#include <Modules/QuestModule.h>

// 注意：由 CMake 自动生成！
#include "Shaders/constant_colour_ps.h"

namespace {
    IDirect3DPixelShader9* pshader = nullptr;

    bool ConfigureProgrammablePipeline(IDirect3DDevice9* device)
    {
        if (pshader != nullptr) {
            return true;
        }
        if (device->CreatePixelShader(reinterpret_cast<const DWORD*>(&constant_colour_ps), &pshader) != D3D_OK) {
            pshader = nullptr;
            Log::Error("SymbolsRenderer：无法创建像素着色器");
            return false;
        }
        return true;
    }
}

void SymbolsRenderer::RegisterSettings(ToolboxModule* module)
{
    // SettingColor 与 Color 布局兼容；强制转换使注册表能将其持久化为十六进制字符串
    const auto register_color = [module](const char* key, Color* color) {
        SettingsRegistry::RegisterField(module, key, reinterpret_cast<Colors::SettingColor*>(color));
    };
    register_color("color_quest", &color_quest);
    register_color("color_quest_line", &color_quest_line);
    register_color("color_other_quests", &color_other_quests);
    register_color("color_north", &color_north);
    register_color("color_symbols_modifier", &color_modifier);
}

void SymbolsRenderer::DrawSettings()
{
    ImGui::SmallConfirmButton("恢复默认", "确定吗？", [&](const bool result, void*) {
        if (result) {
            color_quest = 0xFF22EF22;
            color_quest_line = 0xFF22EF22;
            color_other_quests = 0x00006400;
            color_north = 0xFFFF8000;
            color_modifier = 0x001E1E1E;
            Invalidate();
        }
    });
    if (Colors::DrawSettingHueWheel("激活任务标记", &color_quest)) {
        Invalidate();
    }
    if (Colors::DrawSettingHueWheel("任务线条颜色", &color_quest_line)) {
        Invalidate();
    }
    if (Colors::DrawSettingHueWheel("其他任务标记", &color_other_quests)) {
        Invalidate();
    }
    ImGui::ShowHelp("非激活任务的任务标记将以该颜色显示。\n如果此颜色的 Alpha 为 0，非激活任务标记将使用随机颜色。");
    if (Colors::DrawSettingHueWheel("北方标记", &color_north)) {
        Invalidate();
    }
    if (Colors::DrawSettingHueWheel("符号修改器", &color_modifier)) {
        Invalidate();
    }
    ImGui::ShowHelp("每个符号的边框会减去此值，中心会加上此值\n"
        "为零时颜色为纯色，数值较高时阴影更明显。");
}

void SymbolsRenderer::Initialize(IDirect3DDevice9* device)
{
    clear();
    type = D3DPT_TRIANGLELIST;
    vertices.reserve((star_ntriangles + arrow_ntriangles + north_ntriangles) * 3);

    // === 星星 ===
    star_offset = vertices.size();
    for (auto i = 0u; i < star_ntriangles; i++) {
        constexpr float star_size_big = 300.0f;
        constexpr float star_size_small = 150.0f;
        const float angle1 = 2 * (i + 0) * DirectX::XM_PI / star_ntriangles;
        const float angle2 = 2 * (i + 1) * DirectX::XM_PI / star_ntriangles;
        const float size1 = (i + 0) % 2 == 0 ? star_size_small : star_size_big;
        const float size2 = (i + 1) % 2 == 0 ? star_size_small : star_size_big;
        const Color c1 = (i + 0) % 2 == 0 ? color_quest : Colors::Sub(color_quest, color_modifier);
        const Color c2 = (i + 1) % 2 == 0 ? color_quest : Colors::Sub(color_quest, color_modifier);
        vertices.push_back({std::cos(angle1) * size1, std::sin(angle1) * size1, 0.f, c1});
        vertices.push_back({std::cos(angle2) * size2, std::sin(angle2) * size2, 0.f, c2});
        vertices.push_back({0.f, 0.f, 0.f, Colors::Add(color_quest, color_modifier)});
    }

    // === 箭头（任务） ===
    arrow_offset = vertices.size();
    vertices.push_back({0.f, -125.f, 0.f, Colors::Add(color_quest, color_modifier)});
    vertices.push_back({250.f, -250.f, 0.f, color_quest});
    vertices.push_back({0.f, 250.f, 0.f, color_quest});
    vertices.push_back({0.f, 250.f, 0.f, color_quest});
    vertices.push_back({-250.f, -250.f, 0.f, color_quest});
    vertices.push_back({0.f, -125.f, 0.f, Colors::Add(color_quest, color_modifier)});

    // === 箭头（北方） ===
    north_offset = vertices.size();
    vertices.push_back({0.f, -375.f, 0.f, Colors::Add(color_north, color_modifier)});
    vertices.push_back({250.f, -500.f, 0.f, color_north});
    vertices.push_back({0.f, 0.f, 0.f, color_north});
    vertices.push_back({0.f, 0.f, 0.f, color_north});
    vertices.push_back({-250.f, -500.f, 0.f, color_north});
    vertices.push_back({0.f, -375.f, 0.f, Colors::Add(color_north, color_modifier)});

    D3DVertexBuffer::Initialize(device);
}

void SymbolsRenderer::Render(IDirect3DDevice9* device, float zoom)
{
    if(!initialized)
        Initialize(device);

    if (!ConfigureProgrammablePipeline(device)) {
        return;
    }

    const GW::Agent* me = GW::Agents::GetObservingAgent();
    if (me == nullptr) {
        return;
    }

    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));

    constexpr float pi = std::numbers::pi_v<float>;
    static float tau = 0.0f;
    const float fps = ImGui::GetIO().Framerate;
    tau += 0.05f * 60.0f / std::max(fps, 1.0f);
    if (tau > 10 * pi) {
        tau -= 10 * pi;
    }
    DirectX::XMMATRIX translate{};
    DirectX::XMMATRIX world{};

    const GW::Vec2f mypos = me->pos;
    std::vector<GW::Vec2f> markers_drawn;
    const auto draw_quest_marker = [&](const GW::Quest& quest) {
        const auto active_quest = GW::QuestMgr::GetActiveQuest();
        const bool is_current_quest = active_quest != nullptr && quest.quest_id == active_quest->quest_id;

        if (!Minimap::ShouldDrawAllQuests() && !is_current_quest) {
            return;
        }

        const GW::Vec2f qpos = {quest.marker.x, quest.marker.y};
        if (std::ranges::contains(markers_drawn, qpos))
            return; // 同一位置不绘制多个标记

        const auto quest_im_color = QuestModule::GetQuestColor(quest.quest_id);
        if (!((quest_im_color >> IM_COL32_A_SHIFT) & 0xff)) {
            return; // 跳过不可见任务
        }

        const auto quest_color = ImGui::ColorConvertU32ToFloat4(quest_im_color);
        device->SetPixelShaderConstantF(0, &quest_color.x, 1);

        const float compass_scale = zoom;
        const float marker_scale = 1.0f / compass_scale;
        auto rotate = DirectX::XMMatrixRotationZ(-tau / 5);
        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(marker_scale + std::sin(tau) * 0.3f * marker_scale,
                                                           marker_scale + std::sin(tau) * 0.3f * marker_scale, 1.0f);
        translate = DirectX::XMMatrixTranslation(qpos.x, qpos.y, 0);
        world = rotate * scale * translate;
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
        device->DrawPrimitive(type, star_offset, star_ntriangles);

        GW::Vec2f v = qpos - mypos;
        const float max_quest_range = (GW::Constants::Range::Compass - 250.0f) / compass_scale;
        const float max_quest_range_sqr = max_quest_range * max_quest_range;
        if (GetSquaredNorm(v) > max_quest_range_sqr) {
            v = Normalize(v) * max_quest_range;

            const float angle = std::atan2(v.y, v.x);
            rotate = DirectX::XMMatrixRotationZ(angle - DirectX::XM_PIDIV2);
            scale = DirectX::XMMatrixScaling(marker_scale + std::sin(tau) * 0.3f * marker_scale, marker_scale + std::sin(tau) * 0.3f * marker_scale, 1.0f);
            translate = DirectX::XMMatrixTranslation(me->pos.x + v.x, me->pos.y + v.y, 0);
            world = rotate * scale * translate;
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
            device->DrawPrimitive(type, arrow_offset, arrow_ntriangles);
        }

        markers_drawn.push_back(qpos);
    };

    if (const auto quest_log = GW::QuestMgr::GetQuestLog()) {
        if (pshader == nullptr || device->SetPixelShader(pshader) != D3D_OK) {
            Log::Error("SymbolsRenderer：无法设置像素着色器，中止渲染。");
            return;
        }

        // 先绘制激活任务
        const auto active_quest_id = GW::QuestMgr::GetActiveQuestId();
        if (const auto quest = GW::QuestMgr::GetQuest(active_quest_id)) {
            draw_quest_marker(*quest);
        }
        for (const auto& quest : *quest_log | std::views::filter([active_quest_id](const GW::Quest& q) {
            return q.quest_id != active_quest_id;
        })) {
            draw_quest_marker(quest);
        }

        device->SetPixelShader(nullptr);
    }

    translate = DirectX::XMMatrixTranslation(me->pos.x, me->pos.y + 5000.0f, 0);
    world = translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
    device->DrawPrimitive(type, north_offset, north_ntriangles);
}
