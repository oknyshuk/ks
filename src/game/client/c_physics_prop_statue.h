#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "c_physicsprop.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_StatueProp" } ]]
      C_StatueProp : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_StatueProp, C_PhysicsProp );
	DECLARE_CLIENTCLASS();

	C_StatueProp();
	virtual			~C_StatueProp();

	virtual void Spawn( void );

	virtual void ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:

	[[= ks::reflect::Net{} ]] CHandle<CBaseAnimating>		m_hInitBaseAnimating;

	[[= ks::reflect::Net{} ]] bool	m_bShatter;
	[[= ks::reflect::Net{} ]] int		m_nShatterFlags;
	[[= ks::reflect::Net{} ]] Vector	m_vShatterPosition;
	[[= ks::reflect::Net{} ]] Vector	m_vShatterForce;
};
