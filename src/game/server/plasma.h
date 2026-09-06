//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef	__PLASMA__
#define __PLASMA__

#include "reflect_annotations.h"
#pragma once

#include "fire_smoke.h"

//==================================================
// CPlasma
//==================================================

//NOTENOTE: Mirrored in cl_dll/c_plasma.cpp
#define	bitsPLASMA_FREE		0x00000002

class [[= ks::reflect::NetTable{ .name = "DT_Plasma" } ]]
      [[= ks::reflect::From<"m_flScale", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flScaleTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_nFlags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CPlasma : public CBaseFire
{
public:
	DECLARE_CLASS( CPlasma, CBaseFire );

	CPlasma( void );
	virtual	~CPlasma( void );
	void	EnableSmoke( int state );

	void	Precache( void );

	DECLARE_SERVERCLASS();

public:

	//Client-side
	CNetworkVar( int, m_nPlasmaModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nPlasmaModelIndex2, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nGlowModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );

	//Server-side
};

#endif	//__PLASMA__
