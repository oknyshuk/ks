// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: implements various common send proxies

import std;
import ks.tier1;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "sendproxy.h"
#include "baseentity.h"
#include "team.h"
#include "player.h"

void SendProxy_Color32ToInt( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	color32 *pIn = (color32*)pData;
	*((unsigned int*)&pOut->m_Int) = ((unsigned int)pIn->r << 24) | ((unsigned int)pIn->g << 16) | ((unsigned int)pIn->b << 8) | ((unsigned int)pIn->a);
}

void SendProxy_EHandleToInt( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID)
{
	CBaseHandle *pHandle = (CBaseHandle*)pVarData;

	if ( pHandle && pHandle->Get() )
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

constexpr auto TIME_BITS = 24;

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


void CSendProxyRecipients::SetRecipient( int iClient )
{
	m_Bits.set( iClient );
}

void CSendProxyRecipients::ClearRecipient( int iClient )
{
	m_Bits.reset( iClient );
}

void CSendProxyRecipients::SetOnly( int iClient )
{
	m_Bits.reset();
	SetRecipient( iClient );

	CBaseEntity *pEntity = CBaseEntity::Instance( iClient + 1 );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( pEntity );
		if ( pPlayer->IsSplitScreenPlayer() )
		{
			if ( pPlayer->GetSplitScreenPlayerOwner() )
			{
				SetRecipient( pPlayer->GetSplitScreenPlayerOwner()->entindex() - 1 );
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
				SetRecipient( iEntIndex - 1 );
			}
		}
	}
}

void CSendProxyRecipients::ExcludeOnly( int iClient )
{
	m_Bits.set();
	ClearRecipient( iClient );
	CBaseEntity *pEntity = CBaseEntity::Instance( iClient + 1 );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast< CBasePlayer * >( pEntity );
		if ( pPlayer->IsSplitScreenPlayer() )
		{
			if ( pPlayer->GetSplitScreenPlayerOwner() )
			{
				ClearRecipient( pPlayer->GetSplitScreenPlayerOwner()->entindex() - 1 );
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
				ClearRecipient( iEntIndex - 1 );
			}
		}
	}
}
