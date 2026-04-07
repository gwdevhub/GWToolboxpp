#include "D3DContainers.h"
#include "stdafx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// D3DVertex
D3DVertex::D3DVertex(const float x, const float y, const float z, const DWORD color) : x(x), y(y), z(z), color(color) {}

D3DVertex::D3DVertex(const float x, const float y, const DWORD color) : x(x), y(y), z(0.f), color(color) {}

// D3DQuad
D3DQuad::D3DQuad(const D3DVec2f& tl, const D3DVec2f& br, DWORD color)
{
    const D3DVertex TL{tl.x, tl.y, color};
    const D3DVertex TR{br.x, tl.y, color};
    const D3DVertex BR{br.x, br.y, color};
    const D3DVertex BL{tl.x, br.y, color};
    t[0] = {TL, TR, BR};
    t[1] = {TL, BR, BL};
}

// D3DLine
D3DLine::D3DLine(const D3DVec2f& a, const D3DVec2f& b, float thickness, DWORD color)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = sqrtf(dx * dx + dy * dy);
    const float nx = (dy / len) * thickness;
    const float ny = (dx / len) * thickness;
    const D3DVertex TL{a.x + nx, a.y - ny, color};
    const D3DVertex TR{b.x + nx, b.y - ny, color};
    const D3DVertex BR{b.x - nx, b.y + ny, color};
    const D3DVertex BL{a.x - nx, a.y + ny, color};
    t[0] = {TL, TR, BR};
    t[1] = {TL, BR, BL};
}

// D3DDiamond
D3DDiamond::D3DDiamond(const D3DVec2f& pos, float radius, DWORD color)
{
    const D3DVertex top{pos.x, pos.y + radius, color};
    const D3DVertex right{pos.x + radius, pos.y, color};
    const D3DVertex bot{pos.x, pos.y - radius, color};
    const D3DVertex left{pos.x - radius, pos.y, color};
    t[0] = {top, right, bot};
    t[1] = {top, bot, left};
}

// D3DVelocityArrow
D3DVelocityArrow::D3DVelocityArrow(const D3DVec2f& pos, const D3DVec2f& velocity, float length, float half_width, DWORD color)
{
    const float vlen_sq = velocity.x * velocity.x + velocity.y * velocity.y;
    if (vlen_sq < 1.0f) return;
    const float vlen = sqrtf(vlen_sq);
    const float dx = velocity.x / vlen;
    const float dy = velocity.y / vlen;
    const float nx = -dy, ny = dx;
    const D3DVec2f tip{pos.x + dx * length, pos.y + dy * length};
    t[0] = {D3DVertex(tip.x, tip.y, color), D3DVertex(pos.x + nx * half_width, pos.y + ny * half_width, color), D3DVertex(pos.x - nx * half_width, pos.y - ny * half_width, color)};
    valid = true;
}

D3DVertexBuffer::~D3DVertexBuffer() {
    Terminate();
}

// D3DVertexBuffer
void D3DVertexBuffer::Initialize(IDirect3DDevice9* device)
{
    dirty = false;
    UploadVertices(device);
    initialized = true;
}
void D3DVertexBuffer::Invalidate()
{

    initialized = false;
}
void D3DVertexBuffer::Terminate()
{
    if (buffer) buffer->Release();
    buffer = nullptr;
    buffer_byte_size = 0;
}
void D3DVertexBuffer::UploadVertices(IDirect3DDevice9* device)
{
    count = 0;
    const size_t byte_size = vertices.size() * sizeof(D3DVertex);
    if (!byte_size) return;

    if (buffer && byte_size > buffer_byte_size) {
        buffer->Release();
        buffer = nullptr;
    }

    if (!buffer) {
        if (FAILED(device->CreateVertexBuffer(byte_size, D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr))) return;
        buffer_byte_size = byte_size;
    }

    void* ptr = nullptr;
    if (FAILED(buffer->Lock(0, byte_size, &ptr, 0))) {
        if (buffer) buffer->Release();
        buffer = nullptr;
        return;
    }
    memcpy(ptr, vertices.data(), byte_size);
    buffer->Unlock();

    switch (type) {
        case D3DPT_TRIANGLELIST:
            count = vertices.size() / 3;
            break;
        case D3DPT_LINELIST:
            count = vertices.size() / 2;
            break;
        case D3DPT_LINESTRIP:
            count = vertices.size() - 1;
            break;
        default:
            count = vertices.size();
            break;
    }
}
void D3DVertexBuffer::Render(IDirect3DDevice9* device)
{
    if (dirty) Invalidate();
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (!buffer || !count) return;
    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
    device->DrawPrimitive(type, 0, count);
}

void D3DTriangleBuffer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLELIST;
    D3DVertexBuffer::Initialize(device);
}

D3DCircle::D3DCircle(const D3DVec2f& center, float radius, float thickness, DWORD color, int segment_count)
{
    D3DVec2f prev = {center.x + radius, center.y};
    for (int i = 1; i <= segment_count; i++) {
        const float a = static_cast<float>(i) / segment_count * M_PI * 2;
        const D3DVec2f cur = {center.x + radius * cosf(a), center.y + radius * sinf(a)};
        push_back(D3DLine(prev, cur, thickness, color));
        prev = cur;
    }
}

D3DTeardrop::D3DTeardrop(const D3DVec2f& pos, float radius, float rotation, DWORD color, DWORD center_color) : cos_r(cosf(rotation)), sin_r(sinf(rotation)), radius(radius)
{
    type = D3DPT_TRIANGLELIST;
    constexpr size_t n = 8;
    vertices.resize(n * 3);
    for (size_t i = 2; i < vertices.size(); i += 3) {
        vertices[i].x = pos.x;
        vertices[i].y = pos.y;
    }
    RebuildRim();
    SetColor(color);
    SetCenterColor(center_color);
}

