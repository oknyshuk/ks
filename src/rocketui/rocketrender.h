#ifndef ROCKETRENDER_H
#define ROCKETRENDER_H

#include <RmlUi/Core/RenderInterface.h>
#include <atomic>
#include <cstdint>
#include <d3d9.h>
#include <mutex>
#include <utility>
#include <vector>

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
// THREADING: RmlUi is single-threaded and runs entirely on the MAIN thread
// (Context::Update + Context::Render). This class is therefore split by thread,
// not by mutex:
//   * MAIN thread (recording): the Rml::RenderInterface overrides do NO device
//     work. CompileGeometry/GenerateTexture/LoadTexture allocate CPU-side
//     handles; RenderGeometry/scissor/clip/transform/RenderToClipMask append
//     POD commands to the current RocketCmdList; Release* push handles into a
//     frame-tagged graveyard.
//   * RENDER thread (replay): Replay() walks a finished RocketCmdList and does
//     the actual D3D9 calls, lazily (re)creating MANAGED resources. It never
//     touches RmlUi state. The command list is handed over by ownership through
//     the material-system queued call, so the two threads never share it.
// The only cross-thread state is the graveyard (its own mutex) and m_deviceGen
// (atomic). Per-frame device render state (m_stencilRef/m_projT/...) is touched
// only during replay on the one render thread.

// Handles handed to RmlUi. Allocated on the main thread (CPU data only); their
// D3D9 resources are created/destroyed on the render thread. Defined in the .cpp.
struct RocketGeometry;
struct RocketTexture;
struct RocketCmdList;

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

  // --- Recording driver (MAIN thread) ---
  void NextFrame() { ++m_frame; }  // one bump per RunFrame; tags lists/releases
  void *BeginRecord();             // returns a fresh RocketCmdList* as void*
  void EndRecord() { m_target = nullptr; }

  // --- Replay driver (RENDER thread) ---
  void Replay(void *list);  // execute recorded commands on the device
  void FreeList(void *list); // drain graveyard for list->frame, then delete list

  // Bump on GENUINE device re-creation only (different device pointer). Live
  // handles then re-create their D3D9 resources from retained CPU data on their
  // next draw. Plain Reset() keeps MANAGED resources, so do NOT bump then.
  void BumpDeviceGen() { ++m_deviceGen; }

  // Rml::RenderInterface (recording; MAIN thread)
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
  void SetupRenderState();
  void ReleaseResources();
  void CreateShaders();
  void ReleaseShaders();
  void UploadWVP(const D3DMATRIX &world);

  // Replay-side device ops (RENDER thread).
  bool EnsureGeometry(RocketGeometry *g);
  bool EnsureTexture(RocketTexture *t);
  void DrawGeometryDev(RocketGeometry *g, Rml::Vector2f translation,
                       Rml::TextureHandle texture);
  void RenderToClipMaskDev(Rml::ClipMaskOperation operation, RocketGeometry *g,
                           Rml::Vector2f translation);
  void DrainReleased(uint32_t listFrame); // free entries with releaseFrame < listFrame

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

  // Recording state (MAIN thread only).
  RocketCmdList *m_target = nullptr; // list currently being recorded into
  uint32_t m_frame = 0;              // frame counter, tags lists and releases

  // Bumped only on genuine device re-creation (render/device-callback thread),
  // read during replay (render thread). Atomic so the read can't tear.
  std::atomic<uint32_t> m_deviceGen{1};

  // Graveyard: handles RmlUi released on the main thread. Actual D3D9 release +
  // delete happens on the render thread once no in-flight list references them
  // (see DrainReleased). Never freed on the main thread (would let the
  // allocator hand the address to a new handle while replay is still behind).
  // ponytail: one coarse mutex; uncontended (brief main-thread push vs
  // once-per-list render-thread drain). Lock-free ring only if a profile says so.
  std::mutex m_deadMutex;
  std::vector<std::pair<RocketGeometry *, uint32_t>> m_deadGeom;
  std::vector<std::pair<RocketTexture *, uint32_t>> m_deadTex;
};

#endif // ROCKETRENDER_H
