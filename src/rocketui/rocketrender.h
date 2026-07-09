#ifndef ROCKETRENDER_H
#define ROCKETRENDER_H

#include <RmlUi/Core/RenderInterface.h>
#include <d3d9.h>

// INVARIANT: this renderer must never hold a D3DPOOL_DEFAULT resource.
// D3D9 refuses IDirect3DDevice9::Reset() while any "losable" (D3DPOOL_DEFAULT)
// resource is alive, and the UI keeps rendering during the device-lost window
// (e.g. resolution changes), so any default-pool resource would be recreated
// faster than Reset() can drain it -> permanent reset-fail loop / freeze.
// All geometry/textures therefore use D3DPOOL_MANAGED (survives Reset, not
// losable), and UI draws straight onto the already-bound backbuffer. Do not
// introduce offscreen render targets / effect buffers without a device-lost
// guard.
//
// THREADING: this single shared instance serves both the HUD and menu contexts
// and holds per-frame render state (m_stencilRef, m_projT, m_transformEnabled,
// m_hasStencil, plus all bound device state). That is safe only because every
// BeginFrame/Render/EndFrame call runs on one render thread. Do not render two
// contexts concurrently (the HUD/menu mutexes are separate and would NOT
// protect this shared state).
class RocketRenderD3D9 : public Rml::RenderInterface {
public:
  static RocketRenderD3D9 m_Instance;

  RocketRenderD3D9();
  ~RocketRenderD3D9();

  bool Initialize(IDirect3DDevice9 *pDevice);
  void Shutdown();

  void BeginFrame();
  void EndFrame();

  void Reinitialize(IDirect3DDevice9 *pDevice);
  void SetD3D9Device(IDirect3DDevice9 *pDevice) { m_pDevice = pDevice; }
  void SetScreenSize(int width, int height);

  // Rml::RenderInterface
  Rml::CompiledGeometryHandle
  CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                  Rml::Span<const int> indices) override;
  void RenderGeometry(Rml::CompiledGeometryHandle handle,
                      Rml::Vector2f translation,
                      Rml::TextureHandle texture) override;
  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

  Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions,
                                 const Rml::String &source) override;
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                     Rml::Vector2i source_dimensions) override;
  void ReleaseTexture(Rml::TextureHandle texture_handle) override;

  void EnableScissorRegion(bool enable) override;
  void SetScissorRegion(Rml::Rectanglei region) override;

  void EnableClipMask(bool enable) override;
  void RenderToClipMask(Rml::ClipMaskOperation operation,
                        Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation) override;

  void SetTransform(const Rml::Matrix4f *transform) override;

  static constexpr Rml::TextureHandle TextureEnableWithoutBinding =
      Rml::TextureHandle(-1);

private:
  struct D3DVertex {
    float x, y, z;
    DWORD color; // D3DCOLOR (BGRA)
    float u, v;
  };

  struct GeometryData {
    IDirect3DVertexBuffer9 *vb;
    IDirect3DIndexBuffer9 *ib;
    UINT vertexCount;
    UINT indexCount;
  };

  void SetupRenderState();
  void ReleaseResources();
  void CreateShaders();
  void ReleaseShaders();
  void UploadWVP(const D3DMATRIX &world);

  IDirect3DDevice9 *m_pDevice;

  int m_width, m_height;
  bool m_transformEnabled;
  bool m_hasStencil; // Does the bound depth-stencil have a stencil buffer?
  int m_stencilRef;

  D3DMATRIX m_d3dTransform;
  D3DMATRIX m_d3dProjection;
  float m_projT[16] = {}; // Transposed projection — cached for fast path

  // Programmable shaders — bypass DXVK's FF shader generation.
  IDirect3DVertexShader9 *m_vs = nullptr;
  IDirect3DPixelShader9 *m_psTextured = nullptr;
  IDirect3DPixelShader9 *m_psUntextured = nullptr;
  IDirect3DVertexDeclaration9 *m_vtxDecl = nullptr;
};

#endif // ROCKETRENDER_H
