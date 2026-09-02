#include "rocketuiimpl.h"
#include "rocketfilesystem.h"
#include "rocketrender.h"
#include "rocketsystem.h"

#include <rocketui/rmlui.h>

#include <cstdio>
#include <cstring>

#include "inputsystem/iinputsystem.h"
#include "rocketkeys.h"

// Helper to get current key modifier state for RmlUi
static int GetRmlKeyModifierState() {
  int state = 0;
  if (g_pInputSystem->IsButtonDown(KEY_LCONTROL) ||
      g_pInputSystem->IsButtonDown(KEY_RCONTROL))
    state |= Rml::Input::KM_CTRL;
  if (g_pInputSystem->IsButtonDown(KEY_LSHIFT) ||
      g_pInputSystem->IsButtonDown(KEY_RSHIFT))
    state |= Rml::Input::KM_SHIFT;
  if (g_pInputSystem->IsButtonDown(KEY_LALT) ||
      g_pInputSystem->IsButtonDown(KEY_RALT))
    state |= Rml::Input::KM_ALT;
  return state;
}

RocketUIImpl RocketUIImpl::m_Instance;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(RocketUIImpl, IRocketUI,
                                  ROCKETUI_INTERFACE_VERSION,
                                  RocketUIImpl::m_Instance)

ConVar rocket_enable("rocket_enable", "1", 0, "Enables RocketUI");

CON_COMMAND_F(rocket_reload, "Reloads all RocketUI Documents", FCVAR_NONE) {
  if (RocketUIImpl::m_Instance.ReloadDocuments()) {
    ConMsg("[RocketUI] Documents Reloaded.\n");
  } else {
    ConMsg("[RocketUI] Error reloading Documents!\n");
  }
}

CON_COMMAND_F(rocket_debug, "Open/Close the RocketUI Debugger", FCVAR_NONE) {
  RocketUIImpl::m_Instance.ToggleDebugger();
}

// Names the document that currently takes the mouse, i.e. what IsConsumingInput()
// is answering. Run it from a key bind: an open console is itself a claimant.
CON_COMMAND_F(rocket_input_owner, "Name the document that owns mouse input",
              FCVAR_NONE) {
  Rml::ElementDocument *doc = RocketUIImpl::m_Instance.FindInputClaimant();
  ConMsg("[RocketUI] mouse owner: %s\n",
         doc ? doc->GetSourceURL().c_str() : "(none - the game has it)");
}

RocketUIImpl::RocketUIImpl()
    : m_pDevice(nullptr), m_pDeviceCallbacks(nullptr), m_bDeviceActive(false)
#ifdef USE_SDL
      ,
      m_pLauncherMgr(nullptr)
#endif
      ,
      m_pShaderDeviceMgr(nullptr), m_pShaderAPI(nullptr), m_pEngine(nullptr),
      m_fTime(0.0f), m_ctxMenu(nullptr),
      m_ctxHud(nullptr), m_ctxCurrent(nullptr), m_debuggerHost(nullptr),
      m_pInputHook(nullptr) {
}

bool RocketUIImpl::Connect(CreateInterfaceFn factory) {
  if (!factory)
    return false;

  if (!BaseClass::Connect(factory))
    return false;

  // Nothing in this library reaches the console without this: a ConVar or
  // CON_COMMAND in a shared library is only a local object until it is published
  // to g_pCVar. rocket_enable, rocket_reload, rocket_debug and rocket_input_owner
  // were all invisible (and unsettable) before this call existed.
  ConVar_Register();

#ifdef USE_SDL
  m_pLauncherMgr = (ILauncherMgr *)factory(SDLMGR_INTERFACE_VERSION, nullptr);
#endif

  m_pShaderDeviceMgr =
      (IShaderDeviceMgr *)factory(SHADER_DEVICE_MGR_INTERFACE_VERSION, nullptr);
  m_pEngine =
      (IVEngineClient *)factory(VENGINE_CLIENT_INTERFACE_VERSION, nullptr);
  m_pShaderAPI = (IShaderAPI *)factory(SHADERAPI_INTERFACE_VERSION, nullptr);

  if (!m_pShaderDeviceMgr || !m_pEngine || !m_pShaderAPI) {
    Warning("RocketUI: missing expected interface\n");
    return false;
  }

  return true;
}

