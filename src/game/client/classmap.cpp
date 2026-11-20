// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "edict.h"
#include "iclientthinkable.h"
#include "client_thinklist.h"
#include "cliententitylist.h"
#include "iclientalphaproperty.h"
#include "iclientrenderable.h"
#include "basehandle.h"
#include "engine/ICollideable.h"
#include "icliententity.h"
#include "icliententitylist.h"
module ks.client;

import ks.math.ssequaternion;
import ks.platform;
import ks.simd;
import ks.tier1.map_ops;
import ks.tier1.strtools;
import :baseentity;
import :particlemgr;
import :iinput;
import :baseanimatedtextureproxy;
import :sharedvar;
import :props;
import :util;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import std;
import ks.core.dbg;
import ks.core.scratcharena;
import ks.core.time;
import ks.core.types;
import ks.core.vprof;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import :precache;
import :achievements_and_stats_interface;
import :noopanimcontroller;
import ks.tier1.slot_pool;
import ks.core.basetypes;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.game.entity_cast;
import ks.tier1.vector_ops;
import ks.core.tslist;

class CBaseEntity;

class classentry_t
{
public:
	classentry_t()
	{
		mapname[ 0 ] = 0;
		factory = 0;
		size = -1;
	}

	char const *GetMapName() const
	{
		return mapname;
	}

	void SetMapName( char const *newname )
	{
		V_strncpy( mapname, newname, sizeof( mapname ) );
	}

	DISPATCHFUNCTION	factory;
	int					size;
private:
	char				mapname[ 40 ];
};

class CClassMap : public IClassMap
{
public:
	virtual void			Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory /*= 0*/ );
	virtual const char		*Lookup( const char *classname );
	virtual CBaseEntity	*CreateEntity( const char *mapname );
	virtual int				GetClassSize( const char *classname );

private:
	ks::idict< classentry_t > m_ClassDict;
};

IClassMap& GetClassMap( void )
{
	static CClassMap g_Classmap;
	return g_Classmap;
}

void CClassMap::Add( const char *mapname, const char *classname, int size, DISPATCHFUNCTION factory = 0 )
{
	const char *map = Lookup( classname );
	if ( map && !V_strcasecmp( mapname, map ) )
		return;

	classentry_t element;
	element.SetMapName( mapname );
	element.factory = factory;
	element.size = size;
	m_ClassDict.insert_or_assign( classname, element );
}

const char *CClassMap::Lookup( const char *classname )
{
	static classentry_t lookup; 

	auto it = m_ClassDict.find( classname );
	if ( it == m_ClassDict.end() )
		return nullptr;

	lookup = it->second;
	return lookup.GetMapName();
}

CBaseEntity *CClassMap::CreateEntity( const char *mapname )
{
	for ( auto &[classname, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( V_stricmp( lookup->GetMapName(), mapname ) )
			continue;

		if ( !lookup->factory )
		{
			continue;
		}

		return ( *lookup->factory )();
	}

	return nullptr;
}

int CClassMap::GetClassSize( const char *classname )
{
	for ( auto &[name, entry] : m_ClassDict )
	{
		classentry_t *lookup = &entry;

		if ( V_strcmp( lookup->GetMapName(), classname ) )
			continue;

		return lookup->size;
	}

	return -1;
}

static factorylist_t s_factories;

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData )
{
	s_factories = sourceData;
}

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData )
{
	destData = s_factories;
}

class CBaseEntity;

using namespace ::ks::reflect;

CClientThinkList g_ClientThinkList;

static ConVar  report_clientthinklist( side_registry(), "report_clientthinklist", "0", FCVAR_CHEAT, "List all clientside entities thinking and time - will report and turn itself off." );

CClientThinkList::CClientThinkList()
{
}

CClientThinkList::~CClientThinkList()
{
}

//-----------------------------------------------------------------------------
// Methods of IGameSystem
//-----------------------------------------------------------------------------
bool CClientThinkList::Init()
{
	m_nIterEnum = 0;
	m_bInThinkLoop = false;
	return true;
}

void CClientThinkList::Shutdown()
{
}

void CClientThinkList::LevelInitPreEntity()
{
	m_nIterEnum = 0;
}

void CClientThinkList::LevelShutdownPreEntity()
{
}

void CClientThinkList::LevelShutdownPostEntity()
{
}

void CClientThinkList::PreRender()
{
}

void CClientThinkList::Update( float frametime )
{
}

//-----------------------------------------------------------------------------
// Sets the client think
//-----------------------------------------------------------------------------
void CClientThinkList::SetNextClientThink( ClientThinkHandle_t hThink, float flNextTime )
{
	if ( hThink == INVALID_THINK_HANDLE )
		return;

	if ( m_bInThinkLoop )
	{
		// Queue up all changes
		int i = std::ssize( m_aChangeList ); m_aChangeList.emplace_back();
		m_aChangeList[i].m_hEnt = INVALID_CLIENTENTITY_HANDLE;
		m_aChangeList[i].m_hThink = hThink;
		m_aChangeList[i].m_flNextTime = flNextTime;
		return;
	}

	if ( flNextTime == CLIENT_THINK_NEVER )
	{
		RemoveThinkable( hThink );
	}
	else
	{
		GetThinkEntry( hThink )->m_flNextClientThink = flNextTime;
	}
}

