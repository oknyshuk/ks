// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "shareddefs.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "npcevent.h"
#include "ModelSoundsCache.h"
#include "studio.h"
#include "eventlist.h"
#include "scriptevent.h"

extern ISoundEmitterSystemBase *soundemitterbase;

CStudioHdr *ModelSoundsCache_LoadModel( char const *filename );
void ModelSoundsCache_PrecacheScriptSound( const char *soundname );
void ModelSoundsCache_FinishModel( CStudioHdr *hdr );
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hdr - 
// Output : static void
//-----------------------------------------------------------------------------

void VerifySequenceIndex( CStudioHdr *pstudiohdr );

// HACK:  This must match the #define in cl_animevent.h in the client .dll code!!!
#define CL_EVENT_SOUND				5004
#define CL_EVENT_FOOTSTEP_LEFT		6004
#define CL_EVENT_FOOTSTEP_RIGHT		6005
#define CL_EVENT_MFOOTSTEP_LEFT		6006
#define CL_EVENT_MFOOTSTEP_RIGHT	6007


extern ISoundEmitterSystemBase *soundemitterbase;

CModelSoundsCache::CModelSoundsCache()
{
}

CModelSoundsCache::CModelSoundsCache( const CModelSoundsCache& src )
{
	sounds = src.sounds;
}

char const *CModelSoundsCache::GetSoundName( int index )
{
	return soundemitterbase->GetSoundName( sounds.begin()[ index ] );
}

void CModelSoundsCache::Save( ks::byte_writer& buf )
{
	buf.write< short >( std::ssize( sounds ) );
	
	for ( int i = 0; i < std::ssize( sounds ); ++i )
	{
		buf.write_cstring( GetSoundName( i ) );
	}
}

void CModelSoundsCache::Restore( ks::byte_reader& buf )
{
	unsigned short c;

	c = (unsigned short)buf.read< short >();

	for ( int i = 0; i < c; ++i )
	{
		char soundname[ 512 ];

		buf.read_cstring( soundname, sizeof( soundname ) );

		int idx = soundemitterbase->GetSoundIndex( soundname );
		if ( soundemitterbase->IsValidIndex( idx ) )
		{
			sounds.insert( idx );
		}
	}
}

void CModelSoundsCache::Rebuild( char const *filename )
{
	sounds.clear();

	CStudioHdr *hdr = ModelSoundsCache_LoadModel( filename );

	if ( hdr )
	{
		// Precache all sounds referenced in animation events
		BuildAnimationEventSoundList( hdr, sounds );
		ModelSoundsCache_FinishModel( hdr );
	}
}

void CModelSoundsCache::PrecacheSoundList()
{
	for ( int i = 0; i < std::ssize( sounds ); ++i )
	{
		ModelSoundsCache_PrecacheScriptSound( GetSoundName( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : sounds - 
//			*soundname - 
//-----------------------------------------------------------------------------
void CModelSoundsCache::FindOrAddScriptSound( std::flat_set< int >& sounds, char const *soundname )
{
	int soundindex = soundemitterbase->GetSoundIndex( soundname );
	if ( soundemitterbase->IsValidIndex( soundindex ) )
	{
		// Only add it once per model...
		sounds.insert( soundindex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : *hdr - 
//			sounds - 
//-----------------------------------------------------------------------------
void CModelSoundsCache::BuildAnimationEventSoundList( CStudioHdr *hdr, std::flat_set< int >& sounds )
{
	contract_assert( hdr );
	
	// force animation event resolution!!!
	VerifySequenceIndex( hdr );

	// Find all animation events which fire off sound script entries...
	for ( int iSeq=0; iSeq < hdr->GetNumSeq(); iSeq++ )
	{
		mstudioseqdesc_t *pSeq = &hdr->pSeqdesc( iSeq );
		
		// Now read out all the sound events with their timing
		for ( int iEvent=0; iEvent < (int)pSeq->numevents; iEvent++ )
		{
			mstudioevent_t *pEvent = (mstudioevent_for_client_server_t*)pSeq->pEvent( iEvent );
			
			int nEvent = pEvent->Event();
			
			switch ( nEvent )
			{
			default:
				{
					if ( pEvent->type & AE_TYPE_NEWEVENTSYSTEM )
					{
						if ( nEvent == AE_SV_PLAYSOUND )
						{
							FindOrAddScriptSound( sounds, pEvent->pszOptions() );
						}
					}
				}
				break;
			// Old-style client .dll animation event
			case CL_EVENT_SOUND:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;
			case CL_EVENT_FOOTSTEP_LEFT:
			case CL_EVENT_FOOTSTEP_RIGHT:
				{
					char soundname[256];
					char const *options = pEvent->pszOptions();
					if ( !options || !options[0] )
					{
						options = "NPC_CombineS";
					}

					V_snprintf( soundname, 256, "%s.RunFootstepLeft", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.RunFootstepRight", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.FootstepLeft", options );
					FindOrAddScriptSound( sounds, soundname );
					V_snprintf( soundname, 256, "%s.FootstepRight", options );
					FindOrAddScriptSound( sounds, soundname );
				}
				break;
			case AE_CL_PLAYSOUND:
				{
					if ( !( pEvent->type & AE_TYPE_CLIENT ) )
						break;

					if ( pEvent->pszOptions()[0] )
					{
						FindOrAddScriptSound( sounds, pEvent->pszOptions() );
					}
					else
					{
						Warning( "-- Error --:  empty soundname, .qc error on AE_CL_PLAYSOUND in model %s, sequence %s, animevent # %i\n", 
							hdr->pszName(), pSeq->pszLabel(), iEvent+1 );
					}
				}
				break;
			case SCRIPT_EVENT_SOUND:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;

			case SCRIPT_EVENT_SOUND_VOICE:
				{
					FindOrAddScriptSound( sounds, pEvent->pszOptions() );
				}
				break;
			}
		}
	}
}
