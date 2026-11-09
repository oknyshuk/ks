#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ispatialpartition.h"
#include "networkvar.h"

class C_BaseEntity;

//Enumator class for ragdolls being affected by explosive forces
class CRagdollExplosionEnumerator : public IPartitionEnumerator
{
	DECLARE_CLASS_GAMEROOT( CRagdollExplosionEnumerator, IPartitionEnumerator );
public:
	//Forced constructor
	CRagdollExplosionEnumerator( Vector origin, float radius, float magnitude );
	~CRagdollExplosionEnumerator();

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );

public:
	//Data members
	std::vector< C_BaseEntity* > m_Entities;
	Vector	m_vecOrigin;
	float	m_flMagnitude;
	float	m_flRadius;
};
