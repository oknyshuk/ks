#include "rkpanel_options.h"

#include "rkpanel.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "cdll_client_int.h" // extern globals to interfaces like engineclient
#include "tier1/convar.h"
#include "IGameUIFuncs.h"
#include "modes.h"
#include "materialsystem/materialsystem_config.h"

extern IGameUIFuncs *gameuifuncs;
extern IMaterialSystem *materials;

#include <rocketui/rmlui.h>
#include "rkhud_pausemenu.h"
#include "rkmenu_main.h"

Rml::ElementDocument *RocketOptionsDocument::m_pInstance = nullptr;
bool RocketOptionsDocument::m_bVisible = false;
bool RocketOptionsDocument::m_bPopulating = false;

// Push the current ConVar value into one control. The mirror of
// ApplyConVarFromControl, which reads it back out on change.
static void PopulateControlFromConVar( Rml::Element *elem )
{
    const Rml::String convarName = elem->GetAttribute<Rml::String>( "data-convar", "" );
    if ( convarName.empty() )
        return;

    ConVarRef cvar( convarName.c_str() );
    if ( !cvar.IsValid() )
        return;

    const Rml::String tag = elem->GetTagName();
    const Rml::String type = elem->GetAttribute<Rml::String>( "type", "" );
    char valBuf[32];

    if ( tag == "select" )
    {
        V_snprintf( valBuf, sizeof( valBuf ), "%d", cvar.GetInt() );
        if ( auto *sel = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>( elem ) )
            sel->SetValue( Rml::String( valBuf ) );
    }
    else if ( type == "checkbox" )
    {
        if ( cvar.GetBool() )
            elem->SetAttribute( "checked", "" );
        else
            elem->RemoveAttribute( "checked" );
    }
    else if ( type == "range" )
    {
        V_snprintf( valBuf, sizeof( valBuf ), "%.2f", cvar.GetFloat() );
        if ( auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>( elem ) )
            input->SetValue( Rml::String( valBuf ) );

        char labelId[64];
        V_snprintf( labelId, sizeof( labelId ), "%s_val", elem->GetId().c_str() );
        if ( Rml::ElementDocument *doc = elem->GetOwnerDocument() )
            if ( Rml::Element *label = doc->GetElementById( labelId ) )
                label->SetInnerRML( Rml::String( valBuf ) );
    }
}

// Walk the document once instead of naming fifteen controls by hand.
static void PopulateConVarControls( Rml::Element *elem )
{
    if ( !elem )
        return;

    PopulateControlFromConVar( elem );

    for ( int i = 0; i < elem->GetNumChildren(); i++ )
        PopulateConVarControls( elem->GetChild( i ) );
}

// Forward declaration - defined below PopulateControls
static void ApplyVideoMode( Rml::ElementDocument *doc );

// Every control in panel_options.rml names what it does (data-event-click /
// data-event-change), so the two Rml::EventListener subclasses that used to sit
// here -- one walking a chain of `id == "..."` comparisons for the tabs and
// buttons, the other another chain for the settings -- are gone. What is left is
// the part that was never dispatch: the settings that are not a single ConVar.

// A control whose value *is* one ConVar says so with data-convar, and this is
// both halves of that: read on populate, written on change. Adding such an option
// is an attribute in the .rml and nothing here.
static void ApplyConVarFromControl( Rml::Element *target, const Rml::String &value )
{
    const Rml::String convarName = target->GetAttribute<Rml::String>( "data-convar", "" );
    if ( convarName.empty() )
        return;

    ConVarRef cvar( convarName.c_str() );
    if ( !cvar.IsValid() )
        return;

    const Rml::String tag = target->GetTagName();
    const Rml::String type = target->GetAttribute<Rml::String>( "type", "" );

    if ( tag == "select" )
    {
        cvar.SetValue( atoi( value.c_str() ) );
    }
    else if ( type == "checkbox" )
    {
        cvar.SetValue( target->HasAttribute( "checked" ) ? 1 : 0 );
    }
    else if ( type == "range" )
    {
        const float fVal = (float)atof( value.c_str() );
        cvar.SetValue( fVal );

        // Sliders show their number in a sibling <id>_val.
        char labelId[64];
        V_snprintf( labelId, sizeof( labelId ), "%s_val", target->GetId().c_str() );
        if ( Rml::ElementDocument *doc = target->GetOwnerDocument() )
        {
            if ( Rml::Element *label = doc->GetElementById( labelId ) )
            {
                char valBuf[32];
                V_snprintf( valBuf, sizeof( valBuf ), "%.2f", fVal );
                label->SetInnerRML( Rml::String( valBuf ) );
            }
        }
    }
}