void CClientThinkList::SetNextClientThink( ClientEntityHandle_t hEnt, float flNextTime )
{
	if ( flNextTime == CLIENT_THINK_NEVER )
	{
		RemoveThinkable( hEnt );
		return;
	}

	IClientThinkable *pThink = ClientEntityList().GetClientThinkableFromHandle( hEnt );
	if ( !pThink )
		return;

	ClientThinkHandle_t hThink = pThink->GetThinkHandle();

	if ( m_bInThinkLoop )
	{
		// Queue up all changes
		int i = std::ssize( m_aChangeList ); m_aChangeList.emplace_back();
		m_aChangeList[i].m_hEnt = hEnt;
		m_aChangeList[i].m_hThink = hThink;
		m_aChangeList[i].m_flNextTime = flNextTime;
		return;
	}

	// Add it to the list if it's not already in there.
	if ( hThink == INVALID_THINK_HANDLE )
	{
		hThink = (ClientThinkHandle_t)(uintp)m_ThinkEntries.emplace_back();
		pThink->SetThinkHandle( hThink );

		ThinkEntry_t *pEntry = GetThinkEntry( hThink );
		pEntry->m_hEnt = hEnt;
		pEntry->m_nIterEnum = -1;
		pEntry->m_flLastClientThink = 0.0f;
	}

	GetThinkEntry( hThink )->m_flNextClientThink = flNextTime;
}

//-----------------------------------------------------------------------------
// Removes the thinkable from the list
//-----------------------------------------------------------------------------
void CClientThinkList::RemoveThinkable( ClientThinkHandle_t hThink )
{
	if ( hThink == INVALID_THINK_HANDLE )
		return;

	if ( m_bInThinkLoop )
	{
		// Queue up all changes
		int i = std::ssize( m_aChangeList ); m_aChangeList.emplace_back();
		m_aChangeList[i].m_hEnt = INVALID_CLIENTENTITY_HANDLE;
		m_aChangeList[i].m_hThink = hThink;
		m_aChangeList[i].m_flNextTime = CLIENT_THINK_NEVER;
		return;
	}

	ThinkEntry_t *pEntry = GetThinkEntry( hThink );
	IClientThinkable *pThink = ClientEntityList().GetClientThinkableFromHandle( pEntry->m_hEnt );
	if ( pThink )
	{
		pThink->SetThinkHandle( INVALID_THINK_HANDLE );
	}
	m_ThinkEntries.erase( (unsigned short)(uintp)hThink );
}

//-----------------------------------------------------------------------------
// Removes the thinkable from the list
//-----------------------------------------------------------------------------
void CClientThinkList::RemoveThinkable( ClientEntityHandle_t hEnt )
{
	IClientThinkable *pThink = ClientEntityList().GetClientThinkableFromHandle( hEnt );
	if ( pThink )
	{
		ClientThinkHandle_t hThink = pThink->GetThinkHandle();
		if ( hThink != INVALID_THINK_HANDLE )
		{
			RemoveThinkable( hThink );
		}
	}
}

//-----------------------------------------------------------------------------
// Performs the think function
//-----------------------------------------------------------------------------
void CClientThinkList::PerformThinkFunction( ThinkEntry_t *pEntry, float flCurtime )
{
	IClientThinkable *pThink = ClientEntityList().GetClientThinkableFromHandle( pEntry->m_hEnt );
	if ( !pThink )
	{
		RemoveThinkable( pEntry->m_hEnt );
		return;
	}

	if ( pEntry->m_flNextClientThink == CLIENT_THINK_ALWAYS )
	{
		// NOTE: The Think function here could call SetNextClientThink
		// which would cause it to be removed + readded into the list
		pThink->ClientThink();
	}
	else if ( pEntry->m_flNextClientThink == std::numeric_limits<float>::max() )
	{
		// This is an entity that doesn't need to think again; remove it
		RemoveThinkable( pEntry->m_hEnt );
	}
	else
	{
		contract_assert( pEntry->m_flNextClientThink <= flCurtime );

		// Indicate we're not going to think again
		pEntry->m_flNextClientThink = std::numeric_limits<float>::max();

		// NOTE: The Think function here could call SetNextClientThink
		// which would cause it to be readded into the list
		pThink->ClientThink();
	}

	// Set this after the Think calls in case they look at LastClientThink
	pEntry->m_flLastClientThink = flCurtime;
}

