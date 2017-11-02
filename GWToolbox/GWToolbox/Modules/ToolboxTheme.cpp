#include "ToolboxTheme.h"

#include <Modules\Resources.h>
#include <Color.h>

#define IniFilename "Theme.ini"
#define IniSection "Theme"

ToolboxTheme::ToolboxTheme() {
	ini_style = DefaultTheme();
}

ImGuiStyle ToolboxTheme::DefaultTheme() {
	ImGuiStyle style = ImGuiStyle();
	style.WindowRounding = 6.0f;
	style.FrameRounding = 2.0f;
	style.ScrollbarSize = 16.0f;
	style.ScrollbarRounding = 4.0f;
	style.GrabMinSize = 17.0f;
	style.GrabRounding = 2.0f;
	//fixme: rewrite the colors to remove the need for SwapRB
	style.Colors[ImGuiCol_WindowBg] = ImColor(0xD0000000);
	style.Colors[ImGuiCol_TitleBg] = ImColor(0xD7282828);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(0x82282828);
	style.Colors[ImGuiCol_TitleBgActive] = ImColor(0xFF282828);
	style.Colors[ImGuiCol_MenuBarBg] = ImColor(0xFF000000);
	style.Colors[ImGuiCol_ScrollbarBg] = ImColor(0xCC141414);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(0xB3424954);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(0xFF424954);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(0xFF565D68);
	style.Colors[ImGuiCol_SliderGrabActive] = ImColor(0xFFC8C8FF);
	style.Colors[ImGuiCol_Button] = ImColor(0x99344870);
	style.Colors[ImGuiCol_ButtonHovered] = ImColor(0xFF344870);
	style.Colors[ImGuiCol_ButtonActive] = ImColor(0xFF283C68);
	style.Colors[ImGuiCol_CloseButton] = ImColor(0x80808080);
	style.Colors[ImGuiCol_Header] = ImColor(0x73E62800);
	style.Colors[ImGuiCol_HeaderHovered] = ImColor(0xCCF03200);
	style.Colors[ImGuiCol_HeaderActive] = ImColor(0xCCFA3C00);
	style.Colors[ImGuiCol_CloseButtonHovered] = ImColor(0x99BDBDBD);
	style.Colors[ImGuiCol_CloseButtonActive] = ImColor(0xFFBDBDBD);
	return style;
}

void ToolboxTheme::Terminate() {
	ToolboxModule::Terminate();
	if (inifile) inifile->Reset();
	delete inifile;
}

