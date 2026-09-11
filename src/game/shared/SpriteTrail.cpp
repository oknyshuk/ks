//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "reflect_datamap.h"
#include "SpriteTrail.h"

#ifdef CLIENT_DLL

#include "clientsideeffects.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "mathlib/vmatrix.h"
#include "view.h"
#include "beamdraw.h"
#include "enginesprite.h"
#include "tier0/vprof.h"

extern CEngineSprite *Draw_SetSpriteTexture( const model_t *pSpriteModel, int frame, int rendermode );

#endif // CLIENT_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------
#define SCREEN_SPACE_TRAILS 0


//-----------------------------------------------------------------------------
// Save/Restore
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( TrailPoint_t )

#endif

IMPLEMENT_REFLECT_DATAMAP( CSpriteTrail )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( SpriteTrail, DT_SpriteTrail );

IMPLEMENT_REFLECT_TABLE( CSpriteTrail, DT_SpriteTrail );

LINK_ENTITY_TO_CLASS_ALIASED( env_spritetrail, SpriteTrail );

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CSpriteTrail );
#endif

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSpriteTrail::CSpriteTrail( void )
{
#ifdef CLIENT_DLL
	m_nFirstStep = 0;
	m_nStepCount = 0;
#endif

	m_flStartWidthVariance = 0;
	m_vecSkyboxOrigin.Init( 0, 0, 0 );
	m_flSkyboxScale = 1.0f;
	m_flEndWidth = -1.0f;
}

void CSpriteTrail::Spawn( void )
{
#ifdef CLIENT_DLL
	BaseClass::Spawn();
#else

	if ( GetModelName() != NULL_STRING )
	{
		BaseClass::Spawn();
		return;
	}

	SetModelName( m_iszSpriteName );
	BaseClass::Spawn();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NOCLIP );

	SetCollisionBounds( vec3_origin, vec3_origin );
	TurnOn();

#endif
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::Precache( void ) 
{
	BaseClass::Precache();

	if ( m_iszSpriteName != NULL_STRING )
	{
		PrecacheModel( STRING(m_iszSpriteName) );
	}
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::SetLifeTime( float time ) 
{ 
	m_flLifeTime = time; 
}

void CSpriteTrail::SetStartWidth( float flStartWidth )
{ 
	m_flStartWidth = flStartWidth; 
	m_flStartWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetStartWidthVariance( float flStartWidthVariance )
{ 
	m_flStartWidthVariance = flStartWidthVariance; 
	m_flStartWidthVariance /= m_flSkyboxScale;
}

void CSpriteTrail::SetEndWidth( float flEndWidth )
{ 
	m_flEndWidth = flEndWidth; 
	m_flEndWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetTextureResolution( float flTexelsPerInch )
{ 
	m_flTextureRes = flTexelsPerInch; 
	m_flTextureRes *= m_flSkyboxScale;
}

void CSpriteTrail::SetMinFadeLength( float flMinFadeLength )
{
	m_flMinFadeLength = flMinFadeLength;
	m_flMinFadeLength /= m_flSkyboxScale;
}

void CSpriteTrail::SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale )
{
	m_flTextureRes /= m_flSkyboxScale;
	m_flMinFadeLength *= m_flSkyboxScale;
	m_flStartWidth *= m_flSkyboxScale;
	m_flEndWidth *= m_flSkyboxScale;
	m_flStartWidthVariance *= m_flSkyboxScale;

	m_vecSkyboxOrigin = vecSkyboxOrigin;
	m_flSkyboxScale = flSkyboxScale;

	m_flTextureRes *= m_flSkyboxScale;
	m_flMinFadeLength /= m_flSkyboxScale;
	m_flStartWidth /= m_flSkyboxScale;
	m_flEndWidth /= m_flSkyboxScale;
	m_flStartWidthVariance /= m_flSkyboxScale;

	if ( IsInSkybox() )
	{
		AddEFlags( EFL_IN_SKYBOX ); 
	}
	else
	{
		RemoveEFlags( EFL_IN_SKYBOX ); 
	}
}


//-----------------------------------------------------------------------------
// Is the trail in the skybox?
//-----------------------------------------------------------------------------
bool CSpriteTrail::IsInSkybox() const
{
	return (m_flSkyboxScale != 1.0f) || (m_vecSkyboxOrigin != vec3_origin);
}


#ifdef CLIENT_DLL


//-----------------------------------------------------------------------------
// On data update
//-----------------------------------------------------------------------------
void CSpriteTrail::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );
	m_vecPrevSkyboxOrigin = m_vecSkyboxOrigin;
	m_flPrevSkyboxScale = m_flSkyboxScale;
}

void CSpriteTrail::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		if ((m_flPrevSkyboxScale != m_flSkyboxScale) || (m_vecPrevSkyboxOrigin != m_vecSkyboxOrigin))
		{
			ConvertSkybox();
		}
	}
}