//-----------------------------------------------------------------------------
// Add entity to frame think list
//-----------------------------------------------------------------------------
void CClientThinkList::AddEntityToFrameThinkList( ThinkEntry_t *pEntry, bool bAlwaysChain, int &nCount, ThinkEntry_t **ppFrameThinkList )
{
	// We may already have processed this owing to hierarchy rules
	if ( pEntry->m_nIterEnum == m_nIterEnum )
		return;

	// If we're not thinking this frame, we don't have to worry about thinking after our parents
	bool bThinkThisInterval = ( pEntry->m_flNextClientThink == CLIENT_THINK_ALWAYS ) ||
								( pEntry->m_flNextClientThink <= gpGlobals->curtime );

	// This logic makes it so that if a child thinks,
	// *all* hierarchical parents + grandparents will think first, even if some
	// of the parents don't need to think this frame
	if ( !bThinkThisInterval && !bAlwaysChain )
		return;

	// Respect hierarchy
	CBaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( pEntry->m_hEnt );
	if ( pEntity )
	{
		CBaseEntity *pParent = pEntity->GetMoveParent();
		if ( pParent && (pParent->GetThinkHandle() != INVALID_THINK_HANDLE) )
		{
			ThinkEntry_t *pParentEntry = GetThinkEntry( pParent->GetThinkHandle() );
			AddEntityToFrameThinkList( pParentEntry, true, nCount, ppFrameThinkList );
		}
	}

	if ( !bThinkThisInterval )
		return;

	// Add the entry into the list
	pEntry->m_nIterEnum = m_nIterEnum;
	ppFrameThinkList[nCount++] = pEntry;
}

//-----------------------------------------------------------------------------
// Think for all entities that need it
//-----------------------------------------------------------------------------
void CClientThinkList::PerformThinkFunctions()
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "Client Thinks", 1, VPROF_BUDGETGROUP_CLIENT_SIM, false, BUDGETFLAG_CLIENT );

	int nMaxList = (int)m_ThinkEntries.size();
	if ( nMaxList == 0 )
		return;

	++m_nIterEnum;

	// Build a list of entities to think this frame, in order of hierarchy.
	// Do this because the list may be modified during the thinking and also to
	// prevent bad situations where an entity can think more than once in a frame.
	ThinkEntry_t **ppThinkEntryList = scratch.Alloc<ThinkEntry_t*>( nMaxList );
	int nThinkCount = 0;
	for ( ThinkEntry_t &entry : m_ThinkEntries )
	{
		AddEntityToFrameThinkList( &entry, false, nThinkCount, ppThinkEntryList );
		contract_assert( nThinkCount <= nMaxList );
	}

	// While we're in the loop, no changes to the think list are allowed
	m_bInThinkLoop = true;

	if( !report_clientthinklist.GetBool() )
	{
		// Perform thinks on all entities that need it
		for ( int i = 0; i < nThinkCount; ++i )
		{
			PerformThinkFunction( ppThinkEntryList[i], gpGlobals->curtime );		
		}
	}
	else
	{
		CFastTimer fastTimer;

		for ( int i = 0; i < nThinkCount; ++i )
		{
			fastTimer.Start();
			PerformThinkFunction( ppThinkEntryList[i], gpGlobals->curtime );		
			fastTimer.End();

			CBaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( ppThinkEntryList[i]->m_hEnt );
			if ( pEntity )
			{
				Msg( "Entity(%d): %s - %f\n", pEntity->entindex(), pEntity->GetDebugName(), fastTimer.GetDuration().GetMillisecondsF() );
			}
		}

		report_clientthinklist.SetValue( 0 );
	}

	m_bInThinkLoop = false;

	// Apply changes to the think list
	int nCount = std::ssize( m_aChangeList );
	for ( int i = 0; i < nCount; ++i )
	{
		ClientThinkHandle_t hThink = m_aChangeList[i].m_hThink;
		if ( hThink != INVALID_THINK_HANDLE )
		{
			// This can happen if the same think handle was removed twice
			if ( !m_ThinkEntries.contains( (unsigned short)(uintp)hThink ) )
				continue;

			// NOTE: This is necessary for the case where the client entity handle
			// is slammed to NULL during a think interval; the hThink will get stuck
			// in the list and can never leave.
			SetNextClientThink( hThink, m_aChangeList[i].m_flNextTime );
		}
		else
		{
			SetNextClientThink( m_aChangeList[i].m_hEnt, m_aChangeList[i].m_flNextTime );
		}
	}
	m_aChangeList.clear();

	// Clear out the client-side entity deletion list.
	CleanUpDeleteList();
}

