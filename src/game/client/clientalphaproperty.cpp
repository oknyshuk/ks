// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "clientalphaproperty.h"
#include "const.h"
#include "iclientshadowmgr.h"
#include "iclientunknown.h"
#include "iclientrenderable.h"

//-----------------------------------------------------------------------------
// Client alpha property starts here
//-----------------------------------------------------------------------------
CClientAlphaProperty::CClientAlphaProperty( )
{
	m_nRenderFX = kRenderFxNone;
	m_nRenderMode = kRenderNormal;
	m_nDesyncOffset = 0;
	m_hShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_nAlpha = 255;
	m_flFadeScale = 0.0f;	// By default, things don't fade out automagically
	m_nDistFadeStart = 0;
	m_nDistFadeEnd = 0;
	m_bAlphaOverride = false;
	m_bShadowAlphaOverride = false;
	m_nDistanceFadeMode = CLIENT_ALPHA_DISTANCE_FADE_USE_CENTER;
}

void CClientAlphaProperty::Init( IClientUnknown *pUnk )
{
	m_pOuter = pUnk;
}

IClientUnknown*	CClientAlphaProperty::GetIClientUnknown()
{
	return m_pOuter;
}

void CClientAlphaProperty::SetShadowHandle( ClientShadowHandle_t hShadowHandle )
{
	m_hShadowHandle = hShadowHandle;
}

void CClientAlphaProperty::SetAlphaModulation( uint8 a )
{
	m_nAlpha = a;
}

void CClientAlphaProperty::EnableAlphaModulationOverride( bool bEnable )
{
	m_bAlphaOverride = bEnable;
}

void CClientAlphaProperty::EnableShadowAlphaModulationOverride( bool bEnable )
{
	m_bShadowAlphaOverride = bEnable;
}

// Sets an FX function
void CClientAlphaProperty::SetRenderFX( RenderFx_t nRenderFx, RenderMode_t nRenderMode, float flStartTime, float flDuration )
{
	bool bStartTimeUnspecified = ( flStartTime == std::numeric_limits<float>::max() );
	bool bRenderFxChanged = ( m_nRenderFX != nRenderFx );

	switch( nRenderFx )
	{
	case kRenderFxFadeIn:
	case kRenderFxFadeOut:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		break;

	case kRenderFxFadeSlow:
	case kRenderFxSolidSlow:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 4.0f;
		break;

	case kRenderFxFadeFast:
	case kRenderFxSolidFast:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 1.0f;
		break;
	}

	m_nRenderMode = nRenderMode;
	m_nRenderFX = nRenderFx;
	if ( bRenderFxChanged || !bStartTimeUnspecified )
	{
		m_flRenderFxStartTime = flStartTime;
		m_flRenderFxDuration = flDuration;
	}
}

void CClientAlphaProperty::SetDesyncOffset( int nOffset )
{
	m_nDesyncOffset = nOffset;
}

void CClientAlphaProperty::SetDistanceFadeMode( ClientAlphaDistanceFadeMode_t nFadeMode )
{
	// Necessary since m_nDistanceFadeMode is stored in 1 bit
	static_assert( CLIENT_ALPHA_DISTANCE_FADE_MODE_COUNT <= ( 1 << CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT ) );
	m_nDistanceFadeMode = nFadeMode;
}


// Sets fade parameters
void CClientAlphaProperty::SetFade( float flGlobalFadeScale, float flDistFadeStart, float flDistFadeEnd )
{
	if( flDistFadeStart > flDistFadeEnd )
	{
		V_swap( flDistFadeStart, flDistFadeEnd );
	}

	// If a negative value is provided for the min fade distance, then base it off the max.
	if( flDistFadeStart < 0 )
	{
		flDistFadeStart = flDistFadeEnd + flDistFadeStart;
		if( flDistFadeStart < 0 )
		{
			flDistFadeStart = 0;
		}
	}

	contract_assert( flDistFadeStart >= 0 && flDistFadeStart <= 65535 );
	contract_assert( flDistFadeEnd >= 0 && flDistFadeEnd <= 65535 );

	m_nDistFadeStart = (uint16)flDistFadeStart;
	m_nDistFadeEnd = (uint16)flDistFadeEnd;
	m_flFadeScale = flGlobalFadeScale;
}


int CClientAlphaProperty::ComputeRenderEffectBlend( int nRenderEffect ) const
{
	int nBlend = 0;
	float flOffset = ((int)m_nDesyncOffset) * 363.0;// Use ent index to de-sync these fx

	switch( nRenderEffect ) 
	{
	case kRenderFxPulseSlowWide:
		nBlend = m_nAlpha + 0x40 * std::sin( gpGlobals->curtime * 2 + flOffset );	
		break;

	case kRenderFxPulseFastWide:
		nBlend = m_nAlpha + 0x40 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxPulseFastWider:
		nBlend = ( 0xff * fabs(sin( gpGlobals->curtime * 12 + flOffset ) ) );
		break;

	case kRenderFxPulseSlow:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 2 + flOffset );
		break;

	case kRenderFxPulseFast:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxFadeOut:
	case kRenderFxFadeFast:
	case kRenderFxFadeSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, m_nAlpha, 0 );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxFadeIn:
	case kRenderFxSolidFast:
	case kRenderFxSolidSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, 0, m_nAlpha );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxStrobeSlow:
		nBlend = 20 * sin( gpGlobals->curtime * 4 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFast:
		nBlend = 20 * sin( gpGlobals->curtime * 16 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFaster:
		nBlend = 20 * sin( gpGlobals->curtime * 36 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerSlow:
		nBlend = 20 * (sin( gpGlobals->curtime * 2 ) + sin( gpGlobals->curtime * 17 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerFast:
		nBlend = 20 * (sin( gpGlobals->curtime * 16 ) + sin( gpGlobals->curtime * 23 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxNone:
	default:
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
		break;	
	}
	return nBlend;
}

//-----------------------------------------------------------------------------
// Computes alpha value based on render fx
//-----------------------------------------------------------------------------
uint8 CClientAlphaProperty::ComputeRenderAlpha( ) const
{
	if ( m_nRenderMode == kRenderNone || m_nRenderMode == kRenderEnvironmental )
		return 0;

	int nBlend = 0;

	if ( m_nRenderFX > kRenderNone && m_nRenderFX < kRenderFxMax )
	{
		nBlend = ComputeRenderEffectBlend( m_nRenderFX );
	}
	else
	{
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
	}

	if ( m_bAlphaOverride )
	{
		nBlend = m_pOuter->GetClientRenderable()->OverrideAlphaModulation( m_nAlpha );
	}
	nBlend = clamp( nBlend, 0, 255 );

	return nBlend;
}
