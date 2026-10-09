// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

class [[= ks::reflect::NetTable{ .name = "DT_FuncRotating" } ]]
      [[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
      [[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 0 }>{} ]]
      [[= ks::reflect::From<"m_flSimulationTime", ks::reflect::Net{ .enc = ks::reflect::WireEnc::Int }, RecvProxy_SimulationTime>{} ]]
      [[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 1 }>{} ]]
      [[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 2 }>{} ]]
      C_FuncRotating : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncRotating, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FuncRotating();

private:
};

extern void RecvProxy_SimulationTime( const CRecvProxyData *pData, void *pStruct, void *pOut );

IMPLEMENT_REFLECT_CLIENTCLASS( C_FuncRotating, DT_FuncRotating, CFuncRotating )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_FuncRotating::C_FuncRotating()
{
}
