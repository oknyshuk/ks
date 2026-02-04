#include "rocketuiimpl.h"
#include "rocketfilesystem.h"
#include "rocketrender.h"
#include "rocketsystem.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include <cstdio>
#include <cstring>
#include <fontconfig/fontconfig.h>

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
ConVar rocket_verbose("rocket_verbose", "0", 0, "Enables more logging");

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

RocketUIImpl::RocketUIImpl()
    : m_pDevice(nullptr), m_pDeviceCallbacks(nullptr)
#ifdef USE_SDL
      ,
      m_pLauncherMgr(nullptr)
#endif
      ,
      m_pShaderDeviceMgr(nullptr), m_pShaderAPI(nullptr),
      m_pGameUIFuncs(nullptr), m_pEngine(nullptr), m_pGameEventManager(nullptr),
      m_fTime(0.0f), m_bCursorVisible(false), m_ctxMenu(nullptr),
      m_ctxHud(nullptr), m_ctxCurrent(nullptr), m_ctxInput(nullptr),
      m_isDebuggerOpen(false), m_togglePauseMenuFunc(nullptr),
      m_consoleKeyInputFunc(nullptr), m_consoleCharInputFunc(nullptr),
      m_numInputConsumers(0) {
}

bool RocketUIImpl::Connect(CreateInterfaceFn factory) {
  if (!factory)
    return false;

  if (!BaseClass::Connect(factory))
    return false;

#ifdef USE_SDL
  m_pLauncherMgr = (ILauncherMgr *)factory(SDLMGR_INTERFACE_VERSION, nullptr);
#endif

  m_pShaderDeviceMgr =
      (IShaderDeviceMgr *)factory(SHADER_DEVICE_MGR_INTERFACE_VERSION, nullptr);
  m_pGameUIFuncs =
      (IGameUIFuncs *)factory(VENGINE_GAMEUIFUNCS_VERSION, nullptr);
  m_pEngine =
      (IVEngineClient *)factory(VENGINE_CLIENT_INTERFACE_VERSION, nullptr);
  m_pGameEventManager = (IGameEventManager2 *)factory(
      INTERFACEVERSION_GAMEEVENTSMANAGER2, nullptr);
  m_pShaderAPI = (IShaderAPI *)factory(SHADERAPI_INTERFACE_VERSION, nullptr);

  if (!m_pShaderDeviceMgr || !m_pGameUIFuncs || !m_pEngine ||
      !m_pGameEventManager || !m_pShaderAPI) {
    Warning("RocketUI: missing expected interface\n");
    return false;
  }

  return true;
}

void RocketUIImpl::Disconnect() {
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
  m_pGameUIFuncs = nullptr;
  m_pEngine = nullptr;
  m_pGameEventManager = nullptr;
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

const AppSystemInfo_t *RocketUIImpl::GetDependencies(void) {
  return BaseClass::GetDependencies();
}

Rml::Context *RocketUIImpl::AccessHudContext() { return m_ctxHud; }

Rml::Context *RocketUIImpl::AccessMenuContext() { return m_ctxMenu; }

// Find a font file path by family name using fontconfig
static const char *FindSystemFont(const char *fontName) {
  static char fontPath[512];
  fontPath[0] = '\0';

  if (!FcInit()) {
    fprintf(stderr, "[RocketUI] fontconfig init failed\n");
    return nullptr;
  }

  FcPattern *pattern = FcNameParse((const FcChar8 *)fontName);
  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match = FcFontMatch(nullptr, pattern, &result);

  if (match) {
    FcChar8 *file = nullptr;
    if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
      strncpy(fontPath, (const char *)file, sizeof(fontPath) - 1);
      fontPath[sizeof(fontPath) - 1] = '\0';
    }
    FcPatternDestroy(match);
  }

  FcPatternDestroy(pattern);

  return fontPath[0] ? fontPath : nullptr;
}

