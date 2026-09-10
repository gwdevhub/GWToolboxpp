#include "D3DContainers.h"
#include "stdafx.h"
#include "Defines.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

D3DVertex::D3DVertex(const float x, const float y, const float z, const DWORD color) : x(x), y(y), z(z), color(color) {}

D3DVertex::D3DVertex(const float x, const float y, const DWORD color) : x(x), y(y), z(0.f), color(color) {}

D3DQuad::D3DQuad(const D3DVec2f& tl, const D3DVec2f& br, DWORD color)
{
    const D3DVertex TL{tl.x, tl.y, color};
    const D3DVertex TR{br.x, tl.y, color};
    const D3DVertex BR{br.x, br.y, color};
    const D3DVertex BL{tl.x, br.y, color};
    t[0] = {TL, TR, BR};
    t[1] = {TL, BR, BL};
}

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

D3DDiamond::D3DDiamond(const D3DVec2f& pos, float radius, DWORD color)
{
    const D3DVertex top{pos.x, pos.y + radius, color};
    const D3DVertex right{pos.x + radius, pos.y, color};
    const D3DVertex bot{pos.x, pos.y - radius, color};
    const D3DVertex left{pos.x - radius, pos.y, color};
    t[0] = {top, right, bot};
    t[1] = {top, bot, left};
}

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
    DEBUG_ASSERT(!buffer);
}

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

D3DMATRIX MakeOrthoProjection(float w, float h)
{
    D3DMATRIX m = {{
         2.f/w,   0.f,  0.f, 0.f,
          0.f,  -2.f/h, 0.f, 0.f,
          0.f,   0.f,  1.f, 0.f,
         -1.f,   1.f,  0.f, 1.f
    }};
    return m;
}

namespace {
    // Union of every render/texture-stage/sampler state written anywhere in GWToolboxdll.
    constexpr D3DRENDERSTATETYPE GUARDED_RENDER_STATES[] = {
        D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_ANTIALIASEDLINEENABLE, D3DRS_BLENDOP,
        D3DRS_CLIPPING, D3DRS_COLORWRITEENABLE, D3DRS_CULLMODE, D3DRS_DEPTHBIAS,
        D3DRS_DESTBLEND, D3DRS_DESTBLENDALPHA, D3DRS_FILLMODE, D3DRS_FOGENABLE,
        D3DRS_LIGHTING, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_RANGEFOGENABLE, D3DRS_SCISSORTESTENABLE,
        D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SHADEMODE, D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_SPECULARENABLE,
        D3DRS_SRCBLEND, D3DRS_SRCBLENDALPHA, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL,
        D3DRS_STENCILFUNC, D3DRS_STENCILMASK, D3DRS_STENCILPASS, D3DRS_STENCILREF,
        D3DRS_STENCILWRITEMASK, D3DRS_STENCILZFAIL, D3DRS_TEXTUREFACTOR, D3DRS_ZENABLE,
        D3DRS_ZFUNC, D3DRS_ZWRITEENABLE, D3DRS_SRGBWRITEENABLE,
    };
    constexpr D3DTEXTURESTAGESTATETYPE GUARDED_TEXTURE_STAGE_STATES[] = {
        D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
        D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2,
    };
    constexpr D3DSAMPLERSTATETYPE GUARDED_SAMPLER_STATES[] = {
        D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER,
    };
}

D3DStateGuard::D3DStateGuard(IDirect3DDevice9* dev) : device(dev)
{
    static_assert(std::size(GUARDED_RENDER_STATES) == std::extent_v<decltype(render_states)>);
    static_assert(std::size(GUARDED_TEXTURE_STAGE_STATES) == std::extent_v<decltype(texture_stage_states), 1>);
    static_assert(std::size(GUARDED_SAMPLER_STATES) == std::extent_v<decltype(sampler_states)>);

    for (size_t i = 0; i < std::size(GUARDED_RENDER_STATES); i++) {
        device->GetRenderState(GUARDED_RENDER_STATES[i], &render_states[i]);
    }
    for (DWORD stage = 0; stage < std::size(texture_stage_states); stage++) {
        for (size_t i = 0; i < std::size(GUARDED_TEXTURE_STAGE_STATES); i++) {
            device->GetTextureStageState(stage, GUARDED_TEXTURE_STAGE_STATES[i], &texture_stage_states[stage][i]);
        }
    }
    for (size_t i = 0; i < std::size(GUARDED_SAMPLER_STATES); i++) {
        device->GetSamplerState(0, GUARDED_SAMPLER_STATES[i], &sampler_states[i]);
    }
    device->GetTransform(D3DTS_WORLD, &world);
    device->GetTransform(D3DTS_VIEW, &view);
    device->GetTransform(D3DTS_PROJECTION, &projection);
    device->GetVertexShaderConstantF(0, vertex_shader_constants, 12);
    device->GetPixelShaderConstantF(0, pixel_shader_constants, 4);
    device->GetPixelShaderConstantB(0, &pixel_shader_bool, 1);
    device->GetTexture(0, &texture);
    device->GetVertexShader(&vertex_shader);
    device->GetPixelShader(&pixel_shader);
    device->GetVertexDeclaration(&vertex_declaration);
    device->GetIndices(&indices);
    device->GetStreamSource(0, &stream0, &stream0_offset, &stream0_stride);
    device->GetFVF(&fvf);
    device->GetViewport(&viewport);
    device->GetScissorRect(&scissor);
}

D3DStateGuard::~D3DStateGuard()
{
    for (size_t i = 0; i < std::size(GUARDED_RENDER_STATES); i++) {
        device->SetRenderState(GUARDED_RENDER_STATES[i], render_states[i]);
    }
    for (DWORD stage = 0; stage < std::size(texture_stage_states); stage++) {
        for (size_t i = 0; i < std::size(GUARDED_TEXTURE_STAGE_STATES); i++) {
            device->SetTextureStageState(stage, GUARDED_TEXTURE_STAGE_STATES[i], texture_stage_states[stage][i]);
        }
    }
    for (size_t i = 0; i < std::size(GUARDED_SAMPLER_STATES); i++) {
        device->SetSamplerState(0, GUARDED_SAMPLER_STATES[i], sampler_states[i]);
    }
    device->SetTransform(D3DTS_WORLD, &world);
    device->SetTransform(D3DTS_VIEW, &view);
    device->SetTransform(D3DTS_PROJECTION, &projection);
    device->SetVertexShaderConstantF(0, vertex_shader_constants, 12);
    device->SetPixelShaderConstantF(0, pixel_shader_constants, 4);
    device->SetPixelShaderConstantB(0, &pixel_shader_bool, 1);
    device->SetViewport(&viewport);
    device->SetScissorRect(&scissor);

    // SetFVF and SetVertexDeclaration overwrite each other; GetFVF reports 0 when a real
    // declaration (not an FVF-derived one) was bound, so that case has to win.
    if (fvf) {
        device->SetFVF(fvf);
    }
    else {
        device->SetVertexDeclaration(vertex_declaration);
    }

    // Each Get*() above added a reference; drop it once the state is restored.
    device->SetTexture(0, texture);
    if (texture) texture->Release();
    device->SetVertexShader(vertex_shader);
    if (vertex_shader) vertex_shader->Release();
    device->SetPixelShader(pixel_shader);
    if (pixel_shader) pixel_shader->Release();
    if (vertex_declaration) vertex_declaration->Release();
    device->SetIndices(indices);
    if (indices) indices->Release();
    device->SetStreamSource(0, stream0, stream0_offset, stream0_stride);
    if (stream0) stream0->Release();
}