void ToolboxTheme::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(IniFilename).c_str());
	
	ImGui::GetIO().FontGlobalScale = (float)inifile->GetDoubleValue(IniSection, "FontGlobalScale", 1.0);
	ini_style.Alpha = (float)inifile->GetDoubleValue(IniSection, "GlobalAlpha", ini_style.Alpha);
	ini_style.WindowPadding.x = (float)inifile->GetDoubleValue(IniSection, "WindowPaddingX", ini_style.WindowPadding.x);
	ini_style.WindowPadding.y = (float)inifile->GetDoubleValue(IniSection, "WindowPaddingY", ini_style.WindowPadding.y);
	ini_style.WindowRounding = (float)inifile->GetDoubleValue(IniSection, "WindowRounding", ini_style.WindowRounding);
	ini_style.FramePadding.x = (float)inifile->GetDoubleValue(IniSection, "FramePaddingX", ini_style.FramePadding.x);
	ini_style.FramePadding.y = (float)inifile->GetDoubleValue(IniSection, "FramePaddingY", ini_style.FramePadding.y);
	ini_style.FrameRounding = (float)inifile->GetDoubleValue(IniSection, "FrameRounding", ini_style.FrameRounding);
	ini_style.ItemSpacing.x = (float)inifile->GetDoubleValue(IniSection, "ItemSpacingX", ini_style.ItemSpacing.x);
	ini_style.ItemSpacing.y = (float)inifile->GetDoubleValue(IniSection, "ItemSpacingY", ini_style.ItemSpacing.y);
	ini_style.ItemInnerSpacing.x = (float)inifile->GetDoubleValue(IniSection, "ItemInnerSpacingX", ini_style.ItemInnerSpacing.x);
	ini_style.ItemInnerSpacing.y = (float)inifile->GetDoubleValue(IniSection, "ItemInnerSpacingY", ini_style.ItemInnerSpacing.y);
	ini_style.IndentSpacing = (float)inifile->GetDoubleValue(IniSection, "IndentSpacing", ini_style.IndentSpacing);
	ini_style.ScrollbarSize = (float)inifile->GetDoubleValue(IniSection, "ScrollbarSize", ini_style.ScrollbarSize);
	ini_style.ScrollbarRounding = (float)inifile->GetDoubleValue(IniSection, "ScrollbarRounding", ini_style.ScrollbarRounding);
	ini_style.GrabMinSize = (float)inifile->GetDoubleValue(IniSection, "GrabMinSize", ini_style.GrabMinSize);
	ini_style.GrabRounding = (float)inifile->GetDoubleValue(IniSection, "GrabRounding", ini_style.GrabRounding);
	ini_style.WindowTitleAlign.x = (float)inifile->GetDoubleValue(IniSection, "WindowTitleAlignX", ini_style.WindowTitleAlign.x);
	ini_style.WindowTitleAlign.y = (float)inifile->GetDoubleValue(IniSection, "WindowTitleAlignY", ini_style.WindowTitleAlign.y);
	ini_style.ButtonTextAlign.x = (float)inifile->GetDoubleValue(IniSection, "ButtonTextAlignX", ini_style.ButtonTextAlign.x);
	ini_style.ButtonTextAlign.y = (float)inifile->GetDoubleValue(IniSection, "ButtonTextAlignY", ini_style.ButtonTextAlign.y);
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		const char* name = ImGui::GetStyleColorName(i);
		Color color = Colors::Load(inifile, IniSection, name, ImColor(ini_style.Colors[i]));
		ini_style.Colors[i] = ImColor(color);
	}

	ImGui::GetStyle() = ini_style;
}

