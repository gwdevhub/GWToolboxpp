#pragma once

#include <Windows.h>
#include <d3d9.h>

/*
This class represents a window on screen where you can draw.
It stores the view transformation and sets up matrices in RenderBegin()
*/

class Viewer {
public:
	Viewer();

	void RenderSetupClipping(IDirect3DDevice9* device);
	void RenderSetupProjection(IDirect3DDevice9* device);
	virtual void Render(IDirect3DDevice9* device) = 0;

	inline int GetX() const { return location_x_; }
	inline int GetY() const { return location_y_; }
	inline int GetWidth() const { return width_; }
	inline int GetHeight() const { return height_; }

	inline void SetX(int x) { location_x_ = x; }
	inline void SetY(int y) { location_y_ = y; }
	inline void SetWidth(int width) { width_ = width; }
	inline void SetHeight(int height) { height_ = height; }
	inline void SetLocation(int x, int y) { location_x_ = x; location_y_ = y; }
	inline void SetSize(int width, int height) { width_ = width; height_ = height; }

	void Translate(float x, float y) { 
		translation_x_ += x;
		translation_y_ += y;
	}
	inline void Scale(float amount) { scale_ *= amount; }

	void SetTranslation(float x, float y) {
		translation_x_ = x;
		translation_y_ = y;
	}
	inline void SetScale(float scale) { scale_ = scale; }
	inline float GetScale() const { return scale_; }
	inline float GetTranslationX() const { return translation_x_; }
	inline float GetTranslationY() const { return translation_y_; }

protected:
	float translation_x_;
	float translation_y_;
	float scale_;

	int location_x_;
	int location_y_;
	int width_;
	int height_;
};