void RocketUIImpl::Disconnect() {
  ConVar_Unregister();

  if (m_pShaderDeviceMgr) {
    if (m_pDeviceCallbacks) {
      m_pShaderDeviceMgr->RemoveDeviceDependentObject(m_pDeviceCallbacks);
      delete m_pDeviceCallbacks;
      m_pDeviceCallbacks = nullptr;
    }
  }

#ifdef USE_SDL
  m_pLauncherMgr = nullptr;
#endif
  m_pShaderDeviceMgr = nullptr;
  m_pEngine = nullptr;
  m_pShaderAPI = nullptr;

  BaseClass::Disconnect();
}

void *RocketUIImpl::QueryInterface(const char *pInterfaceName) {
  if (!Q_strncmp(pInterfaceName, ROCKETUI_INTERFACE_VERSION,
                 Q_strlen(ROCKETUI_INTERFACE_VERSION) + 1)) {
    return (IRocketUI *)&RocketUIImpl::m_Instance;
  }

  return BaseClass::QueryInterface(pInterfaceName);
}

Rml::Context *RocketUIImpl::AccessHudContext() { return m_ctxHud; }

Rml::Context *RocketUIImpl::AccessMenuContext() { return m_ctxMenu; }

// Font faces are declared in rocketui/fonts.rcss and shipped beside it, so they
// come off the GAME search path like any other asset. Parsing the sheet is the
// point: @font-face registers faces globally, so no document has to link it, and
// RmlUi owns the buffers from here.
void RocketUIImpl::LoadFonts() {
  if (!Rml::Factory::InstanceStyleSheetFile("fonts.rcss"))
    Warning("RocketUI: no rocketui/fonts.rcss; text will not render.\n");
}

Rml::ElementDocument *RocketUIImpl::LoadDocumentFile(
    RocketDesinationContext_t ctx, const char *filepath,
    LoadDocumentFn loadDocumentFunc, UnloadDocumentFn unloadDocumentFunc) {
  Rml::ElementDocument *document;
  Rml::Context *destinationCtx;

  switch (ctx) {
  case ROCKET_CONTEXT_MENU:
    destinationCtx = m_ctxMenu;
    break;
  case ROCKET_CONTEXT_HUD:
    destinationCtx = m_ctxHud;
    break;
  case ROCKET_CONTEXT_CURRENT:
    destinationCtx = m_ctxCurrent;
    break;
  default:
    return nullptr;
  }

  document = destinationCtx->LoadDocument(filepath);

  if (loadDocumentFunc && unloadDocumentFunc) {
    CUtlPair<LoadDocumentFn, UnloadDocumentFn> documentFuncPair(
        loadDocumentFunc, unloadDocumentFunc);
    CUtlPair<RocketDesinationContext_t,
             CUtlPair<LoadDocumentFn, UnloadDocumentFn>>
        reloadFunctionEntry(ctx, documentFuncPair);
    m_documentReloadFuncs.AddToTail(reloadFunctionEntry);
  }

  return document;
}