//-----------------------------------------------------------------------------
// Queued-up entity deletion
//-----------------------------------------------------------------------------
void CClientThinkList::AddToDeleteList( ClientEntityHandle_t hEnt )
{
	// Sanity check!
	contract_assert( hEnt != ClientEntityList().InvalidHandle() );
	if ( hEnt == ClientEntityList().InvalidHandle() )
		return;

	// Check to see if entity is networkable -- don't let it release!
	CBaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( hEnt );
	if ( pEntity )
	{
		// Check to see if the entity is already being removed!
		if ( pEntity->IsMarkedForDeletion() )
			return;

		// Don't add networkable entities to delete list -- the server should
		// take care of this.  The delete list is for client-side only entities.
		if ( !pEntity->GetClientNetworkable() )
		{
			m_aDeleteList.push_back( hEnt );
			pEntity->SetRemovalFlag( true );
		}
	}
}

void CClientThinkList::RemoveFromDeleteList( ClientEntityHandle_t hEnt )
{
	// Sanity check!
	contract_assert( hEnt != ClientEntityList().InvalidHandle() );
	if ( hEnt == ClientEntityList().InvalidHandle() )
		return;

	int nSize = std::ssize( m_aDeleteList );
	for ( int iHandle = 0; iHandle < nSize; ++iHandle )
	{
		if ( m_aDeleteList[iHandle] == hEnt )
		{
			m_aDeleteList[iHandle] = ClientEntityList().InvalidHandle();

			CBaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( hEnt );
			if ( pEntity )
			{
				pEntity->SetRemovalFlag( false );
			}
		}
	}
}

void CClientThinkList::CleanUpDeleteList()
{
	int nThinkCount = std::ssize( m_aDeleteList );
	for ( int iThink = 0; iThink < nThinkCount; ++iThink )
	{
		ClientEntityHandle_t handle = m_aDeleteList[iThink];
		if ( handle != ClientEntityList().InvalidHandle() )
		{
			CBaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( handle );
			if ( pEntity )
			{
				pEntity->SetRemovalFlag( false );
			}

			IClientThinkable *pThink = ClientEntityList().GetClientThinkableFromHandle( handle );
			if ( pThink )
			{
				pThink->Release();
			}
		}
	}

	m_aDeleteList.clear();
}

ClientThinkHandle_t CClientThinkList::GetInvalidThinkHandle()
{
	return (ClientThinkHandle_t)(uintp)m_ThinkEntries.invalid;
}

CClientThinkList::ThinkEntry_t* CClientThinkList::GetThinkEntry( ClientThinkHandle_t hThink )
{
	return &m_ThinkEntries[ (unsigned short)(uintp)hThink ];
}

class CClientAlphaProperty;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Client alpha property starts here
//-----------------------------------------------------------------------------
CClientAlphaProperty::CClientAlphaProperty( )
{
	m_nRenderFX = kRenderFxNone;
	m_nRenderMode = kRenderNormal;
	m_nDesyncOffset = 0;
	m_hShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_nAlpha = 255;
	m_flFadeScale = 0.0f;	// By default, things don't fade out automagically
	m_nDistFadeStart = 0;
	m_nDistFadeEnd = 0;
	m_bAlphaOverride = false;
	m_bShadowAlphaOverride = false;
	m_nDistanceFadeMode = CLIENT_ALPHA_DISTANCE_FADE_USE_CENTER;
}

void CClientAlphaProperty::Init( IClientUnknown *pUnk )
{
	m_pOuter = pUnk;
}

IClientUnknown*	CClientAlphaProperty::GetIClientUnknown()
{
	return m_pOuter;
}

void CClientAlphaProperty::SetShadowHandle( ClientShadowHandle_t hShadowHandle )
{
	m_hShadowHandle = hShadowHandle;
}

void CClientAlphaProperty::SetAlphaModulation( uint8 a )
{
	m_nAlpha = a;
}

void CClientAlphaProperty::EnableAlphaModulationOverride( bool bEnable )
{
	m_bAlphaOverride = bEnable;
}

void CClientAlphaProperty::EnableShadowAlphaModulationOverride( bool bEnable )
{
	m_bShadowAlphaOverride = bEnable;
}

// Sets an FX function
void CClientAlphaProperty::SetRenderFX( RenderFx_t nRenderFx, RenderMode_t nRenderMode, float flStartTime, float flDuration )
{
	bool bStartTimeUnspecified = ( flStartTime == std::numeric_limits<float>::max() );
	bool bRenderFxChanged = ( m_nRenderFX != nRenderFx );

	switch( nRenderFx )
	{
	case kRenderFxFadeIn:
	case kRenderFxFadeOut:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		break;

	case kRenderFxFadeSlow:
	case kRenderFxSolidSlow:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 4.0f;
		break;

	case kRenderFxFadeFast:
	case kRenderFxSolidFast:
		contract_assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 1.0f;
		break;
	}

	m_nRenderMode = nRenderMode;
	m_nRenderFX = nRenderFx;
	if ( bRenderFxChanged || !bStartTimeUnspecified )
	{
		m_flRenderFxStartTime = flStartTime;
		m_flRenderFxDuration = flDuration;
	}
}

void CClientAlphaProperty::SetDesyncOffset( int nOffset )
{
	m_nDesyncOffset = nOffset;
}

