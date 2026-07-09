// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// RocketUI D3D9 Renderer
// Renders RmlUi through D3D9 calls that flow through DXVK's normal pipeline.
// Replaces the standalone Vulkan renderer (rocketrender_dxvk.cpp) to eliminate
// per-frame queue flushes and synchronization overhead.

#ifdef Assert
#undef Assert
#endif

#include "rocketrender.h"
#include <RmlUi/Core.h>

#include <d3d9.h>

#include <string.h>

RocketRenderD3D9 RocketRenderD3D9::m_Instance;

// Convert RmlUi Matrix4f to D3DMATRIX.
// RmlUi uses column-vector convention (v' = M*v), D3D9 uses row-vector (v' =
// v*M). D3D9 needs the mathematical transpose. For column-major storage, a raw
// memcpy into row-major D3DMATRIX naturally produces that transpose. For
// row-major storage, we must transpose explicitly.
static D3DMATRIX ToD3DMatrix(const Rml::Matrix4f &m) {
  D3DMATRIX out;
  const float *d = m.data();
  if constexpr (std::is_same_v<Rml::Matrix4f, Rml::ColumnMajorMatrix4f>) {
    memcpy(&out, d, sizeof(D3DMATRIX));
  } else {
    out._11 = d[0];
    out._12 = d[4];
    out._13 = d[8];
    out._14 = d[12];
    out._21 = d[1];
    out._22 = d[5];
    out._23 = d[9];
    out._24 = d[13];
    out._31 = d[2];
    out._32 = d[6];
    out._33 = d[10];
    out._34 = d[14];
    out._41 = d[3];
    out._42 = d[7];
    out._43 = d[11];
    out._44 = d[15];
  }
  return out;
}

static D3DMATRIX D3DMatMul(const D3DMATRIX &a, const D3DMATRIX &b) {
  D3DMATRIX r;
  const float *A = &a._11;
  const float *B = &b._11;
  float *R = &r._11;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      R[i * 4 + j] = A[i * 4 + 0] * B[0 * 4 + j] + A[i * 4 + 1] * B[1 * 4 + j] +
                     A[i * 4 + 2] * B[2 * 4 + j] + A[i * 4 + 3] * B[3 * 4 + j];
    }
  }
  return r;
}

static const D3DMATRIX kIdentity = {1, 0, 0, 0, 0, 1, 0, 0,
                                    0, 0, 1, 0, 0, 0, 0, 1};

static D3DMATRIX D3DMatTranspose(const D3DMATRIX &m) {
  D3DMATRIX t;
  const float *M = &m._11;
  float *T = &t._11;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      T[i * 4 + j] = M[j * 4 + i];
  return t;
}

// ----- SM 2.0 shader bytecodes (hand-assembled) -----
// These bypass DXVK's fixed-function shader generation, which has known
// issues producing wrong pixel values in some state combinations.
//
// DXVK register type encoding: 5-bit type split across token bits.
//   high 3 bits = (type & 7)   → token bits [30:28]
//   low  2 bits = (type >> 3)  → token bits [12:11]
// Register types used:
//   TEMP=0  INPUT=1  CONST=2  TEXTURE=3  RASTOUT=4
//   ATTROUT=5  TEXCRDOUT=6  COLOROUT=8  SAMPLER=10
//
// VS: transform position by c0-c3 (WVP matrix), pass through color & texcoord.
// PS textured:  output = tex2D(s0, t0) * v0
// PS untextured: output = v0
// clang-format off
static const DWORD kVSBytecode[] = {
  0xFFFE0200,                                       // vs_2_0
  0x0200001F, 0x80000000, 0x100F0000,               // dcl_position v0
  0x0200001F, 0x8000000A, 0x100F0001,               // dcl_color    v1
  0x0200001F, 0x80000005, 0x100F0002,               // dcl_texcoord v2
  0x03000009, 0x40010000, 0x90E40000, 0xA0E40000,   // dp4 oPos.x, v0, c0
  0x03000009, 0x40020000, 0x90E40000, 0xA0E40001,   // dp4 oPos.y, v0, c1
  0x03000009, 0x40040000, 0x90E40000, 0xA0E40002,   // dp4 oPos.z, v0, c2
  0x03000009, 0x40080000, 0x90E40000, 0xA0E40003,   // dp4 oPos.w, v0, c3
  0x02000001, 0x500F0000, 0x90E40001,               // mov oD0, v1
  0x02000001, 0x600F0000, 0x90E40002,               // mov oT0, v2
  0x0000FFFF                                         // end
};

