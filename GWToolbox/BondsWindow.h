#pragma once

#include "EmptyForm.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "logger.h"

class BondsWindow : public EmptyForm {
	static const int party_size = 8;
	static const int n_bonds = 3;
	static const int IMG_SIZE = 22;
	static const int WIDTH = IMG_SIZE * n_bonds;
	static const int HEIGHT = IMG_SIZE * party_size;

	class BondsMonitor : public DragButton {
		enum class Bond { Balth, Life, Prot };

		int GetBond(int xcoord);
		int GetPlayer(int ycoord);

		int hovered_player;
		int hovered_bond;
		bool pressed;
		OSHGui::PictureBox* pics[party_size][n_bonds];
		int buff_id[party_size][n_bonds];

	protected:
		virtual void PopulateGeometry() override;
		virtual void OnMouseDown(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseMove(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseUp(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseLeave(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseClick(const OSHGui::MouseMessage &mouse) override;
	public:
		BondsMonitor();
		void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
		void SaveLocation();
		void UpdateUI();
	};

public:
	BondsWindow();

	inline static const wchar_t* IniSection() { return L"bonds"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyShow() { return L"show"; }
	inline static const char* ThemeKey() { return "bonds"; }

	void Show(bool show);
	inline void UpdateUI() { if (monitor) monitor->UpdateUI(); }

private:
	BondsMonitor* monitor;
};