void CClientAlphaProperty::SetDistanceFadeMode( ClientAlphaDistanceFadeMode_t nFadeMode )
{
	// Necessary since m_nDistanceFadeMode is stored in 1 bit
	static_assert( CLIENT_ALPHA_DISTANCE_FADE_MODE_COUNT <= ( 1 << CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT ) );
	m_nDistanceFadeMode = nFadeMode;
}

// Sets fade parameters
void CClientAlphaProperty::SetFade( float flGlobalFadeScale, float flDistFadeStart, float flDistFadeEnd )
{
	if( flDistFadeStart > flDistFadeEnd )
	{
		V_swap( flDistFadeStart, flDistFadeEnd );
	}

	// If a negative value is provided for the min fade distance, then base it off the max.
	if( flDistFadeStart < 0 )
	{
		flDistFadeStart = flDistFadeEnd + flDistFadeStart;
		if( flDistFadeStart < 0 )
		{
			flDistFadeStart = 0;
		}
	}

	contract_assert( flDistFadeStart >= 0 && flDistFadeStart <= 65535 );
	contract_assert( flDistFadeEnd >= 0 && flDistFadeEnd <= 65535 );

	m_nDistFadeStart = (uint16)flDistFadeStart;
	m_nDistFadeEnd = (uint16)flDistFadeEnd;
	m_flFadeScale = flGlobalFadeScale;
}

