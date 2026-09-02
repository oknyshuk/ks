// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

#include "rkhud_crosshair.h"

#include "rkhud_model.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"

#include <rocketui/rmlui.h>

DECLARE_HUDELEMENT( RkHudCrosshair );

const char *RkHudCrosshair::kDocument = "hud_crosshair.rml";

// External convars defined in weapon_csbase.cpp and counterstrikeviewport.cpp
extern ConVar crosshair;
extern ConVar cl_crosshairsize;
extern ConVar cl_crosshairgap;
extern ConVar cl_crosshairthickness;
extern ConVar cl_crosshaircolor_r;
extern ConVar cl_crosshaircolor_g;
extern ConVar cl_crosshaircolor_b;
extern ConVar cl_crosshairalpha;
extern ConVar cl_crosshairdot;
extern ConVar cl_crosshair_drawoutline;
extern ConVar cl_crosshair_outlinethickness;

// Recoil tracking (baseplayer_shared.cpp)
extern ConVar view_recoil_tracking;

ConVar cl_crosshair_recoil( "cl_crosshair_recoil", "1", FCVAR_ARCHIVE, "Crosshair follows recoil" );

// What hud_crosshair.rml binds to. Whole pixels, because the document derives
// every rect from these with nothing but + - and *2 -- the same integers the
// legacy CHudCrosshair::DrawCrosshairRect calls used, so the lines still land on
// exact pixel boundaries. Layout lives in the .rml now; this is just the maths.
struct CrosshairData
{
    int x0, y0;             // top-left of the vertical/horizontal bars
    int cx, cy;             // aim centre, recoil included
    int gap, bar, thick, ol;
    Rml::String color, outline_color;
    bool outlined, dotted;

    bool operator==( const CrosshairData & ) const = default;
} crosshairData;

static void BindCrosshair( Rml::DataModelConstructor &c )
{
    if ( auto handle = c.RegisterStruct<CrosshairData>() )
    {
        handle.RegisterMember( "x0", &CrosshairData::x0 );
        handle.RegisterMember( "y0", &CrosshairData::y0 );
        handle.RegisterMember( "cx", &CrosshairData::cx );
        handle.RegisterMember( "cy", &CrosshairData::cy );
        handle.RegisterMember( "gap", &CrosshairData::gap );
        handle.RegisterMember( "bar", &CrosshairData::bar );
        handle.RegisterMember( "thick", &CrosshairData::thick );
        handle.RegisterMember( "ol", &CrosshairData::ol );
        handle.RegisterMember( "color", &CrosshairData::color );
        handle.RegisterMember( "outline_color", &CrosshairData::outline_color );
        handle.RegisterMember( "outlined", &CrosshairData::outlined );
        handle.RegisterMember( "dotted", &CrosshairData::dotted );
    }
    c.Bind( "crosshair", &crosshairData );
}
RK_HUD_SECTION( BindCrosshair );

RkHudCrosshair::RkHudCrosshair( const char *pElementName ) : RkHudDocument( pElementName )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

RkHudCrosshair::~RkHudCrosshair() noexcept
{
    Unload();
}

