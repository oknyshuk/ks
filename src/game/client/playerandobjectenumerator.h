#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "ehandle.h"
#include "ispatialpartition.h"
#include "networkvar.h"

class C_BaseEntity;
class C_BasePlayer;

// Enumator class for finding other players and objects close to the
//  local player
class CPlayerAndObjectEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_NOBASE( CPlayerAndObjectEnumerator );
public:
	//Forced constructor
	explicit CPlayerAndObjectEnumerator( float radius );

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

	int	GetObjectCount();
	C_BaseEntity *GetObject( int index );

public:
	//Data members
	float	m_flRadiusSquared;

	std::vector< CHandle< C_BaseEntity > > m_Objects;
	C_BasePlayer *m_pLocal;
};
