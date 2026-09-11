//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "sendproxy.h"
#include "ragdoll_shared.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_RagdollManager", .base = false } ]]
      CRagdollManager : public CBaseEntity
{
public:
	DECLARE_CLASS( CRagdollManager, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CRagdollManager();

	virtual void	Activate();
	virtual int		UpdateTransmitState();

	[[= ks::reflect::Input{ .name = "SetMaxRagdollCount", .type = FIELD_INTEGER } ]] void InputSetMaxRagdollCount(inputdata_t &data);

	int DrawDebugTextOverlays(void);

public:

	void UpdateCurrentMaxRagDollCount();

	CNetworkVar( int,  m_iCurrentMaxRagdollCount, [[= ks::reflect::Net{ .bits = 6 } ]] );

	[[= ks::reflect::Key{ .name = "MaxRagdollCount" } ]] int m_iMaxRagdollCount;

	[[= ks::reflect::Key{ .name = "SaveImportant" } ]] bool m_bSaveImportant;
};


IMPLEMENT_REFLECT_SERVERCLASS( CRagdollManager, DT_RagdollManager )

LINK_ENTITY_TO_CLASS( game_ragdoll_manager, CRagdollManager );

IMPLEMENT_REFLECT_DATAMAP( CRagdollManager )

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
CRagdollManager::CRagdollManager( void )
{
	m_iMaxRagdollCount = -1;
	m_iCurrentMaxRagdollCount = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pInfo - 
// Output : int
//-----------------------------------------------------------------------------
int CRagdollManager::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRagdollManager::Activate()
{
	BaseClass::Activate();
	UpdateCurrentMaxRagDollCount();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRagdollManager::UpdateCurrentMaxRagDollCount()
{
	m_iCurrentMaxRagdollCount = m_iMaxRagdollCount;
	s_RagdollLRU.SetMaxRagdollCount( m_iCurrentMaxRagdollCount );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRagdollManager::InputSetMaxRagdollCount(inputdata_t &inputdata)
{
	m_iMaxRagdollCount = inputdata.value.Int();
	UpdateCurrentMaxRagDollCount();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool RagdollManager_SaveImportant( CAI_BaseNPC *pNPC )
{

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CRagdollManager::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print max ragdoll count
		Q_snprintf(tempstr,sizeof(tempstr),"max ragdoll count: %d", m_iCurrentMaxRagdollCount.Get());
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

