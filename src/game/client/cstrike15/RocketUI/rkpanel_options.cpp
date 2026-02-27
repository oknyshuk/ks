#include "rkpanel_options.h"

#include "cbase.h"
#include "cdll_client_int.h" // extern globals to interfaces like engineclient
#include "tier1/convar.h"
#include "IGameUIFuncs.h"
#include "modes.h"
#include "materialsystem/materialsystem_config.h"

extern IGameUIFuncs *gameuifuncs;
extern IMaterialSystem *materials;

// min/max conflict handled by RMLUI_USE_CUSTOM_ASSERT

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

Rml::ElementDocument *RocketOptionsDocument::m_pInstance = nullptr;
bool RocketOptionsDocument::m_bVisible = false;
bool RocketOptionsDocument::m_bGrabbingInput = false;
bool RocketOptionsDocument::m_bPopulating = false;

// Helper: set a <select> element's value from a ConVar int
static void SetSelectFromConVar( Rml::ElementDocument *doc, const char *elementId, const char *convarName )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    ConVarRef cvar( convarName );
    if( !cvar.IsValid() )
        return;

    char valBuf[32];
    V_snprintf( valBuf, sizeof(valBuf), "%d", cvar.GetInt() );
    sel->SetValue( Rml::String( valBuf ) );
}

// Helper: set a range input's value from a ConVar float
static void SetRangeFromConVar( Rml::ElementDocument *doc, const char *elementId, const char *convarName )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( !elem )
        return;

    auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>( elem );
    if( !input )
        return;

    ConVarRef cvar( convarName );
    if( !cvar.IsValid() )
        return;

    char valBuf[32];
    V_snprintf( valBuf, sizeof(valBuf), "%.2f", cvar.GetFloat() );
    input->SetValue( Rml::String( valBuf ) );

    // Update the value label if it exists
    char labelId[64];
    V_snprintf( labelId, sizeof(labelId), "%s_val", elementId );
    Rml::Element *label = doc->GetElementById( labelId );
    if( label )
        label->SetInnerRML( Rml::String( valBuf ) );
}

// Helper: set a checkbox from a ConVar bool
static void SetCheckboxFromConVar( Rml::ElementDocument *doc, const char *elementId, const char *convarName, bool negate = false )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( !elem )
        return;

    ConVarRef cvar( convarName );
    if( !cvar.IsValid() )
        return;

    bool checked = negate ? !cvar.GetBool() : cvar.GetBool();
    if( checked )
        elem->SetAttribute( "checked", "" );
    else
        elem->RemoveAttribute( "checked" );
}

// Click listener for tabs and buttons only - NOT added to the whole document
class RkOptionsClickListener : public Rml::EventListener
{
public:
    void ProcessEvent( Rml::Event &event ) override
    {
        Rml::Element *current = event.GetCurrentElement();
        if( !current )
            return;

        const Rml::String &id = current->GetId();

        if( id == "tab-video" || id == "tab-audio" || id == "tab-mouse" )
        {
            RocketOptionsDocument::SwitchTab( id.c_str() );
        }
        else if( id == "close" )
        {
            RocketOptionsDocument::ShowPanel( false );
            RocketOptionsDocument::UnloadDialog();
        }
        else if( id == "save" )
        {
            Msg( "Saving settings.\n" );
            engine->ClientCmd_Unrestricted( "mat_savechanges" );
            engine->ClientCmd_Unrestricted( "host_writeconfig" );
        }
    }
};

// Forward declaration - defined below PopulateControls
static void ApplyVideoMode( Rml::ElementDocument *doc );

