// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2006 Valve Corporation

module;
#include <math.h>
#include <time.h>
#include "declare_class.h"
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "worldsize.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "edict.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ihandleentity.h"
#include "irecipientfilter.h"
#include "engine/ivdebugoverlay.h"
#include "server_class.h"
#include "reflect_sendtable.h"
#include "shareddefs.h"
#include "reflect_annotations.h"
#include "model_types.h"
#include "predictable_entity.h"
#include "dt_recv.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :nav_4;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.core.tslist;
import ks.core.vprof;
import ks.math.ssemath;
import ks.tier1.convar;
import ks.tier1.envelope;
import ks.tier1.fixed_pool;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import ks.tier1.keyvalues;

class CBaseEntity;
class CSound;
struct inputdata_t;

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// Some enumerations needed by CSoundEnt
//-----------------------------------------------------------------------------

// identifiers passed to functions that can operate on either list, to indicate which list to operate on.
static constexpr auto SOUNDLISTTYPE_FREE = 1;
static constexpr auto SOUNDLISTTYPE_ACTIVE = 2;

LINK_ENTITY_TO_CLASS( soundent, CSoundEnt );

static CSoundEnt *g_pSoundEnt = nullptr;

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSound )

//=========================================================
// CSound - Clear - zeros all fields for a sound
//=========================================================
void CSound::Clear ( void )
{
	m_vecOrigin			= vec3_origin;
	m_iType				= 0;
	m_iVolume			= 0;
	m_flOcclusionScale	= 0;
	m_flExpireTime		= 0;
	m_bNoExpirationTime = false;
	m_iNext				= SOUNDLIST_EMPTY;
	m_iNextAudible		= SOUNDLIST_EMPTY;
}

//=========================================================
// Reset - clears the volume, origin, and type for a sound,
// but doesn't expire or unlink it. 
//=========================================================
void CSound::Reset ( void )
{
	m_vecOrigin		= vec3_origin;
	m_iType			= 0;
	m_iVolume		= 0;
	m_iNext			= SOUNDLIST_EMPTY;
}

//=========================================================
// FIsSound - returns true if the sound is an Audible sound
//=========================================================
bool CSound::FIsSound ( void )
{
	switch( SoundTypeNoContext() )
	{
	case SOUND_COMBAT:
	case SOUND_WORLD:
	case SOUND_PLAYER:
	case SOUND_DANGER:
	case SOUND_DANGER_SNIPERONLY:
	case SOUND_THUMPER:
	case SOUND_BULLET_IMPACT:
	case SOUND_BUGBAIT:
	case SOUND_PHYSICS_DANGER:
	case SOUND_MOVE_AWAY:
	case SOUND_PLAYER_VEHICLE:
		return true;

	default:
		return false;
	}
}

//=========================================================
// FIsScent - returns true if the sound is actually a scent
// do we really need this function? If a sound isn't a sound,
// it must be a scent. (sjb)
//=========================================================
bool CSound::FIsScent ( void )
{
	switch( m_iType )
	{
	case SOUND_CARCASS:
	case SOUND_MEAT:
	case SOUND_GARBAGE:
		return true;

	default:
		return false;
	}
}

//---------------------------------------------------------
// Returns the sound origin
//---------------------------------------------------------
const Vector& CSound::GetSoundOrigin( void )
{
	if ( ( m_iType & SOUND_CONTEXT_FOLLOW_OWNER ) != 0 )
	{
		if( m_hOwner.Get() != nullptr )
			return m_hOwner->GetAbsOrigin();
	}
	return m_vecOrigin;
}

//---------------------------------------------------------
// This function returns the spot the listener should be
// interested in if he hears the sound. MOST of the time,
// this spot is the same as the sound's origin. But sometimes
// (like with bullet impacts) the entity that owns the 
// sound is more interesting than the actual location of the
// sound effect.
//---------------------------------------------------------
const Vector &CSound::GetSoundReactOrigin( void )
{
	
	// Check pure types.
	switch( m_iType )
	{
	case SOUND_BULLET_IMPACT:
	case SOUND_PHYSICS_DANGER:
		if( m_hOwner.Get() != nullptr )
		{
			// We really want the origin of this sound's 
			// owner.
			return m_hOwner->GetAbsOrigin();
		}
		else
		{
			// If the owner is somehow invalid, we'll settle
			// for the sound's origin rather than a crash.
			return GetSoundOrigin();
		}
		break;
	}

	if( m_iType & SOUND_CONTEXT_REACT_TO_SOURCE )
	{
		if( m_hOwner.Get() != nullptr )
		{
			return m_hOwner->GetAbsOrigin();
		}
	}

	// Check for types with additional context.
	if( m_iType & SOUND_DANGER )
	{
		if( (m_iType & SOUND_CONTEXT_FROM_SNIPER) )
		{
			if( m_hOwner.Get() != nullptr )
			{
				// Be afraid of the sniper's location, not where the bullet will hit.
				return m_hOwner->GetAbsOrigin();
			}
			else
			{
				return GetSoundOrigin();
			}
		}
	}

	return GetSoundOrigin();
}

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Class factory methods
//-----------------------------------------------------------------------------
bool CSoundEnt::InitSoundEnt()
{
	///!!!LATER - do we want a sound ent in deathmatch? (sjb)
	g_pSoundEnt = (CSoundEnt*)CBaseEntity::Create( "soundent", vec3_origin, vec3_angle, GetWorldEntity() );
	if ( !g_pSoundEnt )
	{
		Warning( "**COULD NOT CREATE SOUNDENT**\n" );
		return false;
	}
	g_pSoundEnt->AddEFlags( EFL_KEEP_ON_RECREATE_ENTITIES );
	return true;
}

void CSoundEnt::ShutdownSoundEnt()
{
	if ( g_pSoundEnt )
	{
		g_pSoundEnt->FreeList();
		g_pSoundEnt = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Construction, destruction
//-----------------------------------------------------------------------------
CSoundEnt::CSoundEnt()
{
}

CSoundEnt::~CSoundEnt()
{
}

//=========================================================
// Spawn 
//=========================================================
void CSoundEnt::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	Initialize();

	SetNextThink( gpGlobals->curtime + 1 );
}

void CSoundEnt::OnRestore_Impl()
{
	BaseClass::OnRestore_Impl();

	// Make sure the singleton points to the restored version of this.
	if ( g_pSoundEnt )
	{
		contract_assert( g_pSoundEnt != this );
		UTIL_Remove( g_pSoundEnt );
	}
	g_pSoundEnt = this;
}

//=========================================================
// Think - at interval, the entire active sound list is checked
// for sounds that have ExpireTimes less than or equal
// to the current world time, and these sounds are deallocated.
//=========================================================
void CSoundEnt::Think_Impl ( void )
{
	int iSound;
	int iPreviousSound;

	SetNextThink( gpGlobals->curtime + 0.1 );// how often to check the sound list.

	iPreviousSound = SOUNDLIST_EMPTY;
	iSound = m_iActiveSound; 

	while ( iSound != SOUNDLIST_EMPTY )
	{
		if ( (m_SoundPool[ iSound ].m_flExpireTime <= gpGlobals->curtime && (!m_SoundPool[ iSound ].m_bNoExpirationTime)) || !m_SoundPool[iSound].ValidateOwner() )
		{
			int iNext = m_SoundPool[ iSound ].m_iNext;

			if( displaysoundlist.GetInt() == 1 )
			{
				Msg("  Removed Sound: %d (Time:%f)\n", m_SoundPool[ iSound ].SoundType(), gpGlobals->curtime );
			}
			if( displaysoundlist.GetInt() == 2 && m_SoundPool[ iSound ].IsSoundType( SOUND_DANGER ) )
			{
				Msg("  Removed Danger Sound: %d (time:%f)\n", m_SoundPool[ iSound ].SoundType(), gpGlobals->curtime );
			}

			// move this sound back into the free list
			FreeSound( iSound, iPreviousSound );

			iSound = iNext;
		}
		else
		{
			if( displaysoundlist.GetBool() )
			{
				Vector forward, right, up;
				GetVectors( &forward, &right, &up );
				byte r, g, b;

				// Default to yellow.
				r = 255;
				g = 255;
				b = 0;

				CSound *pSound = &m_SoundPool[ iSound ];

				if( pSound->IsSoundType( SOUND_DANGER ) )
				{
					r = 255;
					g = 0;
					b = 0;
				}

				if( displaysoundlist.GetInt() == 1 || (displaysoundlist.GetInt() == 2 && pSound->IsSoundType( SOUND_DANGER ) ) )
				{
					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + forward * pSound->Volume(), r,g,b, false, 0.1 );
					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - forward * pSound->Volume(), r,g,b, false, 0.1 );

					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + right * pSound->Volume(), r,g,b, false, 0.1 );
					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - right * pSound->Volume(), r,g,b, false, 0.1 );

					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + up * pSound->Volume(), r,g,b, false, 0.1 );
					NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - up * pSound->Volume(), r,g,b, false, 0.1 );

					if( pSound->m_flOcclusionScale != 1.0 )
					{
						// Draw the occluded radius, too.
						r = 0; g = 150; b = 255;
						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + forward * pSound->OccludedVolume(), r,g,b, false, 0.1 );
						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - forward * pSound->OccludedVolume(), r,g,b, false, 0.1 );

						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + right * pSound->OccludedVolume(), r,g,b, false, 0.1 );
						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - right * pSound->OccludedVolume(), r,g,b, false, 0.1 );

						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() + up * pSound->OccludedVolume(), r,g,b, false, 0.1 );
						NDebugOverlay::Line( pSound->GetSoundOrigin(), pSound->GetSoundOrigin() - up * pSound->OccludedVolume(), r,g,b, false, 0.1 );
					}
				}

				DevMsg( 2, "Soundlist: %d / %d  (%d)\n", ISoundsInList( SOUNDLISTTYPE_ACTIVE ),ISoundsInList( SOUNDLISTTYPE_FREE ), ISoundsInList( SOUNDLISTTYPE_ACTIVE ) - m_cLastActiveSounds );
				m_cLastActiveSounds = ISoundsInList ( SOUNDLISTTYPE_ACTIVE );
			}

			iPreviousSound = iSound;
			iSound = m_SoundPool[ iSound ].m_iNext;
		}
	}

}

//=========================================================
// Precache - dummy function
//=========================================================
void CSoundEnt::Precache_Impl ( void )
{
}

//=========================================================
// FreeSound - clears the passed active sound and moves it 
// to the top of the free list. TAKE CARE to only call this
// function for sounds in the Active list!!
//=========================================================
void CSoundEnt::FreeSound ( int iSound, int iPrevious )
{
	if ( !g_pSoundEnt )
	{
		// no sound ent!
		return;
	}

	if ( iPrevious != SOUNDLIST_EMPTY )
	{
		// iSound is not the head of the active list, so
		// must fix the index for the Previous sound
		g_pSoundEnt->m_SoundPool[ iPrevious ].m_iNext = g_pSoundEnt->m_SoundPool[ iSound ].m_iNext;
	}
	else 
	{
		// the sound we're freeing IS the head of the active list.
		g_pSoundEnt->m_iActiveSound = g_pSoundEnt->m_SoundPool [ iSound ].m_iNext;
	}

	// make iSound the head of the Free list.
	g_pSoundEnt->m_SoundPool[ iSound ].m_iNext = g_pSoundEnt->m_iFreeSound;
	g_pSoundEnt->m_iFreeSound = iSound;
}

void CSoundEnt::FreeSound( int iSound )
{
	// no sound ent!
	if ( !g_pSoundEnt || ( iSound == SOUNDLIST_EMPTY ) )
		return;

	int iPrevious = SOUNDLIST_EMPTY;
	for ( int i = g_pSoundEnt->m_iActiveSound; i != SOUNDLIST_EMPTY; iPrevious = i, i = g_pSoundEnt->m_SoundPool[ i ].m_iNext )
	{
		if ( i == iSound )
		{
			FreeSound( iSound, iPrevious );
			return;
		}
	}

	Warning( "Attempted to free unknown sound %d!\n", iSound );
}

//=========================================================
// IAllocSound - moves a sound from the Free list to the 
// Active list returns the index of the alloc'd sound
//=========================================================
int CSoundEnt::IAllocSound( void )
{
	int iNewSound;

	if ( m_iFreeSound == SOUNDLIST_EMPTY )
	{
		// no free sound!
		if ( developer.GetInt() >= 2 )
			Msg( "Free Sound List is full!\n" );

		return SOUNDLIST_EMPTY;
	}

	// there is at least one sound available, so move it to the
	// Active sound list, and return its SoundPool index.
	
	iNewSound = m_iFreeSound;// copy the index of the next free sound

	m_iFreeSound = m_SoundPool[ m_iFreeSound ].m_iNext;// move the index down into the free list. 

	m_SoundPool[ iNewSound ].m_iNext = m_iActiveSound;// point the new sound at the top of the active list.

	m_iActiveSound = iNewSound;// now make the new sound the top of the active list. You're done.

#ifdef DEBUG
	m_SoundPool[ iNewSound ].m_iMyIndex = iNewSound;
#endif // DEBUG

	return iNewSound;
}

