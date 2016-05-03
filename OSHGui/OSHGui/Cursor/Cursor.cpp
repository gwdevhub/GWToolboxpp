/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Cursor.hpp"
#include "../Application.hpp"
#include "../Drawing/Graphics.hpp"

namespace OSHGui{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Cursor::Cursor()
		: geometry_(Application::Instance().GetRenderer().CreateGeometryBuffer()) {
		
	}
	//---------------------------------------------------------------------------
	Cursor::~Cursor() {
	
	}
	//---------------------------------------------------------------------------
	Drawing::GeometryBufferPtr Cursor::GetGeometry() {
		return geometry_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Cursor::Initialize() {
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(Color::White(), PointI(0, 0), SizeI(1, 12));
		g.FillRectangle(Color::White(), PointI(1, 0), SizeI(1, 11));
		g.FillRectangle(Color::White(), PointI(1, 11),SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(2, 1), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(2, 10),SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(3, 2), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(3, 9), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(4, 3), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(5, 4), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(6, 5), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(7, 6), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(8, 7), SizeI(1, 1));
		g.FillRectangle(Color::White(), PointI(4, 8), SizeI(4, 1));

		g.FillRectangle(Color::Black(), PointI(1, 1), SizeI(1, 10));
		g.FillRectangle(Color::Black(), PointI(2, 2), SizeI(1, 8));
		g.FillRectangle(Color::Black(), PointI(3, 3), SizeI(1, 6));
		g.FillRectangle(Color::Black(), PointI(4, 4), SizeI(1, 4));
		g.FillRectangle(Color::Black(), PointI(5, 5), SizeI(1, 3));
		g.FillRectangle(Color::Black(), PointI(6, 6), SizeI(1, 2));
		g.FillRectangle(Color::Black(), PointI(7, 7), SizeI(1, 1));
	}
	//---------------------------------------------------------------------------
}