#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "Point2i.h"
#include "Point2f.h"

/*
This class represents a window on screen where you can draw.
It stores the view transformation and sets up matrices in RenderBegin()
*/

class Viewer {
public:
	Viewer();

	void RenderSetup(IDirect3DDevice9* device);
	void RenderSetupClipping(IDirect3DDevice9* device);
	void RenderSetupViewport(IDirect3DDevice9* device);
	void RenderSetupIdentityTransforms(IDirect3DDevice9* device);
	void RenderSetupWorldTransforms(IDirect3DDevice9* device);
	virtual void Render(IDirect3DDevice9* device) = 0;

	inline int GetX() { return location_.x(); }
	inline int GetY() { return location_.y(); }
	inline int GetWidth() { return size_.x(); }
	inline int GetHeight() { return size_.y(); }
	inline Point2i location() { return location_; }
	inline Point2i size() { return size_; }

	inline void SetX(int x) { location_.x() = x; }
	inline void SetY(int y) { location_.y() = y; }
	inline void SetWidth(int width) { size_.x() = width; }
	inline void SetHeight(int height) { size_.y() = height; }
	inline void SetLocation(Point2i location) { location_ = location; }
	inline void SetLocation(int x, int y) { location_ = Point2i(x, y); }
	inline void SetSize(Point2i size) { size_ = size; }
	inline void SetSize(int width, int height) { size_ = Point2i(width, height); }

	void Translate(float x, float y) { 
		translation_x_ += x;
		translation_y_ += y;
	}
	inline void Scale(float amount) { scale_ *= amount; }
	inline void Rotate(float radians) { rotation_ += radians; }

	void SetTranslation(float x, float y) {
		translation_x_ = x;
		translation_y_ = y;
	}
	inline void SetScale(float scale) { scale_ = scale; }
	inline void SetRotation(float radians) { rotation_ = radians; }

protected:
	float translation_x_;
	float translation_y_;
	float scale_;
	float rotation_;

	Point2i location_;
	Point2i size_;

};