static const DWORD kPSTexturedBytecode[] = {
  0xFFFF0200,                                       // ps_2_0
  0x0200001F, 0x90000000, 0x200F0800,               // dcl_2d s0
  0x0200001F, 0x80000005, 0x300F0000,               // dcl    t0
  0x0200001F, 0x8000000A, 0x100F0000,               // dcl    v0
  0x03000042, 0x000F0000, 0xB0E40000, 0xA0E40800,   // texld  r0, t0, s0
  0x03000005, 0x000F0000, 0x80E40000, 0x90E40000,   // mul    r0, r0, v0
  0x02000001, 0x000F0800, 0x80E40000,               // mov    oC0, r0
  0x0000FFFF                                         // end
};

static const DWORD kPSUntexturedBytecode[] = {
  0xFFFF0200,                                       // ps_2_0
  0x0200001F, 0x8000000A, 0x100F0000,               // dcl    v0
  0x02000001, 0x000F0800, 0x90E40000,               // mov    oC0, v0
  0x0000FFFF                                         // end
};
// clang-format on

RocketRenderD3D9::RocketRenderD3D9()
    : m_pDevice(nullptr), m_width(0), m_height(0), m_transformEnabled(false),
      m_frameActive(false), m_stencilRef(0), m_d3dTransform(kIdentity) {}

RocketRenderD3D9::~RocketRenderD3D9() { Shutdown(); }

bool RocketRenderD3D9::Initialize(IDirect3DDevice9 *pDevice) {
  m_pDevice = pDevice;
  return m_pDevice != nullptr;
}

void RocketRenderD3D9::Shutdown() {
  ReleaseResources();
  m_pDevice = nullptr;
}

void RocketRenderD3D9::ReleaseBackBuffer() { ReleaseResources(); }

void RocketRenderD3D9::Reinitialize(IDirect3DDevice9 *pDevice) {
  ReleaseResources();
  m_pDevice = pDevice;
}

void RocketRenderD3D9::SetScreenSize(int width, int height) {
  if (m_width != width || m_height != height)
    ReleaseResources();
  m_width = width;
  m_height = height;
}

void RocketRenderD3D9::ReleaseResources() {
  if (m_compositeQuad) {
    ReleaseGeometry(m_compositeQuad);
    m_compositeQuad = {};
    m_compositeQuadW = m_compositeQuadH = 0;
  }
  if (m_uiDS) {
    m_uiDS->Release();
    m_uiDS = nullptr;
  }
  if (m_uiSurface) {
    m_uiSurface->Release();
    m_uiSurface = nullptr;
  }
  if (m_uiTexture) {
    m_uiTexture->Release();
    m_uiTexture = nullptr;
  }
  m_uiTargetW = m_uiTargetH = 0;
  ReleaseShaders();
}

void RocketRenderD3D9::EnsureUITarget() {
  if (m_uiTexture && m_uiTargetW == m_width && m_uiTargetH == m_height)
    return;
  if (m_uiSurface) {
    m_uiSurface->Release();
    m_uiSurface = nullptr;
  }
  if (m_uiTexture) {
    m_uiTexture->Release();
    m_uiTexture = nullptr;
  }
  if (FAILED(m_pDevice->CreateTexture(m_width, m_height, 1,
                                      D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &m_uiTexture, nullptr)))
    return;
  m_uiTexture->GetSurfaceLevel(0, &m_uiSurface);
  m_uiTargetW = m_width;
  m_uiTargetH = m_height;
}

