//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basedoor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_FuncMoveLinear" } ]]
      [[= ks::reflect::From<"m_vecVelocity", ks::reflect::Net{}, RecvProxy_LocalVelocity>{} ]]
      [[= ks::reflect::From<"m_fFlags", ks::reflect::Net{}>{} ]]
      C_FuncMoveLinear: public C_BaseToggle
{
public:
	DECLARE_CLASS( C_FuncMoveLinear, C_BaseToggle );
	DECLARE_CLIENTCLASS();

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_vecVelocity );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );

	C_FuncMoveLinear();
	virtual void OnDataChanged( DataUpdateType_t type );
};


IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncMoveLinear, DT_FuncMoveLinear, CFuncMoveLinear )


C_FuncMoveLinear::C_FuncMoveLinear()
{
}

void C_FuncMoveLinear::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetSolid(SOLID_VPHYSICS);
		VPhysicsInitShadow( false, false );
	}
}