//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::ClientThink()
{
	// Update the trail + bounding box
	UpdateTrail();
	UpdateBoundingBox();
}


//-----------------------------------------------------------------------------
// Render bounds
//-----------------------------------------------------------------------------
void CSpriteTrail::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_vecRenderMins;
	maxs = m_vecRenderMaxs;
}


//-----------------------------------------------------------------------------
// Converts the trail when it changes skyboxes
//-----------------------------------------------------------------------------
void CSpriteTrail::ConvertSkybox()
{
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = GetTrailPoint(i);

		VectorSubtract( pPoint->m_vecScreenPos, m_vecPrevSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_vecScreenPos *= m_flPrevSkyboxScale / m_flSkyboxScale;
		VectorSubtract( pPoint->m_vecScreenPos, m_vecSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_flWidthVariance *= m_flPrevSkyboxScale / m_flSkyboxScale;
	}
}


//-----------------------------------------------------------------------------
// Gets at the nth item in the list
//-----------------------------------------------------------------------------
TrailPoint_t *CSpriteTrail::GetTrailPoint( int n )
{
	Assert( n < MAX_SPRITE_TRAIL_POINTS );
	COMPILE_TIME_ASSERT( (MAX_SPRITE_TRAIL_POINTS & (MAX_SPRITE_TRAIL_POINTS-1)) == 0 );
	int nIndex = (n + m_nFirstStep) & MAX_SPRITE_TRAIL_MASK; 
	return &m_vecSteps[nIndex];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::ComputeScreenPosition( Vector *pScreenPos )
{
#if SCREEN_SPACE_TRAILS
	VMatrix	viewMatrix;
	materials->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	*pScreenPos = viewMatrix * GetRenderOrigin();
#else
	*pScreenPos = GetRenderOrigin();
#endif
}


//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateBoundingBox( void )
{
	Vector vecRenderOrigin = GetRenderOrigin();
	m_vecRenderMins = vecRenderOrigin;
	m_vecRenderMaxs = vecRenderOrigin;

	float flMaxWidth = m_flStartWidth;
	if (( m_flEndWidth >= 0.0f ) && ( m_flEndWidth > m_flStartWidth ))
	{
		flMaxWidth = m_flEndWidth;
	}

	Vector mins, maxs;
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		TrailPoint_t *pPoint = GetTrailPoint(i);

		float flActualWidth = (flMaxWidth + pPoint->m_flWidthVariance) * 0.5f;
		Vector size( flActualWidth, flActualWidth, flActualWidth );
		VectorSubtract( pPoint->m_vecScreenPos, size, mins );
		VectorAdd( pPoint->m_vecScreenPos, size, maxs );
		
		VectorMin( m_vecRenderMins, mins, m_vecRenderMins ); 
		VectorMax( m_vecRenderMaxs, maxs, m_vecRenderMaxs ); 
	}
	
	m_vecRenderMins -= vecRenderOrigin; 
	m_vecRenderMaxs -= vecRenderOrigin;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateTrail( void )
{
	// Can't update too quickly
	if ( m_flUpdateTime > gpGlobals->curtime )
		return;

	Vector	screenPos;
	ComputeScreenPosition( &screenPos );
	TrailPoint_t *pLast = m_nStepCount ? GetTrailPoint( m_nStepCount-1 ) : nullptr;
	if ( ( pLast == nullptr ) || ( pLast->m_vecScreenPos.DistToSqr( screenPos ) > 4.0f ) )
	{
		// If we're over our limit, steal the last point and put it up front
		if ( m_nStepCount >= MAX_SPRITE_TRAIL_POINTS )
		{
			--m_nStepCount;
			++m_nFirstStep;
		}

		// Save off its screen position, not its world position
		TrailPoint_t *pNewPoint = GetTrailPoint( m_nStepCount );
		pNewPoint->m_vecScreenPos = screenPos;
		pNewPoint->m_flDieTime	= gpGlobals->curtime + m_flLifeTime;
		pNewPoint->m_flWidthVariance = random->RandomFloat( -m_flStartWidthVariance, m_flStartWidthVariance );
		if (pLast)
		{
			pNewPoint->m_flTexCoord	= pLast->m_flTexCoord + pLast->m_vecScreenPos.DistTo( screenPos ) * m_flTextureRes;
		}
		else
		{
			pNewPoint->m_flTexCoord = 0.0f;
		}

		++m_nStepCount;
	}

	// Don't update again for a bit
	m_flUpdateTime = gpGlobals->curtime + ( m_flLifeTime / (float) MAX_SPRITE_TRAIL_POINTS );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSpriteTrail::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "CSpriteTrail::DrawModel", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	// Must have at least one point
	if ( m_nStepCount < 1 )
		return 1;

	//See if we should draw
	if ( !IsVisible() || ( m_bReadyToDraw == false ) )
		return 0;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( GetModel(), m_flFrame, GetRenderMode() );
	if ( pSprite == nullptr )
		return 0;

	// Specify all the segments.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, m_nStepCount + 1, pSprite->GetMaterial( GetRenderMode() ) );
	
	// Setup the first point, always emanating from the attachment point
	TrailPoint_t *pLast = GetTrailPoint( m_nStepCount-1 );
	TrailPoint_t currentPoint;
	currentPoint.m_flDieTime = gpGlobals->curtime + m_flLifeTime;
	ComputeScreenPosition( &currentPoint.m_vecScreenPos );
	currentPoint.m_flTexCoord = pLast->m_flTexCoord + currentPoint.m_vecScreenPos.DistTo(pLast->m_vecScreenPos) * m_flTextureRes;
	currentPoint.m_flWidthVariance = 0.0f;

#if SCREEN_SPACE_TRAILS
	VMatrix	viewMatrix;
	materials->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	viewMatrix = viewMatrix.InverseTR();
#endif

	TrailPoint_t *pPrevPoint = nullptr;
	float flTailAlphaDist = m_flMinFadeLength;
	for ( int i = 0; i <= m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = (i != m_nStepCount) ? GetTrailPoint(i) : &currentPoint;

		float flLifePerc = (pPoint->m_flDieTime - gpGlobals->curtime) / m_flLifeTime;
		flLifePerc = clamp( flLifePerc, 0.0f, 1.0f );

		color24 c = GetRenderColor();
		BeamSeg_t curSeg;
		curSeg.m_color.r = c.r; curSeg.m_color.g = c.g; curSeg.m_color.b = c.b;

		float flAlphaFade = flLifePerc;
		if ( flTailAlphaDist > 0.0f )
		{
			if ( pPrevPoint )
			{
				float flDist = pPoint->m_vecScreenPos.DistTo( pPrevPoint->m_vecScreenPos );
				flTailAlphaDist -= flDist;
			}

			if ( flTailAlphaDist > 0.0f )
			{
				float flTailFade = Lerp( (m_flMinFadeLength - flTailAlphaDist) / m_flMinFadeLength, 0.0f, 1.0f );
				if ( flTailFade < flAlphaFade )
				{
					flAlphaFade = flTailFade;
				}
			}
		}
		curSeg.m_color.a  = GetRenderBrightness() * flAlphaFade;

#if SCREEN_SPACE_TRAILS
		curSeg.m_vPos = viewMatrix * pPoint->m_vecScreenPos;
#else
		curSeg.m_vPos = pPoint->m_vecScreenPos;
#endif

		if ( m_flEndWidth >= 0.0f )
		{
			curSeg.m_flWidth = Lerp( flLifePerc, m_flEndWidth.Get(), m_flStartWidth.Get() );
		}
		else
		{
			curSeg.m_flWidth = m_flStartWidth.Get();
		}
		curSeg.m_flWidth += pPoint->m_flWidthVariance;
		if ( curSeg.m_flWidth < 0.0f )
		{
			curSeg.m_flWidth = 0.0f;
		}

		curSeg.m_flTexCoord = pPoint->m_flTexCoord;

		segDraw.NextSeg( &curSeg );

		// See if we're done with this bad boy
		if ( pPoint->m_flDieTime <= gpGlobals->curtime )
		{
			// Push this back onto the top for use
			++m_nFirstStep;
			--i;
			--m_nStepCount;
		}

		pPrevPoint = pPoint;
	}

	segDraw.End();

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector const&
//-----------------------------------------------------------------------------
const Vector &CSpriteTrail::GetRenderOrigin( void )
{
	static Vector vOrigin;
	vOrigin = GetAbsOrigin();

	if ( m_hAttachedToEntity )
	{
		C_BaseEntity *ent = m_hAttachedToEntity->GetBaseEntity();
		if ( ent )
		{
			QAngle dummyAngles;
			ent->GetAttachment( m_nAttachment, vOrigin, dummyAngles );
		}
	}

	return vOrigin;
}

const QAngle &CSpriteTrail::GetRenderAngles( void )
{
	return vec3_angle;
}

#endif	//CLIENT_DLL

#if !defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSpriteTrail
//-----------------------------------------------------------------------------
CSpriteTrail *CSpriteTrail::SpriteTrailCreate( const char *pSpriteName, const Vector &origin, bool animate )
{
	CSpriteTrail *pSprite = CREATE_ENTITY( CSpriteTrail, "env_spritetrail" );

	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->SetSolid( SOLID_NONE );
	pSprite->SetMoveType( MOVETYPE_NOCLIP );
	
	UTIL_SetSize( pSprite, vec3_origin, vec3_origin );

	if ( animate )
	{
		pSprite->TurnOn();
	}

	return pSprite;
}

#endif	//CLIENT_DLL == false
