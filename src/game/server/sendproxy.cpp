// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) Valve Corporation

module;
#include <math.h>
#include <string.h>
#include "dt_send.h"
#include "edict.h"
#include "networkvar.h"
#include "studio.h"
#include "server_class.h"
#include "declare_class.h"
#include "eiface.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "util.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "closedcaptions.h"
#include "filesystem.h"
#include "irecipientfilter.h"
#include "soundchars.h"
#include "tier2/tier2_logging.h"
#include "usermessages_schema.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.math.mathlib;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :vscript;
import :fish;
import :sharedvar;
import :baseentity;
import :player;
import :nav_4;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.vprof;
import ks.game.entity_cast;
import ks.math.compressed_vector;
import ks.math.ssemath;
import ks.math.vector;
import ks.tier1.strtools;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.tier1.convar;
import ks.core.dbg;
import ks.core.platform;
import ks.math.vmatrix;
import ks.tier1.keyvalues;
import ks.math.vector2d;
import ks.core.cmdline;
import ks.core.color;
import ks.core.logging;
import ks.core.scratcharena;
import ks.tier1.cvar;
import ks.tier1.iconvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.intern_pool;
import ks.tier1.registry;

class CBaseEntity;
class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;

// Purpose: implements various common send proxies

void SendProxy_Color32ToInt( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	color32 *pIn = (color32*)pData;
	*((unsigned int*)&pOut->m_Int) = ((unsigned int)pIn->r << 24) | ((unsigned int)pIn->g << 16) | ((unsigned int)pIn->b << 8) | ((unsigned int)pIn->a);
}

void SendProxy_EHandleToInt( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	CBaseHandle *pHandle = (CBaseHandle*)pVarData;

	if ( pHandle && EntityFromHandle( *pHandle ) )
	{
		int iSerialNum = pHandle->GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
		pOut->m_Int = pHandle->GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
	}
	else
	{
		pOut->m_Int = INVALID_NETWORKED_EHANDLE_VALUE;
	}
}

void SendProxy_IntAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	int *pInt = (int *)pVarData;

	pOut->m_Int = (*pInt) + 1;
}

void SendProxy_ShortAddOne( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	short *pInt = (short *)pVarData;

	pOut->m_Int = (*pInt) + 1;
}

SendProp SendPropBool(
	const char *pVarName,
	int offset,
	int sizeofVar )
{
	contract_assert( sizeofVar == sizeof( bool ) );
	return SendPropInt( pVarName, offset, sizeofVar, 1, SPROP_UNSIGNED );
}

SendProp SendPropEHandle(
	const char *pVarName,
	int offset,
	int flags,
	int sizeofVar,
	SendVarProxyFn proxyFn )
{
	return SendPropInt( pVarName, offset, sizeofVar, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED|flags, proxyFn );
}

SendProp SendPropIntWithMinusOneFlag( const char *pVarName, int offset, int sizeofVar, int nBits, SendVarProxyFn proxyFn )
{
	return SendPropInt( pVarName, offset, sizeofVar, nBits, SPROP_UNSIGNED, proxyFn );
}

//-----------------------------------------------------------------------------
// Purpose: Proxy that only sends data to team members
// Input  : *pStruct - 
//			*pData - 
//			*pOut - 
//			objectID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void* SendProxy_OnlyToTeam( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	CBaseEntity *pEntity = (CBaseEntity*)pStruct;
	if ( pEntity )
	{
		CTeam *pTeam = pEntity->GetTeam();
		if ( pTeam )
		{
			pRecipients->ClearAllRecipients();
			for ( int i=0; i < pTeam->GetNumPlayers(); i++ )
				pRecipients->SetRecipient( pTeam->GetPlayer( i )->GetClientIndex() );
		
			return (void*)pVarData;
		}
	}

	return nullptr;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_OnlyToTeam );

static constexpr auto TIME_BITS = 24;

// This table encodes edict data.
static void SendProxy_Time( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID )
{
	float clock_base = std::floor( gpGlobals->curtime );
	float t = *( float * )pVarData;
	float dt = t - clock_base;
	int addt = Floor2Int( 1000.0f * dt + 0.5f );
	// TIME_BITS bits gives us TIME_BITS-1 bits plus sign bit
	int maxoffset = 1 << ( TIME_BITS - 1);

	addt = clamp( addt, -maxoffset, maxoffset );

	pOut->m_Int = addt;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVarName - 
//			sizeofVar - 
//			flags - 
//			pId - 
// Output : SendProp
//-----------------------------------------------------------------------------
SendProp SendPropTime(
	const char *pVarName,
	int offset,
	int sizeofVar )
{
//	return SendPropInt( pVarName, offset, sizeofVar, TIME_BITS, 0, SendProxy_Time );
	// FIXME:  Re-enable above when it doesn't cause lots of deltas
	return SendPropFloat( pVarName, offset, sizeofVar, -1, SPROP_NOSCALE );
}

void SendProxy_StringT_To_String( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID )
{
	string_t &str = *((string_t*)pVarData);
	pOut->m_pString = STRING( str );
}

SendProp SendPropStringT( const char *pVarName, int offset, int sizeofVar )
{
	// Make sure it's the right type.
	contract_assert( sizeofVar == sizeof( string_t ) );

	return SendPropString( pVarName, offset, DT_MAX_STRING_BUFFERSIZE, 0, SendProxy_StringT_To_String );
}

void SendProxyRecipients_SetOnly( CSendProxyRecipients *pRecipients, int iClient )
{
	pRecipients->m_Bits.reset();
	pRecipients->SetRecipient( iClient );

	CBaseEntity *pEntity = CBaseEntity::Instance( iClient + 1 );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( pEntity );
		if ( pPlayer->IsSplitScreenPlayer() )
		{
			if ( pPlayer->GetSplitScreenPlayerOwner() )
			{
				pRecipients->SetRecipient( pPlayer->GetSplitScreenPlayerOwner()->entindex() - 1 );
			}
			else
			{
				contract_assert( !"CSendProxyRecipients::SetOnly:  NULL pPlayer->GetSplitScreenPlayerOwner()" );
			}
		}
		else
		{
			std::vector< CHandle< CBasePlayer> > &list = pPlayer->GetSplitScreenPlayers();
			for ( int i = 0; i < std::ssize( list ); ++i )
			{
				CBasePlayer *pSplitPlayer = list[ i ];
				if ( !pSplitPlayer )
				{
					continue;
				}

				int iEntIndex = pSplitPlayer->entindex();
				pRecipients->SetRecipient( iEntIndex - 1 );
			}
		}
	}
}

void SendProxyRecipients_ExcludeOnly( CSendProxyRecipients *pRecipients, int iClient )
{
	pRecipients->m_Bits.set();
	pRecipients->ClearRecipient( iClient );
	CBaseEntity *pEntity = CBaseEntity::Instance( iClient + 1 );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( pEntity );
		if ( pPlayer->IsSplitScreenPlayer() )
		{
			if ( pPlayer->GetSplitScreenPlayerOwner() )
			{
				pRecipients->ClearRecipient( pPlayer->GetSplitScreenPlayerOwner()->entindex() - 1 );
			}
		}
		else
		{
			std::vector< CHandle< CBasePlayer> > &list = pPlayer->GetSplitScreenPlayers();
			for ( int i = 0; i < std::ssize( list ); ++i )
			{
				CBasePlayer *pSplitPlayer = list[ i ];
				if ( !pSplitPlayer )
				{
					continue;
				}

				int iEntIndex = pSplitPlayer->entindex();
				pRecipients->ClearRecipient( iEntIndex - 1 );
			}
		}
	}
}

class CAnimationLayer;

using namespace ::ks::reflect;

// -----------------------------------------------------------------------------
// CSequenceTransitioner implementation.
// -----------------------------------------------------------------------------

void CSequenceTransitioner::CheckForSequenceChange( 
	CStudioHdr *hdr,
	int nCurSequence, 
	bool bForceNewSequence,
	bool bInterpolate )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	// FIXME?: this should detect that what's been asked to be drawn isn't what was expected
	// due to not only sequence change, by frame index, rate, or whatever.  When that happens, 
	// it should insert the previous rules.

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	if (currentblend->m_flLayerAnimtime && 
		(currentblend->GetSequence() != nCurSequence || bForceNewSequence ))
	{
		if ( nCurSequence < 0 || nCurSequence >= hdr->GetNumSeq() )
		{
			// remove all entries
			m_animationQueue.clear();
		}
		else
		{
			mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( nCurSequence );
			// sequence changed
			if ((seqdesc.flags & STUDIO_SNAP) || !bInterpolate )
			{
				// remove all entries
				m_animationQueue.clear();
			}
			else
			{
				mstudioseqdesc_t &prevseqdesc = hdr->pSeqdesc( currentblend->GetSequence() );
				currentblend->m_flLayerFadeOuttime = std::min( prevseqdesc.fadeouttime, seqdesc.fadeintime );
				/*
				// clip blends to time remaining
				if ( !IsSequenceLooping(hdr, currentblend->GetSequence()) )
				{
					float length = Studio_Duration( hdr, currentblend->GetSequence(), flPoseParameter ) / currentblend->GetPlaybackRate();
					float timeLeft = (1.0 - currentblend->GetCycle()) * length;
					if (timeLeft < currentblend->m_flLayerFadeOuttime)
						currentblend->m_flLayerFadeOuttime = timeLeft;
				}
				*/
			}
			if ( std::ssize( m_animationQueue ) > 2 )
				m_animationQueue.erase( m_animationQueue.begin(), m_animationQueue.begin() + 1 );
		}
		// push previously set sequence
		m_animationQueue.emplace_back();
		currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];
	}

	currentblend->SetSequence( -1 );
	currentblend->m_flLayerAnimtime = 0.0;
	currentblend->m_flLayerFadeOuttime = 0.0;
}

void CSequenceTransitioner::UpdateCurrent( 
	CStudioHdr *hdr,
	int nCurSequence, 
	float flCurCycle,
	float flCurPlaybackRate,
	float flCurTime )
{
	// sequence may be set before model is initialized
	if ( hdr == nullptr)
		return;

	if (m_animationQueue.empty())
	{
		m_animationQueue.emplace_back();
	}

	CAnimationLayer *currentblend = &m_animationQueue[std::ssize( m_animationQueue )-1];

	// keep track of current sequence
	currentblend->SetSequence( nCurSequence );
	currentblend->m_flLayerAnimtime = flCurTime;
	currentblend->SetCycle( flCurCycle );
	currentblend->SetPlaybackRate( flCurPlaybackRate );

	// calc blending weights for previous sequences
	int i;
	for (i = 0; i < std::ssize( m_animationQueue ) - 1;)
	{
 		float s = m_animationQueue[i].GetFadeout( flCurTime );

		if (s > 0)
		{
			m_animationQueue[i].SetWeight( s );
			i++;
		}
		else
		{
			m_animationQueue.erase( m_animationQueue.begin() + i );
		}
	}
}

class CBaseEntity;
class CTimedEventMgr;

using namespace ::ks::reflect;

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

//-----------------------------------------------------------------------------
// inline methods // TODOMO does inline work on virtual functions ?
//-----------------------------------------------------------------------------
CBaseNetworkable* CServerNetworkProperty::GetBaseNetworkable()
{
	return nullptr;
}

IServerUnknown* CServerNetworkProperty::GetBaseEntity()
{
	return EntityCast<IServerUnknown>( m_pOuter );
}

CBaseEntity *CServerNetworkProperty::GetOuter()
{
	return m_pOuter;
}

PVSInfo_t *CServerNetworkProperty::GetPVSInfo()
{
	return &m_PVSInfo;
}

//-----------------------------------------------------------------------------
// Marks the PVS information dirty
//-----------------------------------------------------------------------------
void CServerNetworkProperty::MarkPVSInformationDirty()
{
	if ( m_pPev )
	{
		m_pPev->m_fStateFlags |= FL_EDICT_DIRTY_PVS_INFORMATION;
	}
}

//-----------------------------------------------------------------------------
// Sets/gets the network parent
//-----------------------------------------------------------------------------
void CServerNetworkProperty::SetNetworkParent( EHANDLE hParent )
{
	m_hParent = hParent;
}

//-----------------------------------------------------------------------------
// Methods related to the net state mgr
//-----------------------------------------------------------------------------
void CServerNetworkProperty::NetworkStateForceUpdate()
{ 
	if ( m_pPev )
		m_pPev->StateChanged();
}

void CServerNetworkProperty::NetworkStateChanged()
{ 
	// If we're using the timer, then ignore this call.
	if ( m_TimerEvent.IsRegistered() )
	{
		// If we're waiting for a timer event, then queue the change so it happens
		// when the timer goes off.
		m_bPendingStateChange = true;
	}
	else
	{
		if ( m_pPev )
			m_pPev->StateChanged();
	}
}

void CServerNetworkProperty::NetworkStateChanged( unsigned short varOffset )
{ 
	// If we're using the timer, then ignore this call.
	if ( m_TimerEvent.IsRegistered() )
	{
		// If we're waiting for a timer event, then queue the change so it happens
		// when the timer goes off.
		m_bPendingStateChange = true;
	}
	else
	{
		if ( m_pPev )
			m_pPev->StateChanged( varOffset );
	}
}

//-----------------------------------------------------------------------------
// Methods to get the entindex + edict
//-----------------------------------------------------------------------------
int CServerNetworkProperty::entindex() const
{
	return ENTINDEX( m_pPev );
}