bool RocketUIImpl::LoadFont(const char *fontFamily, const char * /*unused*/) {
  const char *fontPath = FindSystemFont(fontFamily);
  if (!fontPath) {
    fprintf(stderr, "[RocketUI] Failed to find font: %s\n", fontFamily);
    return false;
  }

  // Load font file directly via stdio (RmlUi's FileInterface uses Source FS
  // which can't access system paths)
  FILE *fp = fopen(fontPath, "rb");
  if (!fp) {
    fprintf(stderr, "[RocketUI] Failed to open font file: %s\n", fontPath);
    return false;
  }

  fseek(fp, 0, SEEK_END);
  size_t fontLen = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (fontLen >= (8 * 1024 * 1024)) {
    fprintf(stderr, "[RocketUI] Font is over 8MB! Not loading: %s\n", fontPath);
    fclose(fp);
    return false;
  }

  unsigned char *fontBuffer = new unsigned char[fontLen];
  m_fontAllocs.AddToTail(fontBuffer);

  if (fread(fontBuffer, 1, fontLen, fp) != fontLen) {
    fprintf(stderr, "[RocketUI] Failed to read font file: %s\n", fontPath);
    fclose(fp);
    return false;
  }
  fclose(fp);

  // RmlUi 6.x API: LoadFontFace(Span<const byte> data, family, style, weight,
  // fallback)
  Rml::Span<const Rml::byte> fontSpan(fontBuffer, fontLen);
  if (!Rml::LoadFontFace(fontSpan, fontFamily, Rml::Style::FontStyle::Normal,
                         Rml::Style::FontWeight::Auto, true)) {
    fprintf(stderr, "[RocketUI] Failed to load font face: %s\n", fontPath);
    return false;
  }

  return true;
}

bool RocketUIImpl::LoadFonts() {
  bool fontsOK = true;
  // Load common fonts - fontconfig will find them on any Linux system
  fontsOK &= LoadFont("Liberation Sans", nullptr);
  fontsOK &= LoadFont("Liberation Sans:style=Bold", nullptr);
  fontsOK &= LoadFont("Liberation Mono", nullptr);
  fontsOK &= LoadFont("Liberation Mono:style=Bold", nullptr);
  return fontsOK;
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
    if (!m_ctxCurrent) {
      Error("Couldn't load document: %s - loaded before 1st frame.\n",
            filepath);
      return nullptr;
    }
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
  RocketRenderDXVK::m_Instance.SetScreenSize(width, height);

  // Allocate and store system cursors so we can swap to them on the fly
  RocketSystem::m_Instance.InitCursors();

  Rml::SetFileInterface(&RocketFileSystem::m_Instance);
  Rml::SetRenderInterface(&RocketRenderDXVK::m_Instance);
  Rml::SetSystemInterface(&RocketSystem::m_Instance);

  if (!Rml::Initialise()) {
    Warning("RocketUI: Initialise() failed!\n");
    return INIT_FAILED;
  }

  if (!LoadFonts()) {
    Warning("RocketUI: Failed to load fonts.\n");
    return INIT_FAILED;
  }

  m_ctxMenu = Rml::CreateContext("menu", Rml::Vector2i(width, height));
  m_ctxHud = Rml::CreateContext("hud", Rml::Vector2i(width, height));

  if (!m_ctxMenu || !m_ctxHud) {
    Warning("RocketUI: Failed to create Hud/Menu context\n");
    Rml::Shutdown();
    return INIT_FAILED;
  }

  m_ctxMenu->SetDensityIndependentPixelRatio(1.0f);
  m_ctxHud->SetDensityIndependentPixelRatio(1.0f);

  return INIT_OK;
}

