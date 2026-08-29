//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: See header file
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud_locator_target.h"
#include "localize/ilocalize.h"
#include "iinput.h"
#include "hud.h"
#include "iengineui.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar locator_fade_time( "locator_fade_time", "0.3", FCVAR_NONE, "Number of seconds it takes for a lesson to fully fade in/out." );
ConVar locator_lerp_rest( "locator_lerp_rest", "0.25f", FCVAR_NONE, "Number of seconds before moving from the center." );

ConVar locator_target_offset_x( "locator_target_offset_x", "-17", FCVAR_NONE, "How many pixels to offset the locator from the target position." );
ConVar locator_target_offset_y( "locator_target_offset_y", "-64", FCVAR_NONE, "How many pixels to offset the locator from the target position." );


//Precahce the effects

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheLocatorTarget )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_left" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_right" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_up" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_down" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_plain" )
PRECACHE_REGISTER_END()


//------------------------------------
CLocatorTarget::CLocatorTarget( void )
{
	Deactivate( true );
}

//------------------------------------
void CLocatorTarget::Activate( int serialNumber )
{ 
	m_serialNumber = serialNumber; 
	m_frameLastUpdated = gpGlobals->framecount;
	m_isActive = true;

	m_bVisible = true;
	m_bOnscreen = true;
	m_alpha = 0;
	m_fadeStart = gpGlobals->curtime;
	m_bIconNoTarget = false;

	m_offsetX = m_offsetY = 0;

	int iStartX = ScreenWidth() / 2;
	int iStartY = ScreenHeight() / 4;

	m_lastXPos = iStartX;
	m_lastYPos = iStartY;

	m_bDrawArrow = false;
	m_fDrawArrowAngle = 0.0f;
	m_lerpStart = gpGlobals->curtime;
	m_pulseStart = gpGlobals->curtime;
	m_declutterIndex = 0;
	m_lastDeclutterIndex = 0;
	AddIconEffects( LOCATOR_ICON_FX_FADE_IN );
}

//------------------------------------
void CLocatorTarget::Deactivate( bool bNoFade )						
{
	if ( ( engine && engine->IsPaused() ) || ( engineui && engineui->IsGameUIVisible() ) )
	{
		bNoFade = true;
	}

	if ( bNoFade || m_alpha == 0 || 
		 ( m_bOccluded && !( m_iEffectsFlags & LOCATOR_ICON_FX_FORCE_CAPTION ) ) || 
		 ( !m_bOnscreen && ( m_iEffectsFlags & LOCATOR_ICON_FX_NO_OFFSCREEN ) ) )
	{
		m_bOriginInScreenspace = false;

		m_serialNumber = -1;
		m_isActive = false;
		m_frameLastUpdated = 0;
		m_pIcon_onscreen = NULL;
		m_pIcon_offscreen = NULL;
		m_bDrawControllerButton = false;
		m_bDrawControllerButtonOffscreen = false;
		m_iEffectsFlags = LOCATOR_ICON_FX_NONE;
		m_rgbaIconColor = Color( 255, 255, 255, 255 );
		m_captionWide = 0;

		m_pchDrawBindingName = NULL;
		m_pchDrawBindingNameOffscreen = NULL;
		m_widthScale_onscreen = 1.0f;
		m_bOccluded = false;
		m_alpha = 0;
		m_bIsDrawing = false;
		m_bVisible = false;

		m_szUITargetName = "";
		m_szUITargetLookup = "";
		m_nUITargetEdge = 0;

		m_szBinding = "";
		m_iBindingTick = 0;
		m_flNextBindingTick = 0.0f;
		m_flNextOcclusionTest = 0.0f;
		m_iBindingChoicesCount = 0;

		m_wszCaption.RemoveAll();
		m_wszCaption.AddToTail( (wchar_t)0 );

		m_bWasControllerLast = false;
	}
	else if ( !( m_iEffectsFlags & LOCATOR_ICON_FX_FADE_OUT ) )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_IN );
	}
}

//------------------------------------
void CLocatorTarget::Update()							
{
	m_frameLastUpdated = gpGlobals->framecount;

	if ( m_bVisible && ( m_iEffectsFlags & LOCATOR_ICON_FX_FADE_OUT ) )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_OUT );
	}
}

int CLocatorTarget::GetIconX( void )
{
	return m_iconX + ( IsOnScreen() ? locator_target_offset_x.GetInt()+m_offsetX : 0 );
}

int CLocatorTarget::GetIconY( void )
{
	return m_iconY + ( IsOnScreen() ? locator_target_offset_y.GetInt()+m_offsetY : 0 );
}

int CLocatorTarget::GetIconCenterX( void )
{
	return m_centerX + locator_target_offset_x.GetInt() + m_offsetX;
}

