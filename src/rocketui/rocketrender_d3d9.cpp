// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// RocketUI D3D9 Renderer
// Renders RmlUi through D3D9 calls that flow through DXVK's normal pipeline.
// Replaces the standalone Vulkan renderer (rocketrender_dxvk.cpp) to eliminate
// per-frame queue flushes and synchronization overhead.
//
// RmlUi runs single-threaded on the MAIN thread. The Rml::RenderInterface
// overrides here only RECORD (allocate CPU-side handles / append POD commands /
// stage releases); the actual D3D9 work happens on the RENDER thread in
// Replay(), which owns the finished command list. See rocketrender.h.

#ifdef Assert
#undef Assert
#endif

#include "rocketrender.h"
#include "rocketfilesystem.h"
#include <RmlUi/Core.h>

#include <d3d9.h>

#include <stdio.h>
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

// ----- Handle + command types (implementation detail) -----

struct D3DVertex {
  float x, y, z;
  DWORD color; // D3DCOLOR (BGRA)
  float u, v;
};

// RmlUi geometry handle. CPU data is retained for the handle's lifetime so the
// render thread can (re)create the VB/IB — lazily on first draw and again after
// a genuine device re-creation (gen mismatch).
struct RocketGeometry {
  std::vector<D3DVertex> verts;
  std::vector<int> indices;
  IDirect3DVertexBuffer9 *vb = nullptr;
  IDirect3DIndexBuffer9 *ib = nullptr;
  uint32_t gen = 0; // device generation vb/ib belong to (0 = none yet)
};

// RmlUi texture handle. Pixels stored BGRA (A8R8G8B8), retained for lifetime.
struct RocketTexture {
  int w = 0, h = 0;
  std::vector<unsigned char> pixels;
  IDirect3DTexture9 *tex = nullptr;
  uint32_t gen = 0;
};

enum class UICmdType : uint8_t {
  Draw,
  ScissorEnable,
  ScissorRegion,
  ClipEnable,
  ClipRender,
  Transform,
};

struct UICmd {
  UICmdType type;
  RocketGeometry *geom = nullptr;    // Draw, ClipRender
  Rml::TextureHandle texture = 0;    // Draw (0 / -1 sentinel / RocketTexture*)
  Rml::Vector2f translation;         // Draw, ClipRender
  Rml::Rectanglei region;            // ScissorRegion
  bool enable = false;               // ScissorEnable, ClipEnable
  Rml::ClipMaskOperation op{};       // ClipRender
  bool hasTransform = false;         // Transform
  Rml::Matrix4f transform;           // Transform
};

// A frame's worth of recorded UI commands for one context. Built on the main
// thread, ownership handed to the render thread, deleted after replay.
// ponytail: flat UICmd (Matrix4f in every cmd) wastes a little memory; use a
// side vector for transforms only if it ever shows in a profile.
struct RocketCmdList {
  uint32_t frame;
  std::vector<UICmd> cmds;
  explicit RocketCmdList(uint32_t f) : frame(f) {}
};

// ----- lifecycle -----

RocketRenderD3D9::RocketRenderD3D9()
    : m_pDevice(nullptr), m_width(0), m_height(0), m_transformEnabled(false),
      m_hasStencil(false), m_stencilRef(0), m_d3dTransform(kIdentity) {}

RocketRenderD3D9::~RocketRenderD3D9() { Shutdown(); }

bool RocketRenderD3D9::Initialize(IDirect3DDevice9 *pDevice) {
  m_pDevice = pDevice;
  return m_pDevice != nullptr;
}

void RocketRenderD3D9::Shutdown() {
  ReleaseResources();
  // Free any handles still staged for release (nothing is in flight at
  // shutdown). Live handles are released by Rml::Shutdown() before this.
  DrainReleased(0xFFFFFFFFu);
  m_pDevice = nullptr;
}

void RocketRenderD3D9::Reinitialize(IDirect3DDevice9 *pDevice) {
  ReleaseResources();
  m_pDevice = pDevice;
}

void RocketRenderD3D9::ReleaseResources() { ReleaseShaders(); }

