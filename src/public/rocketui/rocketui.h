#ifndef ROCKETUI_H
#define ROCKETUI_H

#include "appframework/iappsystem.h"
#include "shaderapi/IShaderDevice.h"
#include "inputsystem/InputEnums.h"

#define ROCKETUI_INTERFACE_VERSION "RocketUI001"

// Forward declare IRocketUI before using it
class IRocketUI;

namespace Rml
{
    class Element;
    class ElementDocument;
    class Context;
    class EventListener;
}

enum RocketDesinationContext_t
{
    ROCKET_CONTEXT_MENU,
    ROCKET_CONTEXT_HUD,
    ROCKET_CONTEXT_CURRENT, // whichever context is active.
};

inline IRocketUI* RocketUI()
{
    extern IRocketUI* g_pRocketUI;
    return g_pRocketUI;
}

// Polled by RocketUI once per frame (and on every input event) to answer the
// single question "does the UI layer own the mouse right now?". See
// game/client/cstrike15/RocketUI/rkinputclaim.cpp for the one implementation.
typedef bool (*InputClaimQueryFn)(void);

typedef void (*LoadDocumentFn)(void);
typedef void (*UnloadDocumentFn)(void);
typedef void (*TogglePauseMenuFn)(void);
typedef bool (*ConsoleKeyInputFn)(int buttonCode, bool down);
typedef bool (*ConsoleCharInputFn)(wchar_t ch);

class IRocketUI : public IAppSystem
{
public:
    // Updates time mostly
    virtual void RunFrame(float time) = 0;
    // Reload from Disk
    virtual bool ReloadDocuments() = 0;

    // Feed input into UI
    virtual bool HandleInputEvent(const InputEvent_t &event) = 0;

    // Install the predicate that decides whether the UI owns mouse/keyboard input.
    // Ownership is DERIVED (polled) rather than reference counted on purpose: a
    // panel that forgets to "release" cannot strand the game without input, because
    // nothing is stored here that could outlive the panel's own state.
    virtual void SetInputClaimQuery(InputClaimQueryFn queryFunc) = 0;
    virtual bool IsConsumingInput(void) = 0;

    // Document manipulation
    // The Load/Unload functions are for hot-reloading, they will be called on rocket_reload.
    // If you want reloading to work with your element, they must be set.
    virtual Rml::ElementDocument *LoadDocumentFile(RocketDesinationContext_t ctx, const char *filepath,
            LoadDocumentFn loadDocumentFunc = nullptr, UnloadDocumentFn unloadDocumentFunc = nullptr) = 0;

    // Rendering is split so RmlUi stays single-threaded on the MAIN thread:
    //  * RecordHUD/RecordMenu run Context::Render() on the main thread and
    //    return an opaque command list (void*, actually a RocketCmdList*).
    //  * RenderHUDFrame/RenderMenuFrame take that list, replay it on the
    //    material-system render thread, and free it. Passing nullptr is a safe
    //    no-op. Ownership of the list transfers to the callee.
    virtual void *RecordHUD() = 0;
    virtual void *RecordMenu() = 0;
    virtual void RenderHUDFrame(void *cmdList) = 0;
    virtual void RenderMenuFrame(void *cmdList) = 0;

    // Access to the actual contexts in case you need something specific like data-bindings.
    virtual Rml::Context* AccessHudContext() = 0;
    virtual Rml::Context* AccessMenuContext() = 0;

    // The pause menu will register itself here so the library can open/close it via ESC
    virtual void RegisterPauseMenu(TogglePauseMenuFn showPauseMenuFunc) = 0;

    // Console input handlers - console registers these to receive input when visible
    virtual void RegisterConsoleHandlers(ConsoleKeyInputFn keyHandler, ConsoleCharInputFn charHandler) = 0;

    // Override which context receives mouse/input events (nullptr = use current render context)
    virtual void SetInputContext(Rml::Context* ctx) = 0;

    virtual void AddDeviceDependentObject(IShaderDeviceDependentObject *pObject) = 0;
    virtual void RemoveDeviceDependentObject(IShaderDeviceDependentObject *pObject) = 0;

    // NOTE: RmlUi is single-threaded and runs entirely on the MAIN thread here
    // (Update + input + Render-recording). Only recorded command lists cross to
    // the render thread, so NO external locking is needed to touch RmlUi
    // elements from the main thread.
};

#endif // ROCKETUI_H