void RocketUIImpl::Shutdown() {
  Rml::Shutdown();

  // freetype FT_Done_Face has been called. Time to free fonts.
  for (int i = 0; i < m_fontAllocs.Count(); i++) {
    unsigned char *fontAlloc = m_fontAllocs[i];
    delete[] fontAlloc;
  }

  RocketSystem::m_Instance.FreeCursors();
  RocketRenderDXVK::m_Instance.Shutdown();

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

void RocketUIImpl::RunFrame(float time) {
  if (!m_pDevice)
    return;

  m_fTime = time;

  // Update both contexts - menu context needs updates even during gameplay
  // (e.g., console is in menu context but rendered over HUD)
  // Lock mutexes to synchronize with render thread - RmlUi is not thread-safe
  if (m_ctxHud) {
    std::lock_guard<std::mutex> lock(m_mtxHud);
    m_ctxHud->Update();
  }
  if (m_ctxMenu) {
    std::lock_guard<std::mutex> lock(m_mtxMenu);
    m_ctxMenu->Update();
  }
}

void RocketUIImpl::DenyInputToGame(bool value, const char *why) {
  if (value) {
    m_numInputConsumers++;
    m_inputConsumers.AddToTail(CUtlString(why));
  } else {
    m_numInputConsumers--;
    m_inputConsumers.FindAndRemove(CUtlString(why));
  }

  EnableCursor((m_numInputConsumers > 0));

  if (rocket_verbose.GetBool()) {
    ConMsg("input Consumers[%d]: ", m_numInputConsumers);
    for (int i = 0; i < m_inputConsumers.Count(); i++) {
      ConMsg("(%s) ", m_inputConsumers[i].Get());
    }
    ConMsg("\n");
  }
}

bool RocketUIImpl::IsConsumingInput() { return (m_numInputConsumers > 0); }

void RocketUIImpl::EnableCursor(bool state) {
  ConVarRef cl_mouseenable("cl_mouseenable");
  cl_mouseenable.SetValue(!state);

  // Use same approach as VGUI - set cursor and visibility
  if (state) {
    m_pLauncherMgr->SetMouseCursor(
        RocketSystem::m_Instance.m_pCursors[SDL_SYSTEM_CURSOR_ARROW]);
  }
  m_pLauncherMgr->SetMouseVisible(state);

  m_bCursorVisible = state;
}

bool RocketUIImpl::HandleInputEvent(const InputEvent_t &event) {
  if (!m_ctxCurrent)
    return false;

  if (!rocket_enable.GetBool())
    return false;

  Rml::Context *inputCtx = m_ctxInput ? m_ctxInput : m_ctxCurrent;
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

  // Console input handling - highest priority
  if (m_consoleKeyInputFunc || m_consoleCharInputFunc) {
    switch (event.m_nType) {
    case IE_ButtonPressed:
    case IE_ButtonDoubleClicked:
      if (m_consoleKeyInputFunc && isKeyboard) {
        if (m_consoleKeyInputFunc(key, true))
          return true;
      }
      break;
    case IE_ButtonReleased:
      if (m_consoleKeyInputFunc && isKeyboard) {
        if (m_consoleKeyInputFunc(key, false))
          return true;
      }
      break;
    case IE_KeyCodeTyped:
      // Key repeat - only process if it's an actual repeat, not first duplicate
      if (!isFirstKeyCodeTyped && m_consoleKeyInputFunc && isKeyboard) {
        if (m_consoleKeyInputFunc(key, true))
          return true;
      }
      break;
    case IE_KeyTyped:
      if (m_consoleCharInputFunc) {
        if (m_consoleCharInputFunc((wchar_t)event.m_nData))
          return true;
      }
      break;
    default:
      break;
    }
  }

  // Always track mouse location
  if (event.m_nType == IE_AnalogValueChanged && event.m_nData == MOUSE_XY) {
    inputCtx->ProcessMouseMove(event.m_nData2, event.m_nData3, modifiers);
  }

  // Global hotkeys
  if (event.m_nType == IE_ButtonPressed) {
    if (key == KEY_F8) {
      ToggleDebugger();
      return true;
    }
    if (key == KEY_ESCAPE && m_togglePauseMenuFunc && m_pEngine->IsInGame()) {
      m_togglePauseMenuFunc();
    }
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

void RocketUIImpl::RenderHUDFrame() {
  if (!rocket_enable.GetBool())
    return;

  // HUD context is the default for input handling during gameplay
  m_ctxCurrent = m_ctxHud;

  // Lock mutex to synchronize with main thread Update - RmlUi is not
  // thread-safe
  std::lock_guard<std::mutex> lock(m_mtxHud);
  RocketRenderDXVK::m_Instance.BeginFrame();
  m_ctxHud->Render();
  RocketRenderDXVK::m_Instance.EndFrame();
}

void RocketUIImpl::RenderMenuFrame() {
  if (!rocket_enable.GetBool())
    return;

  // When called from main menu (V_RenderVGuiOnly), set menu as current context
  // When called during gameplay, don't change current context - just render
  bool isInGame = m_ctxCurrent == m_ctxHud;

  if (!isInGame) {
    m_ctxCurrent = m_ctxMenu;
  }

  // Lock mutex to synchronize with main thread Update - RmlUi is not
  // thread-safe
  std::lock_guard<std::mutex> lock(m_mtxMenu);
  RocketRenderDXVK::m_Instance.BeginFrame();
  m_ctxMenu->Render();
  RocketRenderDXVK::m_Instance.EndFrame();
}

bool RocketUIImpl::ReloadDocuments() {
  rocket_enable.SetValue(false);
  ThreadSleep(100);

  CUtlVector<CUtlPair<RocketDesinationContext_t,
                      CUtlPair<LoadDocumentFn, UnloadDocumentFn>>>
      copyOfPairs;

  for (int i = 0; i < m_documentReloadFuncs.Count(); i++) {
    copyOfPairs.AddToTail(m_documentReloadFuncs[i]);
  }

  m_documentReloadFuncs.Purge();

  for (int i = 0; i < copyOfPairs.Count(); i++) {
    if (copyOfPairs[i].first == ROCKET_CONTEXT_HUD && m_ctxCurrent != m_ctxHud)
      continue;
    if (copyOfPairs[i].first == ROCKET_CONTEXT_MENU &&
        m_ctxCurrent != m_ctxMenu)
      continue;
    if (copyOfPairs[i].first == ROCKET_CONTEXT_CURRENT) {
      copyOfPairs[i].second.second();
      continue;
    }

    // Unload...
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

  if (m_pDevice == nullptr) {
    // First time initialization
    m_pDevice = pDevice;
    RocketRenderDXVK::m_Instance.Initialize(pDevice);
  } else {
    // Device reset (map change) - release all RmlUi resources that use Vulkan
    // handles Geometry handles and texture descriptor sets become invalid after
    // reinit
    Rml::ReleaseCompiledGeometry(&RocketRenderDXVK::m_Instance);
    Rml::ReleaseTextures(&RocketRenderDXVK::m_Instance);

    // Set m_pDevice to nullptr DURING reinit to prevent RunFrame from running
    // while the renderer is in a half-initialized state (m_initialized = false)
    m_pDevice = nullptr;
    RocketRenderDXVK::m_Instance.Reinitialize(pDevice);
    m_pDevice = pDevice; // Restore AFTER reinit completes
  }

  D3DVIEWPORT9 viewport;
  pDevice->GetViewport(&viewport);
  SetScreenSize(viewport.Width, viewport.Height);
}

void RocketUIImpl::SetScreenSize(int width, int height) {
  m_ctxCurrent = nullptr;

  m_ctxHud->SetDimensions(Rml::Vector2i(width, height));
  m_ctxMenu->SetDimensions(Rml::Vector2i(width, height));

  RocketRenderDXVK::m_Instance.SetScreenSize(width, height);
}

void RocketUIImpl::ToggleDebugger() {
  static bool open = false;
  static bool firstTime = true;

  open = !open;

  if (!m_ctxCurrent)
    return;

  if (open) {
    if (firstTime) {
      if (Rml::Debugger::Initialise(m_ctxCurrent)) {
        firstTime = false;
      } else {
        ConMsg("[RocketUI] Error Initializing Debugger\n");
        return;
      }
    }
    ConMsg("[RocketUI] Opening Debugger\n");
    if (!Rml::Debugger::SetContext(m_ctxCurrent)) {
      ConMsg("[RocketUI] Error setting context!\n");
      return;
    }
    m_isDebuggerOpen = true;
    Rml::Debugger::SetVisible(true);
    DenyInputToGame(true, "RocketUI Debugger");
  } else {
    ConMsg("[RocketUI] Closing Debugger\n");
    Rml::Debugger::SetVisible(false);
    m_isDebuggerOpen = false;
    DenyInputToGame(false, "RocketUI Debugger");
  }
}