void RkHudCrosshair::UpdateCrosshair()
{
    if( !m_pDocument )
        return;

    // Get screen dimensions and calculate center (same approach as original VGUI)
    Rml::Context *ctx = RocketUI()->AccessHudContext();
    if( !ctx )
        return;

    Rml::Vector2i dimensions = ctx->GetDimensions();
    int iCenterX = dimensions.x / 2;
    int iCenterY = dimensions.y / 2;

    // Offset crosshair center so it points where bullets actually go.
    //
    // The camera view applies two offsets (CalcViewAngles):
    //   eyeAngles += viewPunch                          (screen shake, cosmetic)
    //   eyeAngles += aimPunch * view_recoil_tracking    (partial recoil, default 0.45)
    //
    // Bullets land at: baseAngles + aimPunch  (full aim punch, no view punch).
    //
    // Screen center = camera direction, so without correction the crosshair
    // inherits both viewPunch and the partial aimPunch.  To make the crosshair
    // show the true bullet destination we must offset by:
    //   aimPunch * (1 - view_recoil_tracking) - viewPunch
    //
    // The viewPunch subtraction cancels the per-shot screen shake that rocks
    // the camera but doesn't affect bullet trajectory, eliminating the
    // visible jitter during spray.
    if( cl_crosshair_recoil.GetBool() )
    {
        C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
        if( pPlayer )
        {
            if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
                pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );

            if( pPlayer )
            {
                QAngle aimPunch  = pPlayer->GetAimPunchAngle();
                QAngle viewPunch = pPlayer->GetViewPunchAngle();
                float flUntracked = 1.0f - view_recoil_tracking.GetFloat();

                // Net offset from screen center to bullet destination
                float flOffsetPitch = aimPunch[PITCH] * flUntracked - viewPunch[PITCH];
                float flOffsetYaw   = aimPunch[YAW]   * flUntracked - viewPunch[YAW];

                // Project angle offset to screen pixels.
                // GetFOV() is the base horizontal FOV for 4:3; the engine widens it
                // for other aspects but vertical FOV stays constant, so we derive
                // a single focal-length scale from screenHeight and the vertical FOV:
                //   vfov = 2 * atan(tan(baseFov/2) * 3/4)
                //   scale = screenHeight / (2 * tan(vfov/2))
                //         = screenHeight / (1.5 * tan(baseFov/2))
                float fov = pPlayer->GetFOV();
                float flScale = (float)dimensions.y / ( 1.5f * tanf( DEG2RAD( fov ) * 0.5f ) );

                // Pitch: negative = looking up → screen Y decreases
                // Yaw:   positive = looking left → screen X decreases
                iCenterX += (int)( -tanf( DEG2RAD( flOffsetYaw ) )   * flScale );
                iCenterY += (int)(  tanf( DEG2RAD( flOffsetPitch ) ) * flScale );
            }
        }
    }

    // Read convars
    float size = cl_crosshairsize.GetFloat();
    float gap = cl_crosshairgap.GetFloat();
    float thickness = cl_crosshairthickness.GetFloat();
    int r = cl_crosshaircolor_r.GetInt();
    int g = cl_crosshaircolor_g.GetInt();
    int b = cl_crosshaircolor_b.GetInt();
    int alpha = cl_crosshairalpha.GetInt();
    bool drawDot = cl_crosshairdot.GetBool();
    bool drawOutline = cl_crosshair_drawoutline.GetBool();
    float outlineThickness = cl_crosshair_outlinethickness.GetFloat();

    // Clamp values
    if( size < 0 ) size = 0;
    if( thickness < 0.5f ) thickness = 0.5f;
    alpha = clamp( alpha, 0, 255 );
    r = clamp( r, 0, 255 );
    g = clamp( g, 0, 255 );
    b = clamp( b, 0, 255 );
    outlineThickness = clamp( outlineThickness, 0.1f, 3.0f );

    // Scale values to pixels (matching original VGUI scaling)
    // Original uses YRES() macro which scales based on 480 base height
    float scale = dimensions.y / 480.0f;
    int iBarSize = (int)( size * scale );
    int iBarThickness = MAX( 1, (int)( thickness * scale ) );
    int iGap = (int)( gap * scale );
    // Outline thickness is NOT scaled in the original - it's raw pixels
    int iOutline = (int)( outlineThickness + 0.5f );

    // Publish for hud_crosshair.rml, which turns these into the ten rects. Only
    // handed over when something actually moved: with no recoil and no convar
    // edits that is one struct compare per frame instead of forty property
    // parses, and RmlUi re-evaluates the bindings only for what it is told is
    // dirty.
    CrosshairData next;
    next.cx = iCenterX;
    next.cy = iCenterY;
    next.thick = iBarThickness;
    next.bar = iBarSize;
    next.gap = iGap;
    next.ol = iOutline;
    next.x0 = iCenterX - iBarThickness / 2;
    next.y0 = iCenterY - iBarThickness / 2;
    next.outlined = drawOutline;
    next.dotted = drawDot;

    char buf[64];
    V_snprintf( buf, sizeof(buf), "rgba(%d,%d,%d,%d)", r, g, b, alpha );
    next.color = buf;
    V_snprintf( buf, sizeof(buf), "rgba(0,0,0,%d)", alpha );
    next.outline_color = buf;

    if( next == crosshairData )
        return;

    crosshairData = next;
    RkHudDirty( "crosshair" );
}

void RkHudCrosshair::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show( Rml::ModalFlag::None, Rml::FocusFlag::None );
        }

        UpdateCrosshair();
    }
    else
    {
        if( m_bVisible )
        {
            m_pDocument->Hide();
        }
    }

    m_bVisible = bShow;
}

void RkHudCrosshair::SetActive( bool bActive )
{
    ShowPanel( bActive, false );
    CHudElement::SetActive( bActive );
}

bool RkHudCrosshair::ShouldDraw()
{
    if( !crosshair.GetBool() )
        return false;

    C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
    if( !pPlayer )
        return false;

    // Handle spectating - get the target player if in first person spec
    if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
    {
        pPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );
        if( !pPlayer )
            return false;
    }
    else if( !pPlayer->IsAlive() )
    {
        // Dead and not spectating in first person
        return false;
    }

    // Check weapon-specific conditions
    CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
    if( pWeapon )
    {
        // Sniper rifles never show the regular crosshair (they use the scope HUD when scoped)
        if( pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE )
            return false;
    }

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}
