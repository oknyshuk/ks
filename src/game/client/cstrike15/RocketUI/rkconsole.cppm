// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;
#include <stddef.h>
#include <rocketui/rmlui.h>
#include <rocketui/rocketui.h>
#include "rkhud_model.h"
#include "../../hud_element_helper.h"
#include "igameevents.h"
#include "usermessages_schema.h"
module ks.client:cstrike15.RocketUI.rkconsole;

import std;
import ks.core.color;
import ks.platform;
import ks.simd;
import ks.tier1.icvar;
import :particlemgr;
import ks.core.basetypes;
import ks.tier1.convar;
import ks.game.shareddefs;

class RocketConsole
{
public:
    RocketConsole();
    ~RocketConsole();

    void Initialize();
    void Shutdown();

    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const { return m_bVisible; }

    void Clear();

    // Console output, installed with ICvar::InstallConsoleDisplayFunc
    void Display(ConsoleDisplayOp_t op, const Color& clr, const char* pMessage);
    void ColorPrint(const Color& clr, const char* pMessage);
    void Print(const char* pMessage);
    void DPrint(const char* pMessage);
    void GetConsoleText(char* pchText, size_t bufSize) const;

    // Input handling - called from RocketUI input system
    bool HandleKeyInput(int key, bool down);
    bool HandleCharInput(wchar_t ch);

    // Event callback for input changes (auto-update completions)
    void OnInputChange();

private:
    void LoadDocument();
    void UnloadDocument();

    // Input helpers (native input handles editing, we handle special keys)
    std::string GetInputValue() const;
    void SetInputValue(const char* text);
    void SetInputValueAndMoveCursorToEnd(const char* text);
    void MoveCursorToEnd();
    void Submit();
    bool IsInputFocused() const;

    // History
    void AddToHistory(const char* command);
    void HistoryUp();
    void HistoryDown();

    // Autocomplete
    void RebuildCompletionList();
    void ClearCompletionList();
    void ShowCompletionList();
    void HideCompletionList();
    void NextCompletion();
    void PrevCompletion();
    void ApplyCompletion();

    // Output
    void AddOutputLine(const char* text, const Color& color);
    void FlushOutput();
    void ScrollToBottom();

    static void LoadCallback();
    static void UnloadCallback();

private:
    Rml::ElementDocument* m_pDocument;
    Rml::ElementFormControlTextArea* m_elemOutput;  // Readonly textarea for output
    Rml::ElementFormControlInput* m_elemInput;      // Native input element
    Rml::Element* m_elemCompletionList;

    bool m_bVisible;
    bool m_bInitialized;

    // Command history
    std::vector< std::string > m_CommandHistory;
    int m_iHistoryPos;  // -1 = not browsing history
    std::string m_szSavedInput;  // Saved when browsing history

    // Autocomplete
    struct CompletionItem {
        std::string name;
        std::string value;
        bool isCommand;
    };
    std::vector< CompletionItem > m_CompletionList;
    int m_iCompletionIndex;
    bool m_bCompletionVisible;

    // Output buffer (for replay on document load)
    struct OutputLine {
        std::string text;
        Color color;
    };
    std::vector< OutputLine > m_OutputBuffer;
    static const int MAX_OUTPUT_LINES = 1000;
    static const size_t MAX_OUTPUT_SIZE = 64 * 1024;  // ~64KB scrollback, FIFO-trimmed (bounded geometry when visible)

    Color m_PrintColor;
    Color m_DPrintColor;

    // Pending output to batch updates
    Rml::String m_szPendingOutput;
};

RocketConsole& RkConsole();

// The HUD's one data model.
//
// Every hud_*.rml says data-model="hud" and reads its own section, e.g.
// crosshair.x0 or infobar.hp. Before this there were eight models, each with its
// own create/remove/handle plumbing repeated in the element that owned it, and
// each element could only ever see its own data.
//
// A section registers a bind function from its own file with RK_HUD_SECTION, so
// the schema still lives next to the code that fills it. All sections are bound
// when the model is created, which happens before any document loads, so a
// document may read across sections.
//
// The model deliberately outlives documents: it binds file-scope statics, so
// level changes and `rocket_reload` can close every document without touching it.

