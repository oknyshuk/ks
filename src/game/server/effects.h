//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EFFECTS_H
#define EFFECTS_H

#include "reflect_annotations.h"



class CBaseEntity;
class Vector;


//-----------------------------------------------------------------------------
// The rotor wash shooter. It emits gibs when pushed by a rotor wash
//-----------------------------------------------------------------------------
abstract_class IRotorWashShooter
{
public:
	virtual CBaseEntity *DoWashPush( float flWashStartTime, const Vector &vecForce ) = 0;
};


//-----------------------------------------------------------------------------
// Gets at the interface if the entity supports it
//-----------------------------------------------------------------------------
IRotorWashShooter *GetRotorWashShooter( CBaseEntity *pEntity );

class [[= ks::reflect::NetTable{ .name = "DT_QuadraticBeam" } ]]
      CEnvQuadraticBeam : public CPointEntity
{
	DECLARE_CLASS( CEnvQuadraticBeam, CPointEntity );

public:
	void Spawn();
	void SetSpline( const Vector &control, const Vector &target )
	{
		m_targetPosition = target;
		m_controlPosition = control;
	}
	void SetScrollRate( float rate )
	{
		m_scrollRate = rate;
	}

	void SetWidth( float width )
	{
		m_flWidth = width;
	}

private:
	CNetworkVector( m_targetPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVector( m_controlPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::ENC_VECTOR } ]] );
	CNetworkVar( float, m_scrollRate, [[= ks::reflect::Net{ .bits = 8, .low = -4, .high = 4 } ]] );
	CNetworkVar( float, m_flWidth, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	DECLARE_SERVERCLASS();
};
CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner );


#endif // EFFECTS_H
