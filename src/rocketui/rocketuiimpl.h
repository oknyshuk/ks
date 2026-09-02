#ifndef ROCKETUIIMPL_H
#define ROCKETUIIMPL_H

// Include Source engine headers first - they define Assert macro
#include "IGameUIFuncs.h"
#include "appframework/ilaunchermgr.h"
#include "cdll_int.h"
#include "igameevents.h"
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
  IGameUIFuncs *m_pGameUIFuncs;
  IVEngineClient *m_pEngine;
  IGameEventManager2 *m_pGameEventManager;

  // Fonts need to stay for the lifetime of the program. Used directly by
  // freetype. Freed on shutdown.
  CUtlVector<unsigned char *> m_fontAllocs;
  float m_fTime;
  bool m_bCursorVisible;

  // Contexts
  Rml::Context *m_ctxMenu;
  Rml::Context *m_ctxHud;
  Rml::Context *m_ctxCurrent; // Pointer to Hud or Menu (for rendering)
  Rml::Context
      *m_ctxInput; // Override for input routing (nullptr = use m_ctxCurrent)

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

  bool m_isDebuggerOpen;

  TogglePauseMenuFn m_togglePauseMenuFunc;
  ConsoleKeyInputFn m_consoleKeyInputFunc;
  ConsoleCharInputFn m_consoleCharInputFunc;

  // List of Document Reload functions for hot-reloading.
  CUtlVector<CUtlPair<RocketDesinationContext_t,
                      CUtlPair<LoadDocumentFn, UnloadDocumentFn>>>
      m_documentReloadFuncs;

  // Single source of truth for "the UI owns the mouse", owned by the client DLL
  // and polled, never cached. nullptr (no client DLL yet) means the game owns it.
  InputClaimQueryFn m_pInputClaimQuery;

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
  virtual const AppSystemInfo_t *GetDependencies(void);
  virtual AppSystemTier_t GetTier(void) { return APP_SYSTEM_TIER3; }
  virtual void Reconnect(CreateInterfaceFn factory,
                         const char *pInterfaceName) {
    BaseClass::Reconnect(factory, pInterfaceName);
  }

  // IRocketUI Interface Methods
public:
  virtual void RunFrame(float time);
  virtual bool ReloadDocuments();
  virtual bool HandleInputEvent(const InputEvent_t &event);
  virtual void SetInputClaimQuery(InputClaimQueryFn queryFunc) {
    m_pInputClaimQuery = queryFunc;
  }
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
  virtual void RegisterPauseMenu(TogglePauseMenuFn showPauseMenuFunc) {
    m_togglePauseMenuFunc = showPauseMenuFunc;
  }
  virtual void RegisterConsoleHandlers(ConsoleKeyInputFn keyHandler,
                                       ConsoleCharInputFn charHandler) {
    m_consoleKeyInputFunc = keyHandler;
    m_consoleCharInputFunc = charHandler;
  }
  virtual void SetInputContext(Rml::Context *ctx) { m_ctxInput = ctx; }

  void AddDeviceDependentObject(IShaderDeviceDependentObject *pObject);
  void RemoveDeviceDependentObject(IShaderDeviceDependentObject *pObject);

  // Local Class Methods
  RocketUIImpl(void);
  void SetRenderingDevice(IDirect3DDevice9 *pDevice,
                          D3DPRESENT_PARAMETERS *pPresentParameters, HWND hWnd);
  void ToggleDebugger(void);

  Rml::Context *GetCurrentContext() { return m_ctxCurrent; }

  inline float GetTime() const { return m_fTime; }
  void SetScreenSize(int width, int height);

private:
  // Push the derived input-ownership state out to the cursor and the client's
  // mouse gate. Called once per frame from RunFrame.
  void SyncCursorToInputOwner();
  bool LoadFont(const char *filepath, const char *path);
  bool LoadFonts();
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