void RocketRenderD3D9::CreateShaders() {
  if (m_vs)
    return;
  if (FAILED(m_pDevice->CreateVertexShader(kVSBytecode, &m_vs)))
    return;
  if (FAILED(
          m_pDevice->CreatePixelShader(kPSTexturedBytecode, &m_psTextured)) ||
      FAILED(m_pDevice->CreatePixelShader(kPSUntexturedBytecode,
                                          &m_psUntextured))) {
    ReleaseShaders();
    return;
  }

  D3DVERTEXELEMENT9 elems[] = {{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
                                D3DDECLUSAGE_POSITION, 0},
                               {0, 12, D3DDECLTYPE_D3DCOLOR,
                                D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
                               {0, 16, D3DDECLTYPE_FLOAT2,
                                D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,
                                0},
                               D3DDECL_END()};
  if (FAILED(m_pDevice->CreateVertexDeclaration(elems, &m_vtxDecl))) {
    ReleaseShaders();
    return;
  }
}

void RocketRenderD3D9::ReleaseShaders() {
  if (m_vtxDecl) {
    m_vtxDecl->Release();
    m_vtxDecl = nullptr;
  }
  if (m_psUntextured) {
    m_psUntextured->Release();
    m_psUntextured = nullptr;
  }
  if (m_psTextured) {
    m_psTextured->Release();
    m_psTextured = nullptr;
  }
  if (m_vs) {
    m_vs->Release();
    m_vs = nullptr;
  }
}

void RocketRenderD3D9::UploadWVP(const D3DMATRIX &world) {
  // D3D9 row-vector convention: v' = v * WVP.
  // dp4(oPos.x, v, c0) = dot(v, c0), so c0 must be column 0 of WVP.
  // Upload the transpose so each register holds one column.
  D3DMATRIX wvp = D3DMatMul(world, m_d3dProjection);
  D3DMATRIX wvpT = D3DMatTranspose(wvp);
  m_pDevice->SetVertexShaderConstantF(0, &wvpT._11, 4);
}

void RocketRenderD3D9::SetupRenderState() {
  D3DVIEWPORT9 vp = {0, 0, (DWORD)m_width, (DWORD)m_height, 0.0f, 1.0f};
  m_pDevice->SetViewport(&vp);

  // Offset by 0.5 to cancel DXVK's D3D9 half-pixel viewport bias.
  Rml::Matrix4f projection = Rml::Matrix4f::ProjectOrtho(
      0.5f, (float)m_width + 0.5f, (float)m_height + 0.5f, 0.5f, -10000, 10000);
  m_d3dProjection = ToD3DMatrix(projection);

  // Precompute transposed projection — used directly by the per-draw fast
  // path and by composite blits (which use an identity world matrix).
  D3DMATRIX pt = D3DMatTranspose(m_d3dProjection);
  memcpy(m_projT, &pt._11, sizeof(m_projT));

  CreateShaders();

  // Rasterization
  m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
  m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
  m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  m_pDevice->SetRenderState(D3DRS_DITHERENABLE, FALSE);
  m_pDevice->SetRenderState(D3DRS_CLIPPING, FALSE);
  m_pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

  // Premultiplied alpha blend
  m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  m_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

  // Stencil: always pass initially
  m_pDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
  m_pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
  m_pDevice->SetRenderState(D3DRS_STENCILREF, 1);
  m_pDevice->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
  m_pDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
  m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
  m_pDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
  m_pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);

  // DXVK ATOC: reset POINTSIZE/ADAPTIVETESS_Y BEFORE ALPHATESTENABLE,
  // otherwise the AMD ATOC path stays active based on stale POINTSIZE alone.
  m_pDevice->SetRenderState(D3DRS_POINTSIZE, 0x3F800000u); // 1.0f
  m_pDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, 0);
  m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

  // Misc game state that bleeds into the Vulkan pipeline
  m_pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
  m_pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
  m_pDevice->SetRenderState(
      D3DRS_COLORWRITEENABLE,
      D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
          D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

  // Sampler 0
  m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
  m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  m_pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

  // Bind shaders and vertex layout
  m_pDevice->SetVertexShader(m_vs);
  m_pDevice->SetPixelShader(m_psTextured);
  if (m_vtxDecl)
    m_pDevice->SetVertexDeclaration(m_vtxDecl);

  m_transformEnabled = false;
  m_stencilRef = 0;
}

void RocketRenderD3D9::BeginFrame() {
  if (!m_pDevice)
    return;

  // Save the real backbuffer surfaces (AddRef'd by Get*).
  m_pDevice->GetRenderTarget(0, &m_realBackbufferRT);
  m_pDevice->GetDepthStencilSurface(&m_realBackbufferDS);

  // Ensure the non-MSAA UI render target exists.
  EnsureUITarget();

  // Non-MSAA depth-stencil for clip masks on the UI target.
  if (!m_uiDS) {
    m_pDevice->CreateDepthStencilSurface(m_width, m_height, D3DFMT_D24S8,
                                         D3DMULTISAMPLE_NONE, 0, TRUE, &m_uiDS,
                                         nullptr);
  }

  // Disable scissor — the game may have left it enabled,
  // and D3D9's Clear only clears within the scissor rect when active.
  m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

  if (m_uiSurface) {
    m_pDevice->SetRenderTarget(0, m_uiSurface);
    m_pDevice->SetDepthStencilSurface(m_uiDS);
    m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_STENCIL,
                     D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
  } else {
    m_pDevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
  }

  SetupRenderState();
  m_frameActive = true;
}