void RocketRenderD3D9::SetScreenSize(int width, int height) {
  m_width = width;
  m_height = height;
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

  // Stencil: always pass initially (only when a stencil buffer is present).
  m_pDevice->SetRenderState(D3DRS_STENCILENABLE, m_hasStencil ? TRUE : FALSE);
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

  // Render directly onto the currently bound (non-MSAA) backbuffer and its
  // depth-stencil. Clear only stencil for clip masks — never color/depth, so
  // the 3D scene underneath is preserved. Holding no offscreen D3DPOOL_DEFAULT
  // target means device resets (e.g. resolution changes) never stall on alive
  // losable resources.
  m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

  // Clip masks need a stencil buffer. Detect whether the engine's bound DS has
  // one: if not, skip the (otherwise-failing) stencil clear and fall back to
  // scissor-only clipping. Also warn once if RT0 is MSAA — the swapchain is
  // non-MSAA by construction, so this should never fire, but a stray log line
  // beats silent corruption if that assumption ever breaks.
  m_hasStencil = false;
  IDirect3DSurface9 *ds = nullptr;
  if (SUCCEEDED(m_pDevice->GetDepthStencilSurface(&ds)) && ds) {
    D3DSURFACE_DESC d;
    if (SUCCEEDED(ds->GetDesc(&d)))
      m_hasStencil =
          (d.Format == D3DFMT_D24S8 || d.Format == D3DFMT_D24FS8 ||
           d.Format == D3DFMT_D24X4S4 || d.Format == D3DFMT_D15S1 ||
           d.Format == D3DFMT_S8_LOCKABLE);
    ds->Release();
  }

  static bool s_warnedMSAA = false;
  if (!s_warnedMSAA) {
    IDirect3DSurface9 *rt = nullptr;
    if (SUCCEEDED(m_pDevice->GetRenderTarget(0, &rt)) && rt) {
      D3DSURFACE_DESC d;
      if (SUCCEEDED(rt->GetDesc(&d)) && d.MultiSampleType != D3DMULTISAMPLE_NONE) {
        // Render thread: must NOT call into RmlUi (Rml::Log reads shared RmlUi
        // state) — keep this diagnostic on a thread-safe channel so the
        // "replay never touches RmlUi" invariant stays literally true.
        fprintf(stderr, "RocketUI: rendering onto an MSAA render target; "
                        "UI/clip-mask results may be incorrect.\n");
        s_warnedMSAA = true;
      }
      rt->Release();
    }
  }

  if (m_hasStencil)
    m_pDevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);

  SetupRenderState();
}

void RocketRenderD3D9::EndFrame() {}

// ============================================================================
// Recording (MAIN thread) — no device work here.
// ============================================================================

void *RocketRenderD3D9::BeginRecord() {
  RocketCmdList *list = new RocketCmdList(m_frame);
  m_target = list;
  return list;
}

Rml::CompiledGeometryHandle
RocketRenderD3D9::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                  Rml::Span<const int> indices) {
  RocketGeometry *g = new RocketGeometry;
  g->verts.resize(vertices.size());
  for (size_t i = 0; i < vertices.size(); i++) {
    const Rml::Vertex &v = vertices[i];
    D3DVertex &d = g->verts[i];
    d.x = v.position.x;
    d.y = v.position.y;
    d.z = 0.0f;
    // RmlUi RGBA -> D3DCOLOR ARGB (BGRA in memory)
    d.color = ((DWORD)v.colour.alpha << 24) | ((DWORD)v.colour.red << 16) |
              ((DWORD)v.colour.green << 8) | ((DWORD)v.colour.blue);
    d.u = v.tex_coord.x;
    d.v = v.tex_coord.y;
  }
  g->indices.assign(indices.data(), indices.data() + indices.size());
  return reinterpret_cast<Rml::CompiledGeometryHandle>(g);
}

void RocketRenderD3D9::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                      Rml::Vector2f translation,
                                      Rml::TextureHandle texture) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::Draw;
  c.geom = reinterpret_cast<RocketGeometry *>(handle);
  c.texture = texture;
  c.translation = translation;
  m_target->cmds.push_back(c);
}