using RkHudSectionFn = void ( * )( Rml::DataModelConstructor & );

struct RkHudSection
{
    explicit RkHudSection( RkHudSectionFn bindFunc );
};

// Register a section's bind function. Place at file scope:
//     static void BindCrosshair( Rml::DataModelConstructor &c ) { ... }
//     RK_HUD_SECTION( BindCrosshair );

// The HUD model, created (and all sections bound) on first use. Invalid if there
// is no HUD context yet.
Rml::DataModelConstructor RkHudModel();

// Load a document that reads the model, ensuring the model exists in the target
// context first. Every UI document goes through here: RocketUI::LoadDocumentFile
// happily loads a document whose data-model is missing, which renders every
// binding as a blank and logs "Could not locate data model 'hud'" -- which is
// exactly what happened when a panel was opened before any HUD element had ever
// loaded, i.e. at the main menu.
Rml::ElementDocument *RkLoadDocument( RocketDesinationContext_t ctx, const char *file,
                                      LoadDocumentFn onLoad = nullptr,
                                      UnloadDocumentFn onUnload = nullptr );

// Mark one section dirty by the name it was bound under. Only the views reading
// that section are re-evaluated, so one element updating does not cost the rest.
void RkHudDirty( const char *section );

class CHudElement;

// Document lifetime for the RmlUi-backed HUD elements.
//
// Every one of these carried the same pair of free functions: fetch the element
// through GET_HUDELEMENT, warn if that failed, bail if already loaded, check the
// HUD context, load the .rml, warn if that failed, and on the way out Close() and
// null the handle. Nine copies, differing only in the names inside the strings.
//
// What is genuinely per-element -- what to cache from the document, which
// listeners to add, what to do per frame -- stays in the element as OnLoad,
// OnUnload and its own ShowPanel. Visibility and input are deliberately *not*
// managed here: they differ enough per element that a shared version would be a
// pile of flags.
//
// Usage:
//     class RkHudThing : public RkHudDocument<RkHudThing> { ... };
//     const char *RkHudThing::kDocument = "hud_thing.rml";
template <typename T>
class RkHudDocument : public CHudElement
{
public:
    explicit RkHudDocument( const char *pElementName ) : CHudElement( pElementName ) {}

    // CHudElement
    void LevelInit() override { Load(); }
    void LevelShutdown() override { Unload(); }

    Rml::ElementDocument *m_pDocument = nullptr;
    bool m_bVisible = false;

protected:
    // Defaults, hidden by T if it needs them.
    void OnLoad() {}
    void OnUnload() {}

    void Load()
    {
        if ( m_pDocument )
            return;

        m_pDocument = RkLoadDocument( ROCKET_CONTEXT_HUD, T::kDocument, &LoadThunk, &UnloadThunk );
        if ( !m_pDocument )
            return;

        static_cast<T *>( this )->OnLoad();
    }

    void Unload()
    {
        if ( !m_pDocument )
            return;

        static_cast<T *>( this )->OnUnload();
        m_pDocument->Close();
        m_pDocument = nullptr;
        m_bVisible = false;
    }

private:
    // `rocket_reload` hands documents back plain function pointers, so each
    // element needs its own pair. CRTP generates them.
    static void LoadThunk()
    {
        if ( T *element = GET_HUDELEMENT( T ) )
            element->Load();
    }
    static void UnloadThunk()
    {
        if ( T *element = GET_HUDELEMENT( T ) )
            element->Unload();
    }
};

extern ConVar cl_drawhud;

class RkHudBuyMenu : public RkHudDocument<RkHudBuyMenu>
{
public:
    static const char *kDocument;

    explicit RkHudBuyMenu( const char * value );
    virtual ~RkHudBuyMenu();

    // Overrides from CHudElement
    void LevelInit() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

    // Game event handler
    void FireGameEvent( IGameEvent *event );

    void UpdateBuyMenu();
    void OnNewFrameBuyMenu();
};

extern ConVar cl_drawhud;
extern ConVar cl_showtextmsg;

class RkHudChat : public RkHudDocument<RkHudChat> {
public:
    static const char *kDocument;