//=========================================================
// InsertSound - Allocates a free sound and fills it with 
// sound info.
//=========================================================
int CSoundEnt::InsertSound ( int iType, const Vector &vecOrigin, int iVolume, float flDuration, CBaseEntity *pOwner, int soundChannelIndex, CBaseEntity *pSoundTarget )
{
	int	iThisSound;

	if ( !g_pSoundEnt )
		return SOUNDLIST_EMPTY;

	if( soundChannelIndex == SOUNDENT_CHANNEL_UNSPECIFIED )
	{
		// No sound channel specified. So just make a new sound.
		iThisSound = g_pSoundEnt->IAllocSound();
	}
	else
	{
		// If this entity has already got a sound in the soundlist that's on this
		// channel, update that sound. Otherwise add a new one.
		iThisSound = g_pSoundEnt->FindOrAllocateSound( pOwner, soundChannelIndex );
	}

	if ( iThisSound == SOUNDLIST_EMPTY )
	{
		DevMsg( "Could not AllocSound() for InsertSound() (Game DLL)\n" );
		return SOUNDLIST_EMPTY;
	}

	CSound *pSound;

	pSound = &g_pSoundEnt->m_SoundPool[ iThisSound ];

	pSound->SetSoundOrigin( vecOrigin );
	pSound->m_iType = iType;
	pSound->m_iVolume = iVolume;
	pSound->m_flOcclusionScale = 0.5;
	if ( flDuration != std::numeric_limits<float>::max() )
	{
		pSound->m_flExpireTime = gpGlobals->curtime + flDuration;
		pSound->m_bNoExpirationTime = false;
	}
	else
	{
		pSound->m_flExpireTime = std::numeric_limits<float>::max();
		pSound->m_bNoExpirationTime = true;
	}
	pSound->m_hOwner.Set( pOwner );
	pSound->m_hTarget.Set( pSoundTarget );
	pSound->m_ownerChannelIndex = soundChannelIndex;

	// Keep track of whether this sound had an owner when it was made. If the sound has a long duration,
	// the owner could disappear by the time someone hears this sound, so we have to look at this boolean
	// and throw out sounds who have a NULL owner but this field set to true. (sjb) 12/2/2005
	if( pOwner )
	{
		pSound->m_bHasOwner = true;
	}
	else
	{
		pSound->m_bHasOwner = false;
	}

	if( displaysoundlist.GetInt() == 1 )
	{
		Msg("  Added Sound! Type:%d  Duration:%f (Time:%f)\n", pSound->SoundType(), flDuration, gpGlobals->curtime );
	}
	if( displaysoundlist.GetInt() == 2 && (iType & SOUND_DANGER) )
	{
		Msg("  Added Danger Sound! Duration:%f (Time:%f)\n", flDuration, gpGlobals->curtime );
	}

	return iThisSound;
}

//---------------------------------------------------------
//---------------------------------------------------------
int CSoundEnt::FindOrAllocateSound( CBaseEntity *pOwner, int soundChannelIndex )
{
	int iSound = m_iActiveSound; 

	while ( iSound != SOUNDLIST_EMPTY )
	{
		CSound &sound = m_SoundPool[iSound];
		
		if ( sound.m_ownerChannelIndex == soundChannelIndex && sound.m_hOwner == pOwner )
		{
			return iSound;
		}

		iSound = sound.m_iNext;
	}

	return IAllocSound();
}

//=========================================================
// Initialize - clears all sounds and moves them into the 
// free sound list.
//=========================================================
void CSoundEnt::Initialize ( void )
{
  	int i;
	int iSound;

	m_cLastActiveSounds;
	m_iFreeSound = 0;
	m_iActiveSound = SOUNDLIST_EMPTY;

	// In SP, we should only use the first 64 slots so save/load works right.
	// In MP, have one for each player and 32 extras.
	int nTotalSoundsInPool = MAX_WORLD_SOUNDS_SP;
	if ( gpGlobals->maxClients > 1 )
		nTotalSoundsInPool = std::min( ( int ) MAX_WORLD_SOUNDS_MP, gpGlobals->maxClients + 32 );

	if ( gpGlobals->maxClients+16 > nTotalSoundsInPool )
	{
		Warning( "CSoundEnt pool is low on sounds due to high number of clients.\n" );
	}

	for ( i = 0 ; i < nTotalSoundsInPool ; i++ )
	{
		// clear all sounds, and link them into the free sound list.
		m_SoundPool[ i ].Clear();
		m_SoundPool[ i ].m_iNext = i + 1;
	}

	m_SoundPool[ i - 1 ].m_iNext = SOUNDLIST_EMPTY;// terminate the list here.

	// now reserve enough sounds for each client
	for ( i = 0 ; i < gpGlobals->maxClients ; i++ )
	{
		iSound = IAllocSound();

		if ( iSound == SOUNDLIST_EMPTY )
		{
			DevMsg( "Could not AllocSound() for Client Reserve! (DLL)\n" );
			return;
		}

		m_SoundPool[ iSound ].m_bNoExpirationTime = true;
	}
}

//=========================================================
// ISoundsInList - returns the number of sounds in the desired
// sound list.
//=========================================================
int CSoundEnt::ISoundsInList ( int iListType )
{
	int i;
	int iThisSound = SOUNDLIST_EMPTY;

	if ( iListType == SOUNDLISTTYPE_FREE )
	{
		iThisSound = m_iFreeSound;
	}
	else if ( iListType == SOUNDLISTTYPE_ACTIVE )
	{
		iThisSound = m_iActiveSound;
	}
	else
	{
		Msg( "Unknown Sound List Type!\n" );
	}

	if ( iThisSound == SOUNDLIST_EMPTY )
	{
		return 0;
	}

	i = 0;

	while ( iThisSound != SOUNDLIST_EMPTY )
	{
		i++;

		iThisSound = m_SoundPool[ iThisSound ].m_iNext;
	}

	return i;
}

//=========================================================
// ActiveList - returns the head of the active sound list
//=========================================================
int CSoundEnt::ActiveList ( void )
{
	if ( !g_pSoundEnt )
	{
		return SOUNDLIST_EMPTY;
	}

	return g_pSoundEnt->m_iActiveSound;
}

//=========================================================
// FreeList - returns the head of the free sound list
//=========================================================
int CSoundEnt::FreeList ( void )
{
	if ( !g_pSoundEnt )
	{
		return SOUNDLIST_EMPTY;
	}

	return g_pSoundEnt->m_iFreeSound;
}

//=========================================================
// SoundPointerForIndex - returns a pointer to the instance
// of CSound at index's position in the sound pool.
//=========================================================
CSound*	CSoundEnt::SoundPointerForIndex( int iIndex )
{
	if ( !g_pSoundEnt )
	{
		return nullptr;
	}

	if ( iIndex > ( MAX_WORLD_SOUNDS_MP - 1 ) )
	{
		Msg( "SoundPointerForIndex() - Index too large!\n" );
		return nullptr;
	}

	if ( iIndex < 0 )
	{
		Msg( "SoundPointerForIndex() - Index < 0!\n" );
		return nullptr;
	}

	return &g_pSoundEnt->m_SoundPool[ iIndex ];
}

//=========================================================
// Clients are numbered from 1 to MAXCLIENTS, but the client
// reserved sounds in the soundlist are from 0 to MAXCLIENTS - 1,
// so this function ensures that a client gets the proper index
// to his reserved sound in the soundlist.
//=========================================================
int CSoundEnt::ClientSoundIndex ( edict_t *pClient )
{
	int iReturn = ENTINDEX( pClient ) - 1;

	return iReturn;
}

//-----------------------------------------------------------------------------
// Purpose: Return the loudest sound of the specified type at "earposition"
//-----------------------------------------------------------------------------
CSound*	CSoundEnt::GetLoudestSoundOfType( int iType, const Vector &vecEarPosition )
{
	CSound *pLoudestSound = nullptr;

	int iThisSound; 
	int	iBestSound = SOUNDLIST_EMPTY;
	float flBestDist = MAX_COORD_RANGE*MAX_COORD_RANGE;// so first nearby sound will become best so far.
	float flDist;
	CSound *pSound;

	iThisSound = ActiveList();

	while ( iThisSound != SOUNDLIST_EMPTY )
	{
		pSound = SoundPointerForIndex( iThisSound );

		if ( pSound && pSound->m_iType == iType && pSound->ValidateOwner() )
		{
			flDist = ( pSound->GetSoundOrigin() - vecEarPosition ).Length();

			//FIXME: This doesn't match what's in Listen()
			//flDist = UTIL_DistApprox( pSound->GetSoundOrigin(), vecEarPosition );

			if ( flDist <= pSound->m_iVolume && flDist < flBestDist )
			{
				pLoudestSound = pSound;

				iBestSound = iThisSound;
				flBestDist = flDist;
			}
		}

		iThisSound = pSound->m_iNext;
	}

	return pLoudestSound;
}

//-----------------------------------------------------------------------------
// Purpose: Inserts an AI sound into the world sound list.
//-----------------------------------------------------------------------------
class CAISound : public CPointEntity
{
public:
	CAISound()
	{
		// Initialize these new keyvalues appropriately
		// in order to support legacy instances of ai_sound.
		m_iSoundContext = 0x00000000;
		m_iVolume = 0;
		m_flDuration = 0.3;
	}

	DECLARE_CLASS( CAISound, CPointEntity );

	DECLARE_DATADESC();

	// data
	[[= ks::reflect::Key{ .name = "soundtype" } ]] int			m_iSoundType;
	[[= ks::reflect::Key{ .name = "soundcontext" } ]] int			m_iSoundContext;
	[[= ks::reflect::Key{ .name = "volume" } ]] int			m_iVolume;
	[[= ks::reflect::Key{ .name = "duration" } ]] float		m_flDuration;
	[[= ks::reflect::Key{ .name = "locationproxy" } ]] string_t	m_iszProxyEntityName;

