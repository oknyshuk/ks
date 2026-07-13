// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rkhud_scope.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"

#include <RmlUi/Core.h>

DECLARE_HUDELEMENT_DEPTH( RkHudScope, 70 );

extern ConVar cl_crosshair_sniper_width;
ConVar cl_crosshair_sniper_show_normal_inaccuracy( "cl_crosshair_sniper_show_normal_inaccuracy", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "Include standing inaccuracy when determining sniper crosshair blur" );

static void UnloadRkScope()
{
    RkHudScope *pScope = GET_HUDELEMENT( RkHudScope );
    if( !pScope )
        return;

    if( !pScope->m_pDocument )
        return;

    pScope->m_pDocument->Close();
    pScope->m_pDocument = nullptr;
}

static void LoadRkScope()
{
    RkHudScope *pScope = GET_HUDELEMENT( RkHudScope );
    if( !pScope )
        return;

    Rml::Context *hudCtx = RocketUI()->AccessHudContext();
    if( !hudCtx )
    {
        Error( "Couldn't access hudctx!\n" );
    }

    if( pScope->m_pDocument )
        return;

    pScope->m_pDocument = RocketUI()->LoadDocumentFile( ROCKET_CONTEXT_HUD, "hud_scope.rml", &LoadRkScope, &UnloadRkScope );

    if( !pScope->m_pDocument )
    {
        Error( "Couldn't create hud_scope document!\n" );
    }

    pScope->m_fillTop = pScope->m_pDocument->GetElementById( "fill-top" );
    pScope->m_fillBottom = pScope->m_pDocument->GetElementById( "fill-bottom" );
    pScope->m_fillLeft = pScope->m_pDocument->GetElementById( "fill-left" );
    pScope->m_fillRight = pScope->m_pDocument->GetElementById( "fill-right" );
    pScope->m_scope = pScope->m_pDocument->GetElementById( "scope" );

    static const char *kReticleIds[4] = { "v-core", "h-core", "v-blur", "h-blur" };
    for( int i = 0; i < 4; i++ )
        pScope->m_reticle[i] = pScope->m_pDocument->GetElementById( kReticleIds[i] );

    pScope->ShowPanel( false, false );
}

RkHudScope::RkHudScope( const char *pElementName ) : CHudElement( pElementName ),
    m_bVisible( false ),
    m_pDocument( nullptr ),
    m_fillTop( nullptr ),
    m_fillBottom( nullptr ),
    m_fillLeft( nullptr ),
    m_fillRight( nullptr ),
    m_scope( nullptr ),
    m_reticle{},
    m_fAnimInset( 1.0f ),
    m_fLineSpreadDistance( 1.0f )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD );
    SetIgnoreGlobalHudDisable( true );
}

RkHudScope::~RkHudScope() noexcept
{
    UnloadRkScope();
}

void RkHudScope::LevelInit()
{
    LoadRkScope();
}

void RkHudScope::LevelShutdown()
{
    UnloadRkScope();
}

static void SetReticleRect( Rml::Element *e, int x, int y, int w, int h, const char *op )
{
    if( !e )
        return;
    char b[32];
    V_snprintf( b, sizeof(b), "%dpx", x ); e->SetProperty( "left", b );
    V_snprintf( b, sizeof(b), "%dpx", y ); e->SetProperty( "top", b );
    V_snprintf( b, sizeof(b), "%dpx", w ); e->SetProperty( "width", b );
    V_snprintf( b, sizeof(b), "%dpx", h ); e->SetProperty( "height", b );
    e->SetProperty( "opacity", op );
}

