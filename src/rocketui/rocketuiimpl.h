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
#include <RmlUi/Core/ElementDocument.h>
#include <mutex>

class DeviceCallbacks;

class RocketUIImpl : public CTier3AppSystem<IRocketUI> {
  typedef CTier3AppSystem<IRocketUI> BaseClass;

public:
  static RocketUIImpl m_Instance;

protected:
  IDirect3DDevice9 *m_pDevice;
  DeviceCallbacks *m_pDeviceCallbacks;

  ILauncherMgr *m_pLauncherMgr;
  IShaderDeviceMgr *m_pShaderDeviceMgr;
  IShaderAPI *m_pShaderAPI;
  IGameUIFuncs *m_pGameUIFuncs;
  IVEngineClient *m_pEngine;
  IGameEventManager2 *m_pGameEventManager;

  // Fonts need to stay for the lifetime of the program. Used directly by
  // freetype. Freed on shutdown.
  CUtlVector<unsigned char *> m_fontAllocs;
  CUtlVector<CUtlString> m_inputConsumers;
  float m_fTime;
  bool m_bCursorVisible;

  // Contexts
  Rml::Context *m_ctxMenu;
  Rml::Context *m_ctxHud;
  Rml::Context *m_ctxCurrent; // Pointer to Hud or Menu (for rendering)
  Rml::Context *m_ctxInput;   // Override for input routing (nullptr = use m_ctxCurrent)

  // Mutexes to synchronize Update/Render - RmlUi is not thread-safe
  std::mutex m_mtxHud;
  std::mutex m_mtxMenu;

  bool m_isDebuggerOpen;

  TogglePauseMenuFn m_togglePauseMenuFunc;
  ConsoleKeyInputFn m_consoleKeyInputFunc;
  ConsoleCharInputFn m_consoleCharInputFunc;

  // List of Document Reload functions for hot-reloading.
  CUtlVector<CUtlPair<RocketDesinationContext_t,
                      CUtlPair<LoadDocumentFn, UnloadDocumentFn>>>
      m_documentReloadFuncs;

  // if > 0, we are stealing input from the game.
  int m_numInputConsumers;

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
  virtual void DenyInputToGame(bool value, const char *why);
  virtual bool IsConsumingInput(void);
  virtual void EnableCursor(bool state);
  virtual Rml::ElementDocument *
  LoadDocumentFile(RocketDesinationContext_t ctx, const char *filepath,
                   LoadDocumentFn loadDocumentFunc = nullptr,
                   UnloadDocumentFn unloadDocumentFunc = nullptr);
  virtual void RenderHUDFrame();
  virtual void RenderMenuFrame();
  virtual Rml::Context *AccessHudContext();
  virtual Rml::Context *AccessMenuContext();
  virtual void RegisterPauseMenu(TogglePauseMenuFn showPauseMenuFunc) {
    m_togglePauseMenuFunc = showPauseMenuFunc;
  }
  virtual void RegisterConsoleHandlers(ConsoleKeyInputFn keyHandler, ConsoleCharInputFn charHandler) {
    m_consoleKeyInputFunc = keyHandler;
    m_consoleCharInputFunc = charHandler;
  }
  virtual void SetInputContext(Rml::Context* ctx) {
    m_ctxInput = ctx;
  }

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
  bool LoadFont(const char *filepath, const char *path);
  bool LoadFonts();
};

class DeviceCallbacks : public IShaderDeviceDependentObject {
public:
  int m_iRefCount;
  RocketUIImpl *m_pRocketUI;

  DeviceCallbacks(void) : m_iRefCount(1), m_pRocketUI(nullptr) {}

  virtual void DeviceLost(void) {
    // Release back buffer resources to allow D3D9 device reset
    RocketRenderDXVK::m_Instance.ReleaseBackBuffer();
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
