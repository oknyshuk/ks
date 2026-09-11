//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

//-----------------------------------------------------------------------------
// Purpose: Dispatches a beam ring between two entities
//-----------------------------------------------------------------------------
#if !defined( TE_BASEBEAM_H )
#define TE_BASEBEAM_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

#include "basetempentity.h"

abstract_class [[= ks::reflect::NetTable{ .name = "DT_BaseBeam", .base = false } ]]
      CTEBaseBeam : public CBaseTempEntity
{
public:

	DECLARE_CLASS( CTEBaseBeam, CBaseTempEntity );
	DECLARE_SERVERCLASS();


public:
					CTEBaseBeam( const char *name );
	virtual			~CTEBaseBeam( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles ) = 0;
	
public:
	CNetworkVar( int, m_nModelIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nHaloIndex, [[= ks::reflect::Net{ .enc = ks::reflect::ENC_MODELINDEX } ]] );
	CNetworkVar( int, m_nStartFrame, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFrameRate, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fLife, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 25.6 } ]] );
	CNetworkVar( float, m_fWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 128.0 } ]] );
	CNetworkVar( float, m_fEndWidth, [[= ks::reflect::Net{ .bits = 10, .low = 0.0, .high = 128.0 } ]] );
	CNetworkVar( int, m_nFadeLength, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_fAmplitude, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 64.0 } ]] );
	CNetworkVar( int, r, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, g, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, b, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, a, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nSpeed, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( int, m_nFlags, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] );
};

EXTERN_SEND_TABLE(DT_BaseBeam);

#endif // TE_BASEBEAM_H
