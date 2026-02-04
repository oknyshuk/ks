#include "rkhud_crosshair.h"

#include "cbase.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"

// min/max conflict handled by RMLUI_USE_CUSTOM_ASSERT
#include <RmlUi/Core.h>

DECLARE_HUDELEMENT( RkHudCrosshair );

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

static void UnloadRkCrosshair()
{
    RkHudCrosshair *pCrosshair = GET_HUDELEMENT( RkHudCrosshair );
    if( !pCrosshair )
    {
        Warning( "Couldn't grab RkHudCrosshair element to unload!\n" );
        return;
    }

    // Not loaded
    if( !pCrosshair->m_pDocument )
        return;

    pCrosshair->m_pDocument->Close();
    pCrosshair->m_pDocument = nullptr;
}

static void LoadRkCrosshair()
{
    RkHudCrosshair *pCrosshair = GET_HUDELEMENT( RkHudCrosshair );
    if( !pCrosshair )
    {
        Warning( "Couldn't grab RkHudCrosshair element to load!\n" );
        return;
    }

    Rml::Context *hudCtx = RocketUI()->AccessHudContext();
    if( !hudCtx )
    {
        Error( "Couldn't access hudctx!\n" );
        /* Exit */
    }

    if( pCrosshair->m_pDocument )
    {
        Warning( "RkCrosshair already loaded, call unload first!\n" );
        return;
    }

    pCrosshair->m_pDocument = RocketUI()->LoadDocumentFile( ROCKET_CONTEXT_HUD, "hud_crosshair.rml", &LoadRkCrosshair, &UnloadRkCrosshair );

    if( !pCrosshair->m_pDocument )
    {
        Error( "Couldn't create hud_crosshair document!\n" );
        /* Exit */
    }

    // Cache element pointers
    pCrosshair->m_lineTop = pCrosshair->m_pDocument->GetElementById( "line-top" );
    pCrosshair->m_lineBottom = pCrosshair->m_pDocument->GetElementById( "line-bottom" );
    pCrosshair->m_lineLeft = pCrosshair->m_pDocument->GetElementById( "line-left" );
    pCrosshair->m_lineRight = pCrosshair->m_pDocument->GetElementById( "line-right" );
    pCrosshair->m_dot = pCrosshair->m_pDocument->GetElementById( "dot" );

    pCrosshair->m_olTop = pCrosshair->m_pDocument->GetElementById( "ol-top" );
    pCrosshair->m_olBottom = pCrosshair->m_pDocument->GetElementById( "ol-bottom" );
    pCrosshair->m_olLeft = pCrosshair->m_pDocument->GetElementById( "ol-left" );
    pCrosshair->m_olRight = pCrosshair->m_pDocument->GetElementById( "ol-right" );
    pCrosshair->m_olDot = pCrosshair->m_pDocument->GetElementById( "ol-dot" );

    pCrosshair->ShowPanel( false, false );
}