    enum MessageSender
    {
        SERVER,
        FRIEND,
        FOE,
    };
    explicit RkHudChat(const char *value);
    virtual ~RkHudChat();

    bool ChatRaised();

    void AddChatString( const char *username, const char *message, MessageSender sender );
    void AddChatString( const wchar_t *username, const wchar_t *message, MessageSender sender );
    void ClearChatHistory();

    // Starts the typing sequence.
    void StartMessageMode(int mode);
    void StopMessageMode();
    int GetMessageMode()
    {
        return m_iMode;
    }

    // Overrides from CHudElement
    void LevelInit() override;
    void LevelShutdown() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();
    void OnUnload();

    Rml::Element *m_elemChatInput;

    int			m_iMode;

    CUserMessageBinder m_UMCMsgSayText2;
    CUserMessageBinder m_UMCMsgTextMsg;
};

extern ConVar cl_drawhud;

class RkHudCrosshair : public RkHudDocument<RkHudCrosshair> {
public:
    static const char *kDocument;

    explicit RkHudCrosshair(const char *pElementName);
    virtual ~RkHudCrosshair();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

private:
    void UpdateCrosshair();
};

extern ConVar cl_drawhud;

class RkHudInfoBar : public RkHudDocument<RkHudInfoBar> {
public:
    static const char *kDocument;

    explicit RkHudInfoBar(const char *value);
    virtual ~RkHudInfoBar();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

};

extern ConVar cl_drawhud;

class RkHudKillfeed : public RkHudDocument<RkHudKillfeed>
{
public:
    static const char *kDocument;

    explicit RkHudKillfeed( const char *value );
    virtual ~RkHudKillfeed();

    // Overrides from CHudElement
    void LevelInit() override;
    void LevelShutdown() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // Game event handler
    void FireGameEvent( IGameEvent *event );

private:
    void CheckForOldEntries();
    void OnPlayerDeath( IGameEvent *event );
};

class RocketLoadingScreenDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }
private:
    static bool m_bVisible;
};

class RkPauseMenuButtons;

// Dismiss the in-game UI and hand the mouse back to the game. See the definition:
// hiding our own documents is not enough.
void RocketUI_ReturnToGame();

class RocketPauseMenuDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }
private:
    static bool m_bVisible;
};

extern ConVar cl_drawhud;

struct SpottedInfo
{
    // info from ProcessSpottedEntityUpdate
    int entId;
    int classId;
    int originX;
    int originY;
    int originZ;
    int angleYaw;
    bool defuser;
    bool playerWithDefuser;
    bool playerWithC4;

    float timelastSpotted;
};

class RkHudRadar : public RkHudDocument<RkHudRadar>
{
public:
    static const char *kDocument;

    explicit RkHudRadar( const char *value );
    virtual ~RkHudRadar();

    // Overrides from CHudElement
    void LevelInit() override;
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // Hooked msg
    bool MsgFunc_ProcessSpottedEntityUpdate( const ks::net::CCSUsrMsg_ProcessSpottedEntityUpdate &msg );

    void UpdateRadarFrame();
    void UpdateRadarSize();

    float           m_radarWidth;
    float           m_radarHeight;
    float           m_radarCenterX;
    float           m_radarCenterY;
    SpottedInfo     m_spottedPlayers[MAX_PLAYERS];
    SpottedInfo     m_spottedDefuser;
    SpottedInfo     m_spottedC4;

    CUserMessageBinder m_UMCMsgProcessSpottedEntityUpdate;
};

extern ConVar cl_drawhud;

class RkHudRoundTimer : public RkHudDocument<RkHudRoundTimer> {
public:
    static const char *kDocument;

    explicit RkHudRoundTimer( const char *value );
    virtual ~RkHudRoundTimer();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

};

extern ConVar cl_drawhud;

class RkHudScope : public RkHudDocument<RkHudScope> {
public:
    static const char *kDocument;

    explicit RkHudScope(const char *pElementName);
    virtual ~RkHudScope();

    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);

private:
    void UpdateScope();

    // Animation state
    float m_fAnimInset;
    float m_fLineSpreadDistance;
};

extern ConVar cl_drawhud;

class RkHudScoreboard : public RkHudDocument<RkHudScoreboard> {
public:
    static const char *kDocument;