void RocketRenderD3D9::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
  RocketGeometry *g = reinterpret_cast<RocketGeometry *>(geometry);
  if (!g)
    return;
  std::lock_guard<std::mutex> lk(m_deadMutex);
  m_deadGeom.emplace_back(g, m_frame);
}

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
  // Reuse original engine sprites directly: a ".vtf" source is decoded via the
  // engine's VTF loader (which also reads from VPKs), so RmlUi documents can
  // reference e.g. "materials/sprites/scope_arc.vtf".
  if (source.size() >= 4 && source.compare(source.size() - 4, 4, ".vtf") == 0) {
    // A ".t.vtf" suffix requests a transposed (90°-rotated) copy of the sprite,
    // so a horizontal line can reuse a vertically-oriented sprite without
    // rotating the element (the image decorator can't reorient texture UVs).
    bool transpose = false;
    Rml::String path = source;
    const size_t tlen = 6; // ".t.vtf"
    if (source.size() >= tlen &&
        source.compare(source.size() - tlen, tlen, ".t.vtf") == 0) {
      transpose = true;
      path = source.substr(0, source.size() - tlen) + ".vtf";
    }
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    if (!RocketLoadVTF_RGBA(path.c_str(), rgba, w, h))
      return {};
    if (transpose) {
      std::vector<unsigned char> t((size_t)w * h * 4);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
          const unsigned char *s = &rgba[((size_t)y * w + x) * 4];
          unsigned char *d = &t[((size_t)x * h + y) * 4];
          d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
      rgba.swap(t);
      std::swap(w, h);
    }
    texture_dimensions.x = w;
    texture_dimensions.y = h;
    return GenerateTexture({rgba.data(), rgba.size()}, texture_dimensions);
  }

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
  RocketTexture *t = new RocketTexture;
  t->w = dimensions.x;
  t->h = dimensions.y;
  const size_t n = (size_t)dimensions.x * (size_t)dimensions.y;
  t->pixels.resize(n * 4);
  const Rml::byte *src = source_data.data();
  unsigned char *dst = t->pixels.data();
  // RmlUi RGBA -> D3D A8R8G8B8 (BGRA byte order). Done here on the main thread
  // so the render thread's upload is a plain row memcpy.
  for (size_t i = 0; i < n; i++) {
    dst[0] = src[2]; // B
    dst[1] = src[1]; // G
    dst[2] = src[0]; // R
    dst[3] = src[3]; // A
    src += 4;
    dst += 4;
  }
  return reinterpret_cast<Rml::TextureHandle>(t);
}

void RocketRenderD3D9::ReleaseTexture(Rml::TextureHandle texture_handle) {
  if (!texture_handle || texture_handle == TextureEnableWithoutBinding)
    return;
  RocketTexture *t = reinterpret_cast<RocketTexture *>(texture_handle);
  std::lock_guard<std::mutex> lk(m_deadMutex);
  m_deadTex.emplace_back(t, m_frame);
}

void RocketRenderD3D9::EnableScissorRegion(bool enable) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::ScissorEnable;
  c.enable = enable;
  m_target->cmds.push_back(c);
}

void RocketRenderD3D9::SetScissorRegion(Rml::Rectanglei region) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::ScissorRegion;
  c.region = region;
  m_target->cmds.push_back(c);
}

void RocketRenderD3D9::EnableClipMask(bool enable) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::ClipEnable;
  c.enable = enable;
  m_target->cmds.push_back(c);
}

void RocketRenderD3D9::RenderToClipMask(Rml::ClipMaskOperation operation,
                                        Rml::CompiledGeometryHandle geometry,
                                        Rml::Vector2f translation) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::ClipRender;
  c.op = operation;
  c.geom = reinterpret_cast<RocketGeometry *>(geometry);
  c.translation = translation;
  m_target->cmds.push_back(c);
}

void RocketRenderD3D9::SetTransform(const Rml::Matrix4f *transform) {
  if (!m_target)
    return;
  UICmd c;
  c.type = UICmdType::Transform;
  c.hasTransform = (transform != nullptr);
  if (transform)
    c.transform = *transform;
  m_target->cmds.push_back(c);
}

