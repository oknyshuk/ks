// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "ragdoll_shared.h"

class [[= ks::reflect::NetTable{ .name = "DT_RagdollManager", .base = false } ]]
      C_RagdollManager : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_RagdollManager, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_RagdollManager();

// C_BaseEntity overrides.
public:

	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:

	[[= ks::reflect::Net{} ]] int		m_iCurrentMaxRagdollCount;
};

IMPLEMENT_REFLECT_CLIENTCLASS( C_RagdollManager, DT_RagdollManager, CRagdollManager )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
C_RagdollManager::C_RagdollManager()
{
	m_iCurrentMaxRagdollCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_RagdollManager::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	s_RagdollLRU.SetMaxRagdollCount( m_iCurrentMaxRagdollCount );
}