InitReturnVal_t RocketUIImpl::Init(void) {
  InitReturnVal_t nRetVal = BaseClass::Init();
  if (nRetVal != INIT_OK)
    return nRetVal;

  // Register a callback with the ShaderDeviceMgr
  m_pDeviceCallbacks = new DeviceCallbacks();
  m_pDeviceCallbacks->m_pRocketUI = this;
  m_pShaderDeviceMgr->AddDeviceDependentObject(m_pDeviceCallbacks);

  // Default width/height, these get updated in the DeviceCallbacks
  int width = 1920;
  int height = 1080;
  RocketRenderD3D9::m_Instance.SetScreenSize(width, height);

  // Allocate and store system cursors so we can swap to them on the fly
  RocketSystem::m_Instance.InitCursors();

  Rml::SetFileInterface(&RocketFileSystem::m_Instance);
  Rml::SetRenderInterface(&RocketRenderD3D9::m_Instance);
  Rml::SetSystemInterface(&RocketSystem::m_Instance);

  if (!Rml::Initialise()) {
    Warning("RocketUI: Initialise() failed!\n");
    return INIT_FAILED;
  }

  // Not fatal: an unstyled UI beats refusing to initialise. RmlUi logs each face.
  LoadFonts();

  m_ctxMenu = Rml::CreateContext("menu", Rml::Vector2i(width, height));
  m_ctxHud = Rml::CreateContext("hud", Rml::Vector2i(width, height));

  if (!m_ctxMenu || !m_ctxHud) {
    Warning("RocketUI: Failed to create Hud/Menu context\n");
    Rml::Shutdown();
    return INIT_FAILED;
  }

  m_ctxMenu->SetDensityIndependentPixelRatio(1.0f);
  m_ctxHud->SetDensityIndependentPixelRatio(1.0f);

  // The game starts at the main menu; RunFrame re-derives this every frame.
  // Never left null, so ROCKET_CONTEXT_CURRENT works before the first frame.
  m_ctxCurrent = m_ctxMenu;

  return INIT_OK;
}

void RocketUIImpl::Shutdown() {
  Rml::Shutdown();

  RocketSystem::m_Instance.FreeCursors();
  RocketRenderD3D9::m_Instance.Shutdown();

  m_debuggerHost = nullptr;

  if (m_pShaderDeviceMgr) {
    if (m_pDeviceCallbacks) {
      m_pShaderDeviceMgr->RemoveDeviceDependentObject(m_pDeviceCallbacks);
      delete m_pDeviceCallbacks;
      m_pDeviceCallbacks = nullptr;
    }
  }

  m_ctxCurrent = nullptr;

  BaseClass::Shutdown();
}

// Hidden and closed documents render nothing.
static bool HasVisibleDocument(Rml::Context *ctx) {
  for (int i = 0, n = ctx->GetNumDocuments(); i < n; i++) {
    Rml::ElementDocument *doc = ctx->GetDocument(i);
    if (doc && doc->IsVisible())
      return true;
  }
  return false;
}

// Worth spending a frame on? RmlUi lowers its next-update delay when it wants a
// callback (animations, smooth scrolling, our input/resize latches). Anything
// visible counts too: the game writes data models and properties behind RmlUi's
// back. Leaves the menu context idle during a match, and the HUD in the menu.
static bool ContextIsLive(Rml::Context *ctx) {
  return ctx && (ctx->GetNextUpdateDelay() <= 0.0 || HasVisibleDocument(ctx));
}

Rml::Context *RocketUIImpl::GetInputContext() {
  // The menu context owns input whenever it has anything on screen -- every
  // document it holds is interactive. Otherwise input follows the game.
  if (m_ctxMenu && HasVisibleDocument(m_ctxMenu))
    return m_ctxMenu;
  return m_ctxCurrent;
}

void RocketUIImpl::RunFrame(float time) {
  if (!m_pDevice)
    return;

  m_fTime = time;

  // Start a new RmlUi frame: bump the counter that tags command lists and
  // graveyard releases. All RmlUi work (Update, input, Render-recording) is on
  // this main thread, so no lock is needed.
  RocketRenderD3D9::m_Instance.NextFrame();

  // Apply any resize staged by a device reset (render thread) before layout.
  ApplyPendingResize();

  // Which context the game is showing. The engine keeps a level loaded behind the
  // main menu, hence the background check -- same test the HUD system uses.
  m_ctxCurrent = (m_pEngine->IsInGame() && !m_pEngine->IsLevelMainMenuBackground())
                     ? m_ctxHud
                     : m_ctxMenu;

  // The menu context keeps updating during a match: the console lives there.
  if (ContextIsLive(m_ctxHud))
    m_ctxHud->Update();
  if (ContextIsLive(m_ctxMenu))
    m_ctxMenu->Update();

  SyncCursorToInputOwner();
}