// ============================================================================
// Replay (RENDER thread) — the actual device work.
// ============================================================================

bool RocketRenderD3D9::EnsureGeometry(RocketGeometry *g) {
  if (!g || !m_pDevice)
    return false;
  const uint32_t gen = m_deviceGen.load(std::memory_order_relaxed);
  if (g->vb && g->ib && g->gen == gen)
    return true; // already valid on this device

  // Stale (device re-created) or not yet uploaded: (re)create from CPU data.
  if (g->vb) {
    g->vb->Release();
    g->vb = nullptr;
  }
  if (g->ib) {
    g->ib->Release();
    g->ib = nullptr;
  }
  if (g->verts.empty() || g->indices.empty())
    return false;

  const UINT vbSize = (UINT)g->verts.size() * sizeof(D3DVertex);
  const UINT ibSize = (UINT)g->indices.size() * sizeof(int);
  IDirect3DVertexBuffer9 *vb = nullptr;
  IDirect3DIndexBuffer9 *ib = nullptr;
  if (FAILED(m_pDevice->CreateVertexBuffer(vbSize, D3DUSAGE_WRITEONLY, 0,
                                           D3DPOOL_MANAGED, &vb, nullptr)))
    return false;
  if (FAILED(m_pDevice->CreateIndexBuffer(ibSize, D3DUSAGE_WRITEONLY,
                                          D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib,
                                          nullptr))) {
    vb->Release();
    return false;
  }
  void *dst = nullptr;
  if (FAILED(vb->Lock(0, vbSize, &dst, 0))) {
    vb->Release();
    ib->Release();
    return false;
  }
  memcpy(dst, g->verts.data(), vbSize);
  vb->Unlock();
  if (FAILED(ib->Lock(0, ibSize, &dst, 0))) {
    vb->Release();
    ib->Release();
    return false;
  }
  memcpy(dst, g->indices.data(), ibSize);
  ib->Unlock();

  g->vb = vb;
  g->ib = ib;
  g->gen = gen;
  return true;
}

bool RocketRenderD3D9::EnsureTexture(RocketTexture *t) {
  if (!t || !m_pDevice)
    return false;
  const uint32_t gen = m_deviceGen.load(std::memory_order_relaxed);
  if (t->tex && t->gen == gen)
    return true;

  if (t->tex) {
    t->tex->Release();
    t->tex = nullptr;
  }
  if (t->pixels.empty() || t->w <= 0 || t->h <= 0)
    return false;

  IDirect3DTexture9 *tex = nullptr;
  if (FAILED(m_pDevice->CreateTexture(t->w, t->h, 1, 0, D3DFMT_A8R8G8B8,
                                      D3DPOOL_MANAGED, &tex, nullptr)))
    return false;
  D3DLOCKED_RECT lr;
  if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
    tex->Release();
    return false;
  }
  const unsigned char *src = t->pixels.data();
  const size_t rowBytes = (size_t)t->w * 4;
  for (int y = 0; y < t->h; y++)
    memcpy((unsigned char *)lr.pBits + (size_t)y * lr.Pitch,
           src + (size_t)y * rowBytes, rowBytes);
  tex->UnlockRect(0);

  t->tex = tex;
  t->gen = gen;
  return true;
}

void RocketRenderD3D9::DrawGeometryDev(RocketGeometry *g,
                                       Rml::Vector2f translation,
                                       Rml::TextureHandle texture) {
  if (!g || !m_pDevice)
    return;
  if (!EnsureGeometry(g))
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

  // Texture: 0 = none, -1 = enable textured shader without binding, else a
  // RocketTexture*.
  if (!texture) {
    m_pDevice->SetTexture(0, nullptr);
    m_pDevice->SetPixelShader(m_psUntextured);
  } else {
    if (texture != TextureEnableWithoutBinding) {
      RocketTexture *t = reinterpret_cast<RocketTexture *>(texture);
      if (!EnsureTexture(t))
        return;
      m_pDevice->SetTexture(0, t->tex);
    }
    m_pDevice->SetPixelShader(m_psTextured);
  }

  m_pDevice->SetStreamSource(0, g->vb, 0, sizeof(D3DVertex));
  m_pDevice->SetIndices(g->ib);
  m_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                  (UINT)g->verts.size(), 0,
                                  (UINT)g->indices.size() / 3);
}