void RocketRenderD3D9::EndFrame() {
  if (!m_frameActive)
    return;

  // --- Composite the non-MSAA UI target onto the real backbuffer ----------
  if (m_uiSurface && m_realBackbufferRT) {
    m_pDevice->SetRenderTarget(0, m_realBackbufferRT);
    if (m_realBackbufferDS)
      m_pDevice->SetDepthStencilSurface(m_realBackbufferDS);

    // Only scissor/stencil may have changed since BeginFrame's
    // SetupRenderState.
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    EnsureCompositeQuad();
    if (m_compositeQuad) {
      m_pDevice->SetTexture(0, m_uiTexture);
      m_pDevice->SetPixelShader(m_psTextured);
      m_pDevice->SetVertexShaderConstantF(0, m_projT, 4);

      GeometryData *geom = reinterpret_cast<GeometryData *>(m_compositeQuad);
      m_pDevice->SetStreamSource(0, geom->vb, 0, sizeof(D3DVertex));
      m_pDevice->SetIndices(geom->ib);
      m_pDevice->DrawIndexedPrimitive(
          D3DPT_TRIANGLELIST, 0, 0, geom->vertexCount, 0, geom->indexCount / 3);
    }
  }

  // Release the real backbuffer references (AddRef'd by Get* in BeginFrame).
  if (m_realBackbufferRT) {
    m_realBackbufferRT->Release();
    m_realBackbufferRT = nullptr;
  }
  if (m_realBackbufferDS) {
    m_realBackbufferDS->Release();
    m_realBackbufferDS = nullptr;
  }
  m_frameActive = false;
}

// --- Geometry ---

Rml::CompiledGeometryHandle
RocketRenderD3D9::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                  Rml::Span<const int> indices) {
  if (!m_pDevice)
    return {};

  const UINT vbSize = (UINT)vertices.size() * sizeof(D3DVertex);
  const UINT ibSize = (UINT)indices.size() * sizeof(int);

  IDirect3DVertexBuffer9 *vb = nullptr;
  IDirect3DIndexBuffer9 *ib = nullptr;

  if (FAILED(m_pDevice->CreateVertexBuffer(vbSize, D3DUSAGE_WRITEONLY, 0,
                                           D3DPOOL_MANAGED, &vb, nullptr)))
    return {};

  if (FAILED(m_pDevice->CreateIndexBuffer(ibSize, D3DUSAGE_WRITEONLY,
                                          D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib,
                                          nullptr))) {
    vb->Release();
    return {};
  }

  // Fill vertex buffer
  void *vbData = nullptr;
  if (FAILED(vb->Lock(0, vbSize, &vbData, 0))) {
    vb->Release();
    ib->Release();
    return {};
  }
  D3DVertex *dst = (D3DVertex *)vbData;
  for (size_t i = 0; i < vertices.size(); i++) {
    const Rml::Vertex &v = vertices[i];
    dst[i].x = v.position.x;
    dst[i].y = v.position.y;
    dst[i].z = 0.0f;
    // RmlUi RGBA -> D3DCOLOR ARGB (BGRA in memory)
    dst[i].color = ((DWORD)v.colour.alpha << 24) | ((DWORD)v.colour.red << 16) |
                   ((DWORD)v.colour.green << 8) | ((DWORD)v.colour.blue);
    dst[i].u = v.tex_coord.x;
    dst[i].v = v.tex_coord.y;
  }
  vb->Unlock();

  // Fill index buffer
  void *ibData = nullptr;
  if (FAILED(ib->Lock(0, ibSize, &ibData, 0))) {
    vb->Release();
    ib->Release();
    return {};
  }
  memcpy(ibData, indices.data(), ibSize);
  ib->Unlock();

  GeometryData *data = new GeometryData;
  data->vb = vb;
  data->ib = ib;
  data->vertexCount = (UINT)vertices.size();
  data->indexCount = (UINT)indices.size();

  return reinterpret_cast<Rml::CompiledGeometryHandle>(data);
}

