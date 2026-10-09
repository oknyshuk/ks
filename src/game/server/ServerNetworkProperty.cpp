// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "ServerNetworkProperty.h"
#include "gameinterface.h"

extern CTimedEventMgr g_NetworkPropertyEventMgr;


//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CServerNetworkProperty::CServerNetworkProperty()
{
	Init( nullptr );
}


CServerNetworkProperty::~CServerNetworkProperty()
{
	/* Free our transmit proxy.
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->Release();
	}*/

	engine->CleanUpEntityClusterList( &m_PVSInfo );

	// remove the attached edict if it exists
	DetachEdict();
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CServerNetworkProperty::Init( CBaseEntity *pEntity )
{
	// NOTE: We're in pEntity's constructor so we can't call virtual methods of pEntity here
	m_pPev = nullptr;
	m_pOuter = pEntity;
	m_pServerClass = nullptr;
//	m_pTransmitProxy = NULL;
	m_bPendingStateChange = false;
	m_PVSInfo.m_nClusterCount = 0;
	m_TimerEvent.Init( &g_NetworkPropertyEventMgr, this );
}

void CServerNetworkProperty::CacheServerClass()
{
	m_pServerClass = m_pOuter->GetServerClass();
}


//-----------------------------------------------------------------------------
// Connects, disconnects edicts
//-----------------------------------------------------------------------------
void CServerNetworkProperty::AttachEdict( edict_t *pRequiredEdict )
{
	contract_assert( !m_pPev );

	// see if there is an edict allocated for it, otherwise get one from the engine
	if ( !pRequiredEdict )
	{
		pRequiredEdict = engine->CreateEdict();
	}

	m_pPev = pRequiredEdict;
	m_pPev->SetEdict( GetBaseEntity(), true );
}

void CServerNetworkProperty::DetachEdict()
{
	if ( m_pPev )
	{
		m_pPev->SetEdict( nullptr, false );
		engine->RemoveEdict( m_pPev );
		m_pPev = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Entity handles
//-----------------------------------------------------------------------------
IHandleEntity *CServerNetworkProperty::GetEntityHandle( )
{
	return m_pOuter;
}

void CServerNetworkProperty::Release()
{
	delete m_pOuter;
}


//-----------------------------------------------------------------------------
// Returns the network parent
//-----------------------------------------------------------------------------
CServerNetworkProperty* CServerNetworkProperty::GetNetworkParent()
{
	CBaseEntity *pParent = m_hParent.Get();
	return pParent ? pParent->NetworkProp() : nullptr;
}


//-----------------------------------------------------------------------------
// Marks for deletion
//-----------------------------------------------------------------------------
void CServerNetworkProperty::MarkForDeletion()
{
	m_pOuter->AddEFlags( EFL_KILLME );
}

bool CServerNetworkProperty::IsMarkedForDeletion() const
{
	return ( m_pOuter->GetEFlags() & EFL_KILLME ) != 0;
}


//-----------------------------------------------------------------------------
// PVS information
//-----------------------------------------------------------------------------
void CServerNetworkProperty::RecomputePVSInformation()
{
	if ( m_pPev && ( ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) != 0 ) )
	{
		m_pPev->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION;
		engine->BuildEntityClusterList( edict(), &m_PVSInfo );
	}
}


//-----------------------------------------------------------------------------
// Serverclass
//-----------------------------------------------------------------------------
ServerClass* CServerNetworkProperty::GetServerClass()
{
	return m_pServerClass;
}

const char* CServerNetworkProperty::GetClassName() const
{
	return STRING(m_pOuter->m_iClassname);
}


//-----------------------------------------------------------------------------
// Transmit proxies
/*-----------------------------------------------------------------------------
void CServerNetworkProperty::SetTransmitProxy( CBaseTransmitProxy *pProxy )
{
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->Release();
	}

	m_pTransmitProxy = pProxy;
	
	if ( m_pTransmitProxy )
	{
		m_pTransmitProxy->AddRef();
	}
}*/

//-----------------------------------------------------------------------------
// PVS rules
//-----------------------------------------------------------------------------
bool CServerNetworkProperty::IsInPVS( const edict_t *pRecipient, const void *pvs, int pvssize )
{
	RecomputePVSInformation();

	// ignore if not touching a PV leaf
	// negative leaf count is a node number
	// If no pvs, add any entity

	contract_assert( pvs && ( edict() != pRecipient ) );

	unsigned char *pPVS = ( unsigned char * )pvs;
	
	if ( m_PVSInfo.m_nClusterCount < 0 )   // too many clusters, use headnode
	{
		return ( engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pvssize ) != 0);
	}
	
	for ( int i = 0; i < m_PVSInfo.m_nClusterCount; i++ )
	{
		if (pPVS[m_PVSInfo.m_pClusters[i] >> 3] & (1 << (m_PVSInfo.m_pClusters[i] & 7) ))
			return true;
	}

	return false;		// not visible
}


//-----------------------------------------------------------------------------
// PVS: this function is called a lot, so it avoids function calls
//-----------------------------------------------------------------------------
bool CServerNetworkProperty::IsInPVS( const CCheckTransmitInfo *pInfo )
{
	// PVS data must be up to date
	contract_assert( !m_pPev || ( ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) == 0 ) );
	
	int i;

	// Early out if the areas are connected
	if ( !m_PVSInfo.m_nAreaNum2 )
	{
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || engine->CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
				break;
		}
	}
	else
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || clientArea == m_PVSInfo.m_nAreaNum2 )
				break;

			if ( engine->CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
				break;

			if ( engine->CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum2 ) )
				break;
		}
	}

	if ( i == pInfo->m_AreasNetworked )
	{
		// areas not connected
		return false;
	}

	// ignore if not touching a PV leaf
	// negative leaf count is a node number
	// If no pvs, add any entity

	contract_assert( edict() != pInfo->m_pClientEnt );

	unsigned char *pPVS = ( unsigned char * )pInfo->m_PVS;
	
	if ( m_PVSInfo.m_nClusterCount < 0 )   // too many clusters, use headnode
	{
		return (engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pInfo->m_nPVSSize ) != 0);
	}
	
	for ( i = m_PVSInfo.m_nClusterCount; --i >= 0; )
	{
		int nCluster = m_PVSInfo.m_pClusters[i];
		if ( ((int)(pPVS[nCluster >> 3])) & ( 1 << ( nCluster & 7 ) ) )
			return true;
	}

	return false;		// not visible

}


void CServerNetworkProperty::SetUpdateInterval( float val )
{
	if ( val == 0 )
		m_TimerEvent.StopUpdates();
	else
		m_TimerEvent.SetUpdateInterval( val );
}


void CServerNetworkProperty::FireEvent()
{
	// Our timer went off. If our state has changed in the background, then 
	// trigger a state change in the edict.
	if ( m_bPendingStateChange )
	{
		m_pPev->StateChanged();
		m_bPendingStateChange = false;
	}
}
