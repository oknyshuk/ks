// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rkhud_scope.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT_DEPTH( RkHudScope, 70 );

const char *RkHudScope::kDocument = "hud_scope.rml";

extern ConVar cl_crosshair_sniper_width;
ConVar cl_crosshair_sniper_show_normal_inaccuracy( "cl_crosshair_sniper_show_normal_inaccuracy", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "Include standing inaccuracy when determining sniper crosshair blur" );

// What hud_scope.rml binds to. Whole pixels, already clamped: the viewport rect
// and the shot-spread blur are weapon physics, worked out below, while what a
// rect *looks like* stays in the document.
struct ScopeData
{
    int screen_w, screen_h;
    int scope_x, scope_y, scope_w, scope_h, scope_x2;
    int top_h, bottom_y, bottom_h, left_w, right_w;
    int core_x, core_y, thickness;   // sharp cross (steady)
    int blur_x, blur_y, blur;        // soft sprite cross (moving)
    Rml::String opacity;
    bool steady;

    bool operator==( const ScopeData & ) const = default;
} scopeData;

static void BindScope( Rml::DataModelConstructor &c )
{
    if ( auto h = c.RegisterStruct<ScopeData>() )
    {
        h.RegisterMember( "screen_w", &ScopeData::screen_w );
        h.RegisterMember( "screen_h", &ScopeData::screen_h );
        h.RegisterMember( "scope_x", &ScopeData::scope_x );
        h.RegisterMember( "scope_y", &ScopeData::scope_y );
        h.RegisterMember( "scope_w", &ScopeData::scope_w );
        h.RegisterMember( "scope_h", &ScopeData::scope_h );
        h.RegisterMember( "scope_x2", &ScopeData::scope_x2 );
        h.RegisterMember( "top_h", &ScopeData::top_h );
        h.RegisterMember( "bottom_y", &ScopeData::bottom_y );
        h.RegisterMember( "bottom_h", &ScopeData::bottom_h );
        h.RegisterMember( "left_w", &ScopeData::left_w );
        h.RegisterMember( "right_w", &ScopeData::right_w );
        h.RegisterMember( "core_x", &ScopeData::core_x );
        h.RegisterMember( "core_y", &ScopeData::core_y );
        h.RegisterMember( "thickness", &ScopeData::thickness );
        h.RegisterMember( "blur_x", &ScopeData::blur_x );
        h.RegisterMember( "blur_y", &ScopeData::blur_y );
        h.RegisterMember( "blur", &ScopeData::blur );
        h.RegisterMember( "opacity", &ScopeData::opacity );
        h.RegisterMember( "steady", &ScopeData::steady );
    }
    c.Bind( "scope", &scopeData );
}
RK_HUD_SECTION( BindScope );

RkHudScope::RkHudScope( const char *pElementName ) : RkHudDocument( pElementName ),
    m_fAnimInset( 1.0f ),
    m_fLineSpreadDistance( 1.0f )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD );
    SetIgnoreGlobalHudDisable( true );
}

RkHudScope::~RkHudScope() noexcept
{
    Unload();
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

    // --- publish -----------------------------------------------------------
    // The clamps stay here: MAX(y1,0) and friends are logic about a viewport
    // that can be pushed off-screen by the bob, not styling.
    ScopeData next;
    next.screen_w = screenWide;
    next.screen_h = screenTall;
    next.scope_x = x1;
    next.scope_y = y1;
    next.scope_w = x2 - x1;
    next.scope_h = y2 - y1;
    next.scope_x2 = x2;
    next.top_h = MAX( y1, 0 );
    next.bottom_y = y2;
    next.bottom_h = MAX( screenTall - y2, 0 );
    next.left_w = MAX( x1, 0 );
    next.right_w = MAX( screenWide - x2, 0 );

    // Reticle: faithful port of the legacy CHudScope -- a hard toggle between a
    // thin sharp solid line (steady) and Valve's scope_line_blur sprite (moving),
    // the sprite widening + fading with shot spread. The sprite's soft alpha
    // profile (vs a solid bar) is what keeps the viewport from darkening.
    int thickness = cl_crosshair_sniper_width.GetInt();
    if( thickness < 1 )
        thickness = 1;

    float fBlurWidth = powf( m_fLineSpreadDistance, 0.75f );
    float fScreenBlurWidth = fBlurWidth * screenTall / 640.0f;
    next.steady = ( fScreenBlurWidth <= thickness + 0.5f );

    float a = next.steady ? ( ( fBlurWidth < 1.0f ) ? 1.0f : 1.0f / fBlurWidth )
                          : ( ( fBlurWidth < 1.8f ) ? 1.0f : 1.8f / fBlurWidth );
    a = clamp( sqrtf( a ), 140.0f / 255.0f, 1.0f );
    char op[16];
    V_snprintf( op, sizeof(op), "%.3f", a );
    next.opacity = op;

    int bw = (int)( 2.0f * fScreenBlurWidth );
    if( bw < 3 )
        bw = 3; // keep the soft sprite from sampling to nothing near the toggle

    next.thickness = thickness;
    next.core_x = centerX - thickness / 2;
    next.core_y = centerY - thickness / 2;
    next.blur = bw;
    next.blur_x = centerX - bw / 2;
    next.blur_y = centerY - bw / 2;

    if( next == scopeData )
        return;

    scopeData = next;
    RkHudDirty( "scope" );
}

void RkHudScope::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );

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
