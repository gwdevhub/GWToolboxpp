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
		: initialized(false), buffer(nullptr), 
		type(D3DPT_TRIANGLELIST), count(0) {}

	virtual ~VBuffer() {
		if (buffer) buffer->Release();
	}

	virtual void Invalidate() { initialized = false; }

	virtual void Render(IDirect3DDevice9* device) {
		if (!initialized) {
			initialized = true;
			Initialize(device);
		}

		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type, 0, count);
	}

protected:
	IDirect3DVertexBuffer9* buffer;
	D3DPRIMITIVETYPE type;
	unsigned long count;
	bool initialized;

private:
	virtual void Initialize(IDirect3DDevice9* device) = 0;
};