int CClientAlphaProperty::ComputeRenderEffectBlend( int nRenderEffect ) const
{
	int nBlend = 0;
	float flOffset = ((int)m_nDesyncOffset) * 363.0;// Use ent index to de-sync these fx

	switch( nRenderEffect ) 
	{
	case kRenderFxPulseSlowWide:
		nBlend = m_nAlpha + 0x40 * std::sin( gpGlobals->curtime * 2 + flOffset );	
		break;

	case kRenderFxPulseFastWide:
		nBlend = m_nAlpha + 0x40 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxPulseFastWider:
		nBlend = ( 0xff * fabs(sin( gpGlobals->curtime * 12 + flOffset ) ) );
		break;

	case kRenderFxPulseSlow:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 2 + flOffset );
		break;

	case kRenderFxPulseFast:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxFadeOut:
	case kRenderFxFadeFast:
	case kRenderFxFadeSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, m_nAlpha, 0 );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxFadeIn:
	case kRenderFxSolidFast:
	case kRenderFxSolidSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, 0, m_nAlpha );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxStrobeSlow:
		nBlend = 20 * sin( gpGlobals->curtime * 4 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFast:
		nBlend = 20 * sin( gpGlobals->curtime * 16 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFaster:
		nBlend = 20 * sin( gpGlobals->curtime * 36 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerSlow:
		nBlend = 20 * (sin( gpGlobals->curtime * 2 ) + sin( gpGlobals->curtime * 17 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerFast:
		nBlend = 20 * (sin( gpGlobals->curtime * 16 ) + sin( gpGlobals->curtime * 23 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxNone:
	default:
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
		break;	
	}
	return nBlend;
}

//-----------------------------------------------------------------------------
// Computes alpha value based on render fx
//-----------------------------------------------------------------------------
uint8 CClientAlphaProperty::ComputeRenderAlpha( ) const
{
	if ( m_nRenderMode == kRenderNone || m_nRenderMode == kRenderEnvironmental )
		return 0;

	int nBlend = 0;

	if ( m_nRenderFX > kRenderNone && m_nRenderFX < kRenderFxMax )
	{
		nBlend = ComputeRenderEffectBlend( m_nRenderFX );
	}
	else
	{
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
	}

	if ( m_bAlphaOverride )
	{
		nBlend = m_pOuter->GetClientRenderable()->OverrideAlphaModulation( m_nAlpha );
	}
	nBlend = clamp( nBlend, 0, 255 );

	return nBlend;
}

// Returns the current alpha modulation
uint8 CClientAlphaProperty::GetAlphaModulation() const
{
	return m_nAlpha;
}

float CClientAlphaProperty::GetMinFadeDist() const
{
	return m_nDistFadeStart;
}

float CClientAlphaProperty::GetMaxFadeDist() const
{
	return m_nDistFadeEnd;
}

float CClientAlphaProperty::GetGlobalFadeScale() const
{
	return m_flFadeScale;
}

bool CClientAlphaProperty::IgnoresZBuffer( void ) const
{
	return m_nRenderMode == kRenderGlow || m_nRenderMode == kRenderWorldGlow;
}

class CBaseEntity;
class IClientEntityListener;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Purpose: a global list of all the entities in the game.  All iteration through
//			entities is done through this object.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// Create interface
static CClientEntityList s_EntityList;
CBaseEntityList *g_pEntityList = &s_EntityList;

// Expose list to engine
static InterfaceReg s_Reg_CClientEntityList_IClientEntityList( side_registry(), []() -> void * { return static_cast<IClientEntityList *>( &s_EntityList ); }, VCLIENTENTITYLIST_INTERFACE_VERSION );;

// Store local pointer to interface for rest of client .dll only 
//  (CClientEntityList instead of IClientEntityList )
CClientEntityList *cl_entitylist = &s_EntityList; 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientEntityList::CClientEntityList( void )
{
	m_iMaxUsedServerIndex = -1;
	m_iMaxServerEnts = 0;
	Release();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CClientEntityList::~CClientEntityList( void )
{
	Release();
}

//-----------------------------------------------------------------------------
// Purpose: Clears all entity lists and releases entities
//-----------------------------------------------------------------------------
void CClientEntityList::Release( void )
{
	// Free all the entities.
	ClientEntityHandle_t iter = FirstHandle();
	while( iter != InvalidHandle() )
	{
		// Try to call release on anything we can.
		IClientNetworkable *pNet = GetClientNetworkableFromHandle( iter );
		if ( pNet )
		{
			pNet->Release();
		}
		else
		{
			// Try to call release on anything we can.
			IClientThinkable *pThinkable = GetClientThinkableFromHandle( iter );
			if ( pThinkable )
			{
				pThinkable->Release();
			}
		}
		RemoveEntity( iter );

		iter = FirstHandle();
	}

	m_iNumServerEnts = 0;
	m_iMaxServerEnts = 0;
	m_iNumClientNonNetworkable = 0;
	m_iMaxUsedServerIndex = -1;
}

IClientNetworkable* CClientEntityList::GetClientNetworkable( int entnum )
{
	contract_assert( entnum >= 0 );
	contract_assert( entnum < MAX_EDICTS );
	return m_EntityCacheInfo[entnum].m_pNetworkable;
}

EntityCacheInfo_t *CClientEntityList::GetClientNetworkableArray()
{
	return m_EntityCacheInfo;
}

void CClientEntityList::SetDormant( int entityIndex, bool bDormant )
{
	contract_assert( entityIndex >= 0 );
	contract_assert( entityIndex < MAX_EDICTS );
	m_EntityCacheInfo[entityIndex].m_bDormant = bDormant;
}

IClientEntity* CClientEntityList::GetClientEntity( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? pEnt->GetIClientEntity() : 0;
}

int CClientEntityList::NumberOfEntities( bool bIncludeNonNetworkable )
{
	if ( bIncludeNonNetworkable == true )
		 return m_iNumServerEnts + m_iNumClientNonNetworkable;

	return m_iNumServerEnts;
}

void CClientEntityList::SetMaxEntities( int maxents )
{
	m_iMaxServerEnts = maxents;
}

int CClientEntityList::GetMaxEntities( void )
{
	return m_iMaxServerEnts;
}

//-----------------------------------------------------------------------------
// Convenience methods to convert between entindex + ClientEntityHandle_t
//-----------------------------------------------------------------------------
int CClientEntityList::HandleToEntIndex( ClientEntityHandle_t handle )
{
	if ( handle == INVALID_EHANDLE )
		return -1;
	CBaseEntity *pEnt = GetBaseEntityFromHandle( handle );
	return pEnt ? pEnt->entindex() : -1; 
}

//-----------------------------------------------------------------------------
// Purpose: Because m_iNumServerEnts != last index
// Output : int
//-----------------------------------------------------------------------------
int CClientEntityList::GetHighestEntityIndex( void )
{
	return m_iMaxUsedServerIndex;
}

void CClientEntityList::RecomputeHighestEntityUsed( void )
{
	m_iMaxUsedServerIndex = -1;

	// Walk backward looking for first valid index
	int i;
	for ( i = MAX_EDICTS - 1; i >= 0; i-- )
	{
		if ( GetListedEntity( i ) != nullptr )
		{
			m_iMaxUsedServerIndex = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a raw CBaseEntity to the entity list.
// Input  : index - 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------

CBaseEntity* CClientEntityList::GetBaseEntity( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? EntityCast<CBaseEntity>( pEnt->GetBaseEntity() ) : 0;
}

ICollideable* CClientEntityList::GetCollideable( int entnum )
{
	IClientUnknown *pEnt = GetListedEntity( entnum );
	return pEnt ? pEnt->GetCollideable() : 0;
}

IClientNetworkable* CClientEntityList::GetClientNetworkableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientNetworkable() : 0;
}

IClientEntity* CClientEntityList::GetClientEntityFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetIClientEntity() : 0;
}

IClientRenderable* CClientEntityList::GetClientRenderableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientRenderable() : 0;
}

CBaseEntity* CClientEntityList::GetBaseEntityFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? EntityCast<CBaseEntity>( pEnt->GetBaseEntity() ) : 0;
}

ICollideable* CClientEntityList::GetCollideableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetCollideable() : 0;
}

IClientThinkable* CClientEntityList::GetClientThinkableFromHandle( ClientEntityHandle_t hEnt )
{
	IClientUnknown *pEnt = GetClientUnknownFromHandle( hEnt );
	return pEnt ? pEnt->GetClientThinkable() : 0;
}

void CClientEntityList::AddPVSNotifier( IClientUnknown *pUnknown )
{
	IClientRenderable *pRen = pUnknown->GetClientRenderable();
	if ( pRen )
	{
		IPVSNotify *pNotify = pRen->GetPVSNotifyInterface();
		if ( pNotify )
		{
			auto index = m_PVSNotifyInfos.insert( m_PVSNotifyInfos.end(), CPVSNotifyInfo{} );
			CPVSNotifyInfo *pInfo = &*index;
			pInfo->m_pNotify = pNotify;
			pInfo->m_pRenderable = pRen;
			pInfo->m_InPVSStatus = 0;

			[[maybe_unused]] bool bInserted = m_PVSNotifierMap.emplace( pUnknown, index ).second;
			contract_assert( bInserted );
		}
	}
}

void CClientEntityList::RemovePVSNotifier( IClientUnknown *pUnknown )
{
	IClientRenderable *pRenderable = pUnknown->GetClientRenderable();
	if ( pRenderable )
	{
		IPVSNotify *pNotify = pRenderable->GetPVSNotifyInterface();
		if ( pNotify )
		{
			auto index = m_PVSNotifierMap.find( pUnknown );
			if ( index == m_PVSNotifierMap.end() )
			{
				Warning( "PVS notifier not in m_PVSNotifierMap\n" );
				contract_assert( false );
				return;
			}

			auto indexIntoPVSNotifyInfos = index->second;
			
			contract_assert( indexIntoPVSNotifyInfos->m_pNotify == pNotify );
			contract_assert( indexIntoPVSNotifyInfos->m_pRenderable == pRenderable );
			
			m_PVSNotifyInfos.erase( indexIntoPVSNotifyInfos );
			m_PVSNotifierMap.erase( index );
			return;
		}
	}

	// If it didn't report itself as a notifier, let's hope it's not in the notifier list now
	// (which would mean that it reported itself as a notifier earlier, but not now).
}

void CClientEntityList::AddListenerEntity( IClientEntityListener *pListener )
{
	if ( ks::find_index( m_entityListeners, pListener ) >= 0 )
	{
		contract_assert( false && "Can't add listeners multiple times\n" );
		return;
	}
	m_entityListeners.push_back( pListener );
}

void CClientEntityList::RemoveListenerEntity( IClientEntityListener *pListener )
{
	ks::erase_first( m_entityListeners, pListener );
}

void CClientEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int entnum = handle.GetEntryIndex();
	EntityCacheInfo_t *pCache = &m_EntityCacheInfo[entnum];

	if ( entnum >= 0 && entnum < MAX_EDICTS )
	{
		// Update our counters.
		m_iNumServerEnts++;
		if ( entnum > m_iMaxUsedServerIndex )
		{
			m_iMaxUsedServerIndex = entnum;
		}

		// Cache its networkable pointer.
		contract_assert( dynamic_cast< IClientUnknown* >( pEnt ) );
		contract_assert( ((IClientUnknown*)pEnt)->GetClientNetworkable() ); // Server entities should all be networkable.
		pCache->m_pNetworkable = ((IClientUnknown*)pEnt)->GetClientNetworkable();
		pCache->m_bDormant = true;
	}

	IClientUnknown *pUnknown = (IClientUnknown*)pEnt;

	// If this thing wants PVS notifications, hook it up.
	AddPVSNotifier( pUnknown );

	// Store it in a special list for fast iteration if it's a CBaseEntity.
	CBaseEntity *pBaseEntity = EntityCast<CBaseEntity>( pUnknown->GetBaseEntity() );
	if ( pBaseEntity )
	{
		pCache->m_BaseEntitiesIndex = m_BaseEntities.emplace( pBaseEntity );

		if ( pBaseEntity->ObjectCaps() & FCAP_SAVE_NON_NETWORKABLE  )
		{
			 m_iNumClientNonNetworkable++;
		}

		//DevMsg(2,"Created %s\n", pBaseEnt->GetClassname() );
		for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
		{
			m_entityListeners[i]->OnEntityCreated( pBaseEntity );
		}
	}
	else
	{
		pCache->m_BaseEntitiesIndex = m_BaseEntities.invalid;
	}

}

void CClientEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int entnum = handle.GetEntryIndex();
	EntityCacheInfo_t *pCache = &m_EntityCacheInfo[entnum];

	if ( entnum >= 0 && entnum < MAX_EDICTS )
	{
		// This is a networkable ent. Clear out our cache info for it.
		pCache->m_pNetworkable = nullptr;
		m_iNumServerEnts--;

		if ( entnum >= m_iMaxUsedServerIndex )
		{
			RecomputeHighestEntityUsed();
		}
	}

	IClientUnknown *pUnknown = (IClientUnknown*)pEnt;

	// If this is a PVS notifier, remove it.
	RemovePVSNotifier( pUnknown );

	CBaseEntity *pBaseEntity = EntityCast<CBaseEntity>( pUnknown->GetBaseEntity() );

	if ( pBaseEntity )
	{
		if ( pBaseEntity->ObjectCaps() & FCAP_SAVE_NON_NETWORKABLE )
		{
			 m_iNumClientNonNetworkable--;
		}

		//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
		for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
		{
			m_entityListeners[i]->OnEntityDeleted( pBaseEntity );
		}
	}

	if ( pCache->m_BaseEntitiesIndex != m_BaseEntities.invalid )
		m_BaseEntities.erase( pCache->m_BaseEntitiesIndex );

	pCache->m_BaseEntitiesIndex = m_BaseEntities.invalid;
}

// Use this to iterate over all the C_BaseEntities.
CBaseEntity* CClientEntityList::FirstBaseEntity() const
{
	const CEntInfo *pList = FirstEntInfo();
	while ( pList )
	{
		if ( pList->m_pEntity )
		{
			IClientUnknown *pUnk = static_cast<IClientUnknown*>( pList->m_pEntity );
			CBaseEntity *pRet = EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
			if ( pRet )
				return pRet;
		}
		pList = pList->m_pNext;
	}

	return nullptr;

}

CBaseEntity* CClientEntityList::NextBaseEntity( CBaseEntity *pEnt ) const
{
	if ( pEnt == nullptr )
		return FirstBaseEntity();

	// Run through the list until we get a CBaseEntity.
	const CEntInfo *pList = GetEntInfoPtr( pEnt->GetRefEHandle() );
	if ( pList )
	{
		pList = NextEntInfo(pList);
	}

	while ( pList )
	{
		if ( pList->m_pEntity )
		{
			IClientUnknown *pUnk = static_cast<IClientUnknown*>( pList->m_pEntity );
			CBaseEntity *pRet = EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
			if ( pRet )
				return pRet;
		}
		pList = pList->m_pNext;
	}
	
	return nullptr; 
}

// -------------------------------------------------------------------------------------------------- //
// CAllBaseEntityIterator
// -------------------------------------------------------------------------------------------------- //
CAllBaseEntityIterator::CAllBaseEntityIterator()
{
	Restart();
}

void CAllBaseEntityIterator::Restart()
{
	m_CurBaseEntity = 0;
}

CBaseEntity* CAllBaseEntityIterator::Next()
{
	// Walks slots, so entities removed mid-iteration are simply skipped
	const auto &list = ClientEntityList().m_BaseEntities;
	while ( m_CurBaseEntity < list.slot_count() )
	{
		unsigned short h = m_CurBaseEntity++;
		if ( list.contains( h ) )
			return list[h];
	}
	return nullptr;
}

// -------------------------------------------------------------------------------------------------- //
// CBaseEntityIterator
// -------------------------------------------------------------------------------------------------- //
CBaseEntityIterator::CBaseEntityIterator()
{
	Restart();
}

void CBaseEntityIterator::Restart()
{
	m_CurBaseEntity = 0;
}

CBaseEntity* CBaseEntityIterator::Next()
{
	// Skip dormant entities
	const auto &list = ClientEntityList().m_BaseEntities;
	while ( m_CurBaseEntity < list.slot_count() )
	{
		unsigned short h = m_CurBaseEntity++;
		if ( !list.contains( h ) )
			continue;

		CBaseEntity *pRet = list[h];
		if (!pRet->IsDormant())
			return pRet;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
bool	CClientEntityList::IsHandleValid( ClientEntityHandle_t handle ) const
{
	return EntityFromHandle( handle ) != nullptr;
}

IClientUnknown* CClientEntityList::GetListedEntity( int entnum )
{
	IHandleEntity* pHandle = LookupEntityByNetworkIndex( entnum );
	return static_cast< IClientUnknown* >( pHandle );
}

IClientUnknown* CClientEntityList::GetClientUnknownFromHandle( ClientEntityHandle_t hEnt )
{
	return (IClientUnknown*)LookupEntity( hEnt );
}

std::list<CClientEntityList::CPVSNotifyInfo>& CClientEntityList::GetPVSNotifiers()
{
	return m_PVSNotifyInfos;
}

//-----------------------------------------------------------------------------
// Convenience methods to convert between entindex + ClientEntityHandle_t
//-----------------------------------------------------------------------------
ClientEntityHandle_t CClientEntityList::EntIndexToHandle( int entnum )
{
	if ( entnum < -1 )
		return INVALID_EHANDLE;
	IClientUnknown *pUnk = GetListedEntity( entnum );
	return pUnk ? pUnk->GetRefEHandle() : INVALID_EHANDLE;
}