	// Input handlers
	[[= ks::reflect::Input{ .name = "InsertSound", .type = FIELD_INTEGER } ]] void InputInsertSound( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EmitAISound", .type = FIELD_VOID } ]] void InputEmitAISound( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( ai_sound, CAISound );

IMPLEMENT_REFLECT_DATAMAP( CAISound )

//-----------------------------------------------------------------------------
// Purpose: *** OBSOLETE **** Here for legacy support only!
//-----------------------------------------------------------------------------
void CAISound::InputInsertSound( inputdata_t &inputdata )
{
	int iVolume;

	iVolume = inputdata.value.Int();

	Vector vecLocation = GetAbsOrigin();

	if( m_iszProxyEntityName != NULL_STRING )
	{
		CBaseEntity *pProxy = gEntList.FindEntityByName( nullptr, m_iszProxyEntityName );

		if( pProxy )
		{
			vecLocation = pProxy->GetAbsOrigin();
		}
		else
		{
			DevWarning("Warning- ai_sound cannot find proxy entity named '%s'. Using self.\n", STRING(m_iszProxyEntityName) );
		}
	}

	g_pSoundEnt->InsertSound( m_iSoundType, vecLocation, iVolume, m_flDuration, this );
}

void CAISound::InputEmitAISound( inputdata_t &inputdata )
{
	Vector vecLocation = GetAbsOrigin();

	if( m_iszProxyEntityName != NULL_STRING )
	{
		CBaseEntity *pProxy = gEntList.FindEntityByName( nullptr, m_iszProxyEntityName );

		if( pProxy )
		{
			vecLocation = pProxy->GetAbsOrigin();
		}
		else
		{
			DevWarning("Warning- ai_sound cannot find proxy entity named '%s'. Using self.\n", STRING(m_iszProxyEntityName) );
		}
	}

	g_pSoundEnt->InsertSound( m_iSoundType | m_iSoundContext, vecLocation, m_iVolume, m_flDuration, this );
}

bool CSound::DoesSoundExpire() const
{
	return m_bNoExpirationTime == false;
}

float CSound::SoundExpirationTime() const
{
	return m_bNoExpirationTime ? std::numeric_limits<float>::max() : m_flExpireTime;
}

bool CSound::IsSoundType( int nSoundFlags ) const
{
	return (m_iType & nSoundFlags) != 0;
}

int CSound::SoundType( ) const
{
	return m_iType;
}

int CSound::SoundContext( ) const
{
	return m_iType & ALL_CONTEXTS;
}

int CSound::SoundTypeNoContext( ) const
{
	return m_iType & ~ALL_CONTEXTS;
}

int CSound::Volume( ) const
{
	return m_iVolume;
}

int CSound::NextSound() const
{
	return m_iNext;
}

int CSound::SoundChannel( void ) const
{
	return m_ownerChannelIndex;
}

// The owner is considered valid if:
//		-The sound never had an assigned owner (quite common)
//		-The sound was assigned an owner and that owner still exists
bool CSound::ValidateOwner( void ) const
{
	return ( !m_bHasOwner || (m_hOwner.Get() != nullptr) );
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
bool CSoundEnt::IsEmpty( void ) 
{ 
	return m_iActiveSound == SOUNDLIST_EMPTY; 
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

static ConVar soundpatch_captionlength( side_registry(), "soundpatch_captionlength", "2.0", FCVAR_REPLICATED, "How long looping soundpatch captions should display for." );

// Envelope
// This is a class that controls a ramp for a sound (pitch / volume / etc)
class CSoundEnvelope
{
public:
	DECLARE_SIMPLE_DATADESC();

	CSoundEnvelope()
	{
		m_current = 0.0f;
		m_target = 0.0f;
		m_rate = 0.0f;
		m_forceupdate = false;
	}

	void SetTarget( float target, float deltaTime );
	void SetValue( float value );
	bool ShouldUpdate( void );
	void Update( float time );
	inline float	Value( void ) { return m_current; }

private:
	float	m_current;
	float	m_target;
	float	m_rate;
	bool	m_forceupdate;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundEnvelope )

//-----------------------------------------------------------------------------
// Purpose: Set the new target value for this ramp.  Reach this target in deltaTime
//			seconds from now
// Input  : target - new target value
//			deltaTime - time to reach target
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetTarget( float target, float deltaTime )
{
	float deltaValue = target - m_current;

	if ( deltaValue && deltaTime > 0 )
	{
		m_target = target;
		m_rate = std::max<std::common_type_t<double, float>>( 0.1, fabs(deltaValue / deltaTime) );
	}
	else
	{
		if ( target != m_current )
		{
			m_forceupdate = true;
		}

		SetValue( target );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Instantaneously set the value of this ramp
// Input  : value - new value
//-----------------------------------------------------------------------------
void CSoundEnvelope::SetValue( float value )
{
	if ( m_target != value )
	{
		m_forceupdate = true;
	}

	m_current = m_target = value;
	m_rate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if I need to update this envelope
// Output : Returns true if this envelope is changing
//-----------------------------------------------------------------------------
bool CSoundEnvelope::ShouldUpdate( void )
{
	if ( m_forceupdate )
	{
		m_forceupdate = false;
		return true;
	}

	if ( m_current != m_target )
	{
		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Update the envelope for the current frame time
// Input  : time - amount of time that has passed
//-----------------------------------------------------------------------------
void CSoundEnvelope::Update( float deltaTime )
{
	m_current = Approach( m_target, m_current, m_rate * deltaTime );
}

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	DECLARE_SIMPLE_DATADESC();

	CCopyRecipientFilter() : m_Flags(0) {}

	void Init( IRecipientFilter *pSrc )
	{
		m_Flags = FLAG_ACTIVE;
		if ( pSrc->IsReliable() )
		{
			m_Flags |= FLAG_RELIABLE;
		}

		if ( pSrc->IsInitMessage() )
		{
			m_Flags |= FLAG_INIT_MESSAGE;
		}

		for ( int i = 0; i < pSrc->GetRecipientCount(); i++ )
		{
			int index = pSrc->GetRecipientIndex( i );
			
			if ( index >= 0 )
				m_Recipients.push_back( index );
		}
	}

	bool IsActive() const 
	{
		return (m_Flags & FLAG_ACTIVE) != 0;
	}

	virtual bool IsReliable( void ) const
	{
		return (m_Flags & FLAG_RELIABLE) != 0;
	}

	virtual int	GetRecipientCount( void ) const
	{
		return std::ssize( m_Recipients );
	}

	virtual int	GetRecipientIndex( int slot ) const
	{
		return m_Recipients[ slot ];
	}

	virtual bool IsInitMessage( void ) const
	{
		return (m_Flags & FLAG_INIT_MESSAGE) != 0;
	}

	virtual bool AddRecipient( CBasePlayer *player )
	{
		contract_assert( player );

		int index = player->entindex();

		if ( index < 0 )
			return false;

		// Already in list
		if ( ks::find_index( m_Recipients, index ) != -1 )
			return false;

		m_Recipients.push_back( index );
		return true;
	}

private:
	enum
	{
		FLAG_ACTIVE = 0x1,
		FLAG_RELIABLE = 0x2,
		FLAG_INIT_MESSAGE = 0x4,
	};

	int m_Flags;
	std::vector< int > m_Recipients;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CCopyRecipientFilter )

// This is the a basic sound controller, a "patch"
// It has envelopes for pitch and volume and can manage state changes to those
class CSoundPatch : public ks::pooled< CSoundPatch >
{
public:
	DECLARE_SIMPLE_DATADESC();

	static int g_SoundPatchCount;
	CSoundPatch()
	{
		g_SoundPatchCount++;
		m_iszSoundName = NULL_STRING;
		m_iszSoundScriptName = NULL_STRING;
		m_flCloseCaptionDuration = soundpatch_captionlength.GetFloat();
		m_soundOrigin.Init();
		m_soundEntityIndex = -1;
		m_guid = -1;

		m_hSoundScriptHash = SOUNDEMITTER_INVALID_HASH;
		m_nSoundEntryVersion = 1;
	}
	~CSoundPatch()
	{
		g_SoundPatchCount--;
	}

	void	Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
				soundlevel_t iSoundLevel, const Vector *pSoundOrigin, float scriptVolume = 1.0f );
	void	ChangePitch( float pitchTarget, float deltaTime );
	void	ChangeVolume( float volumeTarget, float deltaTime );
	void	FadeOut( float deltaTime, bool destroyOnFadeout );
	float	GetPitch( void );
	float	GetVolume( void );
	string_t GetName() { return m_iszSoundName; };

	string_t GetScriptName() { return m_iszSoundScriptName; }
	// UNDONE: Don't call this, use the controller to shut down
	void	Shutdown( void );
	bool	Update( float time, float deltaTime );
	void	Reset( void );
	void	StartSound( float flStartTime = 0 );
	void	ResumeSound( void );
	int		IsPlaying( void ) { return m_isPlaying; }
	float	GetShutdownTime( void ) const { return m_shutdownTime; } // TERROR: debugging
	void	AddPlayerPost( CBasePlayer *pPlayer );
	void	SetCloseCaptionDuration( float flDuration ) { m_flCloseCaptionDuration = flDuration; }

	void	SetBaseFlags( int iFlags ) { m_baseFlags = iFlags; }
	
	// Returns the ent index
	int		EntIndex() const;

private:
	// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
	// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
	float	GetVolumeForEngine( void );

private:
	CSoundEnvelope	m_pitch;
	CSoundEnvelope	m_volume;

	int				m_guid;
	soundlevel_t	m_soundlevel;
	float			m_shutdownTime;
	string_t		m_iszSoundName;
	string_t		m_iszSoundScriptName;
	HSOUNDSCRIPTHASH m_hSoundScriptHash;
	int             m_nSoundEntryVersion;
	EHANDLE			m_hEnt;
	int				m_entityChannel;
	int				m_soundEntityIndex;
	Vector			m_soundOrigin;
	int				m_flags;
	int				m_baseFlags;
	int				m_isPlaying;
	float			m_flScriptVolume;	// Volume for this sound in sounds.txt
	CCopyRecipientFilter m_Filter;

	float			m_flCloseCaptionDuration;

	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< CSoundPatch >;
};

int CSoundPatch::g_SoundPatchCount = 0;

static void report_soundpatch( const CCommand &args );
static ConCommand report_soundpatch_command( side_registry(), "report_soundpatch", report_soundpatch, "reports sound patch count" );
static void report_soundpatch( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	Msg("Current sound patches: %d\n", CSoundPatch::g_SoundPatchCount );
}
ks::fixed_pool CSoundPatch::s_Allocator( sizeof(CSoundPatch), 64, ks::fixed_pool::grow::fast, "CSoundPatch pool", alignof(CSoundPatch) );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CSoundPatch )

//-----------------------------------------------------------------------------
// Purpose: Setup the patch
// Input  : nEntIndex - index of the edict that owns the sound channel
//			channel - This is a sound channel (CHAN_ITEM, CHAN_STATIC)
//			*pSoundName - sound script string name
//			attenuation - attenuation of this sound (not animated)
//-----------------------------------------------------------------------------
void CSoundPatch::Init( IRecipientFilter *pFilter, CBaseEntity *pEnt, int channel, const char *pSoundName, 
			soundlevel_t soundlevel, const Vector *pSoundOrigin , float scriptVolume)
{
	m_hEnt = pEnt;
	if ( pEnt )
	{
		m_soundEntityIndex = pEnt->entindex();
	}
	m_entityChannel = channel;

	// if not a direct wave reference, crack the script
	CSoundParameters params;
	if ( !V_stristr( pSoundName, ".wav" ) && !V_stristr( pSoundName, ".mp3" ) &&
		CBaseEntity::GetParametersForSound( pSoundName, params, nullptr ) )
	{
		m_flScriptVolume = params.volume;
		// This has to be the actual .wav because rndwave would cause a bunch of new .wavs to play... bad...
		//  e.g., when you pitch shift it would start a different wav instead.

		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_hSoundScriptHash = params.m_hSoundScriptHash;
		m_nSoundEntryVersion = params.m_nSoundEntryVersion;

		pSoundName = params.soundname;
		m_soundlevel = params.soundlevel;

		// TERROR: if we say we want CHAN_USER_BASE + N, we mean it!
		if ( m_entityChannel < CHAN_USER_BASE )
		{
			m_entityChannel = params.channel;
		}
	}
	else
	{
		m_iszSoundScriptName = AllocPooledString( pSoundName );
		m_flScriptVolume = scriptVolume;
		m_soundlevel = soundlevel;
	}

	m_iszSoundName = AllocPooledString( pSoundName );
	m_volume.SetValue( 0 );
	m_pitch.SetValue( 0 );
	m_isPlaying = false;
	m_shutdownTime = 0;
	m_Filter.Init( pFilter );
	m_baseFlags = 0;
	if( pSoundOrigin )
	{
		m_soundOrigin.x = pSoundOrigin->x;
		m_soundOrigin.y = pSoundOrigin->y;
		m_soundOrigin.z = pSoundOrigin->z;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Ramps the pitch to a new value
// Input  : pitchTarget - new value
//			deltaTime - seconds to reach the value
//-----------------------------------------------------------------------------
void CSoundPatch::ChangePitch( float pitchTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_PITCH;
	m_pitch.SetTarget( pitchTarget, deltaTime );
}

//-----------------------------------------------------------------------------
// Purpose: Ramps the volume to a new value
// Input  : volumeTarget - new volume
//			deltaTime - seconds to reach the new volume 
//-----------------------------------------------------------------------------
void CSoundPatch::ChangeVolume( float volumeTarget, float deltaTime )
{
	m_flags |= SND_CHANGE_VOL;
	if ( volumeTarget > 1.0 )
		volumeTarget = 1.0;
	m_volume.SetTarget( volumeTarget, deltaTime );
}

//-----------------------------------------------------------------------------
// Purpose: Fade volume to zero AND SHUT DOWN THIS SOUND
// Input  : deltaTime - seconds before done/shutdown
//-----------------------------------------------------------------------------
void CSoundPatch::FadeOut( float deltaTime, bool destroyOnFadeout )
{
	ChangeVolume( 0, deltaTime );
	if ( !destroyOnFadeout )
	{
		m_shutdownTime = g_pEffects->Time() + deltaTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current pitch
//-----------------------------------------------------------------------------
float CSoundPatch::GetPitch( void )
{
	return m_pitch.Value();
}

//-----------------------------------------------------------------------------
// Purpose: Get the sound's current volume
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolume( void )
{
	return m_volume.Value();
}

//-----------------------------------------------------------------------------
// Returns the ent index
//-----------------------------------------------------------------------------
int CSoundPatch::EntIndex() const
{
	contract_assert( !m_hEnt.IsValid() || m_hEnt.Get() );
	return m_hEnt.Get() ? m_hEnt->entindex() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
// This function is an internal method of accessing the real volume passed into the engine (i.e. post multiply)
// Output : float
//-----------------------------------------------------------------------------
float CSoundPatch::GetVolumeForEngine( void )
{
	return ( m_flScriptVolume * m_volume.Value() );
}

//-----------------------------------------------------------------------------
// Purpose: Stop the sound
//-----------------------------------------------------------------------------
void CSoundPatch::Shutdown( void )
{
//	Msg( "Removing sound %s\n", m_pszSoundName );
	if ( m_isPlaying )
	{
		int entIndex = -1;
		if ( m_hEnt.Get() )
		{
			entIndex = EntIndex();
		}
		else
		{
			// may have deleted the entity after starting the sound, but before stopping the sound, try the saved index
			// this will handle that case so a sound patch doesn't get stuck on
			entIndex = m_soundEntityIndex;
		}
		contract_assert( entIndex >= 0 );
		// BUGBUG: Don't crash in release mode
		if ( entIndex >= 0 )
		{
			if( m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
			{
				CBaseEntity::StopSound( entIndex, STRING( m_iszSoundScriptName ) );
			}
			else
			{
				CBaseEntity::StopSound( entIndex, m_entityChannel, STRING( m_iszSoundName ) );
			}
		}
		m_isPlaying = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update all envelopes and send appropriate data to the client
// Input  : time - new global clock
//			deltaTime - amount of time that has passed
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundPatch::Update( float time, float deltaTime )
{
	CVProfScope _( "CSoundPatch::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_shutdownTime && time > m_shutdownTime )
	{
		Shutdown();
		return false;
	}

	if ( EntIndex() < 0 )
	{
		// FIXME:  The pointer to this soundpatch is probably leaked since no entity is around to clean it up (ywb)
		DevWarning( "CSoundPatch::Update:  Removing CSoundPatch (%s) with NULL EHandle\n", STRING(m_iszSoundName) );
		return false;
	}

	if ( m_pitch.ShouldUpdate() )
	{
		m_pitch.Update( deltaTime );
		m_flags |= SND_CHANGE_PITCH;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_PITCH;
	}

	if ( m_volume.ShouldUpdate() )
	{
		m_volume.Update( deltaTime );
		m_flags |= SND_CHANGE_VOL;
	}
	else 
	{
		m_flags &= ~SND_CHANGE_VOL;
	}

	// if ( m_flags && m_Filter.IsActive() )
	if ( m_flags )
	{
		// SoundPatches take volumes between 0 & 1, and use that to multiply the sounds.txt specified volume.
		// Because of this, we need to always set the SND_CHANGE_VOL flag when we emit sound, or it'll use the scriptfile's instead.
		m_flags |= SND_CHANGE_VOL;

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = m_flags;
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );

		m_flags = 0;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Sound is going to start playing again, clear any shutdown time
//-----------------------------------------------------------------------------
void CSoundPatch::Reset( void )
{
	m_shutdownTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Start playing the sound - send updates to the client
//-----------------------------------------------------------------------------
void CSoundPatch::StartSound( float flStartTime )
{
//	Msg( "Start sound %s\n", m_pszSoundName );
	m_flags = 0;
	if ( m_Filter.IsActive() )
	{
		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		if ( V_stristr( STRING(m_iszSoundName), "music" ) )
		{
			ep.m_nFlags = m_baseFlags;
		}
		else
		{
			ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		}
		ep.m_nFlags |= SND_GENERATE_GUID;				// We need GUID in all cases, even for threaded sounds
		ep.m_nPitch = (int)m_pitch.Value();
		ep.m_bEmitCloseCaption = false;

		if ( flStartTime )
		{
			ep.m_flSoundTime = flStartTime;
		}

//#ifdef CLIENT_DLL
		{
			CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
		}
		CBaseEntity::EmitCloseCaption( m_Filter, EntIndex(), STRING( m_iszSoundScriptName ), ep.m_UtlVecSoundOrigin, m_flCloseCaptionDuration, true );
	}
	m_isPlaying = true;
}

//-----------------------------------------------------------------------------
// Purpose: resumes playing the sound on restore
//-----------------------------------------------------------------------------
void CSoundPatch::ResumeSound( void )
{
	if ( IsPlaying() && m_Filter.IsActive() )
	{
		if ( EntIndex() >= 0 )
		{
			EmitSound_t ep;
			ep.m_nChannel = m_entityChannel;
			ep.m_pSoundName = STRING(m_iszSoundName);
			ep.m_flVolume = GetVolumeForEngine();
			ep.m_SoundLevel = m_soundlevel;
			ep.m_nFlags = (SND_CHANGE_VOL | SND_CHANGE_PITCH | m_baseFlags);
			ep.m_nPitch = (int)m_pitch.Value();

			ep.m_hSoundScriptHash = m_hSoundScriptHash;
			ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

			// only pass the position if it's coming from the world
			if( EntIndex() == 0 )
				ep.m_pOrigin = &m_soundOrigin;

			CBaseEntity::EmitSound( m_Filter, EntIndex(), ep );
		}
		else
		{
			// FIXME: Lost the entity on restore. It might have been suppressed by the save/restore system.
			// This will probably leak the sound patch since there's no one to delete it, but the next
			// call to CSoundPatch::Update should at least remove it from the list of sound patches.
			DevWarning( "CSoundPatch::ResumeSound: Lost EHAndle on restore - destroy the sound patch in your entity's StopLoopingSounds! (%s)\n", STRING( m_iszSoundName ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: A new player's entered the game. See if we need to restart our sound.
//-----------------------------------------------------------------------------
void CSoundPatch::AddPlayerPost( CBasePlayer *pPlayer )
{
	if ( m_Filter.IsActive() && m_Filter.AddRecipient(pPlayer) )
	{
		// Alrighty, he's new. We need to restart our sound just to him.
		// Create a new filter just to him.
		CSingleUserRecipientFilter filter( pPlayer );

		EmitSound_t ep;
		ep.m_nChannel = m_entityChannel;
		ep.m_pSoundName = STRING(m_iszSoundName);
		ep.m_flVolume = GetVolumeForEngine();
		ep.m_SoundLevel = m_soundlevel;
		ep.m_nFlags = (SND_CHANGE_VOL | m_baseFlags);
		ep.m_nPitch = (int)m_pitch.Value();

		ep.m_hSoundScriptHash = m_hSoundScriptHash;
		ep.m_nSoundEntryVersion = m_nSoundEntryVersion;

		// only pass the position if it's coming from the world
		if( EntIndex() == 0 )
			ep.m_pOrigin = &m_soundOrigin;

		CBaseEntity::EmitSound( filter, EntIndex(), ep );
	}
}

// This is an entry in the command queue.  It's used to queue up various pitch and volume changes
// so you can define an envelope without writing timing code in an entity.  Existing queued commands
// can be deleted later if the envelope changes dynamically.
struct SoundCommand_t : public ks::pooled< SoundCommand_t >
{
	SoundCommand_t( void ) { std::memset( this, 0, sizeof(*this) ); }
	SoundCommand_t( CSoundPatch *pSound, float executeTime, soundcommands_t command, float deltaTime, float value ) : m_pPatch(pSound), m_time(executeTime), m_deltaTime(deltaTime), m_command(command), m_value(value) {}

	CSoundPatch		*m_pPatch;
	float			m_time;
	float			m_deltaTime;
	soundcommands_t	m_command;
	float			m_value;
	
	SoundCommand_t	*m_pNext;

	DECLARE_SIMPLE_DATADESC();
	private:
		static ks::fixed_pool s_Allocator;
		friend class ks::pooled< SoundCommand_t >;
};

ks::fixed_pool SoundCommand_t::s_Allocator( sizeof(SoundCommand_t), 32, ks::fixed_pool::grow::fast, "SoundCommand_t pool", alignof(SoundCommand_t) );

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( SoundCommand_t )

typedef std::unique_ptr< SoundCommand_t > SOUNDCOMMANDPTR;

bool SoundCommandLessFunc( const SOUNDCOMMANDPTR &lhs, const SOUNDCOMMANDPTR &rhs )	
{ 
	// NOTE: A greater time means "less" priority
	return ( lhs->m_time > rhs->m_time );	
}

// This implements the sound controller
class CSoundControllerImp : public CSoundEnvelopeController, public CAutoGameSystemPerFrame
{
	//-----------------------------------------------------------------------------
	// internal functions, private to this file
	//-----------------------------------------------------------------------------
public:
	CSoundControllerImp( void ) : CAutoGameSystemPerFrame( "CSoundControllerImp" )
	{
	}

	void ProcessCommand( SoundCommand_t *pCmd );
	void RemoveFromList( CSoundPatch *pSound );

	virtual void	OnRestore();

	//-----------------------------------------------------------------------------
	// external interface functions (from CSoundEnvelopeController)
	//-----------------------------------------------------------------------------
public:

	// Start this sound playing, or reset if already playing with new volume/pitch
	void			Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime = 0 );
	void			CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue );
	
	void			SystemReset( void );
	void			SystemUpdate( void );
	void			CommandClear( CSoundPatch *pSound );
	void			Shutdown( CSoundPatch *pSound );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						float attenuation, const Vector *pSoundOrigin, float scriptVolume = 1.0f );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, const char *pSoundName, 
						soundlevel_t soundlevel );

	CSoundPatch		*SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es );

	void			SoundDestroy( CSoundPatch *pSound );
	void			SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime );
	void			SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime );
	void			SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout );
	float			SoundGetPitch( CSoundPatch *pSound );
	float			SoundGetVolume( CSoundPatch *pSound );

	string_t		SoundGetName( CSoundPatch *pSound ) { return pSound->GetName(); }
	string_t		SoundGetScriptName( CSoundPatch *pSound ) { return pSound->GetScriptName(); }
	void			SoundSetCloseCaptionDuration( CSoundPatch *pSound, float flDuration ) { pSound->SetCloseCaptionDuration(flDuration); }

	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints );
	float			SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope );

	void			CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer );

	// Inserts the command into the list, sorted by time
	void			CommandInsert( SOUNDCOMMANDPTR pCommand );

	virtual void PreClientUpdate()
	{
		SystemUpdate();
	}

	virtual void LevelShutdownPreEntity() 
	{
		SystemReset();
	}
	
private:
	std::vector< CSoundPatch * >			m_soundList;
	std::vector< SOUNDCOMMANDPTR >		m_commandList;	// heap by SoundCommandLessFunc (ks::heap_*)
	float				m_flLastTime;
};

// Execute a command from the list
// currently only 3 commands 
// UNDONE: Add start command?
void CSoundControllerImp::ProcessCommand( SoundCommand_t *pCmd )
{
	switch( pCmd->m_command )
	{
	case SOUNDCTRL_CHANGE_VOLUME:
		pCmd->m_pPatch->ChangeVolume( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_CHANGE_PITCH:
		pCmd->m_pPatch->ChangePitch( pCmd->m_value, pCmd->m_deltaTime );
		break;

	case SOUNDCTRL_STOP:
		pCmd->m_pPatch->Shutdown();
		break;

	case SOUNDCTRL_DESTROY:
		RemoveFromList( pCmd->m_pPatch );
		delete pCmd->m_pPatch;
		pCmd->m_pPatch = nullptr;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove this sound from the sound list & shutdown (not in external interface)
// Input  : *pSound - patch to remove
//-----------------------------------------------------------------------------
void CSoundControllerImp::RemoveFromList( CSoundPatch *pSound )
{
	ks::erase_first( m_soundList, pSound );
	pSound->Shutdown();
}

//-----------------------------------------------------------------------------
// Start this sound playing, or reset if already playing with new volume/pitch
//-----------------------------------------------------------------------------
void CSoundControllerImp::Play( CSoundPatch *pSound, float volume, float pitch, float flStartTime )
{
	// reset the vars
	pSound->Reset();

	pSound->ChangeVolume( volume, 0 );
	pSound->ChangePitch( pitch, 0 );

	if ( pSound->IsPlaying() )
	{
		// remove any previous commands in the queue
		CommandClear( pSound );
	}
	else
	{
		m_soundList.push_back( pSound );
		pSound->StartSound( flStartTime );
	}
}

//-----------------------------------------------------------------------------
// Inserts the command into the list, sorted by time
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandInsert( SOUNDCOMMANDPTR pCommand )
{
	ks::heap_push( m_commandList, std::move( pCommand ), SoundCommandLessFunc );
}

//-----------------------------------------------------------------------------
// Purpose: puts a command into the queue
// Input  : *pSound - patch this command affects
//			executeDeltaTime - relative time to execute this command
//			command - command to execute (SOUNDCTRL_*)
//			commandTime - commands have 2 parameters, a time and a value
//			value - 
// Output : 	void
//-----------------------------------------------------------------------------
void CSoundControllerImp::CommandAdd( CSoundPatch *pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float commandValue )
{
	CommandInsert( std::make_unique< SoundCommand_t >( pSound, g_pEffects->Time() + executeDeltaTime, command, commandTime, commandValue ) );
}

// Reset the whole system (level change, etc.)
void CSoundControllerImp::SystemReset( void )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
	
		// shutdown all active sounds
		pNode->Shutdown();
	}

	// clear the list
	m_soundList.clear(); m_soundList.shrink_to_fit();

	// clear the command queue
	m_commandList.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Update the active sounds, dequeue any events and move the ramps
//-----------------------------------------------------------------------------
void CSoundControllerImp::SystemUpdate( void )
{
	CVProfScope _( "CSoundControllerImp::SystemUpdate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	float time = g_pEffects->Time();
	float deltaTime = time - m_flLastTime;
	
	// handle clock resets
	if ( deltaTime < 0 )
		deltaTime = 0;

	m_flLastTime = time;

	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:processcommandlist", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		while ( !m_commandList.empty() )
		{
			// Commands are sorted by time.
			// process any that should occur by the current time
			if ( time >= m_commandList.front()->m_time )
			{
				SOUNDCOMMANDPTR pCmd = std::move( m_commandList.front() );
				ks::heap_pop( m_commandList, SoundCommandLessFunc );
				ProcessCommand( pCmd.get() );
			}
			else
			{
				break;
			}
		}
	}

	// NOTE: Because this loop goes from the end to the beginning
	// we can fast remove inside it without breaking the indexing
	{
		CVProfScope _( "CSoundControllerImp::SystemUpdate:removesounds", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
		for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
		{
			CSoundPatch *pNode = m_soundList[i];
			if ( !pNode->Update( time, deltaTime ) )
			{
				pNode->Reset();
				ks::swap_remove( m_soundList, i );
			}
		}
	}
}

// Remove any envelope commands from the list (dynamically changing envelope)
void CSoundControllerImp::CommandClear( CSoundPatch *pSound )
{
	for ( int i = std::ssize( m_commandList )-1; i >= 0; i-- )
	{
		if ( m_commandList[ i ]->m_pPatch == pSound )
		{
			ks::heap_erase( m_commandList, i, SoundCommandLessFunc );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: immediately stop playing this sound 
// Input  : *pSound - Patch to shut down
//-----------------------------------------------------------------------------
void CSoundControllerImp::Shutdown( CSoundPatch *pSound )
{
	if ( !pSound )
		return;

	pSound->Shutdown();
	CommandClear( pSound );
	RemoveFromList( pSound );
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const char *pSoundName )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), CHAN_AUTO, pSoundName, SNDLVL_NORM, nullptr );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), nullptr, scriptVolume );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, float attenuation, const Vector *pSoundOrigin, float scriptVolume )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, ATTN_TO_SNDLVL( attenuation ), pSoundOrigin, scriptVolume );

	return pSound;
}
CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, int channel, 
			const char *pSoundName, soundlevel_t soundlevel )
{
	CSoundPatch *pSound = new CSoundPatch;
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), channel, pSoundName, soundlevel, nullptr );

	return pSound;
}

CSoundPatch *CSoundControllerImp::SoundCreate( IRecipientFilter& filter, int nEntIndex, const EmitSound_t &es )
{
	CSoundPatch *pSound = new CSoundPatch;

	// FIXME: This is done so we don't have to futz with the public interface
	IHandleEntity* pEnt = ( nEntIndex != -1 ) ? g_pEntityList->LookupEntityByNetworkIndex( nEntIndex ) : nullptr;
	pSound->Init( &filter, static_cast<CBaseEntity*>(pEnt), es.m_nChannel, es.m_pSoundName, es.m_SoundLevel, es.m_pOrigin );
	pSound->ChangeVolume( es.m_flVolume, 0 );
	pSound->ChangePitch( es.m_nPitch, 0 );

	if ( es.m_nFlags & SND_SHOULDPAUSE )
	{
		pSound->SetBaseFlags( SND_SHOULDPAUSE );
	}

	return pSound;
}

void CSoundControllerImp::SoundDestroy( CSoundPatch	*pSound )
{
	if ( !pSound )
		return;

	Shutdown( pSound );
	delete pSound;
}

void CSoundControllerImp::SoundChangePitch( CSoundPatch *pSound, float pitchTarget, float deltaTime )
{
	pSound->ChangePitch( pitchTarget, deltaTime );
}

void CSoundControllerImp::SoundChangeVolume( CSoundPatch *pSound, float volumeTarget, float deltaTime )
{
	pSound->ChangeVolume( volumeTarget, deltaTime );
}

float CSoundControllerImp::SoundGetPitch( CSoundPatch *pSound )
{
	return pSound->GetPitch();
}

float CSoundControllerImp::SoundGetVolume( CSoundPatch *pSound )
{
	return pSound->GetVolume();
}

void CSoundControllerImp::SoundFadeOut( CSoundPatch *pSound, float deltaTime, bool destroyOnFadeout )
{
	if ( destroyOnFadeout && (deltaTime == 0.0f) )
	{
		SoundDestroy( pSound );
		return;
	}

	pSound->FadeOut( deltaTime, destroyOnFadeout );
	if ( destroyOnFadeout )
	{
		CommandAdd( pSound, deltaTime, SOUNDCTRL_DESTROY, 0.0f, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*points - List of enevelope points
//			numPoints - Number of points provided
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopePoint_t *points, int numPoints )
{
	float	amplitude	= 0.0f;
	float	duration	= 0.0f;
	float	totalDuration	= 0.0f;

	contract_assert( points );

	// Clear out all previously acting commands
	CommandClear( pSound );

	// Evaluate and queue all points
	for ( int i = 0; i < numPoints; i++ )
	{
		// See if we're keeping our last amplitude for this new point
		if ( ( points[i].amplitudeMin != -1.0f ) || ( points[i].amplitudeMax != -1.0f ) )
		{
			amplitude = random_valve->RandomFloat( points[i].amplitudeMin, points[i].amplitudeMax );
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting amplitude value in envelope!  (Cannot be -1)\n" );
		}

		// See if we're keeping our last duration for this new point
		if ( ( points[i].durationMin != -1.0f ) || ( points[i].durationMax != -1.0f ) )
		{
			duration = random_valve->RandomFloat( points[i].durationMin, points[i].durationMax );	
			//duration = points[i].durationMin;
		}
		else if ( i == 0 )
		{
			// Can't do this on the first entry
			Msg( "Invalid starting duration value in envelope! (Cannot be -1)\n" );
		}

		// Queue the command
		CommandAdd( pSound, totalDuration, soundCommand, duration, amplitude );

		// Tack this command's duration onto the running duration
		totalDuration += duration;
	}

	return totalDuration;
}

//-----------------------------------------------------------------------------
// Purpose: Queue a list of envelope points into a sound patch's event list
// Input  : *pSound - The sound patch to be operated on
//			soundCommand - Type of operation the envelope describes
//			*envelope - The envelope description to be queued
// Output : float - Returns the total duration of the envelope
//-----------------------------------------------------------------------------
float CSoundControllerImp::SoundPlayEnvelope( CSoundPatch *pSound, soundcommands_t soundCommand, envelopeDescription_t *envelope )
{
	return SoundPlayEnvelope( pSound, soundCommand, envelope->pPoints, envelope->nNumPoints );
}

//-----------------------------------------------------------------------------
// Purpose: Looping sounds are often started in entity spawn/activate functions. 
//			In singleplayer, the player's not ready to receive sounds then, so restart 
//			and SoundPatches that are active and have no receivers.
//-----------------------------------------------------------------------------
void CSoundControllerImp::CheckLoopingSoundsForPlayer( CBasePlayer *pPlayer )
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		pNode->AddPlayerPost( pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resumes saved soundpatches
//-----------------------------------------------------------------------------
void CSoundControllerImp::OnRestore()
{
	for ( int i = std::ssize( m_soundList )-1; i >=0; i-- )
	{
		CSoundPatch *pNode = m_soundList[i];
		if ( pNode && pNode->IsPlaying() )
		{
			pNode->ResumeSound();
		}
	}
}

//-----------------------------------------------------------------------------
// Singleton accessors
//-----------------------------------------------------------------------------
static CSoundControllerImp g_Controller;
CSoundEnvelopeController &CSoundEnvelopeController::GetController( void )
{
	return g_Controller;
}

class CBaseEntity;
class CBasePlayer;
class CBaseTrigger;
class CEnvSoundscape;
struct inputdata_t;

using namespace ::ks::reflect;

ConVar soundscape_debug( side_registry(), "soundscape_debug", "0", FCVAR_CHEAT, "When on, draws lines to all env_soundscape entities. Green lines show the active soundscape, red lines show soundscapes that aren't in range, and white lines show soundscapes that are in range, but not the active soundscape." );

// ----------------------------------------------------------------------------- //
// CEnvSoundscapeProxy stuff.
// ----------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( env_soundscape_proxy, CEnvSoundscapeProxy );

IMPLEMENT_REFLECT_DATAMAP( CEnvSoundscapeProxy )

CEnvSoundscapeProxy::CEnvSoundscapeProxy()
{
	m_MainSoundscapeName = NULL_STRING;
}

void CEnvSoundscapeProxy::Activate_Impl()
{
	if ( m_MainSoundscapeName != NULL_STRING )
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_MainSoundscapeName );
		if ( pEntity )
		{
			m_hProxySoundscape = dynamic_cast< CEnvSoundscape* >( pEntity );
		}
	}

	if ( m_hProxySoundscape )
	{
		// Copy the relevant parameters from our main soundscape.
		m_soundscapeIndex = m_hProxySoundscape->m_soundscapeIndex;

		for ( int i=0; i < std::size( m_positionNames ); i++ )
			m_positionNames[i] = m_hProxySoundscape->m_positionNames[i];
	}
	else
	{
		Warning( "env_soundscape_proxy can't find target soundscape: '%s'\n", STRING( m_MainSoundscapeName ) );
	}

	BaseClass::Activate_Impl();
}

// ----------------------------------------------------------------------------- //
// CEnvSoundscape stuff.
// ----------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( env_soundscape, CEnvSoundscape );

IMPLEMENT_REFLECT_DATAMAP( CEnvSoundscape )

CEnvSoundscape::CEnvSoundscape()
{
	m_soundscapeName = NULL_STRING;
	m_soundscapeIndex = -1;
	m_soundscapeEntityId = -1;
	m_bDisabled = false;
	g_SoundscapeSystem.AddSoundscapeEntity( this );
}

CEnvSoundscape::~CEnvSoundscape()
{
	g_SoundscapeSystem.RemoveSoundscapeEntity( this );
}

void CEnvSoundscape::InputEnable( inputdata_t &inputdata )
{
	if (!IsEnabled())
	{
		Enable();
	}
}

void CEnvSoundscape::InputDisable( inputdata_t &inputdata )
{
	if (IsEnabled())
	{
		Disable();
	}
}

void CEnvSoundscape::InputToggleEnabled( inputdata_t &inputdata )
{
	if ( IsEnabled() )
	{
		Disable();
	}
	else
	{
		Enable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the laser is currently active.
//-----------------------------------------------------------------------------
bool CEnvSoundscape::IsEnabled( void ) const
{
	return !m_bDisabled && g_pGameRules->AllowSoundscapes();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvSoundscape::Disable( void )
{
	m_bDisabled = true;

	// Reset if we are the currently active soundscape
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvSoundscape::Enable( void )
{
	m_bDisabled = false;

	// Force the player to recheck soundscapes
}

bool CEnvSoundscape::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "soundscape"))
	{
		m_soundscapeName = AllocPooledString( szValue );
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

// returns true if the given sound entity is in range 
// and can see the given entity (pTarget)

bool CEnvSoundscape::InRangeOfPlayer( CBasePlayer *pTarget ) 
{
	Vector vecSpot1 = EarPosition();
	Vector vecSpot2 = pTarget->EarPosition();

	// calc range from sound entity to player
	Vector vecRange = vecSpot2 - vecSpot1;
	float range = vecRange.Length();
	if ( m_flRadius > range || m_flRadius == -1 )
	{
		trace_t tr;

		UTIL_TraceLine( vecSpot1, vecSpot2, MASK_SOLID_BRUSHONLY|MASK_WATER, pTarget, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction == 1 && !tr.startsolid )
		{
			return true;
		}
	}

	return false;
}

void CEnvSoundscape::WriteAudioParamsTo( audioparams_t &audio )
{
	if ( !g_SoundscapeSystem.IsValidIndex( m_soundscapeIndex ) )
	{
		Warning("Setting invalid soundscape, %s, as the active soundscape. There is probably no script entry matching this name. BUG THIS!\n", STRING(m_soundscapeName) );
	}

	audio.entIndex = m_soundscapeEntityId;
	audio.soundscapeIndex = m_soundscapeIndex;
	audio.localBits = 0;
	for ( int i = 0; i < std::size( m_positionNames ); i++ )
	{
		if ( m_positionNames[i] != NULL_STRING )
		{
			// We are a valid entity for a sound position
			CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_positionNames[i], this, this );
			if ( pEntity )
			{
				audio.localBits |= 1<<i;
				audio.localSound.Set( i, pEntity->GetAbsOrigin() );
			}
		}
	}

	m_OnPlay.FireOutput( this, this );
}

//
// A client that is visible and in range of a sound entity will
// have its soundscape set by that sound entity.  If two or more
// sound entities are contending for a client, then the nearest
// sound entity to the client will set the client's soundscape.
// A client's soundscape will remain set to its prior value until
// a new in-range, visible sound entity resets a new soundscape.
//

// CONSIDER: if player in water state, autoset and underwater soundscape? 
void CEnvSoundscape::UpdateForPlayer( ss_update_t &update )
{
	if ( !IsEnabled() )
	{
		if ( update.pCurrentSoundscape == this )
		{
			update.pCurrentSoundscape = nullptr;
			update.currentDistance = 0;
			update.bInRange = false;
		}
		return;
	}

	// calc range from sound entity to player
	Vector target = EarPosition();
	float range = (update.playerPosition - target).Length();
	
	if ( update.pCurrentSoundscape == this )
	{
		update.currentDistance = range;
		update.bInRange = false;
		if ( m_flRadius > range || m_flRadius == -1 )
		{
			trace_t tr;

			update.traceCount++;
			UTIL_TraceLine( target, update.playerPosition, MASK_SOLID_BRUSHONLY|MASK_WATER, update.pPlayer, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction == 1 && !tr.startsolid )
			{
				update.bInRange = true;
			}
		}
	}
	else
	{
		if ( (!update.bInRange || range < update.currentDistance ) && (m_flRadius > range || m_flRadius == -1) )
		{
			trace_t tr;

			update.traceCount++;
			UTIL_TraceLine( target, update.playerPosition, MASK_SOLID_BRUSHONLY|MASK_WATER, update.pPlayer, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction == 1 && !tr.startsolid )
			{
				audioparams_t &audio = update.pPlayer->GetAudioParams();
				WriteAudioParamsTo( audio );
				update.pCurrentSoundscape = this;
				update.bInRange = true;
				update.currentDistance = range;
			}
		}
	}

	if ( soundscape_debug.GetBool() )
	{
		// draw myself
		NDebugOverlay::Box(GetAbsOrigin(), Vector(-10,-10,-10), Vector(10,10,10),  255, 0, 255, 64, NDEBUG_PERSIST_TILL_NEXT_SERVER );

 		if ( update.pPlayer )
		{
			audioparams_t &audio = update.pPlayer->GetAudioParams();
			if ( audio.entIndex != m_soundscapeEntityId )
			{
				if ( InRangeOfPlayer( update.pPlayer ) )
				{
					NDebugOverlay::Line( GetAbsOrigin(), update.pPlayer->WorldSpaceCenter(), 255, 255, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
				}
				else
				{
					NDebugOverlay::Line( GetAbsOrigin(), update.pPlayer->WorldSpaceCenter(), 255, 0, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER  );
				}
			}
			else
			{
				if ( InRangeOfPlayer( update.pPlayer ) )
				{
					NDebugOverlay::Line( GetAbsOrigin(), update.pPlayer->WorldSpaceCenter(), 0, 255, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER  );
				}
  				else
				{
					NDebugOverlay::Line( GetAbsOrigin(), update.pPlayer->WorldSpaceCenter(), 255, 170, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER  );
				}

				// also draw lines to each sound position.
				// we don't store the number of local sound positions, just a bitvector of which ones are on.
				unsigned int soundbits = audio.localBits.Get();
				float periodic = 2.0f * std::sin((std::fmod(gpGlobals->curtime,2.0f) - 1.0f) * std::numbers::pi); // = -4f .. 4f
				for (int ii = 0 ; ii < NUM_AUDIO_LOCAL_SOUNDS ; ++ii )
				{
					if ( soundbits & (1 << ii) )
					{
						const Vector &soundLoc = audio.localSound.Get(ii);
						NDebugOverlay::Line( GetAbsOrigin(), soundLoc, 0, 32 , 255 , false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
						NDebugOverlay::Cross3D( soundLoc, 16.0f + periodic, 0, 0, 255, false, NDEBUG_PERSIST_TILL_NEXT_SERVER );
					}
				}
			}
		}

		NDebugOverlay::EntityTextAtPosition( GetAbsOrigin(), 0, STRING(m_soundscapeName), NDEBUG_PERSIST_TILL_NEXT_SERVER );
	}
}

//
// env_soundscape - spawn a sound entity that will set player soundscape
// when player moves in range and sight.
//
//
void CEnvSoundscape::Spawn_Impl( )
{
	Precache();
}

void CEnvSoundscape::Precache_Impl()
{
	if ( m_soundscapeName == NULL_STRING )
	{
		DevMsg("Found soundscape entity with no soundscape name.\n" );
		return;
	}

	m_soundscapeIndex = g_SoundscapeSystem.GetSoundscapeIndex( STRING(m_soundscapeName) );
	if ( !g_SoundscapeSystem.IsValidIndex( m_soundscapeIndex ) )
	{
		DevWarning("Can't find soundscape: %s\n", STRING(m_soundscapeName) );
	}
}

void CEnvSoundscape::DrawDebugGeometryOverlays_Impl( void )
{
	if ( m_debugOverlays & (OVERLAY_BBOX_BIT|OVERLAY_PIVOT_BIT|OVERLAY_ABSBOX_BIT) )
	{
		CBasePlayer *pPlayer = UTIL_GetListenServerHost();
		if ( pPlayer )
		{
			audioparams_t &audio = pPlayer->GetAudioParams();
			if ( audio.entIndex != m_soundscapeEntityId )
			{
				CBaseEntity *pEnt = pPlayer->GetSoundscapeListener();
				if ( pEnt )
				{
					NDebugOverlay::Line(GetAbsOrigin(), pEnt->WorldSpaceCenter(), 255, 0, 255, false, 0 );
				}
			}
		}
	}

	BaseClass::DrawDebugGeometryOverlays_Impl();
}

// ---------------------------------------------------------------------------------------------------- //
// CEnvSoundscapeTriggerable
// ---------------------------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( env_soundscape_triggerable, CEnvSoundscapeTriggerable );

CEnvSoundscapeTriggerable::CEnvSoundscapeTriggerable()
{
}

void CEnvSoundscapeTriggerable::DelegateStartTouch( CBaseEntity *pEnt )
{
	CBasePlayer *pPlayer = entity_cast<CBasePlayer>( pEnt );
	if ( !pPlayer )
		return;

	// Just in case.. we shouldn't already be in the player's list because it should have 
	// called DelegateEndTouch, but this seems to happen when they're noclipping.
	ks::erase_first( pPlayer->m_hTriggerSoundscapeList, this );

	// Add us to the player's list of soundscapes and 
	pPlayer->m_hTriggerSoundscapeList.insert( pPlayer->m_hTriggerSoundscapeList.begin(), this );
	WriteAudioParamsTo( pPlayer->GetAudioParams() );
}

void CEnvSoundscapeTriggerable::DelegateEndTouch( CBaseEntity *pEnt )
{
	CBasePlayer *pPlayer = entity_cast<CBasePlayer>( pEnt );
	if ( !pPlayer )
		return;

	// Remove us from the ent's list of soundscapes.
	ks::erase_first( pPlayer->m_hTriggerSoundscapeList, this );
	while ( !pPlayer->m_hTriggerSoundscapeList.empty() )
	{
		CEnvSoundscapeTriggerable *pSS = dynamic_cast< CEnvSoundscapeTriggerable* >( pPlayer->m_hTriggerSoundscapeList[0].Get() );
		if ( pSS )
		{
			// Make this one current.
			pSS->WriteAudioParamsTo( pPlayer->GetAudioParams() );
			return;
		}
		else
		{
			pPlayer->m_hTriggerSoundscapeList.erase( pPlayer->m_hTriggerSoundscapeList.begin() + 0 );
		}
	}

	// No soundscapes left.
	pPlayer->GetAudioParams().entIndex = 0;
}

void CEnvSoundscapeTriggerable::Think_Impl()
{
	// Overrides the base class's think and prevents it from running at all.
}

// ---------------------------------------------------------------------------------------------------- //
// CTriggerSoundscape
// ---------------------------------------------------------------------------------------------------- //

class CTriggerSoundscape : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerSoundscape, CBaseTrigger );
	DECLARE_DATADESC();

	CTriggerSoundscape();

	void StartTouch_Impl( CBaseEntity *pOther );
	void EndTouch_Impl( CBaseEntity *pOther );

	void Spawn_Impl();
	void Activate_Impl();

	void PlayerUpdateThink();

private:
	CHandle<CEnvSoundscapeTriggerable> m_hSoundscape;
	[[= ks::reflect::Key{ .name = "soundscape" } ]] string_t m_SoundscapeName;

	std::vector< CBasePlayerHandle > m_spectators; // spectators in our volume
};

LINK_ENTITY_TO_CLASS( trigger_soundscape, CTriggerSoundscape );

IMPLEMENT_REFLECT_DATAMAP( CTriggerSoundscape )

CTriggerSoundscape::CTriggerSoundscape()
{
}

void CTriggerSoundscape::StartTouch_Impl( CBaseEntity *pOther )
{
	if ( m_hSoundscape )
		m_hSoundscape->DelegateStartTouch( pOther );

	BaseClass::StartTouch_Impl( pOther );
}

void CTriggerSoundscape::EndTouch_Impl( CBaseEntity *pOther )
{
	if ( m_hSoundscape )
		m_hSoundscape->DelegateEndTouch( pOther );

	BaseClass::EndTouch_Impl( pOther );
}

void CTriggerSoundscape::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	InitTrigger();

	SetThink( &CTriggerSoundscape::PlayerUpdateThink );
	SetNextThink( gpGlobals->curtime + 0.2 );
}

void CTriggerSoundscape::Activate_Impl()
{
	m_hSoundscape = dynamic_cast< CEnvSoundscapeTriggerable* >( gEntList.FindEntityByName( nullptr, m_SoundscapeName ) );
	BaseClass::Activate_Impl();
}

// look for dead/spectating players in our volume, to call touch on
void CTriggerSoundscape::PlayerUpdateThink()
{
	int i;
	SetNextThink( gpGlobals->curtime + 0.2 );

	std::vector< CBasePlayerHandle > oldSpectators;
	oldSpectators = m_spectators;
	m_spectators.clear();

	for ( i=1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );

		if ( !player )
			continue;

		if ( player->IsAlive() )
			continue;

		// if the spectator is intersecting the trigger, track it, and start a touch if it is just starting to touch
		if ( Intersects( player ) )
		{
			if ( !ks::contains( oldSpectators, player ) )
			{
				StartTouch( player );
			}
			m_spectators.push_back( player );
		}
	}

	// check for spectators who are no longer intersecting
	for ( i=0; i<std::ssize( oldSpectators ); ++i )
	{
		CBasePlayer *player = oldSpectators[i];

		if ( !player )
			continue;

		if ( !ks::contains( m_spectators, player ) )
		{
			EndTouch( player );
		}
	}
}

class CBasePlayer;
class CEnvSoundscape;

using namespace ::ks::reflect;

#define SOUNDSCAPE_MANIFEST_FILE				"scripts/soundscapes_manifest.txt"

static void soundscape_flush( const CCommand &args );
static ConCommand soundscape_flush_command( side_registry(), "soundscape_flush", soundscape_flush, "Flushes the server & client side soundscapes" );
static void soundscape_flush( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( engine->IsDedicatedServer() )
	{
		// If it's a dedicated server, only the server console can run this.
		if ( pPlayer )
			return;
	}
	else
	{
		// If it's a listen server, only the listen server host can run this.
		if ( !pPlayer || pPlayer != UTIL_GetListenServerHost() )
			return;
	}

	g_SoundscapeSystem.FlushSoundscapes();	// don't bother forgetting about the entities
	g_SoundscapeSystem.Init();

	if ( engine->IsDedicatedServer() )
	{
		// If the ds console typed it, send it to everyone.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer	*pSendToPlayer = UTIL_PlayerByIndex( i );
			if ( pSendToPlayer )
				engine->ClientCommand( pSendToPlayer->edict(), "cl_soundscape_flush\n" );
		}
	}
	else
	{
		engine->ClientCommand( pPlayer->edict(), "cl_soundscape_flush\n" );
	}
}

CSoundscapeSystem g_SoundscapeSystem( "CSoundscapeSystem" );

extern ConVar soundscape_debug;

void CSoundscapeSystem::AddSoundscapeFile( const char *filename )
{
	// Open the soundscape data file, and abort if we can't
	KeyValues *pKeyValuesData = new KeyValues( filename );
	if ( filesystem->LoadKeyValues( *pKeyValuesData, IFileSystem::TYPE_SOUNDSCAPE, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = pKeyValuesData;
		while ( pKeys )
		{
			if ( pKeys->GetFirstSubKey() )
			{
				if ( g_pDeveloper->GetBool() )
				{
					if ( std::strstr( pKeys->GetName(), "{" ) )
					{
						Msg("Error parsing soundscape file %s after %s\n", filename, m_soundscapeCount>0 ?m_soundscapes.GetStringText( m_soundscapeCount-1 ) : "FIRST" );
					}
				}
				m_soundscapes.AddString( pKeys->GetName(), m_soundscapeCount );

				m_soundscapeCount++;
			}
			pKeys = pKeys->GetNextKey();
		}
	}
	pKeyValuesData->deleteThis();
}

static void sv_soundscape_printdebuginfo( const CCommand &args );
static ConCommand sv_soundscape_printdebuginfo_command( side_registry(), "sv_soundscape_printdebuginfo", sv_soundscape_printdebuginfo, "print soundscapes", FCVAR_CHEAT );
static void sv_soundscape_printdebuginfo( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_SoundscapeSystem.PrintDebugInfo();
}

void CSoundscapeSystem::PrintDebugInfo()
{
	Msg( "\n------- SERVER SOUNDSCAPES -------\n" );
	for ( auto key=m_soundscapes.First(); key != m_soundscapes.InvalidIndex(); key = m_soundscapes.Next( key ) )
	{
		int id = m_soundscapes.GetIDForKey( key );
		const char *pName = m_soundscapes.GetStringForKey( key );

		Msg( "- %d: %s\n", id, pName );
	}
	Msg( "-------- SOUNDSCAPE ENTITIES -----\n" );
	for( int entityIndex = 0; entityIndex < std::ssize( m_soundscapeEntities ); ++entityIndex )
	{
		CEnvSoundscape *currentSoundscape = m_soundscapeEntities[entityIndex];
		Msg("- %d: %s x:%.4f y:%.4f z:%.4f\n", 
			entityIndex, 
			STRING(currentSoundscape->GetSoundscapeName()), 
			currentSoundscape->GetAbsOrigin().x,
			currentSoundscape->GetAbsOrigin().y,
			currentSoundscape->GetAbsOrigin().z
			);
	}
	Msg( "----------------------------------\n\n" );
}

bool CSoundscapeSystem::Init()
{
	m_soundscapeCount = 0;

	const char *mapname = STRING( gpGlobals->mapname );
	const char *mapSoundscapeFilename = nullptr;
	if ( mapname && *mapname )
	{
		mapSoundscapeFilename = UTIL_VarArgs( "scripts/soundscapes_%s.txt", V_GetFileName( mapname ) );
	}

	KeyValues *manifest = new KeyValues( SOUNDSCAPE_MANIFEST_FILE );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDSCAPE, SOUNDSCAPE_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSoundscapeFile( sub->GetString() );
				if ( mapSoundscapeFilename && FStrEq( sub->GetString(), mapSoundscapeFilename ) )
				{
					mapSoundscapeFilename = nullptr; // we've already loaded the map's soundscape
				}
				continue;
			}

			Warning( "CSoundscapeSystem::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SOUNDSCAPE_MANIFEST_FILE, sub->GetName() );
		}

		if ( mapSoundscapeFilename && filesystem->FileExists( mapSoundscapeFilename ) )
		{
			AddSoundscapeFile( mapSoundscapeFilename );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SOUNDSCAPE_MANIFEST_FILE );
	}
	manifest->deleteThis();
	m_activeIndex = 0;

	return true;
}

void CSoundscapeSystem::FlushSoundscapes( void )
{
	m_soundscapeCount = 0;
	m_soundscapes.ClearStrings();
}

void CSoundscapeSystem::Shutdown()
{
	FlushSoundscapes();
	m_soundscapeEntities.clear();
	m_activeIndex = 0;

}

void CSoundscapeSystem::LevelInitPreEntity()
{
	g_SoundscapeSystem.Shutdown();
	g_SoundscapeSystem.Init();
}

void CSoundscapeSystem::LevelInitPostEntity()
{
	std::vector< bbox_t > clusterbounds;
	int clusterCount = engine->GetClusterCount();
	clusterbounds.assign( clusterCount, {} );
	engine->GetAllClusterBounds( clusterbounds.data(), clusterCount );
	m_soundscapesInCluster.assign( clusterCount, {} );
	for ( int i = 0; i < clusterCount; i++ )
	{
		m_soundscapesInCluster[i].soundscapeCount = 0;
		m_soundscapesInCluster[i].firstSoundscape = 0;
	}
	unsigned char myPVS[16 * 1024];
	std::vector< short > clusterIndexList;
	std::vector< short > soundscapeIndexList;

	// find the clusters visible from each soundscape
	// add this soundscape to the list of soundscapes for that cluster, clip cluster bounds to radius
	for ( int i = 0; i < std::ssize( m_soundscapeEntities ); i++ )
	{
		Vector position = m_soundscapeEntities[i]->GetAbsOrigin();
		float radius = m_soundscapeEntities[i]->m_flRadius;
		float radiusSq = radius * radius;
		engine->GetPVSForCluster( engine->GetClusterForOrigin( position ), sizeof( myPVS ), myPVS );
		for ( int j = 0; j < clusterCount; j++ )
		{
			if ( myPVS[ j >> 3 ] & (1<<(j&7)) )
			{
				float distSq = CalcSqrDistanceToAABB( clusterbounds[j].mins, clusterbounds[j].maxs, position );
				if ( distSq < radiusSq )
				{
					m_soundscapesInCluster[j].soundscapeCount++;
					clusterIndexList.push_back( j );
					// UNDONE: Technically you just need a soundscape index and a count for this list.
					soundscapeIndexList.push_back( i );
				}
			}
		}
	}

	// basically this part is like a radix sort
	// this is how many entries we need in the soundscape index list
	m_soundscapeIndexList.assign( std::ssize( soundscapeIndexList ), {} );

	// now compute the starting index of each cluster
	int firstSoundscape = 0;
	for ( int i = 0; i < clusterCount; i++ )
	{
		m_soundscapesInCluster[i].firstSoundscape = firstSoundscape;
		firstSoundscape += m_soundscapesInCluster[i].soundscapeCount;
		m_soundscapesInCluster[i].soundscapeCount = 0;
	}
	// now add each soundscape index to the appropriate cluster's list
	// The resulting list is precomputing all soundscapes that need to be checked for a player
	// in each cluster.  This is used to accelerate the per-frame operations
	for ( int i = 0; i < std::ssize( soundscapeIndexList ); i++ )
	{
		int cluster = clusterIndexList[i];
		int outIndex = m_soundscapesInCluster[cluster].soundscapeCount + m_soundscapesInCluster[cluster].firstSoundscape;
		m_soundscapesInCluster[cluster].soundscapeCount++;
		m_soundscapeIndexList[outIndex] = soundscapeIndexList[i];
	}
}

int	CSoundscapeSystem::GetSoundscapeIndex( const char *pName )
{
	return m_soundscapes.GetStringID( pName );
}

bool CSoundscapeSystem::IsValidIndex( int index )
{
	if ( index >= 0 && index < m_soundscapeCount )
		return true;
	return false;
}

void CSoundscapeSystem::AddSoundscapeEntity( CEnvSoundscape *pSoundscape )
{
	if ( ks::find_index( m_soundscapeEntities, pSoundscape ) == -1 )
	{
		int index = std::ssize( m_soundscapeEntities ); m_soundscapeEntities.push_back( pSoundscape );
		pSoundscape->m_soundscapeEntityId = index + 1;
	}
}

void CSoundscapeSystem::RemoveSoundscapeEntity( CEnvSoundscape *pSoundscape )
{
	ks::erase_first( m_soundscapeEntities, pSoundscape );
	pSoundscape->m_soundscapeEntityId = -1;
}

void CSoundscapeSystem::FrameUpdatePostEntityThink()
{
	int total = std::ssize( m_soundscapeEntities );
	if ( total > 0 )
	{
		int traceCount = 0;
		int playerCount = 0;
		// budget to do 2 players with 2 traces each tick.  Anything more expensive than that should
		// get spread across multiple ticks
		int maxPlayers = 2;
		int maxTraces = maxPlayers * 2;
		if ( soundscape_debug.GetBool() )
		{
			maxTraces = 9999;
			maxPlayers = MAX_PLAYERS;
		}

		// load balance across server ticks a bit by limiting the numbers of players (get cluster for origin)
		// and traces processed in a single tick.  In single player this will update the player every tick
		// because it always does at least one player's full load of work
		for ( int i = 0; i < gpGlobals->maxClients && traceCount <= maxTraces && playerCount <= maxPlayers; i++ )
		{
			m_activeIndex = (m_activeIndex+1) % gpGlobals->maxClients;
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( m_activeIndex + 1 );
			if ( pPlayer && pPlayer->IsNetClient() )
			{
				// check to see if this is the sound entity that is 
				// currently affecting this player
				audioparams_t &audio = pPlayer->GetAudioParams();

				// if we got this far, we're looking at an entity that is contending
				// for current player sound. the closest entity to player wins.
				CEnvSoundscape *pCurrent = nullptr;
				if ( audio.entIndex > 0 && audio.entIndex <= std::ssize( m_soundscapeEntities ) )
				{
					int ssIndex = audio.entIndex - 1;
					pCurrent = m_soundscapeEntities[ssIndex];
				}
				ss_update_t update;
				update.pPlayer = pPlayer;
				update.pCurrentSoundscape = pCurrent;
				update.playerPosition = pPlayer->EarPosition();
				update.bInRange = false;
				update.currentDistance = 0;
				update.traceCount = 0;
				if ( pCurrent )
				{
					pCurrent->UpdateForPlayer(update);
				}

				int clusterIndex = engine->GetClusterForOrigin( update.playerPosition );
			
				if ( clusterIndex >= 0 && clusterIndex < std::ssize( m_soundscapesInCluster ) )
				{
					// find all soundscapes that could possibly attach to this player and update them
					for ( int j = 0; j < m_soundscapesInCluster[clusterIndex].soundscapeCount; j++ )
					{
						int ssIndex = m_soundscapeIndexList[m_soundscapesInCluster[clusterIndex].firstSoundscape + j];
						if ( m_soundscapeEntities[ssIndex] == update.pCurrentSoundscape )
							continue;
						m_soundscapeEntities[ssIndex]->UpdateForPlayer( update );
					}
				}
				playerCount++;
				traceCount += update.traceCount;
			}
		}
	}
}

void CSoundscapeSystem::AddSoundscapeSounds( KeyValues *pSoundscape, int soundscapeIndex )
{
	return;
}

void CSoundscapeSystem::PrecacheSounds( int soundscapeIndex )
{
	return;
}

#undef SOUNDSCAPE_MANIFEST_FILE

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Spatial entity.

inline float SPATIAL_ENT_THINK_RATE() { return TICK_INTERVAL; }

static const char *s_pFadeInContextThink = "SpatialEntityFadeInThink";
static const char *s_pFadeOutContextThink = "SpatialEntityFadeOutThink";

IMPLEMENT_REFLECT_DATAMAP( CSpatialEntity )

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
IMPLEMENT_REFLECT_SERVERCLASS( CSpatialEntity, DT_SpatialEntity )

CSpatialEntity::CSpatialEntity() : BaseClass()
{
	m_bEnabled = true;
	m_MinFalloff = 0.0f;
	m_MaxFalloff = 1000.0f;
	m_flMaxWeight = 1.0f;
	m_flCurWeight.Set( 0.0f );
	m_flFadeInDuration = 0.0f;
	m_flFadeOutDuration = 0.0f;
	m_flStartFadeInWeight = 0.0f;
	m_flStartFadeOutWeight = 0.0f;
	m_flTimeStartFadeIn = 0.0f;
	m_flTimeStartFadeOut = 0.0f;
	m_lookupFilename = NULL_STRING;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CSpatialEntity::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CSpatialEntity::Spawn_Impl( void )
{
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT | EFL_DIRTY_ABSTRANSFORM );
	Precache();
	SetSolid( SOLID_NONE );

	// To fade in/out the weight.
	SetContextThink( &CSpatialEntity::FadeInThink, TICK_NEVER_THINK, s_pFadeInContextThink );
	SetContextThink( &CSpatialEntity::FadeOutThink, TICK_NEVER_THINK, s_pFadeOutContextThink );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
		m_flCurWeight.Set ( 0.0f );
	}
	else
	{
		m_bEnabled = true;
		m_flCurWeight.Set ( 1.0f );
	}

	BaseClass::Spawn_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up internal vars needed for fade in lerping
//-----------------------------------------------------------------------------
void CSpatialEntity::FadeIn ( void )
{
	m_bEnabled = true;
	m_flTimeStartFadeIn = gpGlobals->curtime;
	m_flStartFadeInWeight = m_flCurWeight;
	SetNextThink ( gpGlobals->curtime + SPATIAL_ENT_THINK_RATE(), s_pFadeInContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Sets up internal vars needed for fade out lerping
//-----------------------------------------------------------------------------
void CSpatialEntity::FadeOut ( void )
{
	m_bEnabled = false;
	m_flTimeStartFadeOut = gpGlobals->curtime;
	m_flStartFadeOutWeight = m_flCurWeight;
	SetNextThink ( gpGlobals->curtime + SPATIAL_ENT_THINK_RATE(), s_pFadeOutContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Fades lookup weight from CurWeight->MaxWeight
//-----------------------------------------------------------------------------
void CSpatialEntity::FadeInThink( void )
{
	// Check for conditions where we shouldnt fade in
	if (		m_flFadeInDuration <= 0 ||  // not set to fade in
		m_flCurWeight >= m_flMaxWeight ||  // already past max weight
		!m_bEnabled ||  // fade in/out mutex
		m_flMaxWeight == 0.0f ||  // min==max
		m_flStartFadeInWeight >= m_flMaxWeight )  // already at max weight
	{
		SetNextThink ( TICK_NEVER_THINK, s_pFadeInContextThink );
		return;
	}

	// If we started fading in without fully fading out, use a truncated duration
	float flTimeToFade = m_flFadeInDuration;
	if ( m_flStartFadeInWeight > 0.0f )
	{	
		float flWeightRatio		= m_flStartFadeInWeight / m_flMaxWeight;
		flWeightRatio = clamp ( flWeightRatio, 0.0f, 0.99f );
		flTimeToFade			= m_flFadeInDuration * (1.0 - flWeightRatio);
	}	

	contract_assert( flTimeToFade > 0.0f );
	float flFadeRatio = (gpGlobals->curtime - m_flTimeStartFadeIn) / flTimeToFade;
	flFadeRatio = clamp ( flFadeRatio, 0.0f, 1.0f );
	m_flStartFadeInWeight = clamp ( m_flStartFadeInWeight, 0.0f, 1.0f );

	m_flCurWeight = Lerp( flFadeRatio, m_flStartFadeInWeight, m_flMaxWeight );

	SetNextThink( gpGlobals->curtime + SPATIAL_ENT_THINK_RATE(), s_pFadeInContextThink );
}

//-----------------------------------------------------------------------------
// Purpose: Fades lookup weight from CurWeight->0.0 
//-----------------------------------------------------------------------------
void CSpatialEntity::FadeOutThink( void )
{
	// Check for conditions where we shouldn't fade out
	if ( m_flFadeOutDuration <= 0 || // not set to fade out
		m_flCurWeight <= 0.0f || // already faded out
		m_bEnabled || // fade in/out mutex
		m_flMaxWeight == 0.0f  || // min==max
		m_flStartFadeOutWeight <= 0.0f )// already at min weight
	{
		SetNextThink ( TICK_NEVER_THINK, s_pFadeOutContextThink );
		return;
	}

	// If we started fading out without fully fading in, use a truncated duration
	float flTimeToFade = m_flFadeOutDuration;
	if ( m_flStartFadeOutWeight < m_flMaxWeight )
	{	
		float flWeightRatio		= m_flStartFadeOutWeight / m_flMaxWeight;
		flWeightRatio = clamp ( flWeightRatio, 0.01f, 1.0f );
		flTimeToFade			= m_flFadeOutDuration * flWeightRatio;
	}	

	contract_assert( flTimeToFade > 0.0f );
	float flFadeRatio = (gpGlobals->curtime - m_flTimeStartFadeOut) / flTimeToFade;
	flFadeRatio = clamp ( flFadeRatio, 0.0f, 1.0f );
	m_flStartFadeOutWeight = clamp ( m_flStartFadeOutWeight, 0.0f, 1.0f );

	m_flCurWeight = Lerp( 1.0f - flFadeRatio, 0.0f, m_flStartFadeOutWeight );

	SetNextThink( gpGlobals->curtime + SPATIAL_ENT_THINK_RATE(), s_pFadeOutContextThink );
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CSpatialEntity::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;

	if ( m_flFadeInDuration > 0.0f )
	{
		FadeIn();
	}
	else
	{
		m_flCurWeight = m_flMaxWeight;
	}

}

void CSpatialEntity::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;

	if ( m_flFadeOutDuration > 0.0f )
	{
		FadeOut();
	}
	else
	{
		m_flCurWeight = 0.0f;
	}

}

void CSpatialEntity::InputSetFadeInDuration( inputdata_t& inputdata )
{
	m_flFadeInDuration = inputdata.value.Float();
}

void CSpatialEntity::InputSetFadeOutDuration( inputdata_t& inputdata )
{
	m_flFadeOutDuration = inputdata.value.Float();
}

using namespace ::ks::reflect;

// Purpose: Dynamic light at the end of a spotlight

LINK_ENTITY_TO_CLASS(spotlight_end, CSpotlightEnd);

IMPLEMENT_REFLECT_SERVERCLASS( CSpotlightEnd, DT_SpotlightEnd )

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CSpotlightEnd::Spawn_Impl( void )
{
	Precache();
	m_flLightScale  = 100;
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

int CSpotlightEnd::ObjectCaps_Impl( void)
{
	// Don't save and don't go across transitions
	return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; 
}

class CBaseEntity;
class CBaseViewModel;
class CSprite;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

LINK_ENTITY_TO_CLASS_ALIAS( env_glow, CSprite ); // For backwards compatibility, remove when no longer needed.
LINK_ENTITY_TO_CLASS_ALIAS( env_sprite_clientside, CSprite );

IMPLEMENT_REFLECT_DATAMAP( CSprite )

IMPLEMENT_NETWORKCLASS_ALIASED( Sprite, DT_Sprite );

IMPLEMENT_REFLECT_TABLE( CSprite, DT_Sprite );

LINK_ENTITY_TO_CLASS_ALIASED( env_sprite, Sprite );

CSprite::CSprite()
{
	m_flGlowProxySize = 2.0f;
	m_flHDRColorScale = 1.0f;
}

CSprite::~CSprite()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	m_flFrame = 0;

	Precache();
	SetModel( STRING( GetModelName() ) );
	CollisionProp()->SetSurroundingBoundsType( USE_GAME_CODE );

	m_flMaxFrame = (float)modelinfo->GetModelFrameCount( GetModel() ) - 1;
	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );

	if ( m_flGlowProxySize > MAX_GLOW_PROXY_SIZE )
	{
		// Clamp on Spawn to prevent per-frame spew
		DevWarning( "env_sprite at setpos %0.0f %0.0f %0.0f has invalid glow size %f - clamping to %f\n",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z, m_flGlowProxySize.Get(), MAX_GLOW_PROXY_SIZE );
		m_flGlowProxySize = MAX_GLOW_PROXY_SIZE;
	}
	if ( GetEntityName() != NULL_STRING && !(m_spawnflags & SF_SPRITE_STARTON) )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
	
	// Worldcraft only sets y rotation, copy to Z
	if ( GetLocalAngles().y != 0 && GetLocalAngles().z == 0 )
	{
		QAngle angles = GetLocalAngles();

		angles.z = angles.y;
		angles.y = 0;

		SetLocalAngles( angles );
	}

	// Clamp our scale if necessary
	float scale = m_flSpriteScale;
	
	if ( scale < 0 || scale > MAX_SPRITE_SCALE )
	{
		DevMsg( "LEVEL DESIGN ERROR: Sprite %s with bad scale %f [0..%f]\n", GetDebugName(), m_flSpriteScale.Get(), MAX_SPRITE_SCALE );
		scale = clamp( m_flSpriteScale.Get(), 0, MAX_SPRITE_SCALE );
	}

	//Set our state
	SetBrightness( GetRenderAlpha() );
	SetScale( scale );

	// Server has no use for client-only entities.
	// Seems like a waste to create the entity, only to UTIL_Remove it on Spawn, but this pattern works safely...
	if ( FClassnameIs( this, "env_sprite_clientside" ) )
	{
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initialize absmin & absmax to the appropriate box
//-----------------------------------------------------------------------------
void CSprite::EnableWorldSpaceScale( bool bEnable )
{
	m_bWorldSpaceScale = bEnable;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize absmin & absmax to the appropriate box
//-----------------------------------------------------------------------------
void CSprite::ComputeWorldSpaceSurroundingBox_Impl( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	float flScale = m_flSpriteScale * 0.5f;

	if ( m_bWorldSpaceScale == false )
	{
		// Find the height and width of the source of the sprite
		float width = modelinfo->GetModelSpriteWidth( GetModel() );
		float height = modelinfo->GetModelSpriteHeight( GetModel() );
		flScale *= std::max( width, height );
	}

	pVecWorldMins->Init( -flScale, -flScale, -flScale );
	pVecWorldMaxs->Init( flScale, flScale, flScale );
	*pVecWorldMins += GetAbsOrigin();
	*pVecWorldMaxs += GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szModelName - 
//-----------------------------------------------------------------------------
void CSprite::SetModel_Impl( const char *szModelName )
{
	int index = modelinfo->GetModelIndex( szModelName );
	const model_t *model = modelinfo->GetModel( index );
	if ( model && modelinfo->GetModelType( model ) != mod_sprite )
	{
		Msg( "Setting CSprite to non-sprite model %s\n", szModelName?szModelName:"NULL" );
	}

	UTIL_SetModel( this, szModelName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::Precache_Impl( void )
{
	if ( GetModelName() != NULL_STRING )
	{
		PrecacheModel( STRING( GetModelName() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//-----------------------------------------------------------------------------
void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	SetModelName( MAKE_STRING(pSpriteName) );
	SetLocalOrigin( origin );
	Spawn();
}

int CSprite::UpdateTransmitState_Impl( void )
{
	if ( GetMoveParent() )
	{
		// we must call ShouldTransmit() if we have a move parent
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}
	else
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
}

int CSprite::ShouldTransmit_Impl( const CCheckTransmitInfo *pInfo )
{
	// Certain entities like sprites and ropes are strewn throughout the level and they rarely change.
	// For these entities, it's more efficient to transmit them once and then always leave them on
	// the client. Otherwise, the server will have to send big bursts of data with the entity states
	// as they come in and out of the PVS.
	
	if ( GetMoveParent() )
	{
		CBaseViewModel *pViewModel = ToBaseViewModel( GetMoveParent() );

		if ( pViewModel )
		{
			return pViewModel->ShouldTransmit( pInfo );
		}
	}
	
	return FL_EDICT_ALWAYS;
}
 
//-----------------------------------------------------------------------------
// Purpose: Fixup parent after restore
//-----------------------------------------------------------------------------
void CSprite::OnRestore_Impl()
{
	BaseClass::OnRestore_Impl();

	// Reset attachment after save/restore
	if ( GetFollowedEntity() )
	{
		SetAttachment( GetFollowedEntity(), m_nAttachment );
	}
	else
	{
		// Clear attachment
		m_hAttachedToEntity = nullptr;
		m_nAttachment = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSprite
//-----------------------------------------------------------------------------
CSprite *CSprite::SpriteCreate( const char *pSpriteName, const Vector &origin, bool animate )
{
	CSprite *pSprite = CREATE_ENTITY( CSprite, "env_sprite" );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->SetSolid( SOLID_NONE );
	UTIL_SetSize( pSprite, vec3_origin, vec3_origin );
	pSprite->SetMoveType( MOVETYPE_NONE );
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSprite
//-----------------------------------------------------------------------------
CSprite *CSprite::SpriteCreatePredictable( const char *module, int line, const char *pSpriteName, const Vector &origin, bool animate )
{
	CSprite *pSprite = ( CSprite * )CBaseEntity::CreatePredictedEntityByName( "env_sprite", module, line );
	if ( pSprite )
	{
		pSprite->SpriteInit( pSpriteName, origin );
		pSprite->SetSolid( SOLID_NONE );
		pSprite->SetSize( vec3_origin, vec3_origin );
		pSprite->SetMoveType( MOVETYPE_NONE );
		if ( animate )
			pSprite->TurnOn();
	}

	return pSprite;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::AnimateThink( void )
{
	Animate( m_flSpriteFramerate * (gpGlobals->curtime - m_flLastTime) );

	SetNextThink( gpGlobals->curtime );
	m_flLastTime			= gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->curtime > m_flDieTime )
	{
		Remove( );
	}
	else
	{
		AnimateThink();
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scaleSpeed - 
//			fadeSpeed - 
//-----------------------------------------------------------------------------
void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	m_flSpeed = scaleSpeed;
	m_iHealth = fadeSpeed;
	SetThink( &CSprite::ExpandThink );

	SetNextThink( gpGlobals->curtime );
	m_flLastTime	= gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->curtime - m_flLastTime;
	SetSpriteScale( m_flSpriteScale + m_flSpeed * frametime );

	int sub = (int)(m_iHealth * frametime);
	if ( sub > GetRenderAlpha() )
	{
		SetRenderAlpha( 0 );
		Remove( );
	}
	else
	{
		SetRenderAlpha( GetRenderAlpha() - sub );
		SetNextThink( gpGlobals->curtime );
		m_flLastTime		= gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frames - 
//-----------------------------------------------------------------------------
void CSprite::Animate( float frames )
{ 
	m_flFrame += frames;
	if ( m_flFrame > m_flMaxFrame )
	{
		if ( m_spawnflags & SF_SPRITE_ONCE )
		{
			TurnOff();
		}
		else
		{
			if ( m_flMaxFrame > 0 )
				m_flFrame = std::fmod( m_flFrame, m_flMaxFrame );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSprite::SetBrightness( int brightness, float time )
{
	m_nBrightness			= brightness;	//Take our current position as our starting position
	m_flBrightnessTime		= time;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::SetSpriteScale( float scale )
{
	if ( scale != m_flSpriteScale )
	{
		m_flSpriteScale		= scale;	//Take our current position as our new starting position
		// The surrounding box is based on sprite scale... it changes, box is dirty
		CollisionProp()->MarkSurroundingBoundsDirty();
	}
}

void CSprite::SetScale( float scale, float time )
{
	m_flScaleTime		= time;
	SetSpriteScale( scale );
	// The surrounding box is based on sprite scale... it changes, box is dirty
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::TurnOff( void )
{
	AddEffects( EF_NODRAW );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSprite::TurnOn( void )
{
	RemoveEffects( EF_NODRAW );
	if ( (m_flSpriteFramerate && m_flMaxFrame > 1.0)
		|| (m_spawnflags & SF_SPRITE_ONCE) 
		)
	{
		SetThink( &CSprite::AnimateThink );
		SetNextThink( gpGlobals->curtime );
		m_flLastTime = gpGlobals->curtime;
	}
	m_flFrame = 0;
}

// DVS TODO: Obsolete Use handler
void CSprite::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on = !IsEffectActive( EF_NODRAW );
	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			TurnOff();
		}
		else
		{
			TurnOn();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that hides the sprite.
//-----------------------------------------------------------------------------
void CSprite::InputHideSprite( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that hides the sprite.
//-----------------------------------------------------------------------------
void CSprite::InputShowSprite( inputdata_t &inputdata )
{
	TurnOn();
}

void CSprite::InputColorRedValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( nNewColor, m_clrRender->g, m_clrRender->b );
}

void CSprite::InputColorGreenValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, nNewColor, m_clrRender->b );
}

void CSprite::InputColorBlueValue( inputdata_t &inputdata )
{
	int nNewColor = clamp( inputdata.value.Float(), 0, 255 );
	SetColor( m_clrRender->r, m_clrRender->g, nNewColor );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that toggles the sprite between hidden and shown.
//-----------------------------------------------------------------------------
void CSprite::InputToggleSprite( inputdata_t &inputdata )
{
	if ( !IsEffectActive( EF_NODRAW ) )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: oriented sprites
//			CSprites swap the roll and yaw angle inputs, and rotate the yaw 180 degrees
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_SERVERCLASS( CSpriteOriented, DT_SpriteOriented )
LINK_ENTITY_TO_CLASS_ALIASED( env_sprite_oriented, SpriteOriented );

void CSpriteOriented::Spawn_Impl( void )
{
	// save a copy of the angles, CSprite swaps the yaw and roll
	QAngle angles = GetAbsAngles();
	BaseClass::Spawn_Impl();
	// ORIENTED sprites "forward" vector points in the players "view" direction, not the direction "out" from the sprite (gah)
	angles.y = anglemod( angles.y + 180 );
	SetAbsAngles( angles );
}

int CSprite::ObjectCaps_Impl( void)
{
	int flags = 0;
	
	if ( IsTemporary() )
	{
		flags = FCAP_DONT_SAVE;
	}
	
	return (BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) | flags; 
}

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: an entity which turns on and off counting and display of the particle
// performance metric

//-----------------------------------------------------------------------------
// Purpose: Entity that particle performance measuring
//-----------------------------------------------------------------------------
class CParticlePerformanceMonitor : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ParticlePerformanceMonitor" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CParticlePerformanceMonitor, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn_Impl( void );
	int		UpdateTransmitState_Impl( void );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOnDisplay", .type = FIELD_VOID } ]] void	InputTurnOnDisplay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOffDisplay", .type = FIELD_VOID } ]] void	InputTurnOffDisplay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StartMeasuring", .type = FIELD_VOID } ]] void	InputStartMeasuring( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopMeasuring", .type = FIELD_VOID } ]] void	InputStopMeasuring( inputdata_t &inputdata );

private:
	CNetworkVar( bool, m_bDisplayPerf, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bMeasurePerf, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
};

LINK_ENTITY_TO_CLASS( env_particle_performance_monitor, CParticlePerformanceMonitor );

IMPLEMENT_REFLECT_DATAMAP( CParticlePerformanceMonitor )

IMPLEMENT_REFLECT_SERVERCLASS( CParticlePerformanceMonitor, DT_ParticlePerformanceMonitor )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticlePerformanceMonitor::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	m_bDisplayPerf = false;
	m_bMeasurePerf = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CParticlePerformanceMonitor::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CParticlePerformanceMonitor::InputTurnOnDisplay( inputdata_t &inputdata )
{
	m_bDisplayPerf = true;
}

void CParticlePerformanceMonitor::InputTurnOffDisplay( inputdata_t &inputdata )
{
	m_bDisplayPerf = false;
}

void CParticlePerformanceMonitor::InputStartMeasuring( inputdata_t &inputdata )
{
	m_bMeasurePerf = true;
}

void CParticlePerformanceMonitor::InputStopMeasuring( inputdata_t &inputdata )
{
	m_bMeasurePerf = false;
}

using namespace ::ks::reflect;

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Save/Restore
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP( CSpriteTrail )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( SpriteTrail, DT_SpriteTrail );

IMPLEMENT_REFLECT_TABLE( CSpriteTrail, DT_SpriteTrail );

LINK_ENTITY_TO_CLASS_ALIASED( env_spritetrail, SpriteTrail );

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSpriteTrail::CSpriteTrail( void )
{

	m_flStartWidthVariance = 0;
	m_vecSkyboxOrigin.Init( 0, 0, 0 );
	m_flSkyboxScale = 1.0f;
	m_flEndWidth = -1.0f;
}

void CSpriteTrail::Spawn_Impl( void )
{

	if ( GetModelName() != NULL_STRING )
	{
		BaseClass::Spawn_Impl();
		return;
	}

	SetModelName( m_iszSpriteName );
	BaseClass::Spawn_Impl();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NOCLIP );

	SetCollisionBounds( vec3_origin, vec3_origin );
	TurnOn();

}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::Precache_Impl( void ) 
{
	BaseClass::Precache_Impl();

	if ( m_iszSpriteName != NULL_STRING )
	{
		PrecacheModel( STRING(m_iszSpriteName) );
	}
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::SetLifeTime( float time ) 
{ 
	m_flLifeTime = time; 
}

void CSpriteTrail::SetStartWidth( float flStartWidth )
{ 
	m_flStartWidth = flStartWidth; 
	m_flStartWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetStartWidthVariance( float flStartWidthVariance )
{ 
	m_flStartWidthVariance = flStartWidthVariance; 
	m_flStartWidthVariance /= m_flSkyboxScale;
}

void CSpriteTrail::SetEndWidth( float flEndWidth )
{ 
	m_flEndWidth = flEndWidth; 
	m_flEndWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetTextureResolution( float flTexelsPerInch )
{ 
	m_flTextureRes = flTexelsPerInch; 
	m_flTextureRes *= m_flSkyboxScale;
}

void CSpriteTrail::SetMinFadeLength( float flMinFadeLength )
{
	m_flMinFadeLength = flMinFadeLength;
	m_flMinFadeLength /= m_flSkyboxScale;
}

void CSpriteTrail::SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale )
{
	m_flTextureRes /= m_flSkyboxScale;
	m_flMinFadeLength *= m_flSkyboxScale;
	m_flStartWidth *= m_flSkyboxScale;
	m_flEndWidth *= m_flSkyboxScale;
	m_flStartWidthVariance *= m_flSkyboxScale;

	m_vecSkyboxOrigin = vecSkyboxOrigin;
	m_flSkyboxScale = flSkyboxScale;

	m_flTextureRes *= m_flSkyboxScale;
	m_flMinFadeLength /= m_flSkyboxScale;
	m_flStartWidth /= m_flSkyboxScale;
	m_flEndWidth /= m_flSkyboxScale;
	m_flStartWidthVariance /= m_flSkyboxScale;

	if ( IsInSkybox() )
	{
		AddEFlags( EFL_IN_SKYBOX ); 
	}
	else
	{
		RemoveEFlags( EFL_IN_SKYBOX ); 
	}
}

//-----------------------------------------------------------------------------
// Is the trail in the skybox?
//-----------------------------------------------------------------------------
bool CSpriteTrail::IsInSkybox() const
{
	return (m_flSkyboxScale != 1.0f) || (m_vecSkyboxOrigin != vec3_origin);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSpriteTrail
//-----------------------------------------------------------------------------
CSpriteTrail *CSpriteTrail::SpriteTrailCreate( const char *pSpriteName, const Vector &origin, bool animate )
{
	CSpriteTrail *pSprite = CREATE_ENTITY( CSpriteTrail, "env_spritetrail" );

	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->SetSolid( SOLID_NONE );
	pSprite->SetMoveType( MOVETYPE_NOCLIP );
	
	UTIL_SetSize( pSprite, vec3_origin, vec3_origin );

	if ( animate )
	{
		pSprite->TurnOn();
	}

	return pSprite;
}

class CBaseEntity;
class CSteamJet;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements the server side of a steam jet particle system entity.

//Networking
IMPLEMENT_REFLECT_SERVERCLASS( CSteamJet, DT_SteamJet )

LINK_ENTITY_TO_CLASS( env_steam, CSteamJet );
LINK_ENTITY_TO_CLASS_ALIAS( env_steamjet, CSteamJet ); // For support of legacy env_steamjet, which faced left instead of forward.

//Save/restore
IMPLEMENT_REFLECT_DATAMAP( CSteamJet )

CSteamJet::CSteamJet( void )
{
	m_flRollSpeed = 8.0f;
}
//-----------------------------------------------------------------------------
// Purpose: Called before spawning, after key values have been set.
//-----------------------------------------------------------------------------
void CSteamJet::Spawn_Impl( void )
{
	Precache();

	//
	// Legacy env_steamjet pointed left instead of forward.
	//
	if ( FClassnameIs( this, "env_steamjet" ))
	{
		m_bFaceLeft = true;
	}

	if ( m_InitialState )
	{
		m_bEmit = true;
	}
}

void CSteamJet::Precache_Impl( void )
{
	PrecacheMaterial( "particle/particle_smokegrenade" );
	PrecacheMaterial( "sprites/heatwave" );
}

 void CSteamJet::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
 {
	if (!pActivator->IsPlayer())
	{
		if (useType == USE_ON)
		{
			m_bEmit = true;
		}
		else if (useType == USE_OFF)
		{
			m_bEmit = false;
		}
	}
 }

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the steam jet on/off.
//-----------------------------------------------------------------------------
void CSteamJet::InputToggle(inputdata_t &data)
{
	m_bEmit = !m_bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning on the steam jet.
//-----------------------------------------------------------------------------
void CSteamJet::InputTurnOn(inputdata_t &data)
{
	m_bEmit = true;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for turning off the steam jet.
//-----------------------------------------------------------------------------
void CSteamJet::InputTurnOff(inputdata_t &data)
{
	m_bEmit = false;
}

KS_ENTITY_ABSTRACT_KIND( CSpatialEntity )