void RkHudScope::UpdateScope()
{
    if( !m_pDocument )
        return;

    C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return;

    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return;
    }

    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( !pWeapon || pWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE )
        return;

    Rml::Context *ctx = RocketUI()->AccessHudContext();
    if( !ctx )
        return;

    Rml::Vector2i dim = ctx->GetDimensions();
    int screenWide = dim.x;
    int screenTall = dim.y;

    const float kScopeMinFOV = 25.0f;
    float flTargetFOVForZoom = MAX( pWeapon->GetZoomFOV( pWeapon->GetCSZoomLevel() ), kScopeMinFOV );

    // Reset animation when not scoped
    if( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() && !pPlayer->m_bIsScoped )
    {
        m_fAnimInset = 2;
        m_fLineSpreadDistance = 20;
    }

    if( flTargetFOVForZoom == pPlayer->GetDefaultFOV() || !pPlayer->m_bIsScoped )
        return;

    CBaseViewModel *baseViewModel = pPlayer->GetViewModel( 0 );
    if( !baseViewModel )
        return;
    CPredictedViewModel *viewModel = dynamic_cast<CPredictedViewModel *>( baseViewModel );
    if( !viewModel )
        return;

    float fHalfFov = DEG2RAD( flTargetFOVForZoom ) * 0.5f;
    float fInaccuracyIn640x480Pixels = 320.0f / tanf( fHalfFov );

    float fWeaponInaccuracy = pWeapon->GetInaccuracy() + pWeapon->GetSpread();
    if( !cl_crosshair_sniper_show_normal_inaccuracy.GetBool() )
        fWeaponInaccuracy -= pWeapon->GetInaccuracyStand( Secondary_Mode ) + pWeapon->GetSpread();
    fWeaponInaccuracy = MAX( fWeaponInaccuracy, 0 );

    float fRawSpreadDistance = fWeaponInaccuracy * fInaccuracyIn640x480Pixels;
    float fSpreadDistance = clamp( fRawSpreadDistance, 0.0f, 100.0f );

    // Animate blur
    float flInsetGoal = fSpreadDistance * ( 0.4f / 30.0f );
    m_fAnimInset = Approach( flInsetGoal, m_fAnimInset, fabsf( ( flInsetGoal - m_fAnimInset ) * gpGlobals->frametime ) * 19.0f );
    m_fLineSpreadDistance = RemapValClamped( gpGlobals->frametime * 140.0f, 0.0f, 1.0f, m_fLineSpreadDistance, fRawSpreadDistance );

    float flAccuracyFishtail = pWeapon->GetAccuracyFishtail();
    int offsetX = (int)( viewModel->GetBobState().m_flRawLateralBob * ( screenTall / 14.0f ) + flAccuracyFishtail );
    int offsetY = (int)( viewModel->GetBobState().m_flRawVerticalBob * ( screenTall / 14.0f ) );

    float flInacDisplayBlur = m_fAnimInset * 0.04f;
    if( flInacDisplayBlur > 0.22f )
        flInacDisplayBlur = 0.22f;

    // Calculate scope bounds (same as original CHudScope)
    int inset = (int)( ( screenTall / 14.0f ) + ( flInacDisplayBlur * ( screenTall * 0.5f ) ) );

    // y1,y2 = top and bottom of scope viewport
    // x1,x2 = left and right of scope viewport (centered, square based on height)
    int y1 = inset + offsetY;
    int x1 = ( screenWide - screenTall ) / 2 + inset + offsetX;
    int y2 = screenTall - inset + offsetY;
    int x2 = screenWide - ( ( screenWide - screenTall ) / 2 + inset ) + offsetX;

    int centerX = ( screenWide / 2 ) + offsetX;
    int centerY = ( screenTall / 2 ) + offsetY;

    char buf[32];

    // Size the circular corner-mask square to the scope viewport rect.
    if( m_scope )
    {
        V_snprintf( buf, sizeof(buf), "%dpx", x1 );        m_scope->SetProperty( "left", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y1 );        m_scope->SetProperty( "top", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", x2 - x1 );   m_scope->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y2 - y1 );   m_scope->SetProperty( "height", buf );
    }

    // Position fill rectangles (black areas around scope viewport)
    // Top: full width, from 0 to y1
    if( m_fillTop )
    {
        m_fillTop->SetProperty( "left", "0px" );
        m_fillTop->SetProperty( "top", "0px" );
        V_snprintf( buf, sizeof(buf), "%dpx", screenWide );
        m_fillTop->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", MAX( y1, 0 ) );
        m_fillTop->SetProperty( "height", buf );
    }

    // Bottom: full width, from y2 to screen bottom
    if( m_fillBottom )
    {
        m_fillBottom->SetProperty( "left", "0px" );
        V_snprintf( buf, sizeof(buf), "%dpx", y2 );
        m_fillBottom->SetProperty( "top", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", screenWide );
        m_fillBottom->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", MAX( screenTall - y2, 0 ) );
        m_fillBottom->SetProperty( "height", buf );
    }

    // Left: from y1 to y2, left edge to x1
    if( m_fillLeft )
    {
        m_fillLeft->SetProperty( "left", "0px" );
        V_snprintf( buf, sizeof(buf), "%dpx", y1 );
        m_fillLeft->SetProperty( "top", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", MAX( x1, 0 ) );
        m_fillLeft->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y2 - y1 );
        m_fillLeft->SetProperty( "height", buf );
    }

    // Right: from y1 to y2, x2 to right edge
    if( m_fillRight )
    {
        V_snprintf( buf, sizeof(buf), "%dpx", x2 );
        m_fillRight->SetProperty( "left", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y1 );
        m_fillRight->SetProperty( "top", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", MAX( screenWide - x2, 0 ) );
        m_fillRight->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y2 - y1 );
        m_fillRight->SetProperty( "height", buf );
    }

    // Reticle: a crisp solid core plus soft gradient wings that grow out of the
    // core with movement/inaccuracy. Axis-native (no rotation); at rest the
    // wings collapse to zero size, leaving only the crisp core. Spread magnitude
    // mirrors the legacy CHudScope.
    // Reticle: faithful port of the legacy CHudScope — a hard toggle between a
    // thin sharp solid line (steady) and Valve's scope_line_blur sprite (moving),
    // the sprite widening + fading with shot spread. The sprite's soft alpha
    // profile (vs a solid bar) is what keeps the viewport from darkening.
    int thickness = cl_crosshair_sniper_width.GetInt();
    if( thickness < 1 )
        thickness = 1;

    float fBlurWidth = powf( m_fLineSpreadDistance, 0.75f );
    float fScreenBlurWidth = fBlurWidth * screenTall / 640.0f;
    bool steady = ( fScreenBlurWidth <= thickness + 0.5f );

    float a = steady ? ( ( fBlurWidth < 1.0f ) ? 1.0f : 1.0f / fBlurWidth )
                     : ( ( fBlurWidth < 1.8f ) ? 1.0f : 1.8f / fBlurWidth );
    a = clamp( sqrtf( a ), 140.0f / 255.0f, 1.0f );
    char op[16];
    V_snprintf( op, sizeof(op), "%.3f", a );

    if( steady )
    {
        // Sharp thin solid cross; collapse the blur sprites to zero size.
        SetReticleRect( m_reticle[0], centerX - thickness / 2, 0, thickness, screenTall, op );
        SetReticleRect( m_reticle[1], 0, centerY - thickness / 2, screenWide, thickness, op );
        SetReticleRect( m_reticle[2], 0, 0, 0, 0, op );
        SetReticleRect( m_reticle[3], 0, 0, 0, 0, op );
    }
    else
    {
        // Soft blur sprite cross (widens + fades); collapse the sharp cross.
        int bw = (int)( 2.0f * fScreenBlurWidth );
        if( bw < 3 )
            bw = 3; // keep the soft sprite from sampling to nothing near the toggle
        SetReticleRect( m_reticle[2], centerX - bw / 2, 0, bw, screenTall, op );
        SetReticleRect( m_reticle[3], 0, centerY - bw / 2, screenWide, bw, op );
        SetReticleRect( m_reticle[0], 0, 0, 0, 0, op );
        SetReticleRect( m_reticle[1], 0, 0, 0, 0, op );
    }
}

void RkHudScope::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
            m_pDocument->Show();

        UpdateScope();
    }
    else
    {
        if( m_bVisible )
            m_pDocument->Hide();
    }

    m_bVisible = bShow;
}

void RkHudScope::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudScope::ShouldDraw()
{
    C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return false;

    if( pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
        return false;

    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return false;
    }
    else if( pPlayer->GetObserverMode() != OBS_MODE_NONE )
    {
        return false;
    }

    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( !pWeapon || pWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE )
        return false;

    if( !pPlayer->m_bIsScoped )
        return false;

    return CHudElement::ShouldDraw();
}