edict_t* CServerNetworkProperty::GetEdict() const
{
	// This one's virtual, that's why we have to two other versions
	return m_pPev;
}

edict_t *CServerNetworkProperty::edict()
{
	return m_pPev;
}

const edict_t *CServerNetworkProperty::edict() const
{
	return m_pPev;
}

//-----------------------------------------------------------------------------
// Sets the edict pointer (for swapping edicts)
//-----------------------------------------------------------------------------
void CServerNetworkProperty::SetEdict( edict_t *pEdict )
{
	m_pPev = pEdict;
}

int CServerNetworkProperty::AreaNum() const
{
	const_cast<CServerNetworkProperty*>(this)->RecomputePVSInformation();
	return m_PVSInfo.m_nAreaNum;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Shadow control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class CShadowControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ShadowControl", .base = false } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CShadowControl, CBaseEntity );

	CShadowControl();

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState_Impl();
	[[= ks::reflect::Input{ .name = "SetAngles", .type = FIELD_STRING } ]] void InputSetAngles( inputdata_t &inputdata );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkVector( m_shadowDirection, [[= ks::reflect::Net{ .bits = -1, .flags = SPROP_NOSCALE, .enc = ks::reflect::WireEnc::Vector } ]] [[= ks::reflect::Key{ .name = "direction", .input = true } ]] );
	CNetworkColor32( m_shadowColor, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_Color32ToInt32, ks::reflect::WireSide::Send>{} ]] [[= ks::reflect::Key{ .name = "color", .input = true } ]] );
	CNetworkVar( float, m_flShadowMaxDist, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "distance" } ]] [[= ks::reflect::Key{ .name = "SetDistance", .input = true } ]] );
	CNetworkVar( bool, m_bDisableShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "disableallshadows" } ]] [[= ks::reflect::Key{ .name = "SetShadowsDisabled", .input = true } ]] );
	CNetworkVar( bool, m_bEnableLocalLightShadows, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "enableshadowsfromlocallights" } ]] [[= ks::reflect::Key{ .name = "SetShadowsFromLocalLightsEnabled", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(shadow_control, CShadowControl);

IMPLEMENT_REFLECT_DATAMAP( CShadowControl )

IMPLEMENT_REFLECT_SERVERCLASS( CShadowControl, DT_ShadowControl )

CShadowControl::CShadowControl()
{
	m_shadowDirection.Init( 0.2, 0.2, -2 );
	m_flShadowMaxDist = 50.0f;
	m_shadowColor.Init( 64, 64, 64, 0 );
	m_bDisableShadows = false;
	m_bEnableLocalLightShadows = false;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CShadowControl::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CShadowControl::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_shadowColor = tmp;
		return true;
	}

	if ( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );
		if (angles == vec3_angle)
		{
			angles.Init( 80, 30, 0 );
		}
		Vector vForward;
		AngleVectors( angles, &vForward );
		m_shadowDirection = vForward;
		return true;
	}

	// For backward compatibility...
	if ( FStrEq( szKeyName, "direction" ) )
	{
		// Only use this if angles haven't been set...
		if ( fabs(m_shadowDirection->LengthSqr() - 1.0f) > 1e-3 )
		{
			Vector vTemp;
			UTIL_StringToVector( vTemp.Base(), szValue );
			m_shadowDirection = vTemp;
		}
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CShadowControl::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CShadowControl::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandSimTimer, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatchBase, CSimpleSimTimer )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CStopwatch, CStopwatchBase )

IMPLEMENT_REFLECT_DATAMAP_SIMPLE_( CRandStopwatch, CStopwatchBase )

//-----------------------------------------------------------------------------

float CSimpleSimTimer::GetRemaining() const
{
	float result = m_next - gpGlobals->curtime;
	if (result < 0 )
		return 0;
	return result;
}

void CSimTimer::Reset( float interval)
{
	if ( interval == -1.0 )
	{
		m_next = gpGlobals->curtime + m_interval;
	}
	else
	{
		m_next = gpGlobals->curtime + interval;
	}
}

void CRandSimTimer::Reset()
{
	if ( m_maxInterval == 0 )
		m_next = gpGlobals->curtime + m_minInterval;
	else
		m_next = gpGlobals->curtime + random_valve->RandomFloat( m_minInterval, m_maxInterval );
}

void CSimpleStopwatch::Start( float minCountdown, float maxCountdown)
{
	m_fIsRunning = true;
	CSimpleSimTimer::Set( minCountdown, maxCountdown );
}

void CStopwatch::Start( float intervalOverride)
{
	m_fIsRunning = true;
	m_next = gpGlobals->curtime + intervalOverride;
}

void CRandStopwatch::Start( float minOverride, float maxOverride)
{
	m_fIsRunning = true;
	if ( maxOverride == 0 )
		m_next = gpGlobals->curtime + minOverride;
	else
		m_next = gpGlobals->curtime + random_valve->RandomFloat( minOverride, maxOverride );
}

bool CThinkOnceSemaphore::EnterThink()
{
	if ( m_lastTime == gpGlobals->curtime )
		return false;
	m_lastTime = gpGlobals->curtime;
	return true;
}

bool CThinkOnceSemaphore::DidThink() const
{
	return ( gpGlobals->curtime == m_lastTime );

}

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Simple entity to switch the map's 2D skybox texture when triggered.

//-----------------------------------------------------------------------------
// CSkyboxSwapper
//-----------------------------------------------------------------------------
class CSkyboxSwapper : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CSkyboxSwapper, CServerOnlyPointEntity );
public:
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	void Precache_Impl( void );

	[[= ks::reflect::Input{ .name = "Trigger", .type = FIELD_VOID } ]] void InputTrigger( inputdata_t &inputdata );

protected:
	[[= ks::reflect::Key{ .name = "SkyboxName" } ]] string_t m_iszSkyboxName;
};

LINK_ENTITY_TO_CLASS(skybox_swapper, CSkyboxSwapper);

IMPLEMENT_REFLECT_DATAMAP( CSkyboxSwapper )

//-----------------------------------------------------------------------------
// Purpose: not much
//-----------------------------------------------------------------------------
void CSkyboxSwapper::Spawn_Impl( void )
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Precache the skybox materials.
//-----------------------------------------------------------------------------
void CSkyboxSwapper::Precache_Impl( void )
{
	if ( V_strlen( m_iszSkyboxName.ToCStr() ) == 0 )
	{
		Warning( "skybox_swapper (%s) has no skybox specified!\n", GetEntityName().ToCStr() );
		return;
	}

	char  name[ MAX_PATH ];
	const char *skyboxsuffix[ 6 ] = { "rt", "bk", "lf", "ft", "up", "dn" };
	for ( int i = 0; i < 6; i++ )
	{
		V_snprintf( name, sizeof( name ), "skybox/%s%s", m_iszSkyboxName.ToCStr(), skyboxsuffix[i] );
		PrecacheMaterial( name );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that triggers the skybox swap.
//-----------------------------------------------------------------------------
void CSkyboxSwapper::InputTrigger( inputdata_t &inputdata )
{
	static ConVarRef skyname( "sv_skyname", false );
	if ( !skyname.IsValid() )
	{
		Warning( "skybox_swapper (%s) trigger input failed - cannot find 'sv_skyname' convar!\n", GetEntityName().ToCStr() );
		return;
	}
	skyname.SetValue( m_iszSkyboxName.ToCStr() );
}

class CSkyCamera;
struct inputdata_t;

using namespace ::ks::reflect;

// automatically hooks in the system's callbacks
CEntityClassList<CSkyCamera> g_SkyList;
template <> CSkyCamera *CEntityClassList<CSkyCamera>::m_pClassList = nullptr;

CHandle<CSkyCamera> g_hActiveSkybox = INVALID_EHANDLE;

//-----------------------------------------------------------------------------
// Retrives the current skycamera
//-----------------------------------------------------------------------------
CSkyCamera*	GetCurrentSkyCamera()
{
	if (g_hActiveSkybox.Get() == nullptr)
	{
		g_hActiveSkybox = GetSkyCameraList();
	}
	return g_hActiveSkybox.Get();
}

CSkyCamera*	GetSkyCameraList()
{
	return g_SkyList.m_pClassList;
}

//=============================================================================

LINK_ENTITY_TO_CLASS( sky_camera, CSkyCamera );

IMPLEMENT_REFLECT_DATAMAP( CSkyCamera )

//-----------------------------------------------------------------------------
// List of maps in HL2 that we must apply our skybox fog fixup hack to
//-----------------------------------------------------------------------------
static const char *s_pBogusFogMaps[] =
{
	"d1_canals_01",
	"d1_canals_01a",
	"d1_canals_02",
	"d1_canals_03",
	"d1_canals_09",
	"d1_canals_10",
	"d1_canals_11",
	"d1_canals_12",
	"d1_canals_13",
	"d1_eli_01",
	"d1_trainstation_01",
	"d1_trainstation_03",
	"d1_trainstation_04",
	"d1_trainstation_05",
	"d1_trainstation_06",
	"d3_c17_04",
	"d3_c17_11",
	"d3_c17_12",
	"d3_citadel_01",
	nullptr
};

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSkyCamera::CSkyCamera()
{
	g_SkyList.Insert( this );
	m_skyboxData.fog.maxdensity = 1.0f;
	m_skyboxData.fog.HDRColorScale = 1.0f;
}

CSkyCamera::~CSkyCamera()
{
	g_SkyList.Remove( this );
}

void CSkyCamera::Spawn_Impl( void ) 
{ 
	m_skyboxData.origin = GetLocalOrigin();
	m_skyboxData.area = engine->GetArea( m_skyboxData.origin );
	
	Precache();
}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CSkyCamera::Activate_Impl( ) 
{
	BaseClass::Activate_Impl();

	if ( m_bUseAngles )
	{
		AngleVectors( GetAbsAngles(), &m_skyboxData.fog.dirPrimary.GetForModify() );
		m_skyboxData.fog.dirPrimary.GetForModify() *= -1.0f; 
	}

}

//-----------------------------------------------------------------------------
// Activate!
//-----------------------------------------------------------------------------
void CSkyCamera::InputActivateSkybox( inputdata_t &inputdata )
{
	g_hActiveSkybox = this;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements the big scary boom-boom machine Antlions fear.

static constexpr auto SLIDESHOW_LIST_BUFFER_MAX = 8192;

struct SlideKeywordList_t
{
	char	szSlideKeyword[64];
};

class CSlideshowDisplay : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_SlideshowDisplay" } ]]
	static constexpr bool ks_class = true;

public:

	DECLARE_CLASS( CSlideshowDisplay, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual ~CSlideshowDisplay();

	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	int  UpdateTransmitState_Impl();
	void SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways );

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void OnRestore_Impl( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetDisplayText", .type = FIELD_STRING } ]] void	InputSetDisplayText( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RemoveAllSlides", .type = FIELD_VOID } ]] void	InputRemoveAllSlides( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddSlides", .type = FIELD_STRING } ]] void	InputAddSlides( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetMinSlideTime", .type = FIELD_FLOAT } ]] void	InputSetMinSlideTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxSlideTime", .type = FIELD_FLOAT } ]] void	InputSetMaxSlideTime( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetCycleType", .type = FIELD_INTEGER } ]] void	InputSetCycleType( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNoListRepeats", .type = FIELD_BOOLEAN } ]] void	InputSetNoListRepeats( inputdata_t &inputdata );

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );
	void BuildSlideShowImagesList( void );

private:

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );

	CNetworkString( m_szDisplayText, 128, [[= ks::reflect::Net{} ]] );

	CNetworkString( m_szSlideshowDirectory, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "directory" } ]] string_t	m_String_tSlideshowDirectory;

	std::vector< SlideKeywordList_t* >		m_SlideKeywordList;
	CNetworkArray( unsigned char, m_chCurrentSlideLists, 16, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_fMinSlideTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 20.0f } ]] [[= ks::reflect::Key{ .name = "minslidetime" } ]] );
	CNetworkVar( float, m_fMaxSlideTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 20.0f } ]] [[= ks::reflect::Key{ .name = "maxslidetime" } ]] );

	CNetworkVar( int, m_iCycleType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "cycletype" } ]] );
	CNetworkVar( bool, m_bNoListRepeats, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "nolistrepeats" } ]] );

	[[= ks::reflect::Key{ .name = "width" } ]] int		m_iScreenWidth;
	[[= ks::reflect::Key{ .name = "height" } ]] int		m_iScreenHeight;

	bool	m_bDoFullTransmit;
};

LINK_ENTITY_TO_CLASS( vgui_slideshow_display, CSlideshowDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CSlideshowDisplay )

IMPLEMENT_REFLECT_SERVERCLASS( CSlideshowDisplay, DT_SlideshowDisplay )

CSlideshowDisplay::~CSlideshowDisplay()
{
}

//-----------------------------------------------------------------------------
// Read in worldcraft data...
//-----------------------------------------------------------------------------
bool CSlideshowDisplay::KeyValue_Impl( const char *szKeyName, const char *szValue ) 
{
	//!! temp hack, until worldcraft is fixed
	// strip the # tokens from (duplicate) key names
	char *s = (char *)std::strchr( szKeyName, '#' );
	if ( s )
	{
		*s = '\0';
	}

	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

int CSlideshowDisplay::UpdateTransmitState_Impl()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CSlideshowDisplay::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
}

