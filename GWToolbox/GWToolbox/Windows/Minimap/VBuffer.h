#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "D3DVertex.h"

/*
This class is essentially a glorified vertex buffer, containing everything
that is necessary to render the vertex buffer. 

classes implementing this class only need to implement Initialize which 
should contain code that:
- populates the vertex buffer "buffer_"
- sets the primitive type "type_"
- sets the primitive count "count_"

afterward just call Render and the vertex buffer will be rendered

you can call Invalidate() to have the initialize be called again on render
*/

class VBuffer {
public:
	VBuffer()
		: initialized_(false), buffer_(nullptr), 
		type_(D3DPT_TRIANGLELIST), count_(0) {}

	~VBuffer() {
		if (buffer_) buffer_->Release();
	}

	inline void Invalidate() { initialized_ = false; }

	virtual void Render(IDirect3DDevice9* device) {
		if (!initialized_) {
			initialized_ = true;
			Initialize(device);
		}

		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type_, 0, count_);
	}

protected:
	IDirect3DVertexBuffer9* buffer_;
	D3DPRIMITIVETYPE type_;
	unsigned long count_;
	bool initialized_;

private:
	virtual void Initialize(IDirect3DDevice9* device) = 0;

};