void D3DTeardrop::SetColor(DWORD c)
{
    if (vertices.empty() || vertices[0].color == c) return;
    for (size_t i = 0; i < vertices.size(); i += 3) {
        vertices[i + 0].color = c;
        vertices[i + 1].color = c;
    }
    dirty = true;
}

void D3DTeardrop::SetCenterColor(DWORD c)
{
    if (vertices.empty() || vertices[2].color == c) return;
    for (size_t i = 2; i < vertices.size(); i += 3) {
        vertices[i].color = c;
    }
    dirty = true;
}

void D3DTeardrop::SetPosition(const D3DVec2f& pos)
{
    if (vertices.empty()) return;
    const float dx = pos.x - vertices[2].x;
    const float dy = pos.y - vertices[2].y;
    if (dx == 0.f && dy == 0.f) return;
    for (auto& v : vertices) {
        v.x += dx;
        v.y += dy;
    }
    dirty = true;
}
void D3DTeardrop::RebuildRim()
{
    constexpr std::pair<float, float> rim[] = {
        {1.8f, 0.0f}, {0.7f, 0.7f}, {0.0f, 1.0f}, {-0.7f, 0.7f}, {-1.0f, 0.0f}, {-0.7f, -0.7f}, {0.0f, -1.0f}, {0.7f, -0.7f},
    };
    constexpr size_t n = std::size(rim);
    const float cx = vertices[2].x;
    const float cy = vertices[2].y;
    for (size_t i = 0; i < n; i++) {
        const size_t base = i * 3;
        const auto& [ax, ay] = rim[i];
        const auto& [bx, by] = rim[(i + 1) % n];
        vertices[base + 0].x = cx + (ax * cos_r - ay * sin_r) * radius;
        vertices[base + 0].y = cy + (ax * sin_r + ay * cos_r) * radius;
        vertices[base + 1].x = cx + (bx * cos_r - by * sin_r) * radius;
        vertices[base + 1].y = cy + (bx * sin_r + by * cos_r) * radius;
    }
    dirty = true;
}
void D3DTeardrop::SetRadius(float r)
{
    if (vertices.empty() || radius == r) return;
    radius = r;
    RebuildRim();
}

void D3DTeardrop::SetRotation(float rotation)
{
    const float new_cos = cosf(rotation);
    const float new_sin = sinf(rotation);
    if (new_cos == cos_r && new_sin == sin_r) return;
    cos_r = new_cos;
    sin_r = new_sin;
    if (vertices.empty()) return;
    RebuildRim();
}

D3DFillCircle::D3DFillCircle(const D3DVec2f& center, float radius, DWORD color, DWORD center_color, int segments)
{
    type = D3DPT_TRIANGLELIST;
    vertices.resize(segments * 3);
    for (size_t i = 2; i < vertices.size(); i += 3) {
        vertices[i].x = center.x;
        vertices[i].y = center.y;
    }
    SetRadius(radius);
    SetColor(color);
    SetCenterColor(center_color);
}

void D3DFillCircle::SetColor(DWORD c)
{
    if (vertices.empty() || vertices[0].color == c) return;
    for (size_t i = 0; i < vertices.size(); i += 3) {
        vertices[i + 0].color = c;
        vertices[i + 1].color = c;
    }
    dirty = true;
}

void D3DFillCircle::SetCenterColor(DWORD c)
{
    if (vertices.empty() || vertices[2].color == c) return;
    for (size_t i = 2; i < vertices.size(); i += 3) {
        vertices[i].color = c;
    }
    dirty = true;
}

void D3DFillCircle::SetRadius(float r)
{
    if (vertices.empty() || vertices[0].x - vertices[2].x == r) return;
    const float cx = vertices[2].x;
    const float cy = vertices[2].y;
    const int n = static_cast<int>(vertices.size() / 3);
    D3DVec2f prev = {cx + r, cy};
    for (int i = 1; i <= n; i++) {
        const float a = static_cast<float>(i) / n * M_PI * 2.f;
        const D3DVec2f cur = {cx + r * cosf(a), cy + r * sinf(a)};
        const int base = (i - 1) * 3;
        vertices[base + 0].x = prev.x;
        vertices[base + 0].y = prev.y;
        vertices[base + 1].x = cur.x;
        vertices[base + 1].y = cur.y;
        prev = cur;
    }
    dirty = true;
}
D3DLineCircle::D3DLineCircle(float radius, DWORD color, int segments) {
    type = D3DPT_LINESTRIP;
    vertices.resize(segments + 1);
    for (int i = 0; i < segments; i++) {
        const float angle = static_cast<float>(i) * (M_PI * 2.f / segments);
        vertices[i] = {radius * cosf(angle), radius * sinf(angle), 0.f, color};
    }
    vertices[segments] = vertices[0];
}
void D3DLineCircle::SetColor(DWORD c)
{
    if (vertices.empty() || vertices[0].color == c) return;
    for (auto& v : vertices)
        v.color = c;
    dirty = true;
}
void D3DLineCircle::SetRadius(float r)
{
    if (vertices.empty() || vertices[0].x == r) return; // x of first vertex == radius at angle 0
    const int n = static_cast<int>(vertices.size()) - 1;
    for (int i = 0; i < n; i++) {
        const float angle = static_cast<float>(i) * (M_PI * 2.f / n);
        vertices[i].x = r * cosf(angle);
        vertices[i].y = r * sinf(angle);
    }
    vertices[n] = vertices[0];
    dirty = true;
}