void CSlideshowDisplay::Spawn_Impl( void )
{
	V_strcpy( m_szSlideshowDirectory.GetForModify(), m_String_tSlideshowDirectory.ToCStr() );
	Precache();

	BaseClass::Spawn_Impl();

	m_bEnabled = false;
	
	// Clear out selected list
	m_chCurrentSlideLists.GetForModify( 0 ) = 0;	// Select all slides to begin with
	for ( int i = 1; i < 16; ++i )
		m_chCurrentSlideLists.GetForModify( i ) = (unsigned char)-1;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;
}

void CSlideshowDisplay::Precache_Impl( void )
{
	BaseClass::Precache_Impl();

	BuildSlideShowImagesList();
}

void CSlideshowDisplay::OnRestore_Impl( void )
{
	BaseClass::OnRestore_Impl();

	BuildSlideShowImagesList();

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

void CSlideshowDisplay::ScreenVisible( bool bVisible )
{
}

void CSlideshowDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

void CSlideshowDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}

void CSlideshowDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CSlideshowDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

void CSlideshowDisplay::InputSetDisplayText( inputdata_t &inputdata )
{
	V_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

void CSlideshowDisplay::InputRemoveAllSlides( inputdata_t &inputdata )
{
	// Clear out selected list
	for ( int i = 0; i < 16; ++i )
		m_chCurrentSlideLists.GetForModify( i ) = (unsigned char)-1;
}

void CSlideshowDisplay::InputAddSlides( inputdata_t &inputdata )
{
	// Find the list with the current keyword
	int iList;
	for ( iList = 0; iList < std::ssize( m_SlideKeywordList ); ++iList )
	{
		if ( V_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, inputdata.value.String() ) == 0 )
			break;
	}

	if ( iList < std::ssize( m_SlideKeywordList ) )
	{
		// Found the keyword list, so add this index to the selected lists
		int iNumCurrentSlideLists;
		for ( iNumCurrentSlideLists = 0; iNumCurrentSlideLists < 16; ++iNumCurrentSlideLists )
		{
			if ( m_chCurrentSlideLists[ iNumCurrentSlideLists ] == (unsigned char)-1 )
				break;
		}

		if ( iNumCurrentSlideLists >= 16 )
			return;

		m_chCurrentSlideLists.GetForModify( iNumCurrentSlideLists ) = iList;
	}
}

void CSlideshowDisplay::InputSetMinSlideTime( inputdata_t &inputdata )
{
	m_fMinSlideTime = inputdata.value.Float();
}

void CSlideshowDisplay::InputSetMaxSlideTime( inputdata_t &inputdata )
{
	m_fMaxSlideTime = inputdata.value.Float();
}

void CSlideshowDisplay::InputSetCycleType( inputdata_t &inputdata )
{
	m_iCycleType = inputdata.value.Int();
}

void CSlideshowDisplay::InputSetNoListRepeats( inputdata_t &inputdata )
{
	m_bNoListRepeats = inputdata.value.Bool();
}

void CSlideshowDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "slideshow_display_screen";
}

void CSlideshowDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CSlideshowDisplay::SpawnControlPanels()
{
}

void CSlideshowDisplay::RestoreControlPanels( void )
{
}

void CSlideshowDisplay::BuildSlideShowImagesList( void )
{
	FileFindHandle_t matHandle;
	char szDirectory[k_cMaxPath];
	char szMatFileName[k_cMaxPath] = {'\0'};
	char szFileBuffer[ SLIDESHOW_LIST_BUFFER_MAX ];
	char *pchCurrentLine = nullptr;

	{
		V_snprintf( szDirectory, sizeof( szDirectory ), "materials/vgui/%s/*.vmt", m_szSlideshowDirectory.Get() );
		const char *pMatFileName = g_pFullFileSystem->FindFirst( szDirectory, &matHandle );

		if ( pMatFileName )
			V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
	}

	int iSlideIndex = 0;

	while ( szMatFileName[ 0 ] )
	{
		char szFileName[k_cMaxPath];
		V_snprintf( szFileName, sizeof( szFileName ), "vgui/%s/%s", m_szSlideshowDirectory.Get(), szMatFileName );
		szFileName[ V_strlen( szFileName ) - 4 ] = '\0';

		PrecacheMaterial( szFileName );	

		// Get material keywords
		char szFullFileName[k_cMaxPath];
		V_snprintf( szFullFileName, sizeof( szFullFileName ), "materials/vgui/%s/%s", m_szSlideshowDirectory.Get(), szMatFileName );

		KeyValues *pMaterialKeys = new KeyValues( "material" );
		bool bLoaded = pMaterialKeys->LoadFromFile( g_pFullFileSystem, szFullFileName, nullptr );
		if ( bLoaded )
		{
			char szKeywords[ 256 ];
			V_strcpy( szKeywords, pMaterialKeys->GetString( "%keywords", "" ) );

			char *pchKeyword = szKeywords;

			while ( pchKeyword[ 0 ] != '\0' )
			{
				char *pNextKeyword = pchKeyword;

				// Skip commas and spaces
				while ( pNextKeyword[ 0 ] != '\0' && pNextKeyword[ 0 ] != ',' )
					++pNextKeyword;

				if ( pNextKeyword[ 0 ] != '\0' )
				{
					pNextKeyword[ 0 ] = '\0';
					++pNextKeyword;

					while ( pNextKeyword[ 0 ] != '\0' && ( pNextKeyword[ 0 ] == ',' || pNextKeyword[ 0 ] == ' ' ) )
						++pNextKeyword;
				}

				// Find the list with the current keyword
				int iList;
				for ( iList = 0; iList < std::ssize( m_SlideKeywordList ); ++iList )
				{
					if ( V_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, pchKeyword ) == 0 )
						break;
				}

				if ( iList >= std::ssize( m_SlideKeywordList ) )
				{
					// Couldn't find the list, so create it
					iList = std::ssize( m_SlideKeywordList ); m_SlideKeywordList.push_back( new SlideKeywordList_t );
					V_strcpy( m_SlideKeywordList[ iList ]->szSlideKeyword, pchKeyword );
				}

				pchKeyword = pNextKeyword;
			}
		}
		pMaterialKeys->deleteThis();
		pMaterialKeys = nullptr;

		// Find the generic list
		int iList;
		for ( iList = 0; iList < std::ssize( m_SlideKeywordList ); ++iList )
		{
			if ( V_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, "" ) == 0 )
				break;
		}

		if ( iList >= std::ssize( m_SlideKeywordList ) )
		{
			// Couldn't find the generic list, so create it
			m_SlideKeywordList.insert( m_SlideKeywordList.begin(), new SlideKeywordList_t ); iList = 0;
			V_strcpy( m_SlideKeywordList[ iList ]->szSlideKeyword, "" );
		}

		{
			const char *pMatFileName = g_pFullFileSystem->FindNext( matHandle );

			if ( pMatFileName )
				V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
			else
				szMatFileName[ 0 ] = '\0';
		}

		++iSlideIndex;
	}

	g_pFullFileSystem->FindClose( matHandle );
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

inline constexpr const char *SMOKETRAIL_ENTITYNAME = "env_smoketrail";
inline constexpr const char *SPORETRAIL_ENTITYNAME = "env_sporetrail";
inline constexpr const char *SPOREEXPLOSION_ENTITYNAME = "env_sporeexplosion";
inline constexpr const char *DUSTTRAIL_ENTITYNAME = "env_dusttrail";

//-----------------------------------------------------------------------------
//Data table
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( SmokeTrail, DT_SmokeTrail )

LINK_ENTITY_TO_CLASS(env_smoketrail, SmokeTrail);

IMPLEMENT_REFLECT_DATAMAP( SmokeTrail )

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
SmokeTrail::SmokeTrail()
{
	m_SpawnRate = 10;
	m_StartColor.GetForModify().Init(0.5, 0.5, 0.5);
	m_EndColor.GetForModify().Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_nAttachment	= 0;
	m_Opacity = 0.5f;
}

