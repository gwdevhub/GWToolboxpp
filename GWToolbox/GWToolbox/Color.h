#pragma once

#include <SimpleIni.h>
#include <imgui.h>

#ifdef RGB
#undef RGB
#endif

typedef ImU32 Color;

namespace Colors {

	static Color ARGB(int a, int r, int g, int b) {
		return (a << IM_COL32_A_SHIFT)
			| (r << IM_COL32_R_SHIFT)
			| (g << IM_COL32_G_SHIFT)
			| (b << IM_COL32_B_SHIFT);
	}

	static Color RGB(int r, int g, int b) {
		return (0xFF << IM_COL32_A_SHIFT)
			| (r << IM_COL32_R_SHIFT)
			| (g << IM_COL32_G_SHIFT)
			| (b << IM_COL32_B_SHIFT);
	}

	static Color Red() { return 0xFFFF0000; }
	static Color Purple() { return 0xFFFF00FF; }

	static Color Load(CSimpleIni* ini, const char* section, const char* key, Color def) {
		try {
			const char* wc = ini->GetValue(section, key, nullptr);
			if (wc == nullptr) return def;
			return std::stoul(wc, nullptr, 16);
		} catch (...) { // invalid argument, out of range, whatever
			return 0xFF000000;
		}
	}

	static void Save(CSimpleIni* ini, const char* section, const char* key, Color val) {
		char buf[64];
		sprintf_s(buf, "0x%X", val);
		ini->SetValue(section, key, buf);
	}

	static void ConvertU32ToInt4(Color color, int* i) {
		i[0] = ((color >> IM_COL32_A_SHIFT) & 0xFF);
		i[1] = ((color >> IM_COL32_R_SHIFT) & 0xFF);
		i[2] = ((color >> IM_COL32_G_SHIFT) & 0xFF);
		i[3] = ((color >> IM_COL32_B_SHIFT) & 0xFF);
	}

	static Color ConvertInt4ToU32(const int* i) {
		return ((i[0] & 0xFF) << IM_COL32_A_SHIFT)
			| ((i[1] & 0xFF) << IM_COL32_R_SHIFT)
			| ((i[2] & 0xFF) << IM_COL32_G_SHIFT)
			| ((i[3] & 0xFF) << IM_COL32_B_SHIFT);
	}

	static bool DrawSetting(const char* text, Color* color, bool alpha = true) {
		int i[4];
		ConvertU32ToInt4(*color, i);

		ImGuiContext* context = ImGui::GetCurrentContext();
		const ImGuiStyle& style = ImGui::GetStyle();

		const int n_components = alpha ? 4 : 4;
		const int first_component = alpha ? 0 : 1;
		const int last_component = 4;

		bool value_changed = false;

		const float w_full = ImGui::CalcItemWidth();
		const float square_sz = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;

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
		ImGui::ColorButton(ImColor(i[1], i[2], i[3]));

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color:\n0x%02X%02X%02X%02X", i[0], i[1], i[2], i[3]);

		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ImGui::Text(text);

		ImGui::PopID();
		ImGui::EndGroup();

		if (value_changed) *color = ConvertInt4ToU32(i);

		return value_changed;
	}

	static Color Add(const Color c1, const Color c2) {
		int i1[4]; int i2[4];
		ConvertU32ToInt4(c1, i1);
		ConvertU32ToInt4(c2, i2);
		for (int i = 0; i < 4; ++i) {
			i1[i] += i2[i];
			if (i1[i] < 0) i1[i] = 0;
			if (i1[i] > 0xFF) i1[i] = 0xFF;
		}
		return ConvertInt4ToU32(i1);
	}
	static Color Sub(const Color c1, const Color c2) {
		int i1[4]; int i2[4];
		ConvertU32ToInt4(c1, i1);
		ConvertU32ToInt4(c2, i2);
		for (int i = 0; i < 4; ++i) {
			i1[i] -= i2[i];
			if (i1[i] < 0) i1[i] = 0;
			if (i1[i] > 0xFF) i1[i] = 0xFF;
		}
		return ConvertInt4ToU32(i1);
	}
}