// Ownership of mouse/keyboard is asked for, never remembered: it is read off the
// documents themselves, so nothing can claim the mouse without being on screen, and
// no panel has to remember to release it. A document opts in by allowing pointer
// events -- the same declaration RmlUi hit-tests against -- so passive HUD documents
// (crosshair, radar, killfeed, ...) declare `pointer-events: none` in their RCSS.
Rml::ElementDocument *RocketUIImpl::FindInputClaimant() {
  for (Rml::Context *ctx : {m_ctxMenu, m_ctxHud}) {
    if (!ctx)
      continue;
    for (int i = 0, n = ctx->GetNumDocuments(); i < n; i++) {
      Rml::ElementDocument *doc = ctx->GetDocument(i);
      if (doc && doc->IsVisible() &&
          doc->GetComputedValues().pointer_events() != Rml::Style::PointerEvents::None)
        return doc;
    }
  }
  return nullptr;
}

bool RocketUIImpl::IsConsumingInput() {
  return Rml::Debugger::IsVisible() || FindInputClaimant() != nullptr;
}

// State the intent every frame, keeping no copy of it: both sinks diff against
// their own authoritative state, and a mode switch rebuilds the window under us,
// so a cache here only suppresses the write that would fix the mismatch -- which
// is what made changing resolution kill mouselook until you pressed ESC twice.
void RocketUIImpl::SyncCursorToInputOwner() {
  const bool bWantCursor = IsConsumingInput();

  // The client's mouselook gate lives in the client DLL, which RocketUI cannot
  // call into directly; cl_mouseenable is the shared switch. ConVar setters drop
  // writes that change nothing, so this is free to repeat.
  static ConVarRef cl_mouseenable("cl_mouseenable");
  if (cl_mouseenable.IsValid())
    cl_mouseenable.SetValue(!bWantCursor);

  if (bWantCursor) {
    m_pLauncherMgr->SetMouseCursor(
        RocketSystem::m_Instance.m_pCursors[SDL_SYSTEM_CURSOR_DEFAULT]);
  }
  m_pLauncherMgr->SetMouseVisible(bWantCursor);
}

