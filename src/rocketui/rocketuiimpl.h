#ifndef ROCKETUIIMPL_H
#define ROCKETUIIMPL_H

// Include Source engine headers first - they define Assert macro
#include "appframework/ilaunchermgr.h"
#include "cdll_int.h"
#include "rocketui/rocketui.h"
#include "shaderapi/IShaderDevice.h"
#include "shaderapi/ishaderapi.h"
#include "tier1/utlpair.h"
#include "tier3/tier3.h"

#include "rocketrender.h"
#include <rocketui/rmlui.h>
#include <atomic>
#include <unordered_set>

class DeviceCallbacks;

class RocketUIImpl : public CTier3AppSystem<IRocketUI> {
  typedef CTier3AppSystem<IRocketUI> BaseClass;
  friend class DeviceCallbacks;

public:
  static RocketUIImpl m_Instance;

protected:
  IDirect3DDevice9 *m_pDevice;
  DeviceCallbacks *m_pDeviceCallbacks;
  // False between a device-lost notification and the following device reset;
  // UI rendering is skipped while lost (see RenderHUDFrame/RenderMenuFrame).
  bool m_bDeviceActive;

  ILauncherMgr *m_pLauncherMgr;
  IShaderDeviceMgr *m_pShaderDeviceMgr;
  IShaderAPI *m_pShaderAPI;
  IVEngineClient *m_pEngine;

  float m_fTime;

  // Contexts
  Rml::Context *m_ctxMenu;
  Rml::Context *m_ctxHud;
  // What the game is showing, derived once per frame in RunFrame. Never assigned
  // from the render path: ownership must not depend on what rendered last.
  Rml::Context *m_ctxCurrent;

  // RmlUi is single-threaded and runs entirely on the MAIN thread: Update,
  // input, and Render-recording all happen here; only recorded command lists
  // cross to the render thread (which never touches RmlUi state). So no lock is
  // needed around RmlUi access.
  //
  // Device-lost/reset/screen-size callbacks fire on the render/async-D3D
  // thread, so SetScreenSize can't mutate the contexts directly. It stages the
  // new size here (lock-free) and RunFrame applies ctx->SetDimensions on the
  // main thread.
  std::atomic<bool> m_resizePending{false};
  std::atomic<int> m_pendingW{0};
  std::atomic<int> m_pendingH{0};

  // Context hosting the debugger documents, or nullptr.
  Rml::Context *m_debuggerHost;

  // List of Document Reload functions for hot-reloading.
  CUtlVector<CUtlPair<RocketDesinationContext_t,
                      CUtlPair<LoadDocumentFn, UnloadDocumentFn>>>
      m_documentReloadFuncs;

  // Client-owned first refusal on input events (console keys, ESC policy, ...).
  InputHookFn m_pInputHook;
  // Key repeat state: tracks keys that just received IE_ButtonPressed.
  // First IE_KeyCodeTyped after IE_ButtonPressed is a duplicate, not a repeat.
  // We skip it, then process subsequent IE_KeyCodeTyped as actual repeats.
  std::unordered_set<int> m_keysAwaitingFirstRepeat;

  // IAppSystem
public:
  virtual bool Connect(CreateInterfaceFn factory);
  virtual void Disconnect(void);
  virtual void *QueryInterface(const char *pInterfaceName);
  virtual InitReturnVal_t Init(void);
  virtual void Shutdown(void);
  virtual AppSystemTier_t GetTier(void) { return APP_SYSTEM_TIER3; }

  // IRocketUI Interface Methods
public:
  virtual void RunFrame(float time);
  virtual bool ReloadDocuments();
  virtual bool HandleInputEvent(const InputEvent_t &event);
  virtual void SetInputHook(InputHookFn hookFunc) { m_pInputHook = hookFunc; }
  virtual bool IsConsumingInput(void);
  virtual Rml::ElementDocument *
  LoadDocumentFile(RocketDesinationContext_t ctx, const char *filepath,
                   LoadDocumentFn loadDocumentFunc = nullptr,
                   UnloadDocumentFn unloadDocumentFunc = nullptr);
  virtual void *RecordHUD();
  virtual void *RecordMenu();
  virtual void RenderHUDFrame(void *cmdList);
  virtual void RenderMenuFrame(void *cmdList);
  virtual Rml::Context *AccessHudContext();
  virtual Rml::Context *AccessMenuContext();

  void AddDeviceDependentObject(IShaderDeviceDependentObject *pObject);
  void RemoveDeviceDependentObject(IShaderDeviceDependentObject *pObject);

  // Local Class Methods
  RocketUIImpl(void);
  void SetRenderingDevice(IDirect3DDevice9 *pDevice,
                          D3DPRESENT_PARAMETERS *pPresentParameters, HWND hWnd);
  void ToggleDebugger(void);

  // The visible document that owns the mouse, or nullptr if the game does.
  Rml::ElementDocument *FindInputClaimant();

  inline float GetTime() const { return m_fTime; }
  void SetScreenSize(int width, int height);

private:
  // The context that receives input and hosts the debugger.
  Rml::Context *GetInputContext();
  // Push the derived input-ownership state out to the cursor and the client's
  // mouse gate. Called once per frame from RunFrame.
  void SyncCursorToInputOwner();
  void LoadFonts();
  // Apply a pending resize (staged by SetScreenSize on the render thread) to
  // the contexts, on the main thread. Called at the top of RunFrame.
  void ApplyPendingResize();
};

class DeviceCallbacks : public IShaderDeviceDependentObject {
public:
  int m_iRefCount;
  RocketUIImpl *m_pRocketUI;

  DeviceCallbacks(void) : m_iRefCount(1), m_pRocketUI(nullptr) {}

  virtual void DeviceLost(void) {
    // Stop UI rendering until the device is reset. The renderer holds no
    // D3DPOOL_DEFAULT resources so this isn't required for Reset() to succeed,
    // but it cheaply enforces the "never touch a lost device" invariant.
    if (m_pRocketUI)
      m_pRocketUI->m_bDeviceActive = false;
  }

  virtual void DeviceReset(void *pDevice, void *pPresentParameters,
                           void *pHWnd) {
    m_pRocketUI->SetRenderingDevice((IDirect3DDevice9 *)pDevice,
                                    (D3DPRESENT_PARAMETERS *)pPresentParameters,
                                    (HWND)pHWnd);
  }

  virtual void ScreenSizeChanged(int width, int height) {
    m_pRocketUI->SetScreenSize(width, height);
  }
};

#endif // ROCKETUIIMPL_H
