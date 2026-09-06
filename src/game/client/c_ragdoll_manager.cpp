//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "ragdoll_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
