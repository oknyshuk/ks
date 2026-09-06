//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef FIRE_SMOKE_H
#define FIRE_SMOKE_H

#include "reflect_annotations.h"
#pragma once

#include "baseparticleentity.h"

//==================================================
// CBaseFire
//==================================================

//NOTENOTE: Reserved for all descendants
#define	bitsFIRE_NONE	0x00000000
#define	bitsFIRE_ACTIVE	0x00000001

class CBaseFire : public CBaseEntity
{
public:
	DECLARE_CLASS( CBaseFire, CBaseEntity );

	CBaseFire( void );
	virtual	~CBaseFire( void );

	virtual void	Scale( float size, float time );
	virtual void	Scale( float start, float size, float time );
	virtual void	Enable( int state = true );

	//Client-side
	CNetworkVar( float, m_flStartScale );
	CNetworkVar( float, m_flScale );
	CNetworkVar( float, m_flScaleTime );
	CNetworkVar( int, m_nFlags );
};

//==================================================
// CFireSmoke
//==================================================

//NOTENOTE: Mirrored in cl_dll/c_fire_smoke.cpp
#define	bitsFIRESMOKE_SMOKE					0x00000002
#define	bitsFIRESMOKE_SMOKE_COLLISION		0x00000004
#define	bitsFIRESMOKE_GLOW					0x00000008
#define	bitsFIRESMOKE_VISIBLE_FROM_ABOVE	0x00000010

class [[= ks::reflect::NetTable{ .name = "DT_FireSmoke" } ]]
      [[= ks::reflect::From<"m_flStartScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flScaleTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_nFlags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CFireSmoke : public CBaseFire
{
public:
	DECLARE_CLASS( CFireSmoke, CBaseFire );

	CFireSmoke( void );
	virtual	~CFireSmoke( void );

	void	Spawn();
	void	Precache();
	void	EnableSmoke( int state = true );
	void	EnableGlow( int state = true );
	void	EnableVisibleFromAbove( int state = true );
	
	DECLARE_SERVERCLASS();

public:

	//Client-side
	CNetworkVar( int, m_nFlameModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nFlameFromAboveModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );

	//Server-side
};

#endif	//FIRE_SMOKE_H