int CLocatorTarget::GetIconCenterY( void )
{
	return m_centerY + locator_target_offset_y.GetInt() + m_offsetY;
}

void CLocatorTarget::SetVisible( bool bVisible )
{
	if ( m_bVisible == bVisible )
	{
		// They are already the same
		return;
	}

	m_bVisible = bVisible;

	if ( bVisible )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_IN );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_OUT );
	}
	else
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_IN );
	}
}
bool CLocatorTarget::IsVisible( void )
{
	return m_bVisible;
}

void CLocatorTarget::SetCaptionText( const char *pszText, const char *pszParam )
{
	wchar_t outbuf[ 256 ];
	outbuf[ 0 ] = L'\0';

	if ( pszParam && pszParam[ 0 ] != '\0' )
	{
		wchar_t wszParamBuff[ 128 ];
		wchar_t *pLocalizedParam = NULL;

		if ( pszParam[ 0 ] == '#' )
		{
			pLocalizedParam = g_pLocalize->Find( pszParam );
		}

		if ( !pLocalizedParam )
		{
			g_pLocalize->ConvertANSIToUnicode( pszParam, wszParamBuff, sizeof( wszParamBuff ) );
			pLocalizedParam = wszParamBuff;
		}

		wchar_t wszTextBuff[ 128 ];
		wchar_t *pLocalizedText = NULL;

		if ( pszText[ 0 ] == '#' )
		{
			pLocalizedText = g_pLocalize->Find( pszText );
		}

		if ( !pLocalizedText )
		{
			g_pLocalize->ConvertANSIToUnicode( pszText, wszTextBuff, sizeof( wszTextBuff ) );
			pLocalizedText = wszTextBuff;
		}

		wchar_t buf[ 256 ];
		g_pLocalize->ConstructString( buf, sizeof(buf), pLocalizedText, 1, pLocalizedParam );

		UTIL_ReplaceKeyBindings( buf, sizeof( buf ), outbuf, sizeof( outbuf ) );
	}
	else
	{
		wchar_t wszTextBuff[ 128 ];
		wchar_t *pLocalizedText = NULL;

		if ( pszText[ 0 ] == '#' )
		{
			pLocalizedText = g_pLocalize->Find( pszText );
		}

		if ( !pLocalizedText )
		{
			g_pLocalize->ConvertANSIToUnicode( pszText, wszTextBuff, sizeof( wszTextBuff ) );
			pLocalizedText = wszTextBuff;
		}

		wchar_t buf[ 256 ];
		Q_wcsncpy( buf, pLocalizedText, sizeof( buf ) );

		UTIL_ReplaceKeyBindings( buf, sizeof(buf), outbuf, sizeof( outbuf ) );
	}

	int len = wcslen( outbuf ) + 1;
	m_wszCaption.RemoveAll();
	m_wszCaption.EnsureCount( len );
	Q_wcsncpy( m_wszCaption.Base(), outbuf, len * sizeof( wchar_t ) );
}

void CLocatorTarget::SetCaptionColor( const char *pszCaptionColor )
{
	int r,g,b;
	r = g = b = 0;

	CSplitString colorValues( pszCaptionColor, "," );

	if( colorValues.Count() == 3 )
	{
		r = atoi( colorValues[0] );
		g = atoi( colorValues[1] );
		b = atoi( colorValues[2] );

		m_captionColor.SetColor( r,g,b, 255 );
	}
	else
	{
		DevWarning( "caption_color format incorrect. RRR,GGG,BBB expected.\n");
	}
}

bool CLocatorTarget::IsStatic()
{
	return ( ( m_iEffectsFlags & LOCATOR_ICON_FX_STATIC ) || IsPresenting() );
}

bool CLocatorTarget::IsPresenting()
{
	return ( gpGlobals->curtime - m_lerpStart < locator_lerp_rest.GetFloat() );
}

void CLocatorTarget::StartTimedLerp()
{
	if ( gpGlobals->curtime - m_lerpStart > locator_lerp_rest.GetFloat() )
	{
		m_lerpStart = gpGlobals->curtime - locator_lerp_rest.GetFloat();
	}
}

void CLocatorTarget::StartPresent()
{
	m_lerpStart = gpGlobals->curtime;
}


void CLocatorTarget::EndPresent()
{
	if ( gpGlobals->curtime - m_lerpStart < locator_lerp_rest.GetFloat() )
	{
		m_lerpStart = gpGlobals->curtime - locator_lerp_rest.GetFloat();
	}
}

void CLocatorTarget::UpdateVguiTarget( void )
{
}

void CLocatorTarget::SetVguiTargetName( const char *pchUITargetName )
{
	if ( Q_strcmp( m_szUITargetName.String(), pchUITargetName ) == 0 )
		return;

	m_szUITargetName = pchUITargetName;

	UpdateVguiTarget();
}