bool RocketUIImpl::HandleInputEvent(const InputEvent_t &event) {
  if (!rocket_enable.GetBool())
    return false;

  Rml::Context *inputCtx = GetInputContext();
  if (!inputCtx)
    return false;

  int modifiers = GetRmlKeyModifierState();
  ButtonCode_t key = (ButtonCode_t)event.m_nData;
  bool isKeyboard = !IsMouseCode(key);

  // Track key state to distinguish first press from repeat.
  // Engine fires BOTH IE_ButtonPressed AND IE_KeyCodeTyped on first press,
  // but only IE_KeyCodeTyped on repeat. We skip the first IE_KeyCodeTyped.
  if (event.m_nType == IE_ButtonPressed && isKeyboard) {
    m_keysAwaitingFirstRepeat.insert(key);
  } else if (event.m_nType == IE_ButtonReleased && isKeyboard) {
    m_keysAwaitingFirstRepeat.erase(key);
  }

  bool isFirstKeyCodeTyped = false;
  if (event.m_nType == IE_KeyCodeTyped) {
    if (m_keysAwaitingFirstRepeat.count(key)) {
      m_keysAwaitingFirstRepeat.erase(key);
      isFirstKeyCodeTyped =
          true; // Skip this - it's duplicate of IE_ButtonPressed
    }
  }

  // Whatever we are about to hand it, this context is no longer idle.
  inputCtx->RequestNextUpdate(0);

  // Client UI policy gets first refusal (console keys, ESC). The duplicate
  // IE_KeyCodeTyped is dropped here as well as below.
  if (m_pInputHook && !isFirstKeyCodeTyped && m_pInputHook(event))
    return true;

  // Always track mouse location
  if (event.m_nType == IE_AnalogValueChanged && event.m_nData == MOUSE_XY) {
    inputCtx->ProcessMouseMove(event.m_nData2, event.m_nData3, modifiers);
  }

  // The debugger is ours, not the client's, so its hotkey stays here.
  if (event.m_nType == IE_ButtonPressed && key == KEY_F8) {
    ToggleDebugger();
    return true;
  }

  // Skip RmlUi processing if nothing wants input or VGUI console is open
  if (!IsConsumingInput() || m_pEngine->Con_IsVisible())
    return false;

  switch (event.m_nType) {
  case IE_ButtonDoubleClicked:
  case IE_ButtonPressed:
    if (!isKeyboard) {
      switch (key) {
      case MOUSE_LEFT:
        inputCtx->ProcessMouseButtonDown(0, modifiers);
        break;
      case MOUSE_RIGHT:
        inputCtx->ProcessMouseButtonDown(1, modifiers);
        break;
      case MOUSE_MIDDLE:
        inputCtx->ProcessMouseButtonDown(2, modifiers);
        break;
      case MOUSE_4:
        inputCtx->ProcessMouseButtonDown(3, modifiers);
        break;
      case MOUSE_5:
        inputCtx->ProcessMouseButtonDown(4, modifiers);
        break;
      case MOUSE_WHEEL_UP:
        inputCtx->ProcessMouseWheel(-1, modifiers);
        break;
      case MOUSE_WHEEL_DOWN:
        inputCtx->ProcessMouseWheel(1, modifiers);
        break;
      default:
        break;
      }
    } else {
      inputCtx->ProcessKeyDown(ButtonToRocketKey(key), modifiers);
    }
    break;

  case IE_ButtonReleased:
    if (!isKeyboard) {
      switch (key) {
      case MOUSE_LEFT:
        inputCtx->ProcessMouseButtonUp(0, modifiers);
        break;
      case MOUSE_RIGHT:
        inputCtx->ProcessMouseButtonUp(1, modifiers);
        break;
      case MOUSE_MIDDLE:
        inputCtx->ProcessMouseButtonUp(2, modifiers);
        break;
      case MOUSE_4:
        inputCtx->ProcessMouseButtonUp(3, modifiers);
        break;
      case MOUSE_5:
        inputCtx->ProcessMouseButtonUp(4, modifiers);
        break;
      default:
        break;
      }
    } else {
      inputCtx->ProcessKeyUp(ButtonToRocketKey(key), modifiers);
    }
    break;

  case IE_KeyTyped: {
    char ascii = (char)((wchar_t)event.m_nData);
    if (ascii != 8) // RmlUi doesn't like backspace here
      inputCtx->ProcessTextInput(ascii);
    break;
  }

  case IE_KeyCodeTyped:
    // Key repeat - only process actual repeats, not the first duplicate
    if (!isFirstKeyCodeTyped)
      inputCtx->ProcessKeyDown(ButtonToRocketKey(key), modifiers);
    break;

  case IE_AnalogValueChanged:
    break;

  default:
    return false;
  }

  return IsConsumingInput();
}

// Recording (MAIN thread): run Context::Render() into a command list which the
// render thread later replays. Returns an opaque RocketCmdList* (or nullptr).
// A context with nothing on screen is skipped: no list, no queued render call.
void *RocketUIImpl::RecordHUD() {
  if (!rocket_enable.GetBool() || !m_ctxHud || !HasVisibleDocument(m_ctxHud))
    return nullptr;

  void *list = RocketRenderD3D9::m_Instance.BeginRecord();
  m_ctxHud->Render();
  RocketRenderD3D9::m_Instance.EndRecord();
  return list;
}