    explicit RkHudScoreboard(const char *value);
    virtual ~RkHudScoreboard();

    // Overrides from CHudElement
    virtual void SetActive(bool bActive);
    virtual bool ShouldDraw(void);
    void ShowPanel(bool bShow, bool force);
    void OnLoad();

    // Some precached elements from the instance.
    Rml::Element *m_elemCtSection;
    Rml::Element *m_elemTSection;

private:
    void Update();
};

class RocketTeamMenuEventListener : public CGameEventListener
{
public:
    void StartAlwaysListenEvents();
    void StopAlwaysListenEvents();
    void FireGameEvent( IGameEvent *event );
};

class RocketTeamMenuDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

private:
    static bool m_bVisible;
    static RocketTeamMenuEventListener *m_pEventListener;
};

class RocketMainMenuDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void RestorePanel( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return showing; }
    static void UpdateDialog( void );
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }
private:
    static bool showing;
};

// The shared half of a menu panel's lifecycle.
//
// Six panels (main menu, pause menu, team menu, loading screen, options, play)
// each had their own copy of: load the document if it isn't up, warn if that
// failed, Close() and forget it on the way out, and Show()/Hide() while tracking a
// visible flag. The panels keep their own statics -- these take them by reference,
// so nothing about how a panel is declared or reached has to change.
//
// What stays with the panel is what differs: which controls to populate, and
// whether it is allowed to open at all.
//
// A panel must not close its own document from one of its data-event callbacks:
// that callback is a data controller, and the close destroys it while the data
// interpreter is still running its expression. So dismissal only hides, and the
// transient panels (play, options) are rebuilt on the next load.

// Loads the document if needed. Returns it, or nullptr if the load failed; the
// caller's pointer is updated either way. onLoad/onUnload are the hot-reload pair
// `rocket_reload` calls.
Rml::ElementDocument *RkPanelLoad( Rml::ElementDocument *&doc, RocketDesinationContext_t ctx,
                                   const char *file, LoadDocumentFn onLoad,
                                   UnloadDocumentFn onUnload );

// Closes and forgets the document. Safe to call when it is not loaded.
void RkPanelUnload( Rml::ElementDocument *&doc, bool &visible );

// Shows or hides an already-loaded document. Panels that own the mouse are asked
// about it through their own OwnsInput(), which reads `visible`.
void RkPanelShow( Rml::ElementDocument *doc, bool &visible, bool show );

class RocketOptionsDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

    // Driven from panel_options.rml (data-event-click / data-event-change).
    static void SwitchTab( const char *tabId );
    static void PopulateResolution( void );

    // True while PopulateControls syncs the controls to the current values, so
    // the change actions can tell that apart from a user edit.
    static bool m_bPopulating;

private:
    static void PopulateControls( void );
    static void PopulateDisplayMode( void );

    static bool m_bVisible;
};

class RocketPlayDocument
{
protected:
    static Rml::ElementDocument *m_pInstance;

public:
    static void LoadDialog( void );
    static void UnloadDialog( void );
    static void ShowPanel( bool bShow, bool immediate = false );
    static bool IsActive() { return m_pInstance != nullptr; }
    static bool IsVisible() { return m_bVisible; }
    static Rml::ElementDocument *GetInstance() { return m_pInstance; }

    // Driven from panel_play.rml (data-event-click).
    static void StartServer( void );

private:
    static void PopulateMapList( void );

    static bool m_bVisible;
};

inline constexpr auto MAX_POPUP_TEXT = 256;

class RocketPopupDocument : Rml::EventListener
{
public:
    RocketPopupDocument( const char *text, float timeout = 5.0f );
    virtual ~RocketPopupDocument();
    bool IsActive() { return m_pInstance != nullptr; }
    bool IsVisible() { return m_bVisible; }
    Rml::ElementDocument *GetInstance() { return m_pInstance; }

    void ProcessEvent( Rml::Event &event ) override;
private:
    Rml::ElementDocument *m_pInstance;
    char m_textBuffer[MAX_POPUP_TEXT];

    float m_fTimeout;
    bool m_bVisible;
    bool m_bGrabbingInput;
};