void RocketRenderD3D9::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                      Rml::Vector2f translation,
                                      Rml::TextureHandle texture) {
  GeometryData *geom = reinterpret_cast<GeometryData *>(handle);
  if (!geom || !m_pDevice)
    return;

  if (m_transformEnabled) {
    D3DMATRIX translate = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, translation.x, translation.y, 0, 1};
    UploadWVP(D3DMatMul(translate, m_d3dTransform));
  } else {
    // Fast path: transpose(translate * proj) only differs from projT in
    // column 3 — avoids a full 4x4 multiply + transpose per draw.
    float c[16];
    memcpy(c, m_projT, sizeof(c));
    float tx = translation.x, ty = translation.y;
    for (int i = 0; i < 16; i += 4)
      c[i + 3] += c[i] * tx + c[i + 1] * ty;
    m_pDevice->SetVertexShaderConstantF(0, c, 4);
  }

  // Texture setup — switch pixel shader instead of texture stage ops.
  if (!texture) {
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetPixelShader(m_psUntextured);
  } else {
    if (texture != TextureEnableWithoutBinding)
      m_pDevice->SetTexture(0, (IDirect3DTexture9 *)texture);
    m_pDevice->SetPixelShader(m_psTextured);
  }

  m_pDevice->SetStreamSource(0, geom->vb, 0, sizeof(D3DVertex));
  m_pDevice->SetIndices(geom->ib);
  m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, geom->vertexCount,
                                  0, geom->indexCount / 3);
}

void RocketRenderD3D9::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
  GeometryData *data = reinterpret_cast<GeometryData *>(geometry);
  if (data) {
    if (data->vb)
      data->vb->Release();
    if (data->ib)
      data->ib->Release();
    delete data;
  }
}

// --- Textures ---

#pragma pack(1)
struct TGAHeader {
  char idLength;
  char colourMapType;
  char dataType;
  short int colourMapOrigin;
  short int colourMapLength;
  char colourMapDepth;
  short int xOrigin;
  short int yOrigin;
  short int width;
  short int height;
  char bitsPerPixel;
  char imageDescriptor;
};
#pragma pack()

Rml::TextureHandle
RocketRenderD3D9::LoadTexture(Rml::Vector2i &texture_dimensions,
                              const Rml::String &source) {
  Rml::FileInterface *file_interface = Rml::GetFileInterface();
  Rml::FileHandle file_handle = file_interface->Open(source);
  if (!file_handle)
    return {};

  file_interface->Seek(file_handle, 0, SEEK_END);
  size_t buffer_size = file_interface->Tell(file_handle);
  file_interface->Seek(file_handle, 0, SEEK_SET);

  if (buffer_size <= sizeof(TGAHeader)) {
    file_interface->Close(file_handle);
    return {};
  }

  using Rml::byte;
  Rml::UniquePtr<byte[]> buffer(new byte[buffer_size]);
  file_interface->Read(buffer.get(), buffer_size, file_handle);
  file_interface->Close(file_handle);

  TGAHeader header;
  memcpy(&header, buffer.get(), sizeof(TGAHeader));

  int color_mode = header.bitsPerPixel / 8;
  const size_t image_size = header.width * header.height * 4;

  if (header.dataType != 2 || color_mode < 3)
    return {};

  const byte *image_src = buffer.get() + sizeof(TGAHeader);
  Rml::UniquePtr<byte[]> image_dest_buffer(new byte[image_size]);
  byte *image_dest = image_dest_buffer.get();
  const bool top_to_bottom = ((header.imageDescriptor & 32) != 0);

  for (long y = 0; y < header.height; y++) {
    long read_index = y * header.width * color_mode;
    long write_index = top_to_bottom
                           ? (y * header.width * 4)
                           : (header.height - y - 1) * header.width * 4;
    for (long x = 0; x < header.width; x++) {
      image_dest[write_index] = image_src[read_index + 2];
      image_dest[write_index + 1] = image_src[read_index + 1];
      image_dest[write_index + 2] = image_src[read_index];
      if (color_mode == 4) {
        const byte alpha = image_src[read_index + 3];
        for (size_t j = 0; j < 3; j++)
          image_dest[write_index + j] =
              byte((image_dest[write_index + j] * alpha) / 255);
        image_dest[write_index + 3] = alpha;
      } else {
        image_dest[write_index + 3] = 255;
      }
      write_index += 4;
      read_index += color_mode;
    }
  }

  texture_dimensions.x = header.width;
  texture_dimensions.y = header.height;
  return GenerateTexture({image_dest_buffer.get(), image_size},
                         texture_dimensions);
}