void ToolboxTheme::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().FontGlobalScale != 1.0f) inifile->SetDoubleValue(IniSection, "FontGlobalScale", ImGui::GetIO().FontGlobalScale);
	if (style.Alpha != ini_style.Alpha) inifile->SetDoubleValue(IniSection, "GlobalAlpha", style.Alpha);
	if (style.WindowPadding.x != ini_style.WindowPadding.x) inifile->SetDoubleValue(IniSection, "WindowPaddingX", style.WindowPadding.x);
	if (style.WindowPadding.y != ini_style.WindowPadding.y) inifile->SetDoubleValue(IniSection, "WindowPaddingY", style.WindowPadding.y);
	if (style.WindowRounding != ini_style.WindowRounding) inifile->SetDoubleValue(IniSection, "WindowRounding", style.WindowRounding);
	if (style.FramePadding.x != ini_style.FramePadding.x) inifile->SetDoubleValue(IniSection, "FramePaddingX", style.FramePadding.x);
	if (style.FramePadding.y != ini_style.FramePadding.y) inifile->SetDoubleValue(IniSection, "FramePaddingY", style.FramePadding.y);
	if (style.FrameRounding != ini_style.FrameRounding) inifile->SetDoubleValue(IniSection, "FrameRounding", style.FrameRounding);
	if (style.ItemSpacing.x != ini_style.ItemSpacing.x) inifile->SetDoubleValue(IniSection, "ItemSpacingX", style.ItemSpacing.x);
	if (style.ItemSpacing.y != ini_style.ItemSpacing.y) inifile->SetDoubleValue(IniSection, "ItemSpacingY", style.ItemSpacing.y);
	if (style.ItemInnerSpacing.x != ini_style.ItemInnerSpacing.x) inifile->SetDoubleValue(IniSection, "ItemInnerSpacingX", style.ItemInnerSpacing.x);
	if (style.ItemInnerSpacing.y != ini_style.ItemInnerSpacing.y) inifile->SetDoubleValue(IniSection, "ItemInnerSpacingY", style.ItemInnerSpacing.y);
	if (style.IndentSpacing != ini_style.IndentSpacing) inifile->SetDoubleValue(IniSection, "IndentSpacing", style.IndentSpacing);
	if (style.ScrollbarSize != ini_style.ScrollbarSize) inifile->SetDoubleValue(IniSection, "ScrollbarSize", style.ScrollbarSize);
	if (style.ScrollbarRounding != ini_style.ScrollbarRounding) inifile->SetDoubleValue(IniSection, "ScrollbarRounding", style.ScrollbarRounding);
	if (style.GrabMinSize != ini_style.GrabMinSize) inifile->SetDoubleValue(IniSection, "GrabMinSize", style.GrabMinSize);
	if (style.GrabRounding != ini_style.GrabRounding) inifile->SetDoubleValue(IniSection, "GrabRounding", style.GrabRounding);
	if (style.WindowTitleAlign.x != ini_style.WindowTitleAlign.x) inifile->SetDoubleValue(IniSection, "WindowTitleAlignX", style.WindowTitleAlign.x);
	if (style.WindowTitleAlign.y != ini_style.WindowTitleAlign.y) inifile->SetDoubleValue(IniSection, "WindowTitleAlignY", style.WindowTitleAlign.y);
	if (style.ButtonTextAlign.x != ini_style.ButtonTextAlign.x) inifile->SetDoubleValue(IniSection, "ButtonTextAlignX", style.ButtonTextAlign.x);
	if (style.ButtonTextAlign.y != ini_style.ButtonTextAlign.y) inifile->SetDoubleValue(IniSection, "ButtonTextAlignY", style.ButtonTextAlign.y);
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		const char* name = ImGui::GetStyleColorName(i);
		Color cur = ImColor(style.Colors[i]);
		if (cur != ImColor(ini_style.Colors[i])) {
			Color color = ImColor(style.Colors[i]);
			Colors::Save(inifile, IniSection, name, color);
		}
	}

	inifile->SaveFile(Resources::GetPath(IniFilename).c_str());

	ini_style = style;
}

void ToolboxTheme::DrawSettingInternal() {
	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::SmallButton("Restore Default")) {
		const ImGuiStyle default_style = DefaultTheme(); // Default style
		style = default_style;
	}
	ImGui::Text("Note: theme is stored in 'Theme.ini' in settings folder. You can share the file or parts of it with other people.");
	ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
	ImGui::DragFloat("Global Font Scale", &ImGui::GetIO().FontGlobalScale, 0.005f, 0.3f, 2.0f, "%.1f");
	ImGui::Text("Sizes");
	ImGui::SliderFloat2("Window Padding", (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("Window Rounding", &style.WindowRounding, 0.0f, 16.0f, "%.0f");
	ImGui::SliderFloat2("Frame Padding", (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("Frame Rounding", &style.FrameRounding, 0.0f, 16.0f, "%.0f");
	ImGui::SliderFloat2("Item Spacing", (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat2("Item InnerSpacing", (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("Indent Spacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("Scrollbar Size", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("Scrollbar Rounding", &style.ScrollbarRounding, 0.0f, 16.0f, "%.0f");
	ImGui::SliderFloat("Grab MinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");
	ImGui::SliderFloat("Grab Rounding", &style.GrabRounding, 0.0f, 16.0f, "%.0f");
	ImGui::Text("Alignment");
	ImGui::SliderFloat2("Window Title Align", (float*)&style.WindowTitleAlign, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat2("Button Text Align", (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f");
	ImGui::Text("Colors");
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		const char* name = ImGui::GetStyleColorName(i);
		ImGui::PushID(i);
		ImGui::ColorEdit4(name, (float*)&style.Colors[i]);
		Color cur = ImColor(style.Colors[i]);
		if (cur != ImColor(ini_style.Colors[i])) {
			ImGui::SameLine();
			if (ImGui::Button("Revert")) style.Colors[i] = ImColor(Colors::Load(inifile, IniSection, name, ImColor(ini_style.Colors[i])));
		}
		ImGui::PopID();
	}
}
