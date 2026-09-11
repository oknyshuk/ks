//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef C_PHYSICS_PROP_STATUE_H
#define C_PHYSICS_PROP_STATUE_H

#include "reflect_annotations.h"

#ifdef _WIN32
#pragma once
#endif


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


#endif // C_PHYSICS_PROP_STATUE_H