Rml::TextureHandle
RocketRenderD3D9::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                  Rml::Vector2i dimensions) {
  if (!m_pDevice)
    return {};

  IDirect3DTexture9 *texture = nullptr;
  HRESULT hr = m_pDevice->CreateTexture(dimensions.x, dimensions.y, 1, 0,
                                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                        &texture, nullptr);
  if (FAILED(hr))
    return {};

  D3DLOCKED_RECT locked;
  hr = texture->LockRect(0, &locked, nullptr, 0);
  if (FAILED(hr)) {
    texture->Release();
    return {};
  }

  const Rml::byte *src = source_data.data();
  for (int y = 0; y < dimensions.y; y++) {
    unsigned char *dest = (unsigned char *)locked.pBits + y * locked.Pitch;
    for (int x = 0; x < dimensions.x; x++) {
      dest[0] = src[2]; // B
      dest[1] = src[1]; // G
      dest[2] = src[0]; // R
      dest[3] = src[3]; // A
      src += 4;
      dest += 4;
    }
  }

  texture->UnlockRect(0);
  return (Rml::TextureHandle)texture;
}

void RocketRenderD3D9::ReleaseTexture(Rml::TextureHandle texture_handle) {
  if (texture_handle)
    ((IDirect3DTexture9 *)texture_handle)->Release();
}

// --- Scissor ---

void RocketRenderD3D9::EnableScissorRegion(bool enable) {
  if (m_pDevice)
    m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, enable ? TRUE : FALSE);
}

void RocketRenderD3D9::SetScissorRegion(Rml::Rectanglei region) {
  if (!m_pDevice)
    return;
  RECT rect;
  rect.left = region.Left();
  rect.top = region.Top();
  rect.right = region.Right();
  rect.bottom = region.Bottom();
  m_pDevice->SetScissorRect(&rect);
}

// --- Clip Masks (stencil) ---

void RocketRenderD3D9::EnableClipMask(bool enable) {
  if (!m_pDevice)
    return;
  if (enable) {
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
  } else {
    m_pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  }
}

void RocketRenderD3D9::RenderToClipMask(Rml::ClipMaskOperation operation,
                                        Rml::CompiledGeometryHandle geometry,
                                        Rml::Vector2f translation) {
  if (!m_pDevice)
    return;

  using Rml::ClipMaskOperation;

  const bool clear_stencil = (operation == ClipMaskOperation::Set ||
                              operation == ClipMaskOperation::SetInverse);
  if (clear_stencil) {
    m_pDevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
  }

  // Disable color writes
  m_pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
  m_pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);

  switch (operation) {
  case ClipMaskOperation::Set:
    m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    m_pDevice->SetRenderState(D3DRS_STENCILREF, 1);
    m_stencilRef = 1;
    break;
  case ClipMaskOperation::SetInverse:
    m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    m_pDevice->SetRenderState(D3DRS_STENCILREF, 1);
    m_stencilRef = 0;
    break;
  case ClipMaskOperation::Intersect:
    m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
    m_stencilRef += 1;
    break;
  }

  RenderGeometry(geometry, translation, {});

  // Restore color writes and stencil test
  m_pDevice->SetRenderState(
      D3DRS_COLORWRITEENABLE,
      D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
          D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
  m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
  m_pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
  m_pDevice->SetRenderState(D3DRS_STENCILREF, m_stencilRef);
}

// --- Transform ---

void RocketRenderD3D9::SetTransform(const Rml::Matrix4f *transform) {
  m_transformEnabled = (transform != nullptr);
  if (transform)
    m_d3dTransform = ToD3DMatrix(*transform);
  else
    m_d3dTransform = kIdentity;
}

void RocketRenderD3D9::EnsureCompositeQuad() {
  if (m_compositeQuad && m_compositeQuadW == m_width &&
      m_compositeQuadH == m_height)
    return;
  if (m_compositeQuad)
    ReleaseGeometry(m_compositeQuad);

  float w = (float)m_width, h = (float)m_height;
  Rml::Vertex verts[4];
  verts[0].position = {0, 0};
  verts[0].colour = {255, 255, 255, 255};
  verts[0].tex_coord = {0, 0};
  verts[1].position = {w, 0};
  verts[1].colour = {255, 255, 255, 255};
  verts[1].tex_coord = {1, 0};
  verts[2].position = {w, h};
  verts[2].colour = {255, 255, 255, 255};
  verts[2].tex_coord = {1, 1};
  verts[3].position = {0, h};
  verts[3].colour = {255, 255, 255, 255};
  verts[3].tex_coord = {0, 1};
  int indices[6] = {0, 1, 2, 0, 2, 3};
  m_compositeQuad = CompileGeometry({verts, 4}, {indices, 6});
  m_compositeQuadW = m_width;
  m_compositeQuadH = m_height;
}