// Change listener for form controls - added to document to catch bubbling change events
class RkOptionsChangeListener : public Rml::EventListener
{
public:
    void ProcessEvent( Rml::Event &event ) override
    {
        // Don't process changes fired during PopulateControls — those are
        // just syncing UI to current ConVar values, not user input
        if( RocketOptionsDocument::m_bPopulating )
            return;

        Rml::Element *target = event.GetTargetElement();
        if( !target )
            return;

        const Rml::String &id = target->GetId();
        Rml::String value = event.GetParameter<Rml::String>( "value", "" );

        // Resolution changed — apply video mode
        if( id == "resolution" )
        {
            ApplyVideoMode( target->GetOwnerDocument() );
            return;
        }

        // Display mode changed — apply and repopulate resolution list
        // (windowed mode filters out modes larger than desktop)
        if( id == "display_mode" )
        {
            ApplyVideoMode( target->GetOwnerDocument() );
            RocketOptionsDocument::PopulateResolution();
            return;
        }

        // Composite: sound quality
        if( id == "snd_quality" )
        {
            int quality = atoi( value.c_str() );
            ConVarRef sndPitch( "Snd_PitchQuality" );
            ConVarRef dspSlow( "dsp_slow_cpu" );
            ConVarRef dspStereo( "dsp_enhance_stereo" );
            switch( quality )
            {
            case 0: // Low
                dspSlow.SetValue( true );
                sndPitch.SetValue( false );
                break;
            case 1: // Medium
                dspSlow.SetValue( false );
                sndPitch.SetValue( false );
                break;
            case 2: // High
            default:
                dspSlow.SetValue( false );
                sndPitch.SetValue( true );
                break;
            }
            if( dspStereo.IsValid() )
                dspStereo.SetValue( 0 );
            return;
        }

        // Composite: closed captions
        if( id == "closedcaptions" )
        {
            int ccMode = atoi( value.c_str() );
            ConVarRef ccSub( "cc_subtitles" );
            int closecaptionVal = 0;
            switch( ccMode )
            {
            case 0: // Disabled
                closecaptionVal = 0;
                ccSub.SetValue( 0 );
                break;
            case 1: // Subtitles & Sound Effects
                closecaptionVal = 1;
                ccSub.SetValue( 0 );
                break;
            case 2: // Subtitles Only
                closecaptionVal = 1;
                ccSub.SetValue( 1 );
                break;
            }
            char cmd[64];
            V_snprintf( cmd, sizeof(cmd), "closecaption %d", closecaptionVal );
            engine->ClientCmd_Unrestricted( cmd );
            return;
        }

        // Reverse mouse: m_pitch negate (positive = not reversed)
        if( id == "m_pitch" )
        {
            ConVarRef pitch( "m_pitch" );
            if( pitch.IsValid() )
            {
                bool checked = target->HasAttribute( "checked" );
                float absVal = fabsf( pitch.GetFloat() );
                if( absVal < 0.0001f )
                    absVal = 0.022f;
                pitch.SetValue( checked ? -absVal : absVal );
            }
            return;
        }

        // Mouse acceleration: m_customaccel (0 or 3)
        if( id == "m_customaccel" )
        {
            ConVarRef accel( "m_customaccel" );
            if( accel.IsValid() )
            {
                bool checked = target->HasAttribute( "checked" );
                accel.SetValue( checked ? 3 : 0 );
            }
            return;
        }

        // Simple convar from data-convar attribute
        Rml::String convarName = target->GetAttribute<Rml::String>( "data-convar", "" );
        if( convarName.empty() )
            return;

        // Determine element type
        Rml::String tag = target->GetTagName();
        if( tag == "select" )
        {
            ConVarRef cvar( convarName.c_str() );
            if( cvar.IsValid() )
                cvar.SetValue( atoi( value.c_str() ) );
        }
        else if( tag == "input" )
        {
            Rml::String type = target->GetAttribute<Rml::String>( "type", "" );
            if( type == "range" )
            {
                float fVal = (float)atof( value.c_str() );
                ConVarRef cvar( convarName.c_str() );
                if( cvar.IsValid() )
                    cvar.SetValue( fVal );

                // Update the value label
                char labelId[64];
                V_snprintf( labelId, sizeof(labelId), "%s_val", target->GetId().c_str() );
                Rml::ElementDocument *doc = target->GetOwnerDocument();
                if( doc )
                {
                    Rml::Element *label = doc->GetElementById( labelId );
                    if( label )
                    {
                        char valBuf[32];
                        V_snprintf( valBuf, sizeof(valBuf), "%.2f", fVal );
                        label->SetInnerRML( Rml::String( valBuf ) );
                    }
                }
            }
            else if( type == "checkbox" )
            {
                ConVarRef cvar( convarName.c_str() );
                if( cvar.IsValid() )
                {
                    bool checked = target->HasAttribute( "checked" );
                    cvar.SetValue( checked ? 1 : 0 );
                }
            }
        }
    }
};

static RkOptionsClickListener optionsClickListener;
static RkOptionsChangeListener optionsChangeListener;

RocketOptionsDocument::RocketOptionsDocument()
{
}

