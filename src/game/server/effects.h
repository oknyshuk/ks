#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "baseentity.h"

class CBaseEntity;


//-----------------------------------------------------------------------------
// The rotor wash shooter. It emits gibs when pushed by a rotor wash
//-----------------------------------------------------------------------------
class IRotorWashShooter
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
	void Spawn_Impl();
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
	CNetworkVector( m_targetPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVector( m_controlPosition, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_COORD, .enc = ks::reflect::WireEnc::Vector } ]] );
	CNetworkVar( float, m_scrollRate, [[= ks::reflect::Net{ .bits = 8, .low = -4, .high = 4 } ]] );
	CNetworkVar( float, m_flWidth, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE } ]] );

	DECLARE_SERVERCLASS();
};
CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner );
