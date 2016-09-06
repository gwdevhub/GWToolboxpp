#pragma once

#include <d3d9.h>

#include "Config.h"

const float fPI = 3.1415927f;

class MinimapUtils {
public:
	struct Color {
		int a;
		int r;
		int g;
		int b;
		Color() : a(0), r(0), g(0), b(0) {}
		Color(DWORD c) :
			a((c >> 24) & 0xFF),
			r((c >> 16) & 0xFF),
			g((c >> 8) & 0xFF),
			b((c) & 0xFF) {}
		Color(int _a, int _r, int _g, int _b)
			: a(_a), r(_r), g(_g), b(_b) {}
		Color(int _r, int _g, int _b) : Color(255, _r, _g, _b) {}
		const Color operator + (const Color& c) const {
			return Color(a + c.a, r + c.r, g + c.g, b + c.b);
		}
		void Clamp() {
			if (a < 0) a = 0;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
			if (a > 0xFF) a = 0xFF;
			if (r > 0xFF) r = 0xFF;
			if (g > 0xFF) g = 0xFF;
			if (b > 0xFF) b = 0xFF;
		}
		DWORD GetDXColor() {
			Clamp();
			return D3DCOLOR_ARGB(a, r, g, b);
		}
	};

	static DWORD IniReadColor(const wchar_t* key, const wchar_t* def) {
		const wchar_t* inisection = L"minimap";
		const wchar_t* wc = Config::IniRead(inisection, key, def);
		Config::IniWrite(inisection, key, wc);
		DWORD c = std::wcstoul(wc, nullptr, 16);
		if (c == LONG_MAX) return D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0x00);
		return c;
	}

	static Color IniReadColor2(const wchar_t* key, const wchar_t* def) {
		const wchar_t* inisection = L"minimap";
		const wchar_t* wc = Config::IniRead(inisection, key, def);
		Config::IniWrite(inisection, key, wc);
		DWORD c = std::wcstoul(wc, nullptr, 16);
		if (c == LONG_MAX) return Color(D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0x00));
		return Color(c);
	}
};