//-----------------------------------------------------------------------------
// Parse data from a map file
//-----------------------------------------------------------------------------
bool SmokeTrail::KeyValue_Impl( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "startcolor" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_StartColor.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "endcolor" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_EndColor.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "emittime" ) )
	{
		m_StopEmitTime = gpGlobals->curtime + std::atof( szValue );
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//-----------------------------------------------------------------------------
void SmokeTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SmokeTrail*
//-----------------------------------------------------------------------------
SmokeTrail* SmokeTrail::CreateSmokeTrail()
{
	CBaseEntity *pEnt = CreateEntityByName(SMOKETRAIL_ENTITYNAME);
	if(pEnt)
	{
		SmokeTrail *pSmoke = dynamic_cast<SmokeTrail*>(pEnt);
		if(pSmoke)
		{
			pSmoke->Activate();
			return pSmoke;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void SmokeTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}

//==================================================
// RocketTrail
//==================================================

//Data table
IMPLEMENT_REFLECT_SERVERCLASS( RocketTrail, DT_RocketTrail )

LINK_ENTITY_TO_CLASS( env_rockettrail, RocketTrail );

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
RocketTrail::RocketTrail()
{
	m_SpawnRate = 10;
	m_StartColor.GetForModify().Init(0.5, 0.5, 0.5);
	m_EndColor.GetForModify().Init(0,0,0);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_nAttachment	= 0;
	m_Opacity = 0.5f;
	m_flFlareScale = 1.5;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void RocketTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SmokeTrail*
//-----------------------------------------------------------------------------
RocketTrail* RocketTrail::CreateRocketTrail()
{
	CBaseEntity *pEnt = CreateEntityByName( "env_rockettrail" );
	
	if( pEnt != nullptr )
	{
		RocketTrail *pTrail = dynamic_cast<RocketTrail*>(pEnt);
		
		if( pTrail != nullptr )
		{
			pTrail->Activate();
			return pTrail;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void RocketTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}

//==================================================
// SporeTrail
//==================================================

IMPLEMENT_REFLECT_SERVERCLASS( SporeTrail, DT_SporeTrail )

LINK_ENTITY_TO_CLASS(env_sporetrail, SporeTrail);

SporeTrail::SporeTrail( void )
{
	m_vecEndColor.GetForModify().Init();

	m_flSpawnRate			= 100.0f;
	m_flParticleLifetime	= 1.0f;
	m_flStartSize			= 1.0f;
	m_flEndSize				= 0.0f;
	m_flSpawnRadius			= 16.0f;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SporeTrail*
//-----------------------------------------------------------------------------
SporeTrail* SporeTrail::CreateSporeTrail()
{
	CBaseEntity *pEnt = CreateEntityByName( SPORETRAIL_ENTITYNAME );
	
	if(pEnt)
	{
		SporeTrail *pSpore = dynamic_cast<SporeTrail*>(pEnt);
		
		if ( pSpore )
		{
			pSpore->Activate();
			return pSpore;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

//==================================================
// SporeExplosion
//==================================================

IMPLEMENT_REFLECT_SERVERCLASS( SporeExplosion, DT_SporeExplosion )

LINK_ENTITY_TO_CLASS( env_sporeexplosion, SporeExplosion );

IMPLEMENT_REFLECT_DATAMAP( SporeExplosion )

SporeExplosion::SporeExplosion( void )
{
	m_flSpawnRate			= 100.0f;
	m_flParticleLifetime	= 1.0f;
	m_flStartSize			= 1.0f;
	m_flEndSize				= 0.0f;
	m_flSpawnRadius			= 16.0f;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	m_bEmit = true;
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SporeExplosion::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	m_bEmit = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : SporeExplosion*
//-----------------------------------------------------------------------------
SporeExplosion *SporeExplosion::CreateSporeExplosion()
{
	CBaseEntity *pEnt = CreateEntityByName( SPOREEXPLOSION_ENTITYNAME );
	
	if ( pEnt )
	{
		SporeExplosion *pSpore = dynamic_cast<SporeExplosion*>(pEnt);
		
		if ( pSpore )
		{
			pSpore->Activate();
			return pSpore;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;
}

void SporeExplosion::InputEnable( inputdata_t &inputdata )
{
	m_bDontRemove = true;
	m_bDisabled = false;
	m_bEmit = true;
}

void SporeExplosion::InputDisable( inputdata_t &inputdata )
{
	m_bDontRemove = true;
	m_bDisabled = true;
	m_bEmit = false;
}

IMPLEMENT_REFLECT_SERVERCLASS( CFireTrail, DT_FireTrail )

LINK_ENTITY_TO_CLASS( env_fire_trail, CFireTrail );

void CFireTrail::Precache_Impl( void )
{
	PrecacheMaterial( "sprites/flamelet1" );
	PrecacheMaterial( "sprites/flamelet2" );
	PrecacheMaterial( "sprites/flamelet3" );
	PrecacheMaterial( "sprites/flamelet4" );
	PrecacheMaterial( "sprites/flamelet5" );
	PrecacheMaterial( "particle/particle_smokegrenade" );
	PrecacheMaterial( "particle/particle_noisesphere" );
}

//-----------------------------------------------------------------------------
// Purpose: Attach the smoke trail to an entity or point 
// Input  : index - entity that has the attachment
//			attachment - point to attach to
//-----------------------------------------------------------------------------
void CFireTrail::FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName )
{
	// For attachments
	if ( pAttachmentName && pEntity && pEntity->GetBaseAnimating() )
	{
		m_nAttachment = pEntity->GetBaseAnimating()->LookupAttachment( pAttachmentName );
	}
	else
	{
		m_nAttachment = 0;
	}

	BaseClass::FollowEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Create and return a new fire trail entity
//-----------------------------------------------------------------------------
CFireTrail *CFireTrail::CreateFireTrail( void )
{
	CBaseEntity *pEnt = CreateEntityByName( "env_fire_trail" );
	
	if ( pEnt )
	{
		CFireTrail *pTrail = dynamic_cast<CFireTrail*>(pEnt);
		
		if ( pTrail )
		{
			pTrail->Activate();
			return pTrail;
		}
		else
		{
			UTIL_Remove( pEnt );
		}
	}

	return nullptr;	
}

//-----------------------------------------------------------------------------
//Data table
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( DustTrail, DT_DustTrail )

LINK_ENTITY_TO_CLASS( env_dusttrail, DustTrail);

IMPLEMENT_REFLECT_DATAMAP( DustTrail )

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
DustTrail::DustTrail()
{
	m_SpawnRate = 10;
	m_Color.GetForModify().Init(0.5, 0.5, 0.5);
	m_ParticleLifetime = 5;
	m_StopEmitTime = 0; // Don't stop emitting particles
	m_MinSpeed = 2;
	m_MaxSpeed = 4;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 35;
	m_EndSize = 55;
	m_SpawnRadius = 2;
	m_bEmit = true;
	m_Opacity = 0.5f;
}

//-----------------------------------------------------------------------------
// Parse data from a map file
//-----------------------------------------------------------------------------
bool DustTrail::KeyValue_Impl( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		m_Color.GetForModify().Init( tmp.r / 255.0f, tmp.g / 255.0f, tmp.b / 255.0f );
		return true;
	}

	if ( FStrEq( szKeyName, "emittime" ) )
	{
		m_StopEmitTime = gpGlobals->curtime + std::atof( szValue );
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//-----------------------------------------------------------------------------
void DustTrail::SetEmit(bool bVal)
{
	m_bEmit = bVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : DustTrail*
//-----------------------------------------------------------------------------
DustTrail* DustTrail::CreateDustTrail()
{
	CBaseEntity *pEnt = CreateEntityByName(DUSTTRAIL_ENTITYNAME);
	if(pEnt)
	{
		DustTrail *pDust = dynamic_cast<DustTrail*>(pEnt);
		if(pDust)
		{
			pDust->Activate();
			return pDust;
		}
		else
		{
			UTIL_Remove(pEnt);
		}
	}

	return nullptr;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements the server side of a steam jet particle system entity.

//Networking
IMPLEMENT_REFLECT_SERVERCLASS( CSmokeStack, DT_SmokeStack )

LINK_ENTITY_TO_CLASS( env_smokestack, CSmokeStack );

//Save/restore

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSmokeStackLightInfo )

IMPLEMENT_REFLECT_DATAMAP( CSmokeStack )

//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
CSmokeStack::CSmokeStack()
{
	std::memset( &m_AmbientLight, 0, sizeof(m_AmbientLight) ); 
	memset( &m_DirLight, 0, sizeof(m_DirLight) ); 

	IMPLEMENT_NETWORKVAR_CHAIN( &m_AmbientLight );
	IMPLEMENT_NETWORKVAR_CHAIN( &m_DirLight );

	m_flTwist = 0;
	SetRenderColor( 0, 0, 0 );
	SetRenderAlpha( 255 );
	m_vWind.GetForModify().Init();
	m_WindAngle = m_WindSpeed = 0;
	m_iMaterialModel = -1;
	m_flRollSpeed = 0.0f;
}

CSmokeStack::~CSmokeStack()
{
}

void CSmokeStack::Spawn_Impl( void )
{
	if ( m_InitialState )
	{
		m_bEmit = true;
	}
}

void CSmokeStack::Activate_Impl()
{
	DetectInSkybox();

	bool bGotDirLight = false;

	// Find local lights.
	CBaseEntity *pTestEnt = nullptr;
	while ( 1 )
	{
		pTestEnt = gEntList.FindEntityByClassname( pTestEnt, PARTICLELIGHT_ENTNAME );
		if ( !pTestEnt )
			break;

		CParticleLight *pLight = (CParticleLight*)pTestEnt;
		if( !FStrEq( STRING(GetEntityName()), STRING(pLight->m_PSName) ) )
			continue;

		CSmokeStackLightInfo *pInfo = &m_AmbientLight;
		if ( pLight->m_bDirectional )
		{
			bGotDirLight = true;
			pInfo = &m_DirLight;
		}

		pInfo->m_flIntensity = pLight->m_flIntensity;
		pInfo->m_vColor = pLight->m_vColor;
		pInfo->m_vPos = pLight->GetAbsOrigin();
	}

	// Put our light colors in 0-1 space.
	m_AmbientLight.m_vColor.GetForModify() /= 255.0f;
	m_DirLight.m_vColor.GetForModify() /= 255.0f;

	BaseClass::Activate_Impl();

	// Legacy support..
	if ( m_iMaterialModel == -1 )
		m_iMaterialModel = PrecacheModel( "particle/SmokeStack.vmt" );
}

bool CSmokeStack::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if( stricmp( szKeyName, "Wind" ) == 0 )
	{
		std::sscanf( szValue, "%f %f %f", &m_vWind.GetForModify().x, &m_vWind.GetForModify().y, &m_vWind.GetForModify().z );
		return true;
	}
	else if( stricmp( szKeyName, "WindAngle" ) == 0 )
	{
		m_WindAngle = std::atoi( szValue );
		RecalcWindVector();
		return true;
	}
	else if( stricmp( szKeyName, "WindSpeed" ) == 0 )
	{
		m_WindSpeed = std::atoi( szValue );
		RecalcWindVector();
		return true;
	}
	else if ( stricmp( szKeyName, "SmokeMaterial" ) == 0 )
	{
		// Make sure we have a vmt extension.
		if ( V_stristr( szValue, ".vmt" ) )
		{
			m_strMaterialModel = AllocPooledString( szValue );
		}
		else
		{
			char str[512];
			V_snprintf( str, sizeof( str ), "%s.vmt", szValue );
			m_strMaterialModel = AllocPooledString( str );
		}
		
		const char *pName = STRING( m_strMaterialModel );
		char szStrippedName[512];

		m_iMaterialModel = PrecacheModel( pName );
		V_StripExtension( pName, szStrippedName, V_strlen(pName)+1 );

		int iLength = V_strlen( szStrippedName );
		szStrippedName[iLength-1] = '\0';

		int iCount = 1;
		char str[512];
		V_snprintf( str, sizeof( str ), "%s%d.vmt", szStrippedName, iCount );
		
		while ( filesystem->FileExists( UTIL_VarArgs( "materials/%s", str ) ) )
		{
			PrecacheModel( str );
			iCount++;
			
			V_snprintf( str, sizeof( str ), "%s%d.vmt", szStrippedName, iCount );
		}

		return true;
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}
}

void CSmokeStack::Precache_Impl()
{
	m_iMaterialModel = PrecacheModel( STRING( m_strMaterialModel ) );
	
	BaseClass::Precache_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the steam jet on/off.
//-----------------------------------------------------------------------------
void CSmokeStack::InputToggle( inputdata_t &inputdata )
{
	m_bEmit = !m_bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the steam jet.
//-----------------------------------------------------------------------------
void CSmokeStack::InputTurnOn( inputdata_t &inputdata )
{
	m_bEmit = true;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the steam jet.
//-----------------------------------------------------------------------------
void CSmokeStack::InputTurnOff( inputdata_t &inputdata )
{
	m_bEmit = false;
}

void CSmokeStack::RecalcWindVector()
{
	m_vWind = Vector(  
		std::cos( DEG2RAD( (float)m_WindAngle ) ) * m_WindSpeed, 
		std::sin( DEG2RAD( (float)m_WindAngle ) ) * m_WindSpeed,
		0 );
	
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Entities relating to in-level sound effects.
//			env_speaker: used for public address announcements over loudspeakers.
//				This tries not to drown out talking NPCs.
//			env_soundscape: controls what sound script an area uses.

// =================== ROOM SOUND FX ==========================================

// ==================== SENTENCE GROUPS, UTILITY FUNCTIONS  ======================================

int fSentencesInit = false;

// ===================== SENTENCE GROUPS, MAIN ROUTINES ========================

// given sentence group index, play random sentence for given entity.
// returns sentenceIndex - which sentence was picked 
// Ipick is only needed if you plan on stopping the sound before playback is done (see SENTENCEG_Stop). 
// sentenceIndex can be used to find the name/length of the sentence

int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch)
{
	char name[64];
	int ipick;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	ipick = engine->SentenceGroupPick( isentenceg, name, sizeof( name ) );
	if (ipick > 0 && name)
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Picks a sentence, but doesn't play it
//-----------------------------------------------------------------------------
int SENTENCEG_PickRndSz(const char *szgroupname)
{
	char name[64];
	int ipick;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
	{
		Warning( "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = engine->SentenceGroupPick(isentenceg, name, sizeof( name ));
	if (ipick >= 0 && name[0])
	{
		return SENTENCEG_Lookup( name );
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Plays a sentence by sentence index
//-----------------------------------------------------------------------------
void SENTENCEG_PlaySentenceIndex( edict_t *entity, int iSentenceIndex, float volume, soundlevel_t soundlevel, int flags, int pitch )
{
	if ( iSentenceIndex >= 0 )
	{
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, iSentenceIndex, volume, soundlevel, flags, pitch );
	}
}

int SENTENCEG_PlayRndSz(edict_t *entity, const char *szgroupname, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch)
{
	char name[64];
	int ipick;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
	{
		Warning( "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = engine->SentenceGroupPick(isentenceg, name, sizeof( name ));
	if (ipick >= 0 && name[0])
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}

	return -1;
}

// play sentences in sequential order from sentence group.  Reset after last sentence.

int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szgroupname, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch, int ipick, int freset)
{
	char name[64];
	int ipicknext;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
		return -1;

	ipicknext = engine->SentenceGroupPickSequential(isentenceg, name, sizeof( name ), ipick, freset);
	if (ipicknext >= 0 && name[0])
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}
	
	return -1;
}

// open sentences.txt, scan for groups, build rgsentenceg
// Should be called from world spawn, only works on the
// first call and is ignored subsequently.
void SENTENCEG_Init()
{
	if (fSentencesInit)
		return;

	engine->PrecacheSentenceFile( "scripts/sentences.txt" );
	fSentencesInit = true;
}

// convert sentence (sample) name to !sentencenum, return !sentencenum

int SENTENCEG_Lookup(const char *sample)
{
	return engine->SentenceIndexFromName( sample + 1 );
}

int SENTENCEG_GetIndex(const char *szrootname)
{
	return engine->SentenceGroupIndexFromName( szrootname );
}

void UTIL_RestartAmbientSounds( void )
{
	CAmbientGeneric *pAmbient = nullptr;
	while ( ( pAmbient = (CAmbientGeneric*) gEntList.FindEntityByClassname( pAmbient, "ambient_generic" ) ) != nullptr )
	{
		if (pAmbient->m_fActive )
		{
			if ( std::strstr( STRING( pAmbient->m_iszSound ), "mp3" ) )
			{
				pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
			}
			pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
		}
	}
}

// play a specific sentence over the HEV suit speaker - just pass player entity, and !sentencename

void UTIL_EmitSoundSuit(edict_t *entity, const char *sample)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = suitvolume.GetFloat();
	if (random_valve->RandomInt(0,1))
		pitch = random_valve->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
	{
		CPASAttenuationFilter filter( GetContainingEntity( entity ) );
		filter.MakeReliable();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = sample;
		ep.m_flVolume = fvol;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_nPitch = pitch;

		CBaseEntity::EmitSound( filter, ENTINDEX(entity), ep );
	}
}

// play a sentence, randomly selected from the passed in group id, over the HEV suit speaker

int UTIL_EmitGroupIDSuit(edict_t *entity, int isentenceg)
{
	float fvol;
	int pitch = PITCH_NORM;
	int sentenceIndex = -1;

	fvol = suitvolume.GetFloat();
	if (random_valve->RandomInt(0,1))
		pitch = random_valve->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
		sentenceIndex = SENTENCEG_PlayRndI(entity, isentenceg, fvol, SNDLVL_NORM, 0, pitch);

	return sentenceIndex;
}

// play a sentence, randomly selected from the passed in groupname

int UTIL_EmitGroupnameSuit(edict_t *entity, const char *groupname)
{
	float fvol;
	int pitch = PITCH_NORM;
	int sentenceIndex = -1;

	fvol = suitvolume.GetFloat();
	if (random_valve->RandomInt(0,1))
		pitch = random_valve->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
		sentenceIndex = SENTENCEG_PlayRndSz(entity, groupname, fvol, SNDLVL_NORM, 0, pitch);

	return sentenceIndex;
}

// ===================== MATERIAL TYPE DETECTION, MAIN ROUTINES ========================
// 
// Used to detect the texture the player is standing on, map the
// texture name to a material type.  Play footstep sound based
// on material type.

char TEXTURETYPE_Find( trace_t *ptr )
{
	const surfacedata_t *psurfaceData = physprops->GetSurfaceData( ptr->surface.surfaceProps );

	return psurfaceData->game.material;
}

class CBaseEntity;
class CBasePlayer;
class CRecipientFilter;
class IGameSystem;

using namespace ::ks::reflect;

// Defined in sceneentity.cpp
bool AttenuateCaption( const char *token, const Vector& listener, std::vector< Vector >& soundorigins );

static void Register_LOG_SND_EMITTERSYSTEM_Tags();
LoggingChannelID_t LOG_SND_EMITTERSYSTEM = LoggingSystem_RegisterLoggingChannel( "SndEmitterSystem", Register_LOG_SND_EMITTERSYSTEM_Tags, LCF_CONSOLE_ONLY, LS_MESSAGE );
void Register_LOG_SND_EMITTERSYSTEM_Tags()
{
LoggingSystem_AddTagToCurrentChannel( "SndEmitterSystem" );

};

ConVar sv_soundemitter_version( side_registry(), "sv_soundemitter_version", "2", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "specfies what version of soundemitter system to use\n" );

ConVar snd_prevent_ss_duplicates( side_registry(), "snd_prevent_ss_duplicates", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "switch to en/disable the prevention of splitscreen audio file duplicates\n" );

ConVar snd_sos_show_server_xmit( side_registry(), "snd_sos_show_server_xmit", "0", FCVAR_CHEAT );

ConVar sv_soundemitter_trace( side_registry(), "sv_soundemitter_trace", "-1", FCVAR_REPLICATED, "Show all EmitSound calls including their symbolic name and the actual wave file they resolved to. (-1 = for nobody, 0 = for everybody, n = for one entity)\n" );
ConVar cc_showmissing( side_registry(), "cc_showmissing", "0", FCVAR_REPLICATED, "Show missing closecaption entries." );

extern ISoundEmitterSystemBase *soundemitterbase;
static ConVar *g_pClosecaption = nullptr;

static bool g_bPermitDirectSoundPrecache = false;

static ConVar cc_norepeat( side_registry(), "cc_norepeat", "5", 0, "In multiplayer games, don't repeat captions more often than this many seconds." );

class CCaptionRepeatMgr
{
public:

	CCaptionRepeatMgr()
	{
	}

	bool CanEmitCaption( unsigned int hash );

	void Clear();

private:

	void RemoveCaptionsBefore( float t );

	struct CaptionItem_t
	{
		unsigned int	hash;
		float			realtime;

		static bool Less( const CaptionItem_t &lhs, const CaptionItem_t &rhs )
		{
			return lhs.hash < rhs.hash;
		}
	};

	std::map< unsigned int, float > m_rbCaptionHistory;
};

static CCaptionRepeatMgr g_CaptionRepeats;

void CCaptionRepeatMgr::Clear()
{
	m_rbCaptionHistory.clear();
}

bool CCaptionRepeatMgr::CanEmitCaption( unsigned int hash )
{
	// Don't cull in single player
	if ( gpGlobals->maxClients == 1 )
		return true;

	float realtime = gpGlobals->realtime;

	RemoveCaptionsBefore( realtime - cc_norepeat.GetFloat() );

	auto [it, bInserted] = m_rbCaptionHistory.try_emplace( hash, realtime );
	if ( bInserted )
	{
		return true;
	}

	float flLastEmitted = it->second;
	if ( realtime - flLastEmitted > cc_norepeat.GetFloat() )
	{
		it->second = realtime;
		return true;
	}

	return false;
}

void CCaptionRepeatMgr::RemoveCaptionsBefore( float t )
{
	std::erase_if( m_rbCaptionHistory, [t]( const auto &entry ) { return entry.second < t; } );
}

void ClearModelSoundsCache();

void WaveTrace( char const *wavname, char const *funcname )
{

	static ks::intern_pool s_WaveTrace;

	// Make sure we only show the message once
	if ( !s_WaveTrace.contains( wavname ) )
	{
		DevMsg( "%s directly referenced wave %s (should use game_sounds.txt system instead)\n", 
			funcname, wavname );
		s_WaveTrace.intern( wavname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &src - 
//-----------------------------------------------------------------------------
EmitSound_t::EmitSound_t( const CSoundParameters &src )
{
	m_nChannel = src.channel;
	m_pSoundName = src.soundname;
	m_flVolume = src.volume;
	m_SoundLevel = src.soundlevel;
	m_nFlags = 0;
	m_nPitch = src.pitch;
	m_pOrigin = 0;
	m_flSoundTime = ( src.delay_msec == 0 ) ? 0.0f : gpGlobals->curtime + ( (float)src.delay_msec / 1000.0f );
	m_pflSoundDuration = 0;
	m_bEmitCloseCaption = true;
	m_bWarnOnMissingCloseCaption = false;
	m_bWarnOnDirectWaveReference = false;
	m_nSpeakerEntity = -1;
	// if sound is tagged as version 2 or higher this will be treated as a soundentry!
	m_hSoundScriptHash = src.m_hSoundScriptHash;
	m_nSoundEntryVersion = src.m_nSoundEntryVersion;
}

void Hack_FixEscapeChars( char *str )
{
	ks::ScratchArena<> scratch;
	int len = V_strlen( str ) + 1;
	char *i = str;
	char *o = (char *)scratch.AllocBytes( len );
	char *osave = o;
	while ( *i )
	{
		if ( *i == '\\' )
		{
			switch (  *( i + 1 ) )
			{
			case 'n':
				*o = '\n';
				++i;
				break;
			default:
				*o = *i;
				break;
			}
		}
		else
		{
			*o = *i;
		}

		++i;
		++o;
	}
	*o = 0;
	V_strncpy( str, osave, len );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSoundEmitterSystem : public CBaseGameSystem
{
public:
	virtual char const *Name() { return "CSoundEmitterSystem"; }

	bool			m_bLogPrecache;
	FileHandle_t	m_hPrecacheLogFile;
	ks::intern_pool m_PrecachedScriptSounds;
	std::vector< AsyncCaption_t > m_ServerCaptions;

public:
	CSoundEmitterSystem( char const *pszName ) :
		m_bLogPrecache( false ),
		m_hPrecacheLogFile( FILESYSTEM_INVALID_HANDLE )
	{
	}

	void LogPrecache( char const *soundname )
	{
		if ( !m_bLogPrecache )
			return;

		// Make sure we only show the message once
		if ( m_PrecachedScriptSounds.contains( soundname ) )
			return;

		if (m_hPrecacheLogFile == FILESYSTEM_INVALID_HANDLE)
		{
			StartLog();
		}

		m_PrecachedScriptSounds.intern( soundname );

		if (m_hPrecacheLogFile != FILESYSTEM_INVALID_HANDLE)
		{
			filesystem->Write("\"", 1, m_hPrecacheLogFile);
			filesystem->Write(soundname, V_strlen(soundname), m_hPrecacheLogFile);
			filesystem->Write("\"\n", 2, m_hPrecacheLogFile);
		}
		else
		{
			Warning( "Disabling precache logging due to file i/o problem!!!\n" );
			m_bLogPrecache = false;
		}
	}

	void StartLog()
	{
		m_PrecachedScriptSounds.clear();

		if ( !m_bLogPrecache )
			return;

		if ( FILESYSTEM_INVALID_HANDLE != m_hPrecacheLogFile )
		{
			return;
		}

		filesystem->CreateDirHierarchy("reslists", "DEFAULT_WRITE_PATH");

		// open the new level reslist
		char path[k_cMaxPath];
		V_snprintf(path, sizeof(path), "reslists\\%s.snd", gpGlobals->mapname.ToCStr() );
		m_hPrecacheLogFile = filesystem->Open(path, "wt", "MOD");
		if (m_hPrecacheLogFile == FILESYSTEM_INVALID_HANDLE)
		{
			Warning( "Unable to open %s for precache logging\n", path );
		}
	}

	void FinishLog()
	{
		if ( FILESYSTEM_INVALID_HANDLE != m_hPrecacheLogFile )
		{
			filesystem->Close( m_hPrecacheLogFile );
			m_hPrecacheLogFile = FILESYSTEM_INVALID_HANDLE;
		}

		m_PrecachedScriptSounds.clear();
	}

	void LoadServerCaptions()
	{
		m_ServerCaptions.clear(); m_ServerCaptions.shrink_to_fit();

		// Server keys off of english file!!!
		AddCaptionFile( "resource/closecaption_english.dat" );
		AddCaptionFile( "resource/subtitles_english.dat" );
	}

	// IServerSystem stuff
	virtual bool Init()
	{
		contract_assert( soundemitterbase );
		m_bLogPrecache = CommandLine()->CheckParm( "-makereslists" ) ? true : false;
		g_pClosecaption = cvar->FindVar("closecaption");
		contract_assert( g_pClosecaption );

		LoadServerCaptions();

		return true;
	}

	void AddCaptionFile( const char *filename )
	{
		int searchPathLen = filesystem->GetSearchPath( "GAME", true, nullptr, 0 );
		ks::ScratchArena<> scratch;
		char *searchPaths = (char *)scratch.AllocBytes( searchPathLen + 1 );
		filesystem->GetSearchPath( "GAME", true, searchPaths, searchPathLen );

		for ( char *path = std::strtok( searchPaths, ";" ); path; path = strtok( nullptr, ";" ) )
		{

			char fullpath[MAX_PATH];
			V_snprintf( fullpath, sizeof( fullpath ), "%s%s", path, filename );
			V_FixSlashes( fullpath );
			V_strlower( fullpath );

			int idx = std::ssize( m_ServerCaptions ); m_ServerCaptions.emplace_back();
			AsyncCaption_t& entry = m_ServerCaptions[ idx  ];
			if ( !entry.LoadFromFile( fullpath ) )
			{
				m_ServerCaptions.erase( m_ServerCaptions.begin() + idx );
			}
			else
			{
				DevMsg( "Server: added caption file: %s\n", fullpath );
			}
		}
	}

	virtual void Shutdown()
	{
		contract_assert( soundemitterbase );
		FinishLog();
	}

	void Flush()
	{
		Shutdown();
		soundemitterbase->Flush();
		Init();
	}

	virtual void TraceEmitSound( int originEnt, char const *fmt, ... )
	{
		if ( sv_soundemitter_trace.GetInt() == -1 )
			return;

		if ( sv_soundemitter_trace.GetInt() != 0 && sv_soundemitter_trace.GetInt() != originEnt )
			return;

		std::va_list	argptr;
		char string[256];
		__builtin_va_start (argptr, fmt);
		V_vsnprintf( string, sizeof( string ), fmt, argptr );
		__builtin_va_end (argptr);

		// Spew to console
		Msg( "%s %s", CBaseEntity::IsServer() ? "(sv)" : "(cl)", string );
	}

	// Precache all wave files referenced in wave or rndwave keys
	virtual void LevelInitPreEntity()
	{

		PreloadSounds();
		
		g_CaptionRepeats.Clear();
	}

	void PreloadSounds( void )
	{
		for ( int i=soundemitterbase->First(); i != soundemitterbase->InvalidIndex(); i=soundemitterbase->Next( i ) )
		{
			CSoundParametersInternal *pParams = soundemitterbase->InternalGetParametersForSound( i );
			if ( pParams->ShouldPreload() )
			{
				InternalPrecacheWaves( i );
			}
			if ( pParams->ShouldAutoCache() )
			{
				PrecacheScriptSound( soundemitterbase->GetSoundName( i ) );
			}
		}

		g_CaptionRepeats.Clear();
	}

	virtual void LevelInitPostEntity()
	{
	}

	virtual void LevelShutdownPostEntity()
	{

		FinishLog();

		g_CaptionRepeats.Clear();
	}
		
	void InternalPrecacheWaves( int soundIndex )
	{
		CSoundParametersInternal *internal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( !internal )
			return;

		int waveCount = internal->NumSoundNames();
		if ( !waveCount )
		{
			DevMsg( "CSoundEmitterSystem:  sounds.txt entry '%s' has no waves listed under 'wave' or 'rndwave' key!!!\n",
				soundemitterbase->GetSoundName( soundIndex ) );
		}
		else
		{
			g_bPermitDirectSoundPrecache = true;

			for( int wave = 0; wave < waveCount; wave++ )
			{
				CBaseEntity::PrecacheSound( soundemitterbase->GetWaveName( internal->GetSoundNames()[ wave ].symbol ) );
			}

			g_bPermitDirectSoundPrecache = false;
		}
	}

	void InternalPrefetchWaves( int soundIndex )
	{
		CSoundParametersInternal *internal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( !internal )
			return;

		int waveCount = internal->NumSoundNames();
		if ( !waveCount )
		{
			DevMsg( "CSoundEmitterSystem:  sounds.txt entry '%s' has no waves listed under 'wave' or 'rndwave' key!!!\n",
				soundemitterbase->GetSoundName( soundIndex ) );
		}
		else
		{
			for( int wave = 0; wave < waveCount; wave++ )
			{
				CBaseEntity::PrefetchSound( soundemitterbase->GetWaveName( internal->GetSoundNames()[ wave ].symbol ) );
			}
		}
	}

	void PrecacheSOSScriptSounds( KeyValues *pRootKV )
	{
		if ( !pRootKV )
			return;

		// iterate through all values
		for ( KeyValues *pValue = pRootKV->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
		{
			const char *pName = pValue->GetName();
			if ( pName && !V_stricmp( pName, "entry_name" ) )
			{
				const char *pScriptName = pValue->GetString();
				if ( pScriptName && pScriptName[0] )
				{
					PrecacheScriptSound( pScriptName );
				}
			}
		}

		// iterate and recurse into each true subkey
		for ( KeyValues *pSubKey = pRootKV->GetFirstTrueSubKey(); pSubKey; pSubKey = pSubKey->GetNextTrueSubKey() )
		{
			PrecacheSOSScriptSounds( pSubKey );
		}
	}

	HSOUNDSCRIPTHASH PrecacheScriptSound( const char *soundname )
	{
		HSOUNDSCRIPTHASH hash = soundemitterbase->HashSoundName( soundname );
		int soundIndex = soundemitterbase->GetSoundIndexForHash( hash );
		if ( !soundemitterbase->IsValidIndex( soundIndex ) )
		{
			if ( V_stristr( soundname, ".wav" ) || V_strstr( soundname, ".mp3" ) )
			{
				g_bPermitDirectSoundPrecache = true;
	
				CBaseEntity::PrecacheSound( soundname );
	
				g_bPermitDirectSoundPrecache = false;
				return SOUNDEMITTER_INVALID_HASH;
			}

			if ( soundname[ 0 ] )
			{
				static ks::intern_pool s_PrecacheScriptSoundFailures;

				// Make sure we only show the message once
				if ( !s_PrecacheScriptSoundFailures.contains( soundname ) )
				{
					Warning( "PrecacheScriptSound '%s' failed, no such sound script entry\n", soundname );
					s_PrecacheScriptSoundFailures.intern( soundname );
				}
			}
			return SOUNDEMITTER_INVALID_HASH;
		}

		LogPrecache( soundname );

		// recursively descend into possible operator stacks to precache all their script sounds
		CSoundParametersInternal *pInternal = soundemitterbase->InternalGetParametersForSound( soundIndex );
		if ( pInternal && !pInternal->HasCached() )
		{
			pInternal->SetCached( true );
			PrecacheSOSScriptSounds( pInternal->GetOperatorsKV() );
		}

		InternalPrecacheWaves( soundIndex );
		return hash;	
	}

	void PrefetchScriptSound( const char *soundname )
	{
		int soundIndex = soundemitterbase->GetSoundIndex( soundname );
		if ( !soundemitterbase->IsValidIndex( soundIndex ) )
		{
			if ( V_stristr( soundname, ".wav" ) || V_strstr( soundname, ".mp3" ) )
			{
				CBaseEntity::PrefetchSound( soundname );
			}
			return;
		}

		InternalPrefetchWaves( soundIndex );
	}
public:

	// utility for cracking parameters
	bool GetSoundEntryParameters( int entindex,  const EmitSound_t & ep, CSoundParameters & params, HSOUNDSCRIPTHASH& handle )
	{
		// Try to deduce the actor's gender
		gender_t gender = GENDER_NONE;
		CBaseEntity *ent = CBaseEntity::Instance( entindex );
		if ( ent )
		{
			char const *actorModel = STRING( ent->GetModelName() );
			gender = soundemitterbase->GetActorGender( actorModel );
		}

		if ( !soundemitterbase->GetParametersForSoundEx( ep.m_pSoundName, handle, params, gender, true ) )
		{
			return false;
		}

		if ( !params.soundname[0] )
			return false;

		if ( !V_strncasecmp( params.soundname, "vo", 2 ) &&
			!( params.channel == CHAN_STREAM ||
			params.channel == CHAN_VOICE ) &&
			params.m_nSoundEntryVersion < 2 )
		{
			DevMsg( "EmitSound:  Voice wave file %s doesn't specify CHAN_VOICE or CHAN_STREAM for sound %s\n",
				params.soundname, ep.m_pSoundName );
		}

		// handle SND_CHANGEPITCH/SND_CHANGEVOL and other sound flags.etc.
		if( ( ep.m_nFlags & SND_CHANGE_PITCH ) || ( ep.m_nFlags & SND_OVERRIDE_PITCH ) )
		{
			params.pitch = ep.m_nPitch;
		}

		if( ep.m_nFlags & SND_CHANGE_VOL )
		{
			params.volume = ep.m_flVolume;
		}

		bool bSwallowed = CEnvMicrophone::OnSoundPlayed( 
			entindex, 
			params.soundname, 
			params.soundlevel, 
			params.volume, 
			ep.m_nFlags, 
			params.pitch, 
			ep.m_pOrigin, 
			ep.m_flSoundTime,
			ep.m_UtlVecSoundOrigin );

		if ( bSwallowed )
			return false;
		return true;
	}

	// spew utility
	void TraceEmitSoundEntry( int handle, const char *pSoundEntryName, CSoundParameters &pSoundParams, int nSeed )
	{

		if( !snd_sos_show_server_xmit.GetInt() )
		{
			return;
		}

		Log_Msg( LOG_SND_EMITTERSYSTEM, Color( 180, 180, 256, 255 ), "Server: Emitting SoundEntry: %i : %s : %s : operators: %s : seed: %i\n", handle, pSoundEntryName, pSoundParams.soundname, pSoundParams.m_pOperatorsKV ? "true" : "false", nSeed );

	}
	// spew utility
	void TraceEmitSoundEntry( HSOUNDSCRIPTHASH handle, const char *pSoundEntryName, const char *pSoundFileName )
	{

		if( !snd_sos_show_server_xmit.GetInt() )
		{
			return;
		}
		Log_Msg( LOG_SND_EMITTERSYSTEM, Color( 180, 180, 256, 255 ), "Server: Emitting SoundEntry: %i : %s : %s\n", handle, pSoundEntryName, pSoundFileName );

	}

	//
	// emitting via a "SoundEntry" as opposed to the actual "SoundFile"
	// ep.m_pSoundName = a "SoundEntry" string
	//
	int EmitSoundByHandle( IRecipientFilter& filter, int entindex, const EmitSound_t & ep, HSOUNDSCRIPTHASH& handle )
	{
		// Whether the important params have come from code, defaults or the
		// script, we emit using them to stay backwards compatible.
		// It is possible, however, to override these values via the script
		// entries.

		CSoundParameters params;
		if( !GetSoundEntryParameters( entindex, ep, params, handle ) )
		{
			return 0;
		}

		// NOTE: This is probably 

		// sound precaching? 
		// NOTE: do something about this, should be irrelevant here
		// because we'll select our actual sound on the other side

		// calculating start time from param.delay_msec
		// does this get moved or just replicated on client?
		float st = ep.m_flSoundTime;
		if ( !st && 
			params.delay_msec != 0 )
		{
			st = gpGlobals->curtime + (float)params.delay_msec / 1000.f;
		}

		// TERROR:
		double startTime = Plat_FloatTime();

		// are we actually treating as a "SoundEntry"?
		int nFlags = ep.m_nFlags;
		if(sv_soundemitter_version.GetInt() > 1 && params.m_nSoundEntryVersion > 1)
		{
			 nFlags |= SND_IS_SCRIPTHANDLE;
			
			 TraceEmitSoundEntry( handle, ep.m_pSoundName, params, params.m_nRandomSeed );

		}

		// Emit via server or client engine call

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );
		
		// use the cracked script params
		int guid = enginesound->EmitSound( 
			filterCopy, 
			entindex, 
			params.channel,
			ep.m_pSoundName,    // gamesound
			handle,				// gamesound handle
			params.soundname,   // soundfile
			params.volume,
			(soundlevel_t)params.soundlevel,
			params.m_nRandomSeed,
			nFlags,
			params.pitch,
			ep.m_pOrigin,
			nullptr,
			&ep.m_UtlVecSoundOrigin,
			true,
			st,
			ep.m_nSpeakerEntity );

		// handle duration query
		// NOTE: This needs to be addressed for soundentry emission
		//
		if ( ep.m_pflSoundDuration )
		{
			double startTime = Plat_FloatTime();
			*ep.m_pflSoundDuration = enginesound->GetSoundDuration( params.soundname );
			float timeSpent = ( Plat_FloatTime() - startTime ) * 1000.0f;
			const float thinkLimit = 10.0f;
			if ( timeSpent > thinkLimit )
			{
				UTIL_LogPrintf( "getting sound duration for %s took %f milliseconds\n", params.soundname, timeSpent );
			}
		}
		
		//// --------------------------------------------------------
		// MattC?
		// TERROR:
		float timeSpent = ( Plat_FloatTime() - startTime ) * 1000.0f;
		const float thinkLimit = 50.0f;
		if ( timeSpent > thinkLimit )
		{
			UTIL_LogPrintf( "EmitSoundByHandle(%s) took %f milliseconds (server)\n",
				ep.m_pSoundName, timeSpent );
		}

		// Debug spew
		TraceEmitSound( entindex, "EmitSound:  '%s' emitted as '%s' (ent %i)\n",
			ep.m_pSoundName, params.soundname, entindex );

		// Don't caption modulations to the sound
		if ( !( ep.m_nFlags & ( SND_CHANGE_PITCH | SND_CHANGE_VOL ) ) )
		{
			EmitCloseCaption( filter, entindex, params, ep );
		}
		return guid;
	}

	//---------------------------------------------------------------------
	// Emits sound via a direct sound file reference, 
	//---------------------------------------------------------------------
	int EmitSoundBySoundFile( IRecipientFilter& filter, int entindex, const EmitSound_t & ep )
	{

		bool bSwallowed = CEnvMicrophone::OnSoundPlayed( 
			entindex, 
			ep.m_pSoundName, 
			ep.m_SoundLevel, 
			ep.m_flVolume, 
			ep.m_nFlags, 
			ep.m_nPitch, 
			ep.m_pOrigin, 
			ep.m_flSoundTime,
			ep.m_UtlVecSoundOrigin );
		if ( bSwallowed )
			return 0;

		// Emission by soundfile is typically because the calling code has 
		// already cracked the soundscript, loaded it's parameters and altered some.
		// However, we want to retain BOTH the calling code's parameters
		// AND the soundscript handle so that we have ALL the data for processing.
		//
		// if this has been updated to include soundscript handle, we can tell
		// by the soundentry version and a valid soundscript handle. We flag it
		// and add the data to transmission.
		//
		int nFlags = ep.m_nFlags;
		const char *pSoundEntryName = ep.m_pSoundName;
		if( ep.m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH &&
			ep.m_nSoundEntryVersion > 1 &&
			sv_soundemitter_version.GetInt() > 1 )
		{
			// reget original soundentry name
			pSoundEntryName = soundemitterbase->GetSoundName( ep.m_hSoundScriptHash );
			nFlags |= SND_IS_SCRIPTHANDLE;
			TraceEmitSoundEntry( ep.m_hSoundScriptHash, pSoundEntryName, ep.m_pSoundName );

		}

		// TERROR:
		double startTime = Plat_FloatTime();
		if ( ep.m_bWarnOnDirectWaveReference && 
			V_stristr( ep.m_pSoundName, ".wav" ) )
		{
			WaveTrace( ep.m_pSoundName, "Emitsound" );
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
		// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
		// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
		
		// Emit sound via direct soundfile reference, unless tagged as a soundentry
		int nGuid = enginesound->EmitSound( 
			filterCopy, 
			entindex, 
			ep.m_nChannel,
			pSoundEntryName,
			ep.m_hSoundScriptHash,
			ep.m_pSoundName, 
			ep.m_flVolume, 
			ep.m_SoundLevel, 
			0 ,
			nFlags, 
			ep.m_nPitch, 
			ep.m_pOrigin,
			nullptr, 
			&ep.m_UtlVecSoundOrigin,
			true, 
			ep.m_flSoundTime,
			ep.m_nSpeakerEntity );

		//// -------------------------------------------------------------------
		if ( ep.m_pflSoundDuration )
		{
			// TERROR:
			UTIL_LogPrintf( "getting wav duration for %s\n", ep.m_pSoundName );
			CVProfScope _( "CSoundEmitterSystem::EmitSound GetSoundDuration (calls engine)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
			*ep.m_pflSoundDuration = enginesound->GetSoundDuration( ep.m_pSoundName );
		}

		TraceEmitSound( entindex, "%f EmitSound:  Raw wave emitted '%s' (ent %i) (vol %f)\n",
			gpGlobals->curtime, ep.m_pSoundName, entindex, ep.m_flVolume );

		// TERROR:
		float timeSpent = ( Plat_FloatTime() - startTime ) * 1000.0f;
		const float thinkLimit = 50.0f;
		if ( timeSpent > thinkLimit )
		{
			UTIL_LogPrintf( "CSoundEmitterSystem::EmitSound(%s) took %f milliseconds (server)\n",
				ep.m_pSoundName, timeSpent );
		}
		return nGuid;
	}

	//
	// Checks for direct soundfile reference and splits to either gamesound handle
	// based emission or direct soundfile emission
	//
	int EmitSound( IRecipientFilter& filter, int entindex, const EmitSound_t & ep )
	{
		CVProfScope _( "CSoundEmitterSystem::EmitSound (calls engine)", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

		// Is this a direct soundfile reference or pre-parameterized call?
		if ( ep.m_pSoundName && 
			( V_stristr( ep.m_pSoundName, ".wav" ) || 
			  V_stristr( ep.m_pSoundName, ".mp3" ) || 
			  ep.m_pSoundName[0] == '!' ))
		{
			return EmitSoundBySoundFile(filter, entindex, ep);
		}
		
		// handle as a script sound entry
		if ( ep.m_hSoundScriptHash == SOUNDEMITTER_INVALID_HASH )
		{
			ep.m_hSoundScriptHash = soundemitterbase->HashSoundName( ep.m_pSoundName );
		}

		return EmitSoundByHandle( filter, entindex, ep, ep.m_hSoundScriptHash );
	}

	void EmitCloseCaption( IRecipientFilter& filter, int entindex, bool fromplayer, char const *token, std::vector< Vector >& originlist, float duration, bool warnifmissing /*= false*/, bool bForceSubtitle = false )
	{
		// Don't use dedicated closecaption ConVar since it will prevent remote clients from getting captions.
		// Okay to use it in SP, since it's the same ConVar, not the FCVAR_USERINFO one
		if ( gpGlobals->maxClients == 1 &&
			!g_pClosecaption->GetBool())
		{
			return;
		}

		// A negative duration means fill it in from the wav file if possible
		if ( duration < 0.0f )
		{
			char const *wav = soundemitterbase->GetWavFileForSound( token, GENDER_NONE );
			if ( wav )
			{
				duration = enginesound->GetSoundDuration( wav );
			}
			else
			{
				duration = 2.0f;
			}
		}

		char lowercase[ 256 ];
		V_strncpy( lowercase, token, sizeof( lowercase ) );
		V_strlower( lowercase );
		if ( V_strstr( lowercase, "\\" ) )
		{
			Hack_FixEscapeChars( lowercase );
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		// Captions only route to host player (there is only one closecaptioning HUD)
		filterCopy.RemoveSplitScreenPlayers();

		if ( !bForceSubtitle )
		{
			// Remove any players who don't want close captions
			CBaseEntity::RemoveRecipientsIfNotCloseCaptioning( (CRecipientFilter &)filterCopy );
		}

		{
			if ( filterCopy.GetRecipientCount() > 0 )
			{
				int c = filterCopy.GetRecipientCount();
				for ( int i = c - 1 ; i >= 0; --i )
				{
					CBasePlayer *player = UTIL_PlayerByIndex( filterCopy.GetRecipientIndex( i ) );
					if ( !player )
						continue;

					Vector playerOrigin = player->GetAbsOrigin();
					soundlevel_t iSoundlevel = soundemitterbase->LookupSoundLevel( lowercase );

					if ( !bForceSubtitle && ( iSoundlevel != SNDLVL_NONE ) && AttenuateCaption( lowercase, playerOrigin, originlist ) )
					{
						filterCopy.RemoveRecipient( player );
					}
				}
			}
		}
		// Anyone left?
		if ( filterCopy.GetRecipientCount() > 0 )
		{

			char lowercase_nogender[ 256 ];
			V_strncpy( lowercase_nogender, lowercase, sizeof( lowercase_nogender ) );
			bool bTriedGender = false;

			CBaseEntity *pActor = CBaseEntity::Instance( entindex );
			if ( pActor )
			{
				char const *pszActorModel = STRING( pActor->GetModelName() );
				gender_t gender = soundemitterbase->GetActorGender( pszActorModel );

				if ( gender == GENDER_MALE )
				{
					V_strncat( lowercase, "_male", sizeof( lowercase ), COPY_ALL_CHARACTERS );
					bTriedGender = true;
				}
				else if ( gender == GENDER_FEMALE )
				{ 
					V_strncat( lowercase, "_female", sizeof( lowercase ), COPY_ALL_CHARACTERS );
					bTriedGender = true;
				}
			}

			unsigned int hash = 0u;
			bool bFound = GetCaptionHash( lowercase, true, hash );

			// if not found, try the no-gender version
			if ( !bFound && bTriedGender )
			{
				bFound = GetCaptionHash( lowercase_nogender, true, hash );
			}

			if ( bFound )
			{
				if ( g_CaptionRepeats.CanEmitCaption( hash ) )
				{
					if ( bForceSubtitle )
					{
						ks::net::CCSUsrMsg_CloseCaptionDirect msg;
						msg.hash = hash;
						msg.duration = clamp( (int)( duration * 10.0f ), 0, 65535 );
						msg.from_player = fromplayer ? 1 : 0;

						// Send forced caption and duration hint down to client
						SendUserMessage( filterCopy, ks::net::CS_UM_CloseCaptionDirect, msg );						
					}
					else
					{
						ks::net::CCSUsrMsg_CloseCaption msg;
						msg.hash = hash;
						msg.duration = clamp( (int)( duration * 10.0f ), 0, 65535 );
						msg.from_player = fromplayer ? 1 : 0;

						// Send caption and duration hint down to client						
						SendUserMessage( filterCopy, ks::net::CS_UM_CloseCaption, msg );
					}
				}
			}
		}
	}

	void EmitCloseCaption( IRecipientFilter& filter, int entindex, const CSoundParameters & params, const EmitSound_t & ep )
	{
		// Don't use dedicated closecaption ConVar since it will prevent remote clients from getting captions.
		// Okay to use it in SP, since it's the same ConVar, not the FCVAR_USERINFO one
		if ( gpGlobals->maxClients == 1 &&
			!g_pClosecaption->GetBool())
		{
			return;
		}

		bool bForceSubtitle = false;

		if ( TestSoundChar( params.soundname, CHAR_SUBTITLED ) )
		{
			bForceSubtitle = true;
		}

		if ( !bForceSubtitle && !ep.m_bEmitCloseCaption )
		{
			return;
		}

		// NOTE:  We must make a copy or else if the filter is owned by a SoundPatch, we'll end up destructively removing
		//  all players from it!!!!
		CRecipientFilter filterCopy;
		filterCopy.CopyFrom( (CRecipientFilter &)filter );

		if ( !bForceSubtitle )
		{
			// Remove any players who don't want close captions
			CBaseEntity::RemoveRecipientsIfNotCloseCaptioning( (CRecipientFilter &)filterCopy );
		}

		// Anyone left?
		if ( filterCopy.GetRecipientCount() <= 0 )
		{
			return;
		}

		float duration = 0.0f;
		if ( ep.m_pflSoundDuration )
		{
			duration = *ep.m_pflSoundDuration;
		}
		else
		{
			duration = enginesound->GetSoundDuration( params.soundname );
		}

		bool fromplayer = false;
		CBaseEntity *ent = CBaseEntity::Instance( entindex );
		if ( ent )
		{
			while ( ent )
			{
				if ( ent->IsPlayer() )
				{
					fromplayer = true;
					break;
				}

				ent = ent->GetOwnerEntity();
			}
		}
		EmitCloseCaption( filter, entindex, fromplayer, ep.m_pSoundName, ep.m_UtlVecSoundOrigin, duration, ep.m_bWarnOnMissingCloseCaption, bForceSubtitle );
	}

	void EmitAmbientSoundAsEntry(CSoundParameters &params, int entindex, const Vector& origin, const char *soundname, float flVolume, int iFlags, int iPitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{
		EmitSound_t ep ;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = soundname;
		ep.m_flVolume = flVolume;
		ep.m_SoundLevel = params.soundlevel;
		ep.m_nFlags = iFlags;
		ep.m_nPitch = iPitch;
		ep.m_pOrigin = &origin;
		ep.m_flSoundTime = soundtime;
		ep.m_pflSoundDuration = duration;
//		ep.	m_bEmitCloseCaption = true;
//			m_bWarnOnMissingCloseCaption = false;
//			m_bWarnOnDirectWaveReference = false;
//			m_nSpeakerEntity = -1;
			// if sound is tagged as version 2 or higher this will be treated as a soundentry!
		ep.m_hSoundScriptHash = params.m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = params.m_nSoundEntryVersion;

		// send sound to all active players
		CReliableBroadcastRecipientFilter filter;

		EmitSoundByHandle( filter, entindex, ep, params.m_hSoundScriptHash );
	}

	void EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, float flVolume, int iFlags, int iPitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{
		// Pull data from parameters
		CSoundParameters params;

		if ( !soundemitterbase->GetParametersForSound( soundname, params, GENDER_NONE ) )
		{
			return;
		}

		// hijack if it's a new style sound
		if( params.m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH && params.m_nSoundEntryVersion > 1 )
		{
			EmitAmbientSoundAsEntry( params, entindex, origin, soundname, flVolume, iFlags, iPitch, soundtime, duration );
			return;
		}

		if( iFlags & SND_CHANGE_PITCH )
		{
			params.pitch = iPitch;
		}

		if( iFlags & SND_CHANGE_VOL )
		{
			params.volume = flVolume;
		}

		engine->EmitAmbientSound(entindex, origin, params.soundname, params.volume, params.soundlevel, iFlags, params.pitch, soundtime );

		bool needsCC = !( iFlags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) );

		float soundduration = 0.0f;
		
		if ( duration
			)
		{
			soundduration = enginesound->GetSoundDuration( params.soundname );
			if ( duration )
			{
				*duration = soundduration;
			}
		}

		TraceEmitSound( entindex, "EmitAmbientSound:  '%s' emitted as '%s' (ent %i)\n",
			soundname, params.soundname, entindex );

		// We only want to trigger the CC on the start of the sound, not on any changes or halting of the sound
		if ( needsCC )
		{
			CRecipientFilter filter;
			filter.AddAllPlayers();
			filter.MakeReliable();

			std::vector< Vector > dummy;
			EmitCloseCaption( filter, entindex, false, soundname, dummy, soundduration, false );
		}

	}

	void StopSoundByHandle( int entindex, const char *soundname, HSOUNDSCRIPTHASH& handle, bool bIsStoppingSpeakerSound = false )
	{
		if ( handle == SOUNDEMITTER_INVALID_HASH )
		{
			handle = soundemitterbase->HashSoundName( soundname );
		}

		int index = soundemitterbase->GetSoundIndexForHash( handle );

		CSoundParametersInternal *params = soundemitterbase->InternalGetParametersForSound( index );
		if ( !params )
		{
			return;
		}

		const char *pSoundEntryName = nullptr;
		if( params->GetSoundEntryVersion() > 1 &&
			sv_soundemitter_version.GetInt() > 1 )
		{
			// reget original soundentry name
			pSoundEntryName = soundemitterbase->GetSoundName( index );
			enginesound->StopSound( 
				entindex, 
				params->GetChannel(), 
				pSoundEntryName,
				handle );
			TraceEmitSoundEntry( handle, pSoundEntryName, soundname );
		}

		// HACK:  we have to stop all sounds if there are > 1 in the rndwave section...
		int c = params->NumSoundNames();
		for ( int i = 0; i < c; ++i )
		{
			char const *wavename = soundemitterbase->GetWaveName( params->GetSoundNames()[ i ].symbol );
			contract_assert( wavename );

			enginesound->StopSound( 
				entindex, 
				params->GetChannel(), 
				wavename );

			TraceEmitSound( entindex, "StopSound:  '%s' stopped as '%s' (ent %i)\n",
				soundname, wavename, entindex );

			if ( bIsStoppingSpeakerSound == false )
			{
				StopSpeakerSounds( wavename );
			}
		}

	}

	void StopSound( int entindex, const char *soundname )
	{
		HSOUNDSCRIPTHASH hash = SOUNDEMITTER_INVALID_HASH;
		StopSoundByHandle( entindex, soundname, hash );
	}

	void StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound = false )
	{
		if ( pSample && ( V_stristr( pSample, ".wav" ) || V_stristr( pSample, ".mp3" ) || pSample[0] == '!' ) )
		{
			enginesound->StopSound( iEntIndex, iChannel, pSample );

			TraceEmitSound( iEntIndex, "StopSound:  Raw wave stopped '%s' (ent %i)\n",
				pSample, iEntIndex );
			if ( bIsStoppingSpeakerSound == false )
			{
				StopSpeakerSounds( pSample );
			}
		}
		else
		{
			// Look it up in sounds.txt and ignore other parameters
			StopSound( iEntIndex, pSample );
		}
	}

	void EmitAmbientSound( int entindex, const Vector &origin, const char *pSample, float volume, soundlevel_t soundlevel, int flags, int pitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
	{
		std::vector< Vector > dummyorigins;

		// Loop through all registered microphones and tell them the sound was just played
		// NOTE: This means that pitch shifts/sound changes on the original ambient will not be reflected in the re-broadcasted sound
		bool bSwallowed = CEnvMicrophone::OnSoundPlayed( 
							entindex, 
							pSample, 
							soundlevel, 
							volume, 
							flags, 
							pitch, 
							&origin, 
							soundtime,
							dummyorigins );
		if ( bSwallowed )
			return;

		if ( pSample && ( V_stristr( pSample, ".wav" ) || V_stristr( pSample, ".mp3" )) )
		{
			engine->EmitAmbientSound( entindex, origin, pSample, volume, soundlevel, flags, pitch, soundtime );

			if ( duration )
			{
				*duration = enginesound->GetSoundDuration( pSample );
			}

			TraceEmitSound( entindex, "EmitAmbientSound:  Raw wave emitted '%s' (ent %i)\n",
				pSample, entindex );
		}
		else
		{
			EmitAmbientSound( entindex, origin, pSample, volume, flags, pitch, soundtime, duration );
		}
	}

	bool GetCaptionHash( char const *pchStringName, bool bWarnIfMissing, unsigned int &hash )
	{
		// hash the string, find in dictionary or return 0u if not there!!!
		std::vector< AsyncCaption_t >& directories = m_ServerCaptions;

		CaptionLookup_t search;
		search.SetHash( pchStringName );
		hash = search.hash;

		const CaptionLookup_t *pEntry = nullptr;
		for ( const AsyncCaption_t &directory : directories )
		{
			auto it = directory.m_CaptionDirectory.find( search );
			if ( it != directory.m_CaptionDirectory.end() )
			{
				pEntry = &*it;
				break;
			}
		}

		if ( !pEntry )
		{
			if ( bWarnIfMissing && cc_showmissing.GetBool() )
			{
				static std::set< unsigned int > s_MissingHashes;
				if ( s_MissingHashes.insert( hash ).second )
				{
					Msg( "Missing caption for %s\n", pchStringName );
				}
			}
			return false;
		}

		// Anything marked as L"" by content folks doesn't need to transmit either!!!
		if ( pEntry->length <= sizeof( wchar_t ) )
		{
			return false;
		}

		return true;
	}
	void StopSpeakerSounds( const char *wavename )	
	{
		// Stop sound on any speakers playing this wav name
		// but don't recurse in if this stopsound is happening on a speaker
		CEnvMicrophone::OnSoundStopped( wavename );
	}
};

static CSoundEmitterSystem g_SoundEmitterSystem( "CSoundEmitterSystem" );

IGameSystem *SoundEmitterSystem()
{
	return &g_SoundEmitterSystem;
}

void SoundSystemPreloadSounds( void )
{
	g_SoundEmitterSystem.PreloadSounds();
}

static void sv_soundemitter_flush( const CCommand &args );
static ConCommand sv_soundemitter_flush_command( side_registry(), "sv_soundemitter_flush", sv_soundemitter_flush, "Flushes the sounds.txt system (server only)", FCVAR_DEVELOPMENTONLY );
static void sv_soundemitter_flush( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	// save the current soundscape
	// kill the system
	g_SoundEmitterSystem.Flush();

	// Redo precache all wave files... (this should work now that we have dynamic string tables)
	g_SoundEmitterSystem.LevelInitPreEntity();

	// These store raw sound indices for faster precaching, blow them away.
	ClearModelSoundsCache();
	// TODO:  when we go to a handle system, we'll need to invalidate handles somehow
}

static void sv_soundemitter_filecheck( const CCommand &args );
static ConCommand sv_soundemitter_filecheck_command( side_registry(), "sv_soundemitter_filecheck", sv_soundemitter_filecheck, "Report missing wave files for sounds and game_sounds files.", FCVAR_DEVELOPMENTONLY );
static void sv_soundemitter_filecheck( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;
	int missing = soundemitterbase->CheckForMissingWavFiles( true );
	DevMsg( "---------------------------\nTotal missing files %i\n", missing );
}

//!!!HACK- Zoid 8/9/2009
//This hack is for L4D DLC2.  We need to reload the soundemitter, but its reference counted by the
//client and the server, so we have to Shutdown() and Init() twice.  
static void sv_soundemitter_reload( const CCommand &args );
static ConCommand sv_soundemitter_reload_command( side_registry(), "sv_soundemitter_reload", sv_soundemitter_reload, "Flushes the sounds.txt system" );
static void sv_soundemitter_reload( const CCommand &args )
{
	// Reload server side captions
	g_SoundEmitterSystem.LoadServerCaptions();
}

static void sv_findsoundname( const CCommand &args );
static ConCommand sv_findsoundname_command( side_registry(), "sv_findsoundname", sv_findsoundname, "Find sound names which reference the specified wave files.", FCVAR_DEVELOPMENTONLY );
static void sv_findsoundname( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() != 2 )
		return;

	int c = soundemitterbase->GetSoundCount();
	int i;

	char const *search = args[ 1 ];
	if ( !search )
		return;

	for ( i = 0; i < c; i++ )
	{
		CSoundParametersInternal *internal = soundemitterbase->InternalGetParametersForSound( i );
		if ( !internal )
			continue;

		int waveCount = internal->NumSoundNames();
		if ( waveCount > 0 )
		{
			for( int wave = 0; wave < waveCount; wave++ )
			{
				char const *wavefilename = soundemitterbase->GetWaveName( internal->GetSoundNames()[ wave ].symbol );

				if ( V_stristr( wavefilename, search ) )
				{
					char const *soundname = soundemitterbase->GetSoundName( i );
					char const *scriptname = soundemitterbase->GetSourceFileForSound( i );

					Msg( "Referenced by '%s:%s' -- %s\n", scriptname, soundname, wavefilename );
				}
			}
		}
	}
}

static void sv_soundemitter_spew( const CCommand &args );
static ConCommand sv_soundemitter_spew_command( side_registry(), "sv_soundemitter_spew", sv_soundemitter_spew, "Print details about a sound.", FCVAR_DEVELOPMENTONLY );
static void sv_soundemitter_spew( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Usage:  soundemitter_spew < sndname >\n" );
		return;
	}

	soundemitterbase->DescribeSound( args.Arg( 1 ) );
}

//-----------------------------------------------------------------------------
// Purpose:  Non-static override for doing the general case of CBroadcastRecipientFilter, and EmitSound( filter, entindex(), etc. );
// Input  : *soundname - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( const char *soundname, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	//VPROF( "CBaseEntity::EmitSound" );
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	ABS_QUERY_GUARD( true );
	CBroadcastRecipientFilter filter;
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, entindex(), params );
}

//-----------------------------------------------------------------------------
// Purpose:  Non-static override for doing the general case of CBroadcastRecipientFilter, and EmitSound( filter, entindex(), etc. );
// Input  : *soundname - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( const char *soundname, HSOUNDSCRIPTHASH& handle, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	// VPROF( "CBaseEntity::EmitSound" );
	ABS_QUERY_GUARD( true );
	CBroadcastRecipientFilter filter;

	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, entindex(), params, handle );
}

void CBaseEntity::ScriptEmitSound( const char *soundname )
{
	EmitSound( soundname );
}

void CBaseEntity::ScriptStopSound( const char *soundname )
{
	StopSound( soundname );
}

float CBaseEntity::ScriptSoundDuration( const char *soundname, const char *actormodel )
{
	float duration = CBaseEntity::GetSoundDuration( soundname, actormodel );
	return duration;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			*soundname - 
//			*pOrigin - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, const Vector *pOrigin /*= NULL*/, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	// VPROF( "CBaseEntity::EmitSound" );
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pOrigin = pOrigin;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, iEntIndex, params );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			*soundname - 
//			*pOrigin - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const char *soundname, HSOUNDSCRIPTHASH& handle, const Vector *pOrigin /*= NULL*/, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	//VPROF( "CBaseEntity::EmitSound" );
	EmitSound_t params;
	params.m_pSoundName = soundname;
	params.m_flSoundTime = soundtime;
	params.m_pOrigin = pOrigin;
	params.m_pflSoundDuration = duration;
	params.m_bWarnOnDirectWaveReference = true;

	return EmitSound( filter, iEntIndex, params, handle );
}

static void Helper_UpdateLastMadeNoiseTime( const IRecipientFilter &filter, int iEntIndex, const EmitSound_t &params )
{
	CBaseEntity * pEnt = nullptr;

	if ( ( filter.GetRecipientCount() > 1 ) ||
		( filter.GetRecipientCount() == 1 && filter.GetRecipientIndex( 0 ) != iEntIndex ) )
	{
		pEnt = UTIL_EntityByIndex( iEntIndex );
	}

	if ( pEnt )
		pEnt->UpdateLastMadeNoiseTime( params.m_pSoundName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			params - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	Helper_UpdateLastMadeNoiseTime( filter, iEntIndex, params );
	
	// VPROF( "CBaseEntity::EmitSound" );
	// Call into the sound emitter system...
	return g_SoundEmitterSystem.EmitSound( filter, iEntIndex, params );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			iEntIndex - 
//			params - 
//-----------------------------------------------------------------------------
int CBaseEntity::EmitSound( IRecipientFilter& filter, int iEntIndex, const EmitSound_t & params, HSOUNDSCRIPTHASH& handle )
{
	CVProfScope _( "CBaseEntity::EmitSound", 0, "CBaseEntity::EmitSound", false, BUDGETFLAG_OTHER );

	Helper_UpdateLastMadeNoiseTime( filter, iEntIndex, params );

	// VPROF( "CBaseEntity::EmitSound" );
	// Call into the sound emitter system...
	return g_SoundEmitterSystem.EmitSoundByHandle( filter, iEntIndex, params, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( const char *soundname )
{

	StopSound( entindex(), soundname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( const char *soundname, HSOUNDSCRIPTHASH& handle )
{

	g_SoundEmitterSystem.StopSoundByHandle( entindex(), soundname, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iEntIndex - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::StopSound( int iEntIndex, const char *soundname )
{
	g_SoundEmitterSystem.StopSound( iEntIndex, soundname );
}

void CBaseEntity::StopSound( int iEntIndex, int iChannel, const char *pSample, bool bIsStoppingSpeakerSound )
{
	g_SoundEmitterSystem.StopSound( iEntIndex, iChannel, pSample, bIsStoppingSpeakerSound );
}

soundlevel_t CBaseEntity::LookupSoundLevel( const char *soundname )
{
	return soundemitterbase->LookupSoundLevel( soundname );
}

soundlevel_t CBaseEntity::LookupSoundLevel( const char *soundname, HSOUNDSCRIPTHASH& handle )
{
	return soundemitterbase->LookupSoundLevelByHandle( soundname, handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *entity - 
//			origin - 
//			flags - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CBaseEntity::EmitAmbientSound( int entindex, const Vector& origin, const char *soundname, int flags, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	g_SoundEmitterSystem.EmitAmbientSound( entindex, origin, soundname, 0.0, flags, 0, soundtime, duration );
}

// HACK HACK:  Do we need to pull the entire SENTENCEG_* wrapper over to the client .dll?

void UTIL_EmitAmbientSound( int entindex, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float soundtime /*= 0.0f*/, float *duration /*=NULL*/ )
{
	if (samp && *samp == '!')
	{
		int sentenceIndex = SENTENCEG_Lookup(samp);
		if (sentenceIndex >= 0)
		{
			char name[32];
			V_snprintf( name, sizeof(name), "!%d", sentenceIndex );
			engine->EmitAmbientSound( entindex, vecOrigin, name, vol, soundlevel, fFlags, pitch, soundtime );
			if ( duration )
			{
				*duration = enginesound->GetSoundDuration( name );
			}

			g_SoundEmitterSystem.TraceEmitSound( entindex, "UTIL_EmitAmbientSound:  Sentence emitted '%s' (ent %i)\n",
				name, entindex );
		}
	}
	else
	{
		g_SoundEmitterSystem.EmitAmbientSound( entindex, vecOrigin, samp, vol, soundlevel, fFlags, pitch, soundtime, duration );
	}
}

static const char *UTIL_TranslateSoundName( const char *soundname, const char *actormodel )
{
	contract_assert( soundname );

	if ( V_stristr( soundname, ".wav" ) || V_stristr( soundname, ".mp3" ) )
	{
		if ( V_stristr( soundname, ".wav" ) )
		{
			WaveTrace( soundname, "UTIL_TranslateSoundName" );
		}
		return soundname;
	}

	return soundemitterbase->GetWavFileForSound( soundname, actormodel );
}

void CBaseEntity::GenderExpandString( char const *in, char *out, int maxlen )
{
	soundemitterbase->GenderExpandString( STRING( GetModelName() ), in, out, maxlen );
}

bool CBaseEntity::GetParametersForSound( const char *soundname, CSoundParameters &params, const char *actormodel )
{
	gender_t gender = soundemitterbase->GetActorGender( actormodel );
	
	return soundemitterbase->GetParametersForSound( soundname, params, gender );
}

bool CBaseEntity::GetParametersForSound( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters &params, const char *actormodel )
{
	gender_t gender = soundemitterbase->GetActorGender( actormodel );
	
	return soundemitterbase->GetParametersForSoundEx( soundname, handle, params, gender );
}

HSOUNDSCRIPTHASH CBaseEntity::PrecacheScriptSound( const char *soundname )
{
	return g_SoundEmitterSystem.PrecacheScriptSound( soundname );
}

// Same as server version of above, but signiture changed so it can be deduced by the macros
void CBaseEntity::VScriptPrecacheScriptSound( const char *soundname )
{
	g_SoundEmitterSystem.PrecacheScriptSound( soundname );
}

void CBaseEntity::PrefetchScriptSound( const char *soundname )
{
	g_SoundEmitterSystem.PrefetchScriptSound( soundname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : float
//-----------------------------------------------------------------------------
float CBaseEntity::GetSoundDuration( const char *soundname, char const *actormodel )
{
	return enginesound->GetSoundDuration( PSkipSoundChars( UTIL_TranslateSoundName( soundname, actormodel ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			*token - 
//			duration - 
//			warnifmissing - 
//-----------------------------------------------------------------------------
void CBaseEntity::EmitCloseCaption( IRecipientFilter& filter, int entindex, char const *token, std::vector< Vector >& soundorigin, float duration, bool warnifmissing /*= false*/ )
{
	bool fromplayer = false;
	CBaseEntity *ent = CBaseEntity::Instance( entindex );
	while ( ent )
	{
		if ( ent->IsPlayer() )
		{
			fromplayer = true;
			break;
		}
		ent = ent->GetOwnerEntity();
	}

	g_SoundEmitterSystem.EmitCloseCaption( filter, entindex, fromplayer, token, soundorigin, duration, warnifmissing );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			preload - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseEntity::PrecacheSound( const char *name )
{
	if ( !g_bPermitDirectSoundPrecache )
	{
		Warning( "Direct precache of %s\n", name );
	}

	// If this is out of order, warn
	if ( !CBaseEntity::IsPrecacheAllowed() )
	{
		if ( !enginesound->IsSoundPrecached( name ) )
		{
			contract_assert( !"CBaseEntity::PrecacheSound:  too late" );

			Warning( "Late precache of %s\n", name );
		}
	}

	bool bret = enginesound->PrecacheSound( name, true );
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseEntity::PrefetchSound( const char *name )
{
	 enginesound->PrefetchSound( name );
}

bool GetCaptionHash( char const *pchStringName, bool bWarnIfMissing, unsigned int &hash )
{
	return g_SoundEmitterSystem.GetCaptionHash( pchStringName, bWarnIfMissing, hash );
}

bool CanEmitCaption( unsigned int hash )
{
	return g_CaptionRepeats.CanEmitCaption( hash );
}