void RocketRenderD3D9::RenderToClipMaskDev(Rml::ClipMaskOperation operation,
                                           RocketGeometry *g,
                                           Rml::Vector2f translation) {
  if (!m_pDevice || !m_hasStencil)
    return; // No stencil buffer: skip mask building, content renders unclipped.

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
    // NOTE: saturating INCR on an 8-bit stencil; >255 nested clips would
    // desync ref vs buffer. Not a concern for current UI depth.
    m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
    m_stencilRef += 1;
    break;
  }

  DrawGeometryDev(g, translation, {});

  // Restore color writes and stencil test
  m_pDevice->SetRenderState(
      D3DRS_COLORWRITEENABLE,
      D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
          D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
  m_pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
  m_pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
  m_pDevice->SetRenderState(D3DRS_STENCILREF, m_stencilRef);
}

void RocketRenderD3D9::Replay(void *p) {
  RocketCmdList *list = reinterpret_cast<RocketCmdList *>(p);
  if (!list || !m_pDevice)
    return;

  for (const UICmd &c : list->cmds) {
    switch (c.type) {
    case UICmdType::Draw:
      DrawGeometryDev(c.geom, c.translation, c.texture);
      break;
    case UICmdType::ScissorEnable:
      m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,
                                c.enable ? TRUE : FALSE);
      break;
    case UICmdType::ScissorRegion: {
      RECT rect = {c.region.Left(), c.region.Top(), c.region.Right(),
                   c.region.Bottom()};
      m_pDevice->SetScissorRect(&rect);
      break;
    }
    case UICmdType::ClipEnable:
      if (m_hasStencil)
        m_pDevice->SetRenderState(D3DRS_STENCILENABLE,
                                  c.enable ? TRUE : FALSE);
      break;
    case UICmdType::ClipRender:
      RenderToClipMaskDev(c.op, c.geom, c.translation);
      break;
    case UICmdType::Transform:
      m_transformEnabled = c.hasTransform;
      m_d3dTransform = c.hasTransform ? ToD3DMatrix(c.transform) : kIdentity;
      break;
    }
  }
}

void RocketRenderD3D9::DrainReleased(uint32_t listFrame) {
  // A handle released in main-frame R had its last recorded draw in a list of
  // frame <= R-1, which is fully replayed before any list of frame > R. So
  // freeing entries with releaseFrame < listFrame can never hit a handle the
  // current or a later in-flight list still references.
  std::vector<RocketGeometry *> geom;
  std::vector<RocketTexture *> tex;
  {
    std::lock_guard<std::mutex> lk(m_deadMutex);
    for (size_t i = 0; i < m_deadGeom.size();) {
      if (m_deadGeom[i].second < listFrame) {
        geom.push_back(m_deadGeom[i].first);
        m_deadGeom[i] = m_deadGeom.back();
        m_deadGeom.pop_back();
      } else {
        ++i;
      }
    }
    for (size_t i = 0; i < m_deadTex.size();) {
      if (m_deadTex[i].second < listFrame) {
        tex.push_back(m_deadTex[i].first);
        m_deadTex[i] = m_deadTex.back();
        m_deadTex.pop_back();
      } else {
        ++i;
      }
    }
  }
  // Release device resources + delete OUTSIDE the lock.
  for (RocketGeometry *g : geom) {
    if (g->vb)
      g->vb->Release();
    if (g->ib)
      g->ib->Release();
    delete g;
  }
  for (RocketTexture *t : tex) {
    if (t->tex)
      t->tex->Release();
    delete t;
  }
}

void RocketRenderD3D9::FreeList(void *p) {
  RocketCmdList *list = reinterpret_cast<RocketCmdList *>(p);
  if (!list)
    return;
  DrainReleased(list->frame);
  delete list;
}
