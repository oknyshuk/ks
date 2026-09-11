//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef C_PHYSICSPROP_H
#define C_PHYSICSPROP_H

#include "reflect_annotations.h"

#include "c_breakableprop.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_PhysicsProp" } ]]
      [[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{}>{} ]]
      C_PhysicsProp : public C_BreakableProp
{
	typedef C_BreakableProp BaseClass;
public:
	DECLARE_CLIENTCLASS();

	// Inherited from IClientUnknown
public:
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// Inherited from IClientModelRenderable
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

	// Other public methods
public:
	C_PhysicsProp();
	virtual ~C_PhysicsProp();

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	bool GetPropDataAngles( const char *pKeyName, QAngle &vecAngles );
	float GetCarryDistanceOffset( void );
	void OnPreDataChanged( DataUpdateType_t updateType );
protected:
	// Networked vars.
	[[= ks::reflect::Net{} ]] bool m_bAwake;
	bool m_bAwakeLastTime;
	bool m_bCanUseStaticLighting;

	Vector m_vecClientOrigin;
	QAngle m_vecClientAngles;
};

#endif // C_PHYSICSPROP_H 