void *RocketUIImpl::RecordMenu() {
  if (!rocket_enable.GetBool() || !m_ctxMenu || !HasVisibleDocument(m_ctxMenu))
    return nullptr;

  void *list = RocketRenderD3D9::m_Instance.BeginRecord();
  m_ctxMenu->Render();
  RocketRenderD3D9::m_Instance.EndRecord();
  return list;
}

// Replay (RENDER thread): execute a recorded list on the device, then free it.
// FreeList also drains the release graveyard for finished frames. cmdList may
// be nullptr (nothing recorded) -> just a no-op free.
void RocketUIImpl::RenderHUDFrame(void *cmdList) {
  RocketRenderD3D9 &r = RocketRenderD3D9::m_Instance;
  if (cmdList && m_pDevice && m_bDeviceActive) {
    r.BeginFrame();
    r.Replay(cmdList);
    r.EndFrame();
  }
  r.FreeList(cmdList);
}

void RocketUIImpl::RenderMenuFrame(void *cmdList) {
  RocketRenderD3D9 &r = RocketRenderD3D9::m_Instance;
  if (cmdList && m_pDevice && m_bDeviceActive) {
    r.BeginFrame();
    r.Replay(cmdList);
    r.EndFrame();
    m_pShaderAPI->ResetRenderState(false);
  }
  r.FreeList(cmdList);
}

bool RocketUIImpl::ReloadDocuments() {
  rocket_enable.SetValue(false);

  // Reload runs on the main thread alongside recording; disabling rocket_enable
  // stops new recording while documents are torn down/rebuilt, and released
  // geometry is reclaimed safely via the frame-tagged graveyard.
  CUtlVector<CUtlPair<RocketDesinationContext_t,
                      CUtlPair<LoadDocumentFn, UnloadDocumentFn>>>
      copyOfPairs;

  for (int i = 0; i < m_documentReloadFuncs.Count(); i++) {
    copyOfPairs.AddToTail(m_documentReloadFuncs[i]);
  }

  m_documentReloadFuncs.Purge();

  for (int i = 0; i < copyOfPairs.Count(); i++) {
    // A document can only be rebuilt while its context is the one in play (a HUD
    // element's loader needs a live HUD). Keep the rest registered, or
    // `rocket_reload` from the menu costs the HUD its hot-reload hooks.
    const bool bReloadable =
        (copyOfPairs[i].first == ROCKET_CONTEXT_HUD && m_ctxCurrent == m_ctxHud) ||
        (copyOfPairs[i].first == ROCKET_CONTEXT_MENU && m_ctxCurrent == m_ctxMenu);

    if (copyOfPairs[i].first == ROCKET_CONTEXT_CURRENT) {
      // Transient panels: close them and let the next open re-register.
      copyOfPairs[i].second.second();
      continue;
    }

    if (!bReloadable) {
      m_documentReloadFuncs.AddToTail(copyOfPairs[i]);
      continue;
    }

    // Unload... (the loader re-registers the pair via LoadDocumentFile)
    copyOfPairs[i].second.second();
    // Load...
    copyOfPairs[i].second.first();
  }

  rocket_enable.SetValue(true);
  return true;
}

void RocketUIImpl::AddDeviceDependentObject(
    IShaderDeviceDependentObject *pObject) {
  if (m_pShaderDeviceMgr) {
    m_pShaderDeviceMgr->AddDeviceDependentObject(pObject);
  }
}

void RocketUIImpl::RemoveDeviceDependentObject(
    IShaderDeviceDependentObject *pObject) {
  if (m_pShaderDeviceMgr) {
    m_pShaderDeviceMgr->RemoveDeviceDependentObject(pObject);
  }
}