static void BindOptions( Rml::DataModelConstructor &c )
{
    using Args = const Rml::VariantList &;

    c.BindEventCallback( "options_tab", []( Rml::DataModelHandle, Rml::Event &ev, Args args ) {
        if ( !args.empty() )
            RocketOptionsDocument::SwitchTab( args[0].Get<Rml::String>().c_str() );
    } );
    c.BindEventCallback( "options_close", []( Rml::DataModelHandle, Rml::Event &, Args ) {
        RocketOptionsDocument::ShowPanel( false );
        RocketOptionsDocument::UnloadDialog();
    } );
    c.BindEventCallback( "options_save", []( Rml::DataModelHandle, Rml::Event &, Args ) {
        Msg( "Saving settings.\n" );
        engine->ClientCmd_Unrestricted( "mat_savechanges" );
        engine->ClientCmd_Unrestricted( "host_writeconfig" );
    } );

    // Resolution and display mode go through mat_setvideomode together; changing
    // the mode also re-filters the resolution list (windowed hides modes larger
    // than the desktop).
    c.BindEventCallback( "apply_video_mode", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        if ( !target )
            return;
        ApplyVideoMode( target->GetOwnerDocument() );
        if ( target->GetId() == "display_mode" )
            RocketOptionsDocument::PopulateResolution();
    } );

    // Settings that are not one ConVar.
    c.BindEventCallback( "apply_snd_quality", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        const int quality = atoi( ev.GetParameter<Rml::String>( "value", "" ).c_str() );
        ConVarRef sndPitch( "Snd_PitchQuality" );
        ConVarRef dspSlow( "dsp_slow_cpu" );
        ConVarRef dspStereo( "dsp_enhance_stereo" );
        dspSlow.SetValue( quality == 0 );
        sndPitch.SetValue( quality >= 2 );
        if ( dspStereo.IsValid() )
            dspStereo.SetValue( 0 );
    } );

    c.BindEventCallback( "apply_closedcaptions", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        const int ccMode = atoi( ev.GetParameter<Rml::String>( "value", "" ).c_str() );
        ConVarRef ccSub( "cc_subtitles" );
        ccSub.SetValue( ccMode == 2 ? 1 : 0 );

        char cmd[64];
        V_snprintf( cmd, sizeof( cmd ), "closecaption %d", ccMode ? 1 : 0 );
        engine->ClientCmd_Unrestricted( cmd );
    } );

    // Reverse mouse is the *sign* of m_pitch, acceleration is m_customaccel 0/3.
    c.BindEventCallback( "apply_m_pitch", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        ConVarRef pitch( "m_pitch" );
        if ( !target || !pitch.IsValid() )
            return;
        float absVal = fabsf( pitch.GetFloat() );
        if ( absVal < 0.0001f )
            absVal = 0.022f;
        pitch.SetValue( target->HasAttribute( "checked" ) ? -absVal : absVal );
    } );

    c.BindEventCallback( "apply_m_customaccel", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        Rml::Element *target = ev.GetTargetElement();
        ConVarRef accel( "m_customaccel" );
        if ( target && accel.IsValid() )
            accel.SetValue( target->HasAttribute( "checked" ) ? 3 : 0 );
    } );

    // Everything else: one control, one ConVar, named by data-convar.
    c.BindEventCallback( "apply_convar", []( Rml::DataModelHandle, Rml::Event &ev, Args ) {
        if ( RocketOptionsDocument::m_bPopulating )
            return;
        if ( Rml::Element *target = ev.GetTargetElement() )
            ApplyConVarFromControl( target, ev.GetParameter<Rml::String>( "value", "" ) );
    } );
}
RK_HUD_SECTION( BindOptions );

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

    // Every control that names a ConVar with data-convar, in one pass.
    PopulateConVarControls( m_pInstance );

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

void RocketOptionsDocument::LoadDialog()
{
    if( m_pInstance )
        return;

    // The controls fire `change` as they initialise, and those events reach us
    // through the document itself (data-event-change) rather than a listener
    // attached afterwards -- so the guard has to be up before the load, or the
    // RML's default values would be written into the ConVars. PopulateControls
    // drops it again.
    m_bPopulating = true;

    if( !RkPanelLoad( m_pInstance, ROCKET_CONTEXT_CURRENT, "panel_options.rml", LoadDialog, UnloadDialog ) )
    {
        m_bPopulating = false;
        return;
    }

    PopulateControls();
}

void RocketOptionsDocument::UnloadDialog()
{
    RkPanelUnload( m_pInstance, m_bVisible );
}

void RocketOptionsDocument::ShowPanel( bool bShow, bool immediate )
{
    if( bShow )
    {
        LoadDialog();
        PopulateControls();   // ConVars can have moved since it was last open
    }

    RkPanelShow( m_pInstance, m_bVisible, bShow );
}