RkHudCrosshair::RkHudCrosshair( const char *pElementName ) : CHudElement( pElementName ),
    m_bVisible( false ),
    m_pDocument( nullptr ),
    m_lineTop( nullptr ),
    m_lineBottom( nullptr ),
    m_lineLeft( nullptr ),
    m_lineRight( nullptr ),
    m_dot( nullptr ),
    m_olTop( nullptr ),
    m_olBottom( nullptr ),
    m_olLeft( nullptr ),
    m_olRight( nullptr ),
    m_olDot( nullptr )
{
    SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

RkHudCrosshair::~RkHudCrosshair() noexcept
{
    UnloadRkCrosshair();
}

void RkHudCrosshair::LevelInit()
{
    LoadRkCrosshair();
}

void RkHudCrosshair::LevelShutdown()
{
    UnloadRkCrosshair();
}

void RkHudCrosshair::UpdateCrosshair()
{
    if( !m_pDocument || !m_lineTop || !m_lineBottom || !m_lineLeft || !m_lineRight )
        return;

    // Get screen dimensions and calculate center (same approach as original VGUI)
    Rml::Context *ctx = RocketUI()->AccessHudContext();
    if( !ctx )
        return;

    Rml::Vector2i dimensions = ctx->GetDimensions();
    int iCenterX = dimensions.x / 2;
    int iCenterY = dimensions.y / 2;

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

    // Build color strings
    char colorStr[64];
    V_snprintf( colorStr, sizeof(colorStr), "rgba(%d,%d,%d,%d)", r, g, b, alpha );

    char outlineColorStr[64];
    V_snprintf( outlineColorStr, sizeof(outlineColorStr), "rgba(0,0,0,%d)", alpha );

    // Helper to set element as a rectangle from (x0,y0) to (x1,y1) - like DrawCrosshairRect
    auto SetRect = [&]( Rml::Element *el, int x0, int y0, int x1, int y1, const char *color )
    {
        if( !el ) return;
        char buf[32];
        V_snprintf( buf, sizeof(buf), "%dpx", x0 );
        el->SetProperty( "left", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y0 );
        el->SetProperty( "top", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", x1 - x0 );
        el->SetProperty( "width", buf );
        V_snprintf( buf, sizeof(buf), "%dpx", y1 - y0 );
        el->SetProperty( "height", buf );
        el->SetProperty( "background-color", color );
    };

    // Draw crosshair lines using absolute screen coordinates (same math as original)
    // Top line (vertical, above center)
    int topX0 = iCenterX - iBarThickness / 2;
    int topX1 = topX0 + iBarThickness;
    int topY0 = iCenterY - iGap - iBarSize;
    int topY1 = iCenterY - iGap;
    if( drawOutline )
    {
        SetRect( m_olTop, topX0 - iOutline, topY0 - iOutline, topX1 + iOutline, topY1 + iOutline, outlineColorStr );
        m_olTop->SetProperty( "display", "block" );
    }
    else if( m_olTop )
    {
        m_olTop->SetProperty( "display", "none" );
    }
    SetRect( m_lineTop, topX0, topY0, topX1, topY1, colorStr );

    // Bottom line (vertical, below center)
    int bottomX0 = iCenterX - iBarThickness / 2;
    int bottomX1 = bottomX0 + iBarThickness;
    int bottomY0 = iCenterY + iGap;
    int bottomY1 = iCenterY + iGap + iBarSize;
    if( drawOutline )
    {
        SetRect( m_olBottom, bottomX0 - iOutline, bottomY0 - iOutline, bottomX1 + iOutline, bottomY1 + iOutline, outlineColorStr );
        m_olBottom->SetProperty( "display", "block" );
    }
    else if( m_olBottom )
    {
        m_olBottom->SetProperty( "display", "none" );
    }
    SetRect( m_lineBottom, bottomX0, bottomY0, bottomX1, bottomY1, colorStr );

    // Left line (horizontal, left of center)
    int leftX0 = iCenterX - iGap - iBarSize;
    int leftX1 = iCenterX - iGap;
    int leftY0 = iCenterY - iBarThickness / 2;
    int leftY1 = leftY0 + iBarThickness;
    if( drawOutline )
    {
        SetRect( m_olLeft, leftX0 - iOutline, leftY0 - iOutline, leftX1 + iOutline, leftY1 + iOutline, outlineColorStr );
        m_olLeft->SetProperty( "display", "block" );
    }
    else if( m_olLeft )
    {
        m_olLeft->SetProperty( "display", "none" );
    }
    SetRect( m_lineLeft, leftX0, leftY0, leftX1, leftY1, colorStr );

    // Right line (horizontal, right of center)
    int rightX0 = iCenterX + iGap;
    int rightX1 = iCenterX + iGap + iBarSize;
    int rightY0 = iCenterY - iBarThickness / 2;
    int rightY1 = rightY0 + iBarThickness;
    if( drawOutline )
    {
        SetRect( m_olRight, rightX0 - iOutline, rightY0 - iOutline, rightX1 + iOutline, rightY1 + iOutline, outlineColorStr );
        m_olRight->SetProperty( "display", "block" );
    }
    else if( m_olRight )
    {
        m_olRight->SetProperty( "display", "none" );
    }
    SetRect( m_lineRight, rightX0, rightY0, rightX1, rightY1, colorStr );

    // Center dot
    int dotX0 = iCenterX - iBarThickness / 2;
    int dotX1 = dotX0 + iBarThickness;
    int dotY0 = iCenterY - iBarThickness / 2;
    int dotY1 = dotY0 + iBarThickness;
    if( m_dot )
    {
        SetRect( m_dot, dotX0, dotY0, dotX1, dotY1, colorStr );
        m_dot->SetProperty( "display", drawDot ? "block" : "none" );
    }
    if( m_olDot )
    {
        if( drawDot && drawOutline )
        {
            SetRect( m_olDot, dotX0 - iOutline, dotY0 - iOutline, dotX1 + iOutline, dotY1 + iOutline, outlineColorStr );
            m_olDot->SetProperty( "display", "block" );
        }
        else
        {
            m_olDot->SetProperty( "display", "none" );
        }
    }
}

void RkHudCrosshair::ShowPanel( bool bShow, bool force )
{
    if( !m_pDocument )
        return;

    if( bShow )
    {
        if( !m_bVisible )
        {
            m_pDocument->Show();
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

#ifdef IRONSIGHT
        // AUG/SG553 hide crosshair when using ironsight
        if( pWeapon->GetIronSightController() && pWeapon->GetIronSightController()->ShouldHideCrossHair() )
            return false;
#endif
    }

    return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}
