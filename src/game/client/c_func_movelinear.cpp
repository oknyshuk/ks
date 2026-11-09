// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basedoor.h"

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
