#pragma once

#include <SimpleIni.h>
#include <imgui.h>
#include <imgui_internal.h>

typedef DWORD Color_t;

namespace Colors {

	static Color_t IniGet(CSimpleIni* ini, const char* section, const char* key, Color_t def) {
		try {
			const char* wc = ini->GetValue(section, key, nullptr);
			if (wc == nullptr) return def;
			return std::stoul(wc, nullptr, 16);
		} catch (...) { // invalid argument, out of range, whatever
			return 0xFF000000;
		}
	}

	static void IniSet(CSimpleIni* ini, const char* section, const char* key, Color_t val) {
		char buf[64];
		sprintf_s(buf, "0x%X", val);
		ini->SetValue(section, key, buf);
	}

	static void u32_to_int4(Color_t color, int* i) {
		i[0] = ((color >> 24) & 0xFF);
		i[1] = ((color >> 16) & 0xFF);
		i[2] = ((color >> 8) & 0xFF);
		i[3] = ((color) & 0xFF);
	}

	static Color_t int4_to_u32(const int* i) {
		return ((i[0] & 0xFF) << 24)
			| ((i[1] & 0xFF) << 16)
			| ((i[2] & 0xFF) << 8)
			| ((i[3] & 0xFF));
	}

	static bool DrawSetting(const char* text, Color_t* color, bool alpha = true) {
		int i[4];
		u32_to_int4(*color, i);

		ImGuiContext* context = ImGui::GetCurrentContext();
		const ImGuiStyle& style = context->Style;

		const int n_components = alpha ? 4 : 4;
		const int first_component = alpha ? 0 : 1;
		const int last_component = 4;

		bool value_changed = false;

		const float w_full = ImGui::CalcItemWidth();
		const float square_sz = context->FontSize + style.FramePadding.y * 2.0f;

		const float w_items_all = w_full - (square_sz + style.ItemInnerSpacing.x);
		const float w_item_one = std::round((w_items_all - (style.ItemInnerSpacing.x) * (n_components - 1)) / (float)n_components);
		const float w_item_last = std::round(w_items_all - (w_item_one + style.ItemInnerSpacing.x) * (n_components - 1));
		
		const char* ids[4] = { "##X", "##Y", "##Z", "##W" };
		const char* fmt[4] = { "A:%3.0f", "R:%3.0f", "G:%3.0f", "B:%3.0f" };

		ImGui::BeginGroup();
		ImGui::PushID(text);
		
		ImGui::PushItemWidth(w_item_one);
		if (!alpha) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w_item_one + style.ItemInnerSpacing.x);
		for (int n = first_component; n < last_component; ++n) {
			if (n > first_component) ImGui::SameLine(0, style.ItemInnerSpacing.x);
			if (n + 1 == last_component) ImGui::PushItemWidth(w_item_last);
			value_changed |= ImGui::DragInt(ids[n], &i[n], 1.0f, 0, 255, fmt[n]);
		}
		ImGui::PopItemWidth();
		ImGui::PopItemWidth();

		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		float s = 1.0f / 255.0f;
		const ImVec4 col_display(i[1] * s, i[2] * s, i[3] * s, 1.0f);
		ImGui::ColorButton(col_display);

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color:\n0x%02X%02X%02X%02X", i[0], i[1], i[2], i[3]);

		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::Text(text);

		ImGui::PopID();
		ImGui::EndGroup();

		if (value_changed) *color = int4_to_u32(i);

		return value_changed;
	}

	static Color_t Add(const Color_t c1, const Color_t c2) {
		int i1[4]; int i2[4];
		u32_to_int4(c1, i1);
		u32_to_int4(c2, i2);
		for (int i = 0; i < 4; ++i) {
			i1[i] += i2[i];
			if (i1[i] < 0) i1[i] = 0;
			if (i1[i] > 0xFF) i1[i] = 0xFF;
		}
		return int4_to_u32(i1);
	}
	static Color_t Sub(const Color_t c1, const Color_t c2) {
		int i1[4]; int i2[4];
		u32_to_int4(c1, i1);
		u32_to_int4(c2, i2);
		for (int i = 0; i < 4; ++i) {
			i1[i] -= i2[i];
			if (i1[i] < 0) i1[i] = 0;
			if (i1[i] > 0xFF) i1[i] = 0xFF;
		}
		return int4_to_u32(i1);
	}
}
