#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.tier1;
#include "ehandle.h"
#include "ispatialpartition.h"
import std;
#include "networkvar.h"
#include "cliententitylist.h"
#include "c_baseentity.h"

class C_BaseEntity;

class CPlayerEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CPlayerEnumerator );
public:
	//Forced constructor
	CPlayerEnumerator( float radius, Vector vecOrigin )
	{
		m_flRadiusSquared = radius * radius;
		m_vecOrigin = vecOrigin;
		m_Objects.clear();
	}

	int	GetObjectCount() { return std::ssize( m_Objects ); }

	C_BaseEntity *GetObject( int index )
	{
		if ( index < 0 || index >= GetObjectCount() )
			return nullptr;

		return m_Objects[ index ];
	}

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
		if ( pEnt == nullptr )
			return ITERATION_CONTINUE;

		if ( !pEnt->IsPlayer() )
			return ITERATION_CONTINUE;

		Vector	deltaPos = pEnt->GetAbsOrigin() - m_vecOrigin;
		if ( deltaPos.LengthSqr() > m_flRadiusSquared )
			return ITERATION_CONTINUE;

		CHandle< C_BaseEntity > h;
		h = pEnt;
		m_Objects.push_back( h );

		return ITERATION_CONTINUE;
	}

public:
	//Data members
	float	m_flRadiusSquared;
	Vector m_vecOrigin;

	std::vector< CHandle< C_BaseEntity > > m_Objects;
};
