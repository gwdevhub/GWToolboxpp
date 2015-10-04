#pragma once

#include "OSHGui\OSHGui.hpp"

#include "ToolboxWindow.h"


class BondsWindow : public ToolboxWindow {
	static const int n_players = 12;
	static const int n_bonds = 3;
	static const int IMG_SIZE = 22;
	static const int WIDTH = IMG_SIZE * n_bonds;
	static const int HEIGHT = IMG_SIZE * n_players;

	class BondsMonitor : public DragButton {
		enum class Bond { Balth, Life, Prot };

		int GetBond(int xcoord);
		int GetPlayer(int ycoord);

		bool freezed;
		int hovered_player;
		int hovered_bond;
		bool pressed;
		int party_size;
		OSHGui::PictureBox* pics[n_players][n_bonds];
		int buff_id[n_players][n_bonds];

		void DropUseBuff(int bond, int player);

	protected:
		virtual void PopulateGeometry() override;
		virtual void OnMouseDown(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseMove(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseUp(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseLeave(const OSHGui::MouseMessage &mouse) override;
	public:
		BondsMonitor();
		void DrawSelf(OSHGui::Drawing::RenderContext &context) override;
		void SaveLocation();
		void UpdateUI();
		inline void SetFreeze(bool b) { freezed = b; }
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
	inline void MainRoutine() {};
	inline void SetFreze(bool b) { monitor->SetFreeze(b); }

private:
	BondsMonitor* monitor;
};