void RocketUIImpl::SetRenderingDevice(IDirect3DDevice9 *pDevice,
                                      D3DPRESENT_PARAMETERS *pPresentParameters,
                                      HWND hWnd) {
  if (!pDevice)
    return;

  // Runs on the render/async-D3D thread (device callback). DEVICE work only;
  // RmlUi context mutations (resize) are staged for the main thread via
  // SetScreenSize -> ApplyPendingResize.
  if (m_pDevice == nullptr) {
    // First time initialization
    m_pDevice = pDevice;
    RocketRenderD3D9::m_Instance.Initialize(pDevice);
  } else {
    // DXVK keeps the SAME device pointer and preserves D3DPOOL_MANAGED
    // resources across Reset(), so RmlUi's cached geometry/textures stay valid.
    // A genuine re-creation (different pointer) invalidates the underlying D3D9
    // resources: bump the device generation so each live handle rebuilds its
    // VB/IB/texture from retained CPU data on its next draw (render thread).
    // The RmlUi-side handles stay valid, so there is nothing to release here.
    if (pDevice != m_pDevice)
      RocketRenderD3D9::m_Instance.BumpDeviceGen();

    // Null m_pDevice DURING reinit so a replay doesn't touch a
    // half-initialized renderer.
    m_pDevice = nullptr;
    RocketRenderD3D9::m_Instance.Reinitialize(pDevice);
    m_pDevice = pDevice; // Restore AFTER reinit completes
  }

  D3DVIEWPORT9 viewport;
  pDevice->GetViewport(&viewport);
  SetScreenSize(viewport.Width, viewport.Height);

  m_bDeviceActive = true;
}

void RocketUIImpl::SetScreenSize(int width, int height) {
  // Called on the render/async-D3D thread (device callback). Set the executor's
  // viewport now (render-thread state), but DEFER the RmlUi context resize to
  // the main thread: SetDimensions mutates the element trees and must not race
  // Update/Render on the main thread.
  RocketRenderD3D9::m_Instance.SetScreenSize(width, height);
  m_pendingW.store(width, std::memory_order_relaxed);
  m_pendingH.store(height, std::memory_order_relaxed);
  m_resizePending.store(true, std::memory_order_release);
}

void RocketUIImpl::ApplyPendingResize() {
  if (!m_resizePending.exchange(false, std::memory_order_acquire))
    return;
  const int w = m_pendingW.load(std::memory_order_relaxed);
  const int h = m_pendingH.load(std::memory_order_relaxed);
  if (m_ctxHud) {
    m_ctxHud->SetDimensions(Rml::Vector2i(w, h));
    m_ctxHud->RequestNextUpdate(0);
  }
  if (m_ctxMenu) {
    m_ctxMenu->SetDimensions(Rml::Vector2i(w, h));
    m_ctxMenu->RequestNextUpdate(0);
  }
}

// F8 / rocket_debug. One context hosts the debugger at a time, and it has to be
// the one receiving input or the debugger's own widgets are dead. RmlUi 6.3 fixed
// re-initialisation, so that host can follow input.
void RocketUIImpl::ToggleDebugger() {
  if (Rml::Debugger::IsVisible()) {
    ConMsg("[RocketUI] Closing Debugger\n");
    Rml::Debugger::SetVisible(false);
    return;
  }

  Rml::Context *host = GetInputContext();
  if (!host)
    return;

  if (m_debuggerHost != host) {
    if (m_debuggerHost)
      Rml::Debugger::Shutdown();
    m_debuggerHost = nullptr;
    if (!Rml::Debugger::Initialise(host)) {
      ConMsg("[RocketUI] Error initializing debugger\n");
      return;
    }
    m_debuggerHost = host;
  }

  ConMsg("[RocketUI] Opening Debugger\n");
  Rml::Debugger::SetVisible(true);
}
