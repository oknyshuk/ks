// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "cdll_client_int.h"
#include "baseanimatedtextureproxy.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CBaseAnimatedTextureProxy::CBaseAnimatedTextureProxy()
{
	Cleanup();
}

CBaseAnimatedTextureProxy::~CBaseAnimatedTextureProxy()
{
	Cleanup();
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CBaseAnimatedTextureProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pAnimatedTextureVarName = pKeyValues->GetString( "animatedTextureVar" );
	if( !pAnimatedTextureVarName )
		return false;

	bool foundVar;
	m_AnimatedTextureVar = pMaterial->FindVar( pAnimatedTextureVarName, &foundVar, false );
	if( !foundVar )
		return false;

	char const* pAnimatedTextureFrameNumVarName = pKeyValues->GetString( "animatedTextureFrameNumVar" );
	if( !pAnimatedTextureFrameNumVarName )
		return false;

	m_AnimatedTextureFrameNumVar = pMaterial->FindVar( pAnimatedTextureFrameNumVarName, &foundVar, false );
	if( !foundVar )
		return false;

	m_FrameRate = pKeyValues->GetFloat( "animatedTextureFrameRate", 15 );
	m_WrapAnimation = !pKeyValues->GetInt( "animationNoWrap", 0 );
	return true;
}

void CBaseAnimatedTextureProxy::Cleanup()
{
	m_AnimatedTextureVar = nullptr;
	m_AnimatedTextureFrameNumVar = nullptr;
}


//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CBaseAnimatedTextureProxy::OnBind( void *pEntity )
{
	contract_assert( m_AnimatedTextureVar );

	if( m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		return;
	}
	ITexture *pTexture;
	pTexture = m_AnimatedTextureVar->GetTextureValue();
	int numFrames = pTexture->GetNumAnimationFrames();

	if ( numFrames <= 0 )
	{
		contract_assert( !"0 frames in material calling animated texture proxy" );
		return;
	}

	// NOTE: Must not use relative time based methods here
	// because the bind proxy can be called many times per frame.
	// Prevent multiple Wrap callbacks to be sent for no wrap mode
	float startTime = GetAnimationStartTime(pEntity);
	float deltaTime = gpGlobals->curtime - startTime;
	float prevTime = deltaTime - gpGlobals->frametime;

	// Clamp..
	if (deltaTime < 0.0f)
		deltaTime = 0.0f;
	if (prevTime < 0.0f)
		prevTime = 0.0f;

	float frame = m_FrameRate * deltaTime;	
	float prevFrame = m_FrameRate * prevTime;

	int intFrame = ((int)frame) % numFrames; 
	int intPrevFrame = ((int)prevFrame) % numFrames;

	// Report wrap situation...
	if (intPrevFrame > intFrame)
	{
		if (m_WrapAnimation)
		{
			AnimationWrapped( pEntity );
		}
		else
		{
			// Only sent the wrapped message once.
			// when we're in non-wrapping mode
			if (prevFrame < numFrames)
				AnimationWrapped( pEntity );
			intFrame = numFrames - 1;
		}
	}

	m_AnimatedTextureFrameNumVar->SetIntValue( intFrame );
}

IMaterial *CBaseAnimatedTextureProxy::GetMaterial()
{
	return m_AnimatedTextureVar->GetOwningMaterial();
}