RocketOptionsDocument::~RocketOptionsDocument()
{
}

void RocketOptionsDocument::SwitchTab( const char *tabId )
{
    if( !m_pInstance )
        return;

    static const char *tabIds[] = { "tab-video", "tab-audio", "tab-mouse" };
    static const char *pageIds[] = { "page-video", "page-audio", "page-mouse" };

    for( int i = 0; i < 3; i++ )
    {
        Rml::Element *tab = m_pInstance->GetElementById( tabIds[i] );
        Rml::Element *page = m_pInstance->GetElementById( pageIds[i] );

        if( !V_strcmp( tabId, tabIds[i] ) )
        {
            if( tab ) tab->SetClass( "active", true );
            if( page ) page->SetClass( "visible", true );
        }
        else
        {
            if( tab ) tab->SetClass( "active", false );
            if( page ) page->SetClass( "visible", false );
        }
    }
}

void RocketOptionsDocument::PopulateResolution()
{
    if( !m_pInstance || !gameuifuncs )
        return;

    Rml::Element *elem = m_pInstance->GetElementById( "resolution" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    // Get current render resolution
    int curWidth = 0, curHeight = 0;
    if( materials )
        materials->GetBackBufferDimensions( curWidth, curHeight );

    // Get desktop resolution of the current display
    int desktopW = 0, desktopH = 0;
    gameuifuncs->GetDesktopResolution( desktopW, desktopH );

    // Check if windowed — only filter by desktop resolution in windowed mode
    // In fullscreen (Wayland FULLSCREEN_DESKTOP), resolution is internal render
    // resolution so any mode should be available
    bool bWindowed = false;
    if( materials )
        bWindowed = materials->GetCurrentConfigForVideoCard().Windowed();

    // Get available video modes from the engine
    vmode_t *pModes = nullptr;
    int nModes = 0;
    gameuifuncs->GetVideoModes( &pModes, &nModes );

    // Clear all existing options from the selectbox widget
    sel->RemoveAll();

    int selectedIdx = -1;
    int bestMatch = INT_MAX;
    int optionCount = 0;

    // Check if current backbuffer resolution is in the mode list.
    // The engine may have auto-adjusted to a display's native resolution
    // (e.g. 1440p) that wasn't in the driver's init-time mode list.
    bool bBackbufferInList = false;
    for( int i = 0; i < nModes; i++ )
    {
        if( pModes[i].width == curWidth && pModes[i].height == curHeight )
        {
            bBackbufferInList = true;
            break;
        }
    }

    // Build options from the driver's mode list (already sorted ascending by engine).
    // Insert the current backbuffer resolution at its sorted position if missing.
    Rml::String optionsRml;
    bool bInsertedBackbuffer = bBackbufferInList;
    for( int i = 0; i < nModes; i++ )
    {
        int w = pModes[i].width;
        int h = pModes[i].height;

        // Don't show modes bigger than the desktop for windowed mode (matches VGUI)
        if( bWindowed && desktopW > 0 && desktopH > 0 && ( w > desktopW || h > desktopH ) )
            continue;

        // Insert backbuffer resolution at its sorted position
        if( !bInsertedBackbuffer && curWidth > 0 && curHeight > 0 &&
            ( w > curWidth || ( w == curWidth && h > curHeight ) ) )
        {
            char opt[128];
            V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", curWidth, curHeight, curWidth, curHeight );
            optionsRml += opt;
            selectedIdx = optionCount;
            optionCount++;
            bInsertedBackbuffer = true;
        }

        char opt[128];
        V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", w, h, w, h );
        optionsRml += opt;

        int diff = abs( w - curWidth ) + abs( h - curHeight );
        if( diff < bestMatch )
        {
            bestMatch = diff;
            selectedIdx = optionCount;
        }
        optionCount++;
    }

    // Backbuffer resolution is higher than all modes — append at end
    if( !bInsertedBackbuffer && curWidth > 0 && curHeight > 0 )
    {
        char opt[128];
        V_snprintf( opt, sizeof(opt), "<option value=\"%dx%d\">%dx%d</option>", curWidth, curHeight, curWidth, curHeight );
        optionsRml += opt;
        selectedIdx = optionCount;
        optionCount++;
    }

    // Inject as RML — options become DOM children, then MoveChildren() moves them
    // to the selectbox widget (same path as static RML options)
    elem->SetInnerRML( optionsRml );

    if( selectedIdx >= 0 )
        sel->SetSelection( selectedIdx );
}

void RocketOptionsDocument::PopulateDisplayMode()
{
    if( !m_pInstance || !materials )
        return;

    const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

    Rml::Element *elem = m_pInstance->GetElementById( "display_mode" );
    if( !elem )
        return;

    auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
    if( !sel )
        return;

    sel->SetValue( config.Windowed() ? "1" : "0" );
}

// Apply resolution + display mode via mat_setvideomode
static void ApplyVideoMode( Rml::ElementDocument *doc )
{
    if( !doc )
        return;

    // Read resolution select
    Rml::Element *resElem = doc->GetElementById( "resolution" );
    Rml::Element *modeElem = doc->GetElementById( "display_mode" );
    if( !resElem || !modeElem )
        return;

    auto *resSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( resElem );
    auto *modeSel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( modeElem );
    if( !resSel || !modeSel )
        return;

    Rml::String resValue = resSel->GetValue();
    Rml::String modeValue = modeSel->GetValue();

    // Parse "WxH" from resolution value
    int w = 0, h = 0;
    if( sscanf( resValue.c_str(), "%dx%d", &w, &h ) != 2 || w <= 0 || h <= 0 )
        return;

    int windowed = atoi( modeValue.c_str() );

    char cmd[128];
    V_snprintf( cmd, sizeof(cmd), "mat_setvideomode %d %d %d", w, h, windowed );
    engine->ClientCmd_Unrestricted( cmd );
}

void RocketOptionsDocument::PopulateControls()
{
    if( !m_pInstance )
        return;

    // Suppress change events while syncing UI to current state
    m_bPopulating = true;

    // Video settings - resolution and display mode
    PopulateResolution();
    PopulateDisplayMode();

    // Video settings - simple convar selects
    SetSelectFromConVar( m_pInstance, "csm_quality_level", "csm_quality_level" );
    SetSelectFromConVar( m_pInstance, "gpu_mem_level", "gpu_mem_level" );
    SetSelectFromConVar( m_pInstance, "cpu_level", "cpu_level" );
    SetSelectFromConVar( m_pInstance, "gpu_level", "gpu_level" );
    SetSelectFromConVar( m_pInstance, "mat_queue_mode", "mat_queue_mode" );
    SetSelectFromConVar( m_pInstance, "mat_picmip", "mat_picmip" );
    SetSelectFromConVar( m_pInstance, "mat_forceaniso", "mat_forceaniso" );
    SetSelectFromConVar( m_pInstance, "mat_antialias", "mat_antialias" );
    SetSelectFromConVar( m_pInstance, "mat_vsync", "mat_vsync" );

    // Video settings - range slider
    SetRangeFromConVar( m_pInstance, "mat_monitorgamma", "mat_monitorgamma" );

    // Audio settings - range sliders
    SetRangeFromConVar( m_pInstance, "volume", "volume" );
    SetRangeFromConVar( m_pInstance, "snd_musicvolume", "Snd_MusicVolume" );

    // Audio settings - simple convar select
    SetSelectFromConVar( m_pInstance, "snd_surround_speakers", "Snd_Surround_Speakers" );

    // Audio settings - composite: sound quality
    {
        ConVarRef sndPitch( "Snd_PitchQuality" );
        ConVarRef dspSlow( "dsp_slow_cpu" );
        int quality = 0; // Low
        if( dspSlow.IsValid() && !dspSlow.GetBool() )
            quality = 1; // Medium
        if( sndPitch.IsValid() && sndPitch.GetBool() )
            quality = 2; // High

        Rml::Element *elem = m_pInstance->GetElementById( "snd_quality" );
        if( elem )
        {
            auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
            if( sel )
            {
                char valBuf[8];
                V_snprintf( valBuf, sizeof(valBuf), "%d", quality );
                sel->SetValue( Rml::String( valBuf ) );
            }
        }
    }

    // Audio settings - composite: closed captions
    {
        ConVarRef closecaption( "closecaption" );
        ConVarRef ccSub( "cc_subtitles" );
        int ccMode = 0; // Disabled
        if( closecaption.IsValid() && closecaption.GetBool() )
        {
            if( ccSub.IsValid() && ccSub.GetBool() )
                ccMode = 2; // Subtitles Only
            else
                ccMode = 1; // Subtitles & Sound Effects
        }

        Rml::Element *elem = m_pInstance->GetElementById( "closedcaptions" );
        if( elem )
        {
            auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( elem );
            if( sel )
            {
                char valBuf[8];
                V_snprintf( valBuf, sizeof(valBuf), "%d", ccMode );
                sel->SetValue( Rml::String( valBuf ) );
            }
        }
    }

    // Mouse settings - range slider
    SetRangeFromConVar( m_pInstance, "sensitivity", "sensitivity" );

    // Mouse settings - simple convar checkboxes
    SetCheckboxFromConVar( m_pInstance, "m_rawinput", "m_rawinput" );
    SetCheckboxFromConVar( m_pInstance, "m_filter", "m_filter" );

    // Mouse settings - reverse mouse (positive m_pitch = not reversed)
    {
        ConVarRef pitch( "m_pitch" );
        Rml::Element *elem = m_pInstance->GetElementById( "m_pitch" );
        if( elem && pitch.IsValid() )
        {
            if( pitch.GetFloat() < 0.0f )
                elem->SetAttribute( "checked", "" );
            else
                elem->RemoveAttribute( "checked" );
        }
    }

    // Mouse settings - acceleration (m_customaccel: 0=off, 3=on)
    {
        ConVarRef accel( "m_customaccel" );
        Rml::Element *elem = m_pInstance->GetElementById( "m_customaccel" );
        if( elem && accel.IsValid() )
        {
            if( accel.GetInt() != 0 )
                elem->SetAttribute( "checked", "" );
            else
                elem->RemoveAttribute( "checked" );
        }
    }

    m_bPopulating = false;
}

// Helper to attach click listener to a specific element by ID
static void AttachClickListener( Rml::ElementDocument *doc, const char *elementId )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( elem )
        elem->AddEventListener( Rml::EventId::Click, &optionsClickListener );
}

