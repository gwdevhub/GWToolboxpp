#pragma once
#include <d3d9.h>
#include <vector>
#include <GWCA/GameContainers/GamePos.h>

struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
typedef unsigned long DWORD;

#define D3DFVF_CUSTOMVERTEX D3DFVF_XYZ | D3DFVF_DIFFUSE

typedef GW::Vec2f D3DVec2f;

struct D3DVertex {
    float x;
    float y;
    float z;
    DWORD color;
    D3DVertex() = default;
    D3DVertex(float x, float y, float z, DWORD color);
    D3DVertex(float x, float y, DWORD color);
};

struct D3DTriangle {
    D3DVertex v[3];
};

template <size_t N>
struct D3DShape {
    D3DTriangle t[N];
};

struct D3DQuad : D3DShape<2> {
    D3DQuad(const D3DVec2f& tl, const D3DVec2f& br, DWORD color);
};

struct D3DLine : D3DShape<2> {
    D3DLine(const D3DVec2f& a, const D3DVec2f& b, float thickness, DWORD color);
};

struct D3DDiamond : D3DShape<2> {
    D3DDiamond(const D3DVec2f& pos, float radius, DWORD color);
};

struct D3DVelocityArrow : D3DShape<1> {
    bool valid = false;
    D3DVelocityArrow(const D3DVec2f& pos, const D3DVec2f& velocity, float length, float half_width, DWORD color);
    bool Valid() const { return valid; }
};


class D3DVertexBuffer {
public:
    virtual ~D3DVertexBuffer();
    virtual void Invalidate();
    virtual void Render(IDirect3DDevice9* device);
    virtual void Terminate();
    virtual void Initialize(IDirect3DDevice9* device); // no longer pure

    virtual void reserve(size_t n) { vertices.reserve(n); }
    virtual void clear()
    {
        vertices.clear();
        dirty = true;
    }
    bool empty() const { return vertices.empty(); }
    virtual void push_back(const D3DVertexBuffer& other)
    {
        vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
        dirty = true;
    }

protected:
    void UploadVertices(IDirect3DDevice9* device); // the memcpy logic

    IDirect3DVertexBuffer9* buffer = nullptr;
    size_t buffer_byte_size = 0;
    D3DPRIMITIVETYPE type = D3DPT_TRIANGLELIST;
    unsigned long count = 0;
    bool initialized = false;
    bool dirty = true;
    std::vector<D3DVertex> vertices;
};

class D3DTriangleBuffer : public D3DVertexBuffer {
public:
    using D3DVertexBuffer::push_back; // bring base overload into scope
    template <size_t N>
    void push_back(const D3DShape<N>& shape)
    {
        for (const auto& tri : shape.t) {
            vertices.insert(vertices.end(), tri.v, tri.v + 3);
        }
        dirty = true;
    }
    void push_back(const D3DTriangle& tri)
    {
        vertices.insert(vertices.end(), tri.v, tri.v + 3);
        dirty = true;
    }

    void reserve(size_t n) override { vertices.reserve(n * 3); }

    void Initialize(IDirect3DDevice9* device) override;
};

class D3DTeardrop : public D3DVertexBuffer {
public:
    D3DTeardrop(const D3DVec2f& pos = {}, float radius = 10.f, float rotation = 0.f, DWORD color = 0xffffffff, DWORD center_color = 0x99999999);

    void SetColor(DWORD c);
    void SetCenterColor(DWORD c);
    void SetPosition(const D3DVec2f& pos);
    void RebuildRim();
    void SetRotation(float rotation);
    void SetRadius(float r);

private:
    float cos_r = 1.f;
    float sin_r = 0.f;
    float radius = 1.f;
};

struct D3DCircle : public D3DTriangleBuffer {
    D3DCircle() = default;
    D3DCircle(const D3DVec2f& center, float radius, float thickness, DWORD color, int segment_count = 64);
};
class D3DFillCircle : public D3DTriangleBuffer {
public:
    D3DFillCircle() = default;
    D3DFillCircle(const D3DVec2f& center, float radius = 10.f, DWORD color = 0x99999999, DWORD center_color = 0xffffffff, int segments = 64);

    void SetColor(DWORD c);
    void SetCenterColor(DWORD c);
    void SetRadius(float r);
};
class D3DLineCircle : public D3DVertexBuffer {
public:
    D3DLineCircle(float radius = 10.f, DWORD color = 0xffffffff, int segments = 64);

    void SetColor(DWORD c);
    void SetRadius(float r);
};
