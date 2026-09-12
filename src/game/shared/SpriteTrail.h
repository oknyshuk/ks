//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SPRITETRAIL_H
#define SPRITETRAIL_H

#include "reflect_annotations.h"

#include "Sprite.h"

#if defined( CLIENT_DLL )
#define CSpriteTrail C_SpriteTrail
#endif


//-----------------------------------------------------------------------------
// Sprite trail
//-----------------------------------------------------------------------------
struct TrailPoint_t
{
	DECLARE_SIMPLE_DATADESC();

	Vector	m_vecScreenPos;
	float	m_flDieTime;
	float	m_flTexCoord;
	float	m_flWidthVariance;
};

class [[= ks::reflect::NetTable{ .name = "DT_SpriteTrail" } ]]
      CSpriteTrail : public CSprite
{
	DECLARE_CLASS( CSpriteTrail, CSprite );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

public:
	CSpriteTrail( void );

	// Sets parameters of the sprite trail
	void SetLifeTime( float time );
	void SetStartWidth( float flStartWidth );
	void SetEndWidth( float flEndWidth );
	void SetStartWidthVariance( float flStartWidthVariance );
	void SetTextureResolution( float flTexelsPerInch );
	void SetMinFadeLength( float flMinFadeLength );
	void SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale );

	// Is the trail in the skybox?
	bool IsInSkybox() const;
	void Spawn( void );
	void Precache( void );

#if defined( CLIENT_DLL ) 
	// Client only code
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual const Vector &GetRenderOrigin( void );
	virtual const QAngle &GetRenderAngles( void );

	// On data update
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void GetRenderBounds( Vector& mins, Vector& maxs );
	virtual void ClientThink();
#else
	// Server only code
	static CSpriteTrail *SpriteTrailCreate( const char *pSpriteName, const Vector &origin, bool animate );
#endif

private:
#if defined( CLIENT_DLL )
	enum
	{
		// NOTE: # of points max must be a power of two!
		MAX_SPRITE_TRAIL_POINTS	= 64,
		MAX_SPRITE_TRAIL_MASK = 0x3F,
	};

	TrailPoint_t *GetTrailPoint( int n );
	void	UpdateTrail( void );
	void	ComputeScreenPosition( Vector *pScreenPos );
	void	ConvertSkybox();
	void	UpdateBoundingBox( void );

	TrailPoint_t	m_vecSteps[MAX_SPRITE_TRAIL_POINTS];
	int	m_nFirstStep;
	int m_nStepCount;
	float m_flUpdateTime;
	Vector m_vecPrevSkyboxOrigin;
	float m_flPrevSkyboxScale;
	Vector m_vecRenderMins;
	Vector m_vecRenderMaxs;
#endif

	CNetworkVar( float, m_flLifeTime, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );	// Amount of time before a new trail segment fades away
	CNetworkVar( float, m_flStartWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "startwidth" } ]] );	// The starting scale
	CNetworkVar( float, m_flEndWidth, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "endwidth" } ]] );	// The ending scale
	CNetworkVar( float, m_flStartWidthVariance, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// The starting scale
	CNetworkVar( float, m_flTextureRes, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Texture resolution along the trail
	CNetworkVar( float, m_flMinFadeLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// The end of the trail must fade out for this many units
	CNetworkVector( m_vecSkyboxOrigin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] );	// What's our skybox origin?
	CNetworkVar( float, m_flSkyboxScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// What's our skybox scale?

	[[= ks::reflect::Key{ .name = "spritename" } ]] string_t m_iszSpriteName;
	[[= ks::reflect::Key{ .name = "animate" } ]] bool	m_bAnimate;
};

#endif // SPRITETRAIL_H