// Helper to remove click listener from a specific element by ID
static void DetachClickListener( Rml::ElementDocument *doc, const char *elementId )
{
    Rml::Element *elem = doc->GetElementById( elementId );
    if( elem )
        elem->RemoveEventListener( Rml::EventId::Click, &optionsClickListener );
}

void RocketOptionsDocument::LoadDialog()
{
    if( !m_pInstance )
    {
        m_pInstance = RocketUI()->LoadDocumentFile( ROCKET_CONTEXT_CURRENT, "panel_options.rml", RocketOptionsDocument::LoadDialog, RocketOptionsDocument::UnloadDialog );
        if( !m_pInstance )
        {
            Error( "Couldn't create rocketui options!\n" );
            /* Exit */
        }

        // Attach click listeners to specific interactive elements only
        AttachClickListener( m_pInstance, "tab-video" );
        AttachClickListener( m_pInstance, "tab-audio" );
        AttachClickListener( m_pInstance, "tab-mouse" );
        AttachClickListener( m_pInstance, "save" );
        AttachClickListener( m_pInstance, "close" );

        // Change listener at document level to catch form control value changes
        m_pInstance->AddEventListener( Rml::EventId::Change, &optionsChangeListener );

        PopulateControls();
    }
}

void RocketOptionsDocument::UnloadDialog()
{
    if( m_pInstance )
    {
        DetachClickListener( m_pInstance, "tab-video" );
        DetachClickListener( m_pInstance, "tab-audio" );
        DetachClickListener( m_pInstance, "tab-mouse" );
        DetachClickListener( m_pInstance, "save" );
        DetachClickListener( m_pInstance, "close" );
        m_pInstance->RemoveEventListener( Rml::EventId::Change, &optionsChangeListener );

        m_pInstance->Close();
        m_pInstance = nullptr;

        if( m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( false, "OptionsPanel" );
            m_bGrabbingInput = false;
        }
    }

    m_bVisible = false;
}

void RocketOptionsDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
    {
        if( !m_pInstance )
            LoadDialog();

        // Refresh controls each time panel is shown
        PopulateControls();

        m_pInstance->Show();

        if( !m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( true, "OptionsPanel" );
            m_bGrabbingInput = true;
        }
    }
    else
    {
        if( m_pInstance )
            m_pInstance->Hide();

        if( m_bGrabbingInput )
        {
            RocketUI()->DenyInputToGame( false, "OptionsPanel" );
            m_bGrabbingInput = false;
        }
    }

    m_bVisible = bShow;
}