void CLocatorTarget::SetVguiTargetLookup( const char *pchUITargetLookup )
{
	m_szUITargetLookup = pchUITargetLookup;
}

void CLocatorTarget::SetVguiTargetEdge( int nVguiEdge )
{
	m_nUITargetEdge = nVguiEdge;
}

void *CLocatorTarget::GetVguiTarget( void )
{
	return NULL;
}

//------------------------------------
void CLocatorTarget::SetOnscreenIconTextureName( const char *pszTexture )
{
	if ( Q_strcmp( m_szOnscreenTexture.String(), pszTexture ) == 0 )
		return;

	m_szOnscreenTexture = pszTexture;
	m_pIcon_onscreen = NULL; // Dirty the onscreen icon so that the Locator will look up the new icon by name.

	m_pulseStart = gpGlobals->curtime;
}

//------------------------------------
void CLocatorTarget::SetOffscreenIconTextureName( const char *pszTexture )
{
	if ( Q_strcmp( m_szOffscreenTexture.String(), pszTexture ) == 0 )
		return;

	m_szOffscreenTexture = pszTexture;
	m_pIcon_offscreen = NULL; // Ditto

	m_pulseStart = gpGlobals->curtime;
}

//------------------------------------
void CLocatorTarget::SetBinding( const char *pszBinding )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	BindingLookupOption_t nBindingLookupFlags = BINDINGLOOKUP_ALL;

	// Only show joystick binds if it's enabled and non-joystick if it's disabled
	nBindingLookupFlags = input->ControllerModeActive() ? BINDINGLOOKUP_JOYSTICK_ONLY : BINDINGLOOKUP_KEYBOARD_ONLY;

	bool bIsControllerNow = ( nBindingLookupFlags != 0 );

	if ( m_bWasControllerLast == bIsControllerNow )
	{
		// We haven't toggled joystick enabled recently, so if it's the same bind, bail
		if ( Q_strcmp( m_szBinding.String(), pszBinding ) == 0 )
		{
			return;
		}
	}

	m_bWasControllerLast = bIsControllerNow;

	m_szBinding = pszBinding;
	m_pIcon_onscreen = NULL; // Dirty the onscreen icon so that the Locator will look up the new icon by name.
	m_pIcon_offscreen = NULL; // ditto.
	m_flNextBindingTick = gpGlobals->curtime + 0.75f;

	// Get a list of all the keys bound to these actions
	m_iBindingChoicesCount = 0;

	// Tokenize the binding name (could be more than one binding)
	int nOriginalToken = 0;
	const char	*pchToken = m_szBinding.String();
	char		szToken[ 128 ];

	pchToken = nexttoken( szToken, pchToken, ';' );

	while ( pchToken )
	{
		// Get the first parameter
		int iTokenBindingCount = 0;
		const char *pchBinding = engine->Key_LookupBindingEx( szToken, nSlot, iTokenBindingCount, nBindingLookupFlags );

		while ( m_iBindingChoicesCount < MAX_LOCATOR_BINDINGS_SHOWN && pchBinding )
		{
			m_pchBindingChoices[ m_iBindingChoicesCount ] = pchBinding;
			m_iBindChoicesOriginalToken[ m_iBindingChoicesCount ] = nOriginalToken;
			++m_iBindingChoicesCount;
			++iTokenBindingCount;

			pchBinding = engine->Key_LookupBindingEx( szToken, nSlot, iTokenBindingCount, nBindingLookupFlags );
		}

		nOriginalToken++;
		pchToken = nexttoken( szToken, pchToken, ';' );
	}

	m_pulseStart = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLocatorTarget::UseBindingImage( char *pchIconTextureName, size_t bufSize )
{
	if ( m_iBindingChoicesCount <= 0 )
	{
		{
			Q_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
			return "#GameUI_Icons_NONE";
		}

		return NULL;
	}

	// Cycle through the list of binds at a rate of 2 per second
	const char *pchBinding = m_pchBindingChoices[ m_iBindingTick % m_iBindingChoicesCount ];

	// We counted at least one binding... this should not be NULL!
	Assert( pchBinding );


	//icon_blank_wide
	/*
	if ( input->ControllerModeActive() && 
		 ( Q_strcmp( pchBinding, "A_BUTTON" ) == 0 || 
		   Q_strcmp( pchBinding, "B_BUTTON" ) == 0 || 
		   Q_strcmp( pchBinding, "X_BUTTON" ) == 0 || 
		   Q_strcmp( pchBinding, "Y_BUTTON" ) == 0 || 
		   Q_strcmp( pchBinding, "L_SHOULDER" ) == 0 || 
		   Q_strcmp( pchBinding, "R_SHOULDER" ) == 0 || 
		   Q_strcmp( pchBinding, "L_TRIGGER" ) == 0 || 
		   Q_strcmp( pchBinding, "R_TRIGGER" ) == 0 || 
		   Q_strcmp( pchBinding, "BACK" ) == 0 || 
		   Q_strcmp( pchBinding, "START" ) == 0 || 
		   Q_strcmp( pchBinding, "STICK1" ) == 0 || 
		   Q_strcmp( pchBinding, "STICK2" ) == 0 || 
		   Q_strcmp( pchBinding, "UP" ) == 0 || 
		   Q_strcmp( pchBinding, "DOWN" ) == 0 || 
		   Q_strcmp( pchBinding, "LEFT" ) == 0 || 
		   Q_strcmp( pchBinding, "RIGHT" ) == 0 ) )
	{
		// Use a blank background for the button icons
		Q_strncpy( pchIconTextureName, "icon_blank", bufSize );
		return pchBinding;
	}
	*/

	if ( Q_strcmp( pchBinding, "MOUSE1" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_mouseLeft", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "MOUSE2" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_mouseRight", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "MOUSE3" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_mouseThree", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "MWHEELUP" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_mouseWheel_up", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "MWHEELDOWN" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_mouseWheel_down", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "UPARROW" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_up", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "LEFTARROW" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_left", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "DOWNARROW" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_down", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "RIGHTARROW" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_right", bufSize );
		return NULL;
	}
	else if ( Q_strcmp( pchBinding, "SEMICOLON" ) == 0 || 
		Q_strcmp( pchBinding, "INS" ) == 0 || 
		Q_strcmp( pchBinding, "DEL" ) == 0 || 
		Q_strcmp( pchBinding, "HOME" ) == 0 || 
		Q_strcmp( pchBinding, "END" ) == 0 || 
		Q_strcmp( pchBinding, "PGUP" ) == 0 || 
		Q_strcmp( pchBinding, "PGDN" ) == 0 || 
		Q_strcmp( pchBinding, "PAUSE" ) == 0 || 
		Q_strcmp( pchBinding, "F10" ) == 0 || 
		Q_strcmp( pchBinding, "F11" ) == 0 || 
		Q_strcmp( pchBinding, "F12" ) == 0 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_generic", bufSize );
		return pchBinding;
	}
	else if ( Q_strlen( pchBinding ) <= 2 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_generic", bufSize );
		return pchBinding;
	}
	else if ( Q_strlen( pchBinding ) <= 6 )
	{
		Q_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
		return pchBinding;
	}
	else
	{
		Q_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
		return pchBinding;
	}

	return pchBinding;
}

//-----------------------------------------------------------------------------
int CLocatorTarget::GetIconWidth( void )
{
	return m_wide;
}

//-----------------------------------------------------------------------------
int CLocatorTarget::GetIconHeight( void )
{
	return m_tall;
}


static CLocatorTarget	g_LocatorTargets[ MAX_LOCATOR_TARGETS ];
static int				g_LocatorSerializer = 1000;

static CLocatorTarget *Locator_FindTargetByHandle( int hTarget )
{
	for( int i = 0 ; i < MAX_LOCATOR_TARGETS ; i++ )
	{
		if( g_LocatorTargets[ i ].m_isActive && g_LocatorTargets[ i ].m_serialNumber == hTarget )
		{
			return &g_LocatorTargets[ i ];
		}
	}

	return NULL;
}

int Locator_AddTarget()
{
	for( int i = 0 ; i < MAX_LOCATOR_TARGETS ; i++ )
	{
		if( !g_LocatorTargets[ i ].m_isActive )
		{
			g_LocatorTargets[ i ].Activate( g_LocatorSerializer );
			g_LocatorSerializer++;

			return g_LocatorTargets[ i ].m_serialNumber;
		}
	}

	DevWarning( "Locator has no free targets!\n" );
	return -1;
}

void Locator_RemoveTarget( int hTarget )
{
	CLocatorTarget *pTarget = Locator_FindTargetByHandle( hTarget );

	if( pTarget )
	{
		pTarget->Deactivate();
	}
}

CLocatorTarget *Locator_GetTargetFromHandle( int hTarget )
{
	return Locator_FindTargetByHandle( hTarget );
}

void Locator_ComputeTargetIconPositionFromHandle( int hTarget )
{
	CLocatorTarget *pTarget = Locator_FindTargetByHandle( hTarget );

	if( pTarget )
	{
		pTarget->m_targetX = 0;
		pTarget->m_targetY = 0;
		pTarget->m_iconX = 0;
		pTarget->m_iconY = 0;
		pTarget->m_centerX = 0;
		pTarget->m_centerY = 0;
	}
}
