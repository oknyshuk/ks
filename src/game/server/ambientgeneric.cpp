// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <stdlib.h>
#include <math.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "util.h"
#include "bone_setup.h"
#include "edict.h"
#include "ai_activity.h"
#include "tier2/tier2_logging.h"
#include "BaseAnimatingOverlay.h"
#include "declare_class.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :player;
import :vscript;
import :sharedvar;
import :baseentity;
import :entity_dispatch;
import :entity_kind;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import :entityutil;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import ks.core.platform;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.strtools;
import ks.core.random;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.math.vmatrix;
import ks.tier1.map_ops;
import ks.game.ai_activity;
import ks.game.iplayeranimstate;
import ks.math.ssemath;
import ks.math.vector2d;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.registry;

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: ambient_generic: a sound emitter used for one-shot and looping sounds.

//-----------------------------------------------------------------------------
// Purpose: Compute a suitable attenuation value given an audible radius
// Input  : radius - 
//			playEverywhere - (disable attenuation)
//-----------------------------------------------------------------------------
static constexpr auto REFERENCE_dB = 60.0;

static constexpr auto AMBIENT_GENERIC_UPDATE_RATE = 5; // update at 5hz
inline constexpr auto AMBIENT_GENERIC_THINK_DELAY = ( 1.0f / float( AMBIENT_GENERIC_UPDATE_RATE ) );

static constexpr auto REFERENCE_dB_DISTANCE = 36.0;

static soundlevel_t ComputeSoundlevel( float radius, bool playEverywhere )
{
	soundlevel_t soundlevel = SNDLVL_NONE;

	if ( radius > 0 && !playEverywhere )
	{
		// attenuation is set to a distance, compute falloff

		float dB_loss = 20 * std::log10( radius / REFERENCE_dB_DISTANCE );

		soundlevel = (soundlevel_t)(int)(40 + dB_loss); // sound at 40dB at reference distance
	}

	return soundlevel;
}

// ==================== GENERIC AMBIENT SOUND ======================================

static constexpr auto CDPVPRESETMAX = 27;

// presets for runtime pitch and vol modulation of ambient sounds

dynpitchvol_t rgdpvpreset[CDPVPRESETMAX] = 
{
	// pitch	pstart	spinup	spindwn	volrun	volstrt	fadein	fadeout	lfotype	lforate	modptch modvol	cspnup		
	{1,	255,	 75,	95,		95,		10,		1,		50,		95, 	0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
	{2,	255,	 85,	70,		88,		10,		1,		20,		88,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
	{3,	255,	100,	50,		75,		10,		1,		10,		75,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{4,	100,	100,	0,		0,		10,		1,		90,		90,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{5,	100,	100,	0,		0,		10,		1,		80,		80,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{6,	100,	100,	0,		0,		10,		1,		50,		70,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{7,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		50,		0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
	{8,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		150,	0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
	{9,	100,	100,	0,		0,		 5,		1,		40,		50,		1,		750,	0,		10,		0,		0,0,0,0,0,0,0,0,0,0},
	{10,128,	100,	50,		75,		10,		1,		30,		40,		2,		 8,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{11,128,	100,	50,		75,		10,		1,		30,		40,		2,		25,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{12,128,	100,	50,		75,		10,		1,		30,		40,		2,		70,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{13,50,		 50,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{14,70,		 70,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{15,90,		 90,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{16,120,	120,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{17,180,	180,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{18,255,	255,	0,		0,		10,		1,		20,		50,		0,		0,		0,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{19,200,	 75,	90,		90,		10,		1,		50,		90,		2,		100,	20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{20,255,	 75,	97,		90,		10,		1,		50,		90,		1,		40,		50,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{21,100,	100,	0,		0,		10,		1,		30,		50,		3,		15,		20,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{22,160,	160,	0,		0,		10,		1,		50,		50,		3,		500,	25,		0,		0,		0,0,0,0,0,0,0,0,0,0},
	{23,255,	 75,	88,		0,		10,		1,		40,		0,		0,		0,		0,		0,		5,		0,0,0,0,0,0,0,0,0,0}, 
	{24,200,	 20,	95,	    70,		10,		1,		70,		70,		3,		20,		50,		0,		0,		0,0,0,0,0,0,0,0,0,0}, 
	{25,180,	100,	50,		60,		10,		1,		40,		60,		2,		90,		100,	100,	0,		0,0,0,0,0,0,0,0,0,0}, 
	{26,60,		 60,	0,		0,		10,		1,		40,		70,		3,		80,		20,		50,		0,		0,0,0,0,0,0,0,0,0,0}, 
	{27,128,	 90,	10,		10,		10,		1,		20,		40,		1,		5,		10,		20,		0,		0,0,0,0,0,0,0,0,0,0}
};

LINK_ENTITY_TO_CLASS( ambient_generic, CAmbientGeneric );

IMPLEMENT_REFLECT_DATAMAP( CAmbientGeneric )

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CAmbientGeneric::Spawn_Impl( void )
{
	m_iSoundLevel = ComputeSoundlevel( m_radius, FBitSet( m_spawnflags, SF_AMBIENT_SOUND_EVERYWHERE )?true:false );
	ComputeMaxAudibleDistance( );

	char *szSoundFile = (char *)STRING( m_iszSound );
	if ( !m_iszSound || std::strlen( szSoundFile ) < 1 )
	{
		Warning( "Empty %s (%s) at %.2f, %.2f, %.2f\n", GetClassname(), GetDebugName(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove(this);
		return;
	}

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	// Set up think function for dynamic modification 
	// of ambient sound's pitch or volume. Don't
	// start thinking yet.

	SetThink(&CAmbientGeneric::RampThink);
	SetNextThink( TICK_NEVER_THINK );

	m_fActive = false;

	if ( FBitSet ( m_spawnflags, SF_AMBIENT_SOUND_NOT_LOOPING ) )
	{
		m_fLooping = false;
	}
	else
	{
		m_fLooping = true;
	}

	m_hSoundSource = nullptr;
	m_nSoundSourceEntIndex = -1;

	Precache( );

	// init all dynamic modulation parms
	InitModulationParms();
}

//-----------------------------------------------------------------------------
// Computes the max audible radius for a given sound level
//-----------------------------------------------------------------------------
static constexpr auto MIN_AUDIBLE_VOLUME = 1.01e-3;

void CAmbientGeneric::ComputeMaxAudibleDistance( )
{
	if (( m_iSoundLevel == SNDLVL_NONE )	|| ( m_radius == 0.0f ))
	{
		m_flMaxRadius = -1.0f;
		return;
	}

	// Sadly, there's no direct way of getting at this. 
	// We have to do an interative computation.
	float flGain = enginesound->GetDistGainFromSoundLevel( m_iSoundLevel, m_radius );
	if ( flGain <= MIN_AUDIBLE_VOLUME )
	{
		m_flMaxRadius = m_radius;
		return;
	}

	float flMinRadius = m_radius; 
	float flMaxRadius = m_radius * 2;
	while ( true )
	{
		// First, find a min + max range surrounding the desired distance gain
		float flGain = enginesound->GetDistGainFromSoundLevel( m_iSoundLevel, flMaxRadius );
		if ( flGain <= MIN_AUDIBLE_VOLUME )
			break;

		// Always audible.
		if ( flMaxRadius > 1e5 )
		{
			m_flMaxRadius = -1.0f;
			return;
		}

		flMinRadius = flMaxRadius;
		flMaxRadius *= 2.0f;
	}

	// Now home in a little bit
	int nInterations = 4;
	while ( --nInterations >= 0 )
	{
		float flTestRadius = (flMinRadius + flMaxRadius) * 0.5f;
		float flGain = enginesound->GetDistGainFromSoundLevel( m_iSoundLevel, flTestRadius );
		if ( flGain <= MIN_AUDIBLE_VOLUME )
		{
			flMaxRadius = flTestRadius;
		}
		else
		{
			flMinRadius = flTestRadius;
		}
	}

	m_flMaxRadius = flMaxRadius;
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for changing pitch.
// Input  : Float new pitch from 0 - 255 (100 = as recorded).
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputPitch( inputdata_t &inputdata )
{
	m_dpv.pitch = clamp( inputdata.value.Float(), 0, 255 );

	SendSound( SND_CHANGE_PITCH );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for changing volume.
// Input  : Float new volume, from 0 - 10.
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputVolume( inputdata_t &inputdata )
{
	//
	// Multiply the input value by ten since volumes are expected to be from 0 - 100.
	//
	m_dpv.vol = clamp( inputdata.value.Float(), 0, 10 ) * 10;
	m_dpv.volfrac = m_dpv.vol << 8;

	SendSound( SND_CHANGE_VOL );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for fading in volume over time.
// Input  : Float volume fade in time 0 - 100 seconds
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputFadeIn( inputdata_t &inputdata )
{
	// cancel any fade out that might be happening
	m_dpv.fadeout = 0;

	m_dpv.fadein = inputdata.value.Float();
	if (m_dpv.fadein > 100) m_dpv.fadein = 100;
	if (m_dpv.fadein < 0) m_dpv.fadein = 0;

	if (m_dpv.fadein > 0)
		m_dpv.fadein = ( 100 << 8 ) / ( m_dpv.fadein * AMBIENT_GENERIC_UPDATE_RATE );

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for fading out volume over time.
// Input  : Float volume fade out time 0 - 100 seconds
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputFadeOut( inputdata_t &inputdata )
{
	// cancel any fade in that might be happening
	m_dpv.fadein = 0;

	m_dpv.fadeout = inputdata.value.Float();

	if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
	if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

	if (m_dpv.fadeout > 0)
		m_dpv.fadeout = ( 100 << 8 ) / ( m_dpv.fadeout * AMBIENT_GENERIC_UPDATE_RATE );

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CAmbientGeneric::Precache_Impl( void )
{
	char *szSoundFile = (char *)STRING( m_iszSound );
	if ( m_iszSound != NULL_STRING && std::strlen( szSoundFile ) > 1 )
	{
		if (*szSoundFile != '!')
		{
			PrecacheScriptSound(szSoundFile);
		}
	}

	if ( !FBitSet (m_spawnflags, SF_AMBIENT_SOUND_START_SILENT ) )
	{
		// start the sound ASAP
		if (m_fLooping)
			m_fActive = true;
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAmbientGeneric::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// Initialize sound source.  If no source was given, or source can't be found
	// then this is the source
	if (m_hSoundSource == nullptr)
	{
		if (m_sSourceEntName != NULL_STRING)
		{
			m_hSoundSource = gEntList.FindEntityByName( nullptr, m_sSourceEntName );
			if ( m_hSoundSource != nullptr )
			{
				m_nSoundSourceEntIndex = m_hSoundSource->entindex();
			}
		}

		if (m_hSoundSource == nullptr)
		{
			m_hSoundSource = this;
			m_nSoundSourceEntIndex = entindex();
		}
		else
		{
			if ( !FBitSet( m_spawnflags, SF_AMBIENT_SOUND_EVERYWHERE ) )
			{
				AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
			}
		}
	}

	// If active start the sound
	if ( m_fActive )
	{
		int flags = SND_SPAWNING;
		// If we are loading a saved game, we can't write into the init/signon buffer here, so just issue
		//  as a regular sound message...
		if ( gpGlobals->eLoadType == MapLoad_Transition ||
			gpGlobals->eLoadType == MapLoad_LoadGame || 
			g_pGameRules->InRoundRestart() )
		{
			flags = SND_NOFLAGS;
		}

		// Tracker 76119:  8/12/07 ywb: 
		//  Make sure pitch and volume are set up to the correct value (especially after restoring a .sav file)
		flags |= ( SND_CHANGE_PITCH | SND_CHANGE_VOL );  

		// Don't bother sending over to client if volume is zero, though
		CSoundParameters params;
		GetParametersForSound( STRING( m_iszSound ), params, nullptr );
		bool isNewScriptSound = params.m_hSoundScriptHash != SOUNDEMITTER_INVALID_HASH && params.m_nSoundEntryVersion > 1;
		bool isLoading = gpGlobals->eLoadType == MapLoad_LoadGame;
		if ( m_dpv.vol > 0 && !(isLoading && isNewScriptSound) )
		{
			SendSound( (SoundFlags_t)flags );
		}

		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Rules about which entities need to transmit along with me
//-----------------------------------------------------------------------------
void CAmbientGeneric::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Ambient generics never transmit; this is just a way for us to ensure
	// the sound source gets transmitted; that's why we don't call pInfo->m_pTransmitEdict->Set
	if ( !m_hSoundSource || m_hSoundSource == this || !m_fActive )
		return;

	// Don't bother sending the position of the source if we have to play everywhere
	if ( FBitSet( m_spawnflags, SF_AMBIENT_SOUND_EVERYWHERE ) )
		return;

	contract_assert( pInfo->m_pClientEnt );
	CBaseEntity *pClient = (CBaseEntity*)(pInfo->m_pClientEnt->GetUnknown());
	if ( !pClient )
		return;

	// Send the sound source if he's close enough
	if ( ( m_flMaxRadius < 0 ) || ( pClient->GetAbsOrigin().DistToSqr( m_hSoundSource->GetAbsOrigin() ) <= m_flMaxRadius * m_flMaxRadius ) )
	{
		m_hSoundSource->SetTransmit( pInfo, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAmbientGeneric::UpdateOnRemove_Impl( void )
{
	if ( m_fActive )
	{
		// Stop the sound we're generating
		SendSound( SND_STOP );
	}

	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: Think at 5hz if we are dynamically modifying pitch or volume of the
//			playing sound.  This function will ramp pitch and/or volume up or
//			down, modify pitch/volume with lfo if active.
//-----------------------------------------------------------------------------
void CAmbientGeneric::RampThink( void )
{
	int pitch = m_dpv.pitch; 
	int vol = m_dpv.vol;
	int flags = 0;
	int fChanged = 0;		// false if pitch and vol remain unchanged this round
	int	prev;

	if (!m_dpv.spinup && !m_dpv.spindown && !m_dpv.fadein && !m_dpv.fadeout && !m_dpv.lfotype)
		return;						// no ramps or lfo, stop thinking

	// ==============
	// pitch envelope
	// ==============
	if (m_dpv.spinup || m_dpv.spindown)
	{
		prev = m_dpv.pitchfrac >> 8;

		if (m_dpv.spinup > 0)
			m_dpv.pitchfrac += m_dpv.spinup;
		else if (m_dpv.spindown > 0)
			m_dpv.pitchfrac -= m_dpv.spindown;

		pitch = m_dpv.pitchfrac >> 8;

		if (pitch > m_dpv.pitchrun)
		{
			pitch = m_dpv.pitchrun;
			m_dpv.spinup = 0;				// done with ramp up
		}

		if (pitch < m_dpv.pitchstart)
		{
			pitch = m_dpv.pitchstart;
			m_dpv.spindown = 0;				// done with ramp down

			// shut sound off
			SendSound( SND_STOP );

			// return without setting m_flNextThink
			return;
		}

		if (pitch > 255) pitch = 255;
		if (pitch < 1) pitch = 1;

		m_dpv.pitch = pitch;

		fChanged |= (prev != pitch);
		flags |= SND_CHANGE_PITCH;
	}

	// ==================
	// amplitude envelope
	// ==================
	if (m_dpv.fadein || m_dpv.fadeout)
	{
		prev = m_dpv.volfrac >> 8;

		if (m_dpv.fadein > 0)
			m_dpv.volfrac += m_dpv.fadein;
		else if (m_dpv.fadeout > 0)
			m_dpv.volfrac -= m_dpv.fadeout;

		vol = m_dpv.volfrac >> 8;

		if (vol > m_dpv.volrun)
		{
			vol = m_dpv.volrun;
			m_dpv.volfrac = vol << 8;
			m_dpv.fadein = 0;				// done with ramp up
		}

		if (vol < m_dpv.volstart)
		{
			vol = m_dpv.volstart;
			m_dpv.vol = vol;
			m_dpv.volfrac = vol << 8;
			m_dpv.fadeout = 0;				// done with ramp down

			// shut sound off
			SendSound( SND_STOP );

			// return without setting m_flNextThink
			return;
		}

		if (vol > 100) 
		{
			vol = 100;
			m_dpv.volfrac = vol << 8;
		}
		if (vol < 1) 
		{
			vol = 1;
			m_dpv.volfrac = vol << 8;
		}

		m_dpv.vol = vol;

		fChanged |= (prev != vol);
		flags |= SND_CHANGE_VOL;
	}

	// ===================
	// pitch/amplitude LFO
	// ===================
	if (m_dpv.lfotype)
	{
		int pos;

		if (m_dpv.lfofrac > 0x6fffffff)
			m_dpv.lfofrac = 0;

		// update lfo, lfofrac/255 makes a triangle wave 0-255
		m_dpv.lfofrac += m_dpv.lforate;
		pos = m_dpv.lfofrac >> 8;

		if (m_dpv.lfofrac < 0)
		{
			m_dpv.lfofrac = 0;
			m_dpv.lforate = abs(m_dpv.lforate);
			pos = 0;
		}
		else if (pos > 255)
		{
			pos = 255;
			m_dpv.lfofrac = (255 << 8);
			m_dpv.lforate = -abs(m_dpv.lforate);
		}

		switch(m_dpv.lfotype)
		{
		case LFO_SQUARE:
			if (pos < 128)
				m_dpv.lfomult = 255;
			else
				m_dpv.lfomult = 0;

			break;
		case LFO_RANDOM:
			if (pos == 255)
				m_dpv.lfomult = random_valve->RandomInt(0, 255);
			break;
		case LFO_TRIANGLE:
		default: 
			m_dpv.lfomult = pos;
			break;
		}

		if (m_dpv.lfomodpitch)
		{
			prev = pitch;

			// pitch 0-255
			pitch += ((m_dpv.lfomult - 128) * m_dpv.lfomodpitch) / 100;

			if (pitch > 255) pitch = 255;
			if (pitch < 1) pitch = 1;

			fChanged |= (prev != pitch);
			flags |= SND_CHANGE_PITCH;
		}

		if (m_dpv.lfomodvol)
		{
			// vol 0-100
			prev = vol;

			vol += ((m_dpv.lfomult - 128) * m_dpv.lfomodvol) / 100;

			if (vol > 100) vol = 100;
			if (vol < 0) vol = 0;

			fChanged |= (prev != vol);
			flags |= SND_CHANGE_VOL;
		}

	}

	// Send update to playing sound only if we actually changed
	// pitch or volume in this routine.

	if (flags && fChanged) 
	{
		if (pitch == PITCH_NORM)
			pitch = PITCH_NORM + 1; // don't send 'no pitch' !

		CBaseEntity* pSoundSource = m_hSoundSource;
		if (pSoundSource)
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), 
				STRING( m_iszSound ), (vol * 0.01), m_iSoundLevel, flags, pitch);
		}
	}

	// update ramps at 5hz
	SetNextThink( gpGlobals->curtime + AMBIENT_GENERIC_THINK_DELAY );
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Init all ramp params in preparation to play a new sound.
//-----------------------------------------------------------------------------
void CAmbientGeneric::InitModulationParms(void)
{
	int pitchinc;

	m_dpv.volrun = m_iHealth * 10;	// 0 - 100
	if (m_dpv.volrun > 100) m_dpv.volrun = 100;
	if (m_dpv.volrun < 0) m_dpv.volrun = 0;

	// get presets
	if (m_dpv.preset != 0 && m_dpv.preset <= CDPVPRESETMAX)
	{
		// load preset values
		m_dpv = rgdpvpreset[m_dpv.preset - 1];

		// fixup preset values, just like
		// fixups in KeyValue routine.
		if (m_dpv.spindown > 0)
			m_dpv.spindown = (101 - m_dpv.spindown) * 64;
		if (m_dpv.spinup > 0)
			m_dpv.spinup = (101 - m_dpv.spinup) * 64;

		m_dpv.volstart *= 10;
		m_dpv.volrun *= 10;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = (101 - m_dpv.fadein) * 64;
		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = (101 - m_dpv.fadeout) * 64;

		m_dpv.lforate *= 256;

		m_dpv.fadeinsav = m_dpv.fadein;
		m_dpv.fadeoutsav = m_dpv.fadeout;
		m_dpv.spinupsav = m_dpv.spinup;
		m_dpv.spindownsav = m_dpv.spindown;
	}

	m_dpv.fadein = m_dpv.fadeinsav;
	m_dpv.fadeout = 0; 

	if (m_dpv.fadein)
		m_dpv.vol = m_dpv.volstart;
	else
		m_dpv.vol = m_dpv.volrun;

	m_dpv.spinup = m_dpv.spinupsav;
	m_dpv.spindown = 0; 

	if (m_dpv.spinup)
		m_dpv.pitch = m_dpv.pitchstart;
	else
		m_dpv.pitch = m_dpv.pitchrun;

	if (m_dpv.pitch == 0)
		m_dpv.pitch = PITCH_NORM;

	m_dpv.pitchfrac = m_dpv.pitch << 8;
	m_dpv.volfrac = m_dpv.vol << 8;

	m_dpv.lfofrac = 0;
	m_dpv.lforate = abs(m_dpv.lforate);

	m_dpv.cspincount = 1;

	if (m_dpv.cspinup) 
	{
		pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

		m_dpv.pitchrun = m_dpv.pitchstart + pitchinc;
		if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;
	}

	if ((m_dpv.spinupsav || m_dpv.spindownsav || (m_dpv.lfotype && m_dpv.lfomodpitch))
		&& (m_dpv.pitch == PITCH_NORM))
		m_dpv.pitch = PITCH_NORM + 1; // must never send 'no pitch' as first pitch
	// if we intend to pitch shift later!
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that begins playing the sound.
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputPlaySound( inputdata_t &inputdata )
{
	if (!m_fActive)
	{
		//Adrian: Stop our current sound before starting a new one!
		SendSound( SND_STOP ); 

		ToggleSound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops playing the sound.
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputStopSound( inputdata_t &inputdata )
{
	if (m_fActive)
	{
		ToggleSound();
	}
}

void CAmbientGeneric::SendSound( SoundFlags_t flags)
{
	char *szSoundFile = (char *)STRING( m_iszSound );
	CBaseEntity* pSoundSource = m_hSoundSource;
	if ( pSoundSource )
	{
		if ( flags == SND_STOP )
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), szSoundFile, 
				0, SNDLVL_NONE, flags, 0);
		}
		else
		{
			UTIL_EmitAmbientSound(pSoundSource->GetSoundSourceIndex(), pSoundSource->GetAbsOrigin(), szSoundFile, 
				(m_dpv.vol * 0.01), m_iSoundLevel, flags, m_dpv.pitch);
		}
	}	
	else
	{
		if ( ( flags == SND_STOP ) && 
			( m_nSoundSourceEntIndex != -1 ) )
		{
			UTIL_EmitAmbientSound(m_nSoundSourceEntIndex, GetAbsOrigin(), szSoundFile, 
				0, SNDLVL_NONE, flags, 0);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops playing the sound.
//-----------------------------------------------------------------------------
void CAmbientGeneric::InputToggleSound( inputdata_t &inputdata )
{
	ToggleSound();
}

//-----------------------------------------------------------------------------
// Purpose: Turns an ambient sound on or off.  If the ambient is a looping sound,
//			mark sound as active (m_fActive) if it's playing, innactive if not.
//			If the sound is not a looping sound, never mark it as active.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CAmbientGeneric::ToggleSound()
{
	// m_fActive is true only if a looping sound is playing.

	if ( m_fActive )
	{// turn sound off

		if (m_dpv.cspinup)
		{
			// Don't actually shut off. Each toggle causes
			// incremental spinup to max pitch

			if (m_dpv.cspincount <= m_dpv.cspinup)
			{	
				int pitchinc;

				// start a new spinup
				m_dpv.cspincount++;

				pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

				m_dpv.spinup = m_dpv.spinupsav;
				m_dpv.spindown = 0;

				m_dpv.pitchrun = m_dpv.pitchstart + pitchinc * m_dpv.cspincount;
				if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;

				SetNextThink( gpGlobals->curtime + 0.1f );
			}

		}
		else
		{
			m_fActive = false;

			// HACKHACK - this makes the code in Precache() work properly after a save/restore
			m_spawnflags |= SF_AMBIENT_SOUND_START_SILENT;

			if (m_dpv.spindownsav || m_dpv.fadeoutsav)
			{
				// spin it down (or fade it) before shutoff if spindown is set
				m_dpv.spindown = m_dpv.spindownsav;
				m_dpv.spinup = 0;

				m_dpv.fadeout = m_dpv.fadeoutsav;
				m_dpv.fadein = 0;
				SetNextThink( gpGlobals->curtime + 0.1f );
			}
			else
			{
				SendSound( SND_STOP ); // stop sound
			}
		}
	}
	else 
	{// turn sound on

		// only toggle if this is a looping sound.  If not looping, each
		// trigger will cause the sound to play.  If the sound is still
		// playing from a previous trigger press, it will be shut off
		// and then restarted.

		if (m_fLooping)
			m_fActive = true;
		else
		{
			// shut sound off now - may be interrupting a long non-looping sound
			SendSound( SND_STOP ); // stop sound
		}

		// init all ramp params for startup

		InitModulationParms();

		SendSound( SND_NOFLAGS ); // send sound

		SetNextThink( gpGlobals->curtime + 0.1f );

	} 
}

// KeyValue - load keyvalue pairs into member data of the
// ambient generic. NOTE: called BEFORE spawn!
bool CAmbientGeneric::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	// NOTE: changing any of the modifiers in this code
	// NOTE: also requires changing InitModulationParms code.

	// preset
	if (FStrEq(szKeyName, "preset"))
	{
		m_dpv.preset = std::atoi(szValue);
	}
	// pitchrun
	else if (FStrEq(szKeyName, "pitch"))
	{
		m_dpv.pitchrun = std::atoi(szValue);

		if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;
		if (m_dpv.pitchrun < 0) m_dpv.pitchrun = 0;
	}		
	// pitchstart
	else if (FStrEq(szKeyName, "pitchstart"))
	{
		m_dpv.pitchstart = std::atoi(szValue);

		if (m_dpv.pitchstart > 255) m_dpv.pitchstart = 255;
		if (m_dpv.pitchstart < 0) m_dpv.pitchstart = 0;
	}
	// spinup
	else if (FStrEq(szKeyName, "spinup"))
	{
		m_dpv.spinup = std::atoi(szValue);

		if (m_dpv.spinup > 100) m_dpv.spinup = 100;
		if (m_dpv.spinup < 0) m_dpv.spinup = 0;

		if (m_dpv.spinup > 0)
			m_dpv.spinup = (101 - m_dpv.spinup) * 64;
		m_dpv.spinupsav = m_dpv.spinup;
	}		
	// spindown
	else if (FStrEq(szKeyName, "spindown"))
	{
		m_dpv.spindown = std::atoi(szValue);

		if (m_dpv.spindown > 100) m_dpv.spindown = 100;
		if (m_dpv.spindown < 0) m_dpv.spindown = 0;

		if (m_dpv.spindown > 0)
			m_dpv.spindown = (101 - m_dpv.spindown) * 64;
		m_dpv.spindownsav = m_dpv.spindown;
	}
	// volstart
	else if (FStrEq(szKeyName, "volstart"))
	{
		m_dpv.volstart = std::atoi(szValue);

		if (m_dpv.volstart > 10) m_dpv.volstart = 10;
		if (m_dpv.volstart < 0) m_dpv.volstart = 0;

		m_dpv.volstart *= 10;	// 0 - 100
	}
	// legacy fadein
	else if (FStrEq(szKeyName, "fadein"))
	{
		m_dpv.fadein = std::atoi(szValue);

		if (m_dpv.fadein > 100) m_dpv.fadein = 100;
		if (m_dpv.fadein < 0) m_dpv.fadein = 0;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = (101 - m_dpv.fadein) * 64;
		m_dpv.fadeinsav = m_dpv.fadein;
	}
	// legacy fadeout
	else if (FStrEq(szKeyName, "fadeout"))
	{
		m_dpv.fadeout = std::atoi(szValue);

		if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
		if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = (101 - m_dpv.fadeout) * 64;
		m_dpv.fadeoutsav = m_dpv.fadeout;
	}
	// fadeinsecs
	else if (FStrEq(szKeyName, "fadeinsecs"))
	{
		m_dpv.fadein = std::atoi(szValue);

		if (m_dpv.fadein > 100) m_dpv.fadein = 100;
		if (m_dpv.fadein < 0) m_dpv.fadein = 0;

		if (m_dpv.fadein > 0)
			m_dpv.fadein = ( 100 << 8 ) / ( m_dpv.fadein * AMBIENT_GENERIC_UPDATE_RATE );
		m_dpv.fadeinsav = m_dpv.fadein;
	}
	// fadeoutsecs
	else if (FStrEq(szKeyName, "fadeoutsecs"))
	{
		m_dpv.fadeout = std::atoi(szValue);

		if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
		if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

		if (m_dpv.fadeout > 0)
			m_dpv.fadeout = ( 100 << 8 ) / ( m_dpv.fadeout * AMBIENT_GENERIC_UPDATE_RATE );
		m_dpv.fadeoutsav = m_dpv.fadeout;
	}
	// lfotype
	else if (FStrEq(szKeyName, "lfotype"))
	{
		m_dpv.lfotype = std::atoi(szValue);
		if (m_dpv.lfotype > 4) m_dpv.lfotype = LFO_TRIANGLE;
	}
	// lforate
	else if (FStrEq(szKeyName, "lforate"))
	{
		m_dpv.lforate = std::atoi(szValue);

		if (m_dpv.lforate > 1000) m_dpv.lforate = 1000;
		if (m_dpv.lforate < 0) m_dpv.lforate = 0;

		m_dpv.lforate *= 256;
	}
	// lfomodpitch
	else if (FStrEq(szKeyName, "lfomodpitch"))
	{
		m_dpv.lfomodpitch = std::atoi(szValue);
		if (m_dpv.lfomodpitch > 100) m_dpv.lfomodpitch = 100;
		if (m_dpv.lfomodpitch < 0) m_dpv.lfomodpitch = 0;
	}

	// lfomodvol
	else if (FStrEq(szKeyName, "lfomodvol"))
	{
		m_dpv.lfomodvol = std::atoi(szValue);
		if (m_dpv.lfomodvol > 100) m_dpv.lfomodvol = 100;
		if (m_dpv.lfomodvol < 0) m_dpv.lfomodvol = 0;
	}
	// cspinup
	else if (FStrEq(szKeyName, "cspinup"))
	{
		m_dpv.cspinup = std::atoi(szValue);
		if (m_dpv.cspinup > 100) m_dpv.cspinup = 100;
		if (m_dpv.cspinup < 0) m_dpv.cspinup = 0;
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

class CAmmoDef;
class CBaseCombatCharacter;

using namespace ::ks::reflect;

// Purpose: Base combat character with no AI

//-----------------------------------------------------------------------------
// Purpose: Return a pointer to the Ammo at the Index passed in
//-----------------------------------------------------------------------------
Ammo_t *CAmmoDef::GetAmmoOfIndex(int nAmmoIndex)
{
	if ( nAmmoIndex >= m_nAmmoIndex )
		return nullptr;

	return &m_AmmoType[ nAmmoIndex ];
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CAmmoDef::Index(const char *psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < m_nAmmoIndex; i++)
	{
		if (stricmp( psz, m_AmmoType[i].pName ) == 0)
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::PlrDamage(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pPlrDmg == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pPlrDmgCVar )
		{
			return m_AmmoType[nAmmoIndex].pPlrDmgCVar->GetFloat();
		}

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pPlrDmg;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::NPCDamage(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pNPCDmg == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pNPCDmgCVar )
		{
			return m_AmmoType[nAmmoIndex].pNPCDmgCVar->GetFloat();
		}

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pNPCDmg;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MaxCarry(int nAmmoIndex, const CBaseCombatCharacter *owner)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return 0;

	if ( m_AmmoType[nAmmoIndex].pMaxCarry == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pMaxCarryCVar )
			return m_AmmoType[nAmmoIndex].pMaxCarryCVar->GetInt();

		return 0;
	}
	else
	{
		return m_AmmoType[nAmmoIndex].pMaxCarry;
	}
}

bool CAmmoDef::CanCarryInfiniteAmmo(int nAmmoIndex)
{
	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return false;

	int maxCarry = m_AmmoType[nAmmoIndex].pMaxCarry;
	if ( maxCarry == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pMaxCarryCVar )
		{
			maxCarry = m_AmmoType[nAmmoIndex].pMaxCarryCVar->GetInt();
		}
	}
	return maxCarry == INFINITE_AMMO ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::DamageType(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].nDamageType;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CAmmoDef::Flags(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].nFlags;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MinSplashSize(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 4;

	return m_AmmoType[nAmmoIndex].nMinSplashSize;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::MaxSplashSize(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 8;

	return m_AmmoType[nAmmoIndex].nMaxSplashSize;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int	CAmmoDef::TracerType(int nAmmoIndex)
{
	if (nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex)
		return 0;

	return m_AmmoType[nAmmoIndex].eTracerType;
}

//-----------------------------------------------------------------------------
// Purpose: Figure out the damage force for a certian ammo type.
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CAmmoDef::DamageForce(int nAmmoIndex)
{
	const float DEFAULT_IMPULSE = 1.0f;

	if ( nAmmoIndex < 1 || nAmmoIndex >= m_nAmmoIndex )
		return DEFAULT_IMPULSE;

	if ( m_AmmoType[nAmmoIndex].pPhysicsForceImpulse == USE_CVAR )
	{
		if ( m_AmmoType[nAmmoIndex].pPhysicsForceImpulseCVar )
			return m_AmmoType[nAmmoIndex].pPhysicsForceImpulseCVar->GetFloat();

		return DEFAULT_IMPULSE;
	}
	else
	{
		return (float)m_AmmoType[nAmmoIndex].pPhysicsForceImpulse;
	}

}

//-----------------------------------------------------------------------------
// Purpose: Create an Ammo type with the name, decal, and tracer.
// Does not increment m_nAmmoIndex because the functions below do so and 
//  are the only entry point.
//-----------------------------------------------------------------------------
bool CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, int nFlags, int minSplashSize, int maxSplashSize )
{
	if (m_nAmmoIndex == MAX_AMMO_TYPES)
		return false;

	int len = std::strlen(name);
	m_AmmoType[m_nAmmoIndex].pName = new char[len+1];
	V_strncpy(m_AmmoType[m_nAmmoIndex].pName, name,len+1);
	m_AmmoType[m_nAmmoIndex].nDamageType	= damageType;
	m_AmmoType[m_nAmmoIndex].eTracerType	= tracerType;
	m_AmmoType[m_nAmmoIndex].nMinSplashSize	= minSplashSize;
	m_AmmoType[m_nAmmoIndex].nMaxSplashSize	= maxSplashSize;
	m_AmmoType[m_nAmmoIndex].nFlags	= nFlags;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Add an ammo type with it's damage & carrying capability specified via cvars
//-----------------------------------------------------------------------------
void CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, 
	char const* plr_cvar, char const* npc_cvar, char const* carry_cvar, 
	char const* impulse_cvar, int nFlags, int minSplashSize, int maxSplashSize)
{
	if ( AddAmmoType( name, damageType, tracerType, nFlags, minSplashSize, maxSplashSize ) == false )
		return;

	if (plr_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pPlrDmgCVar	= cvar->FindVar(plr_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pPlrDmgCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,plr_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pPlrDmg = USE_CVAR;
	}
	if (npc_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pNPCDmgCVar	= cvar->FindVar(npc_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pNPCDmgCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,npc_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pNPCDmg = USE_CVAR;
	}
	if (carry_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pMaxCarryCVar= cvar->FindVar(carry_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pMaxCarryCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,carry_cvar);
		}
		m_AmmoType[m_nAmmoIndex].pMaxCarry = USE_CVAR;
	}
	if (impulse_cvar)
	{
		m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar = cvar->FindVar(impulse_cvar);
		if (!m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar)
		{
			Msg("ERROR: Ammo (%s) found no CVar named (%s)\n",name,impulse_cvar);
		}
		else
		{
			// By default, use whatever value is in the convar at load time.
			// To enable real-time updating of the ammo impulse values, execute "tweak_ammo_impulse" which will set pPhysicsForceImpulse to USE_CVAR.
			m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulse = m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulseCVar->GetInt();
		}
	}

	m_nAmmoIndex++;
}

//-----------------------------------------------------------------------------
// Purpose: Add an ammo type with it's damage & carrying capability specified via integers
//-----------------------------------------------------------------------------
void CAmmoDef::AddAmmoType(char const* name, int damageType, int tracerType, 
	int plr_dmg, int npc_dmg, int carry, int impulse, 
	int nFlags, int minSplashSize, int maxSplashSize )
{
	if ( AddAmmoType( name, damageType, tracerType, nFlags, minSplashSize, maxSplashSize ) == false )
		return;

	m_AmmoType[m_nAmmoIndex].pPlrDmg = plr_dmg;
	m_AmmoType[m_nAmmoIndex].pNPCDmg = npc_dmg;
	m_AmmoType[m_nAmmoIndex].pMaxCarry = carry;
	m_AmmoType[m_nAmmoIndex].pPhysicsForceImpulse = impulse;

	m_nAmmoIndex++;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CAmmoDef::CAmmoDef(void)
{
	// Start with an index of 1.  Client assumes 0 is an invalid ammo type
	m_nAmmoIndex = 1;
	std::memset( m_AmmoType, 0, sizeof( m_AmmoType ) );
}

CAmmoDef::~CAmmoDef( void )
{
	for ( int i = 1; i < MAX_AMMO_TYPES; i++ )
	{
		delete[] m_AmmoType[ i ].pName;
	}
}

class CBaseEntity;
struct animevent_t;

using namespace ::ks::reflect;

#define iabs(i) (( (i) >= 0 ) ? (i) : -(i) )

int ExtractBbox( CStudioHdr *pstudiohdr, int sequence, Vector& mins, Vector& maxs )
{
	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( sequence );
	
	mins = seqdesc.bbmin;

	maxs = seqdesc.bbmax;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : *pstudiohdr - 
//			iSequence - 
//
// Output : mstudioseqdesc_t
//-----------------------------------------------------------------------------

extern int g_nActivityListVersion;
extern int g_nEventListVersion;

void SetEventIndexForSequence( mstudioseqdesc_t &seqdesc )
{
	if ( &seqdesc == nullptr )
		 return;

	if ( seqdesc.numevents == 0 )
		 return;

	seqdesc.flags |= STUDIO_EVENT;

	for ( int index = 0; index < (int)seqdesc.numevents; index++ )
	{
		mstudioevent_for_client_server_t *pevent = (mstudioevent_for_client_server_t*)seqdesc.pEvent( index );

		if ( !pevent )
			 continue;

		if ( pevent->type & AE_TYPE_NEWEVENTSYSTEM )
		{
			const char *pEventName = pevent->pszEventName();
			
			int iEventIndex = EventList_IndexForName( pEventName );
				
			if ( iEventIndex == -1 )
			{
				pevent->event_newsystem = EventList_RegisterPrivateEvent( pEventName );
			}
			else
			{
				pevent->event_newsystem = iEventIndex;
				pevent->type |= EventList_GetEventType( iEventIndex );
			}
		}
	}
}

mstudioevent_for_client_server_t *GetEventIndexForSequence( mstudioseqdesc_t &seqdesc )
{
	if ( !(seqdesc.flags & STUDIO_EVENT) )
	{
		SetEventIndexForSequence( seqdesc );
	}

	return (mstudioevent_for_client_server_t*)seqdesc.pEvent( 0 );
}

void BuildAllAnimationEventIndexes( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return;

	if( pstudiohdr->GetEventListVersion() != g_nEventListVersion )
	{
		for ( int i = 0 ; i < pstudiohdr->GetNumSeq() ; i++ )
		{
			SetEventIndexForSequence( pstudiohdr->pSeqdesc( i ) );
		}

		pstudiohdr->SetEventListVersion( g_nEventListVersion );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ensures that activity / index relationship is recalculated
// Input  :
// Output :
//-----------------------------------------------------------------------------
void ResetEventIndexes( CStudioHdr *pstudiohdr )
{	
	if (! pstudiohdr)
		return;

	pstudiohdr->SetEventListVersion( g_nEventListVersion - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void SetActivityForSequence( CStudioHdr *pstudiohdr, int i )
{
	int iActivityIndex;
	const char *pszActivityName;
	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );

	seqdesc.flags |= STUDIO_ACTIVITY;

	pszActivityName = GetSequenceActivityName( pstudiohdr, i );
	if ( pszActivityName[0] != '\0' )
	{
		iActivityIndex = ActivityList_IndexForName( pszActivityName );
		
		if ( iActivityIndex == -1 )
		{
			// Allow this now.  Animators can create custom activities that are referenced only on the client or by scripts, etc.
			//Warning( "***\nModel %s tried to reference unregistered activity: %s \n***\n", pstudiohdr->name, pszActivityName );
			//Assert(0);
			// HACK: the client and server don't share the private activity list so registering it on the client would hose the server
			seqdesc.activity = ActivityList_RegisterPrivateActivity( pszActivityName );
		}
		else
		{
			seqdesc.activity = iActivityIndex;
		}
	}
}

//=========================================================
// IndexModelSequences - set activity and event indexes for all model
// sequences that have them.
//=========================================================

void IndexModelSequences( CStudioHdr *pstudiohdr )
{
	CVProfScope _( "IndexModelSequences", 2, IsServerDll() ? VPROF_BUDGETGROUP_SERVER_ANIM : VPROF_BUDGETGROUP_CLIENT_ANIMATION, false, BUDGETFLAG_ALL );
	int i;

	if (! pstudiohdr)
		return;

	if (!pstudiohdr->SequencesAvailable())
		return;

	for ( i = 0 ; i < pstudiohdr->GetNumSeq() ; i++ )
	{
		SetActivityForSequence( pstudiohdr, i );
		SetEventIndexForSequence( pstudiohdr->pSeqdesc( i ) );
	}

	pstudiohdr->SetActivityListVersion( g_nActivityListVersion );
}

//-----------------------------------------------------------------------------
// Purpose: Ensures that activity / index relationship is recalculated
// Input  :
// Output :
//-----------------------------------------------------------------------------
void ResetActivityIndexes( CStudioHdr *pstudiohdr )
{	
	if (! pstudiohdr)
		return;

	pstudiohdr->SetActivityListVersion( g_nActivityListVersion - 1 );
}

void VerifySequenceIndex( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if( pstudiohdr->GetActivityListVersion( ) < g_nActivityListVersion ) // sometimes the server's numbers can get ahead of the client's if we're sharing memory between the two, so it's only necessary to reindex if a model is lagging the version number.
	{
		// this model's sequences have not yet been indexed by activity
		IndexModelSequences( pstudiohdr );
	}
}

bool IsInPrediction()
{
	return CBaseEntity::GetPredictionPlayer() != nullptr;
}

static int SelectWeightedSequenceFromMapping( CStudioHdr::CActivityToSequenceMapping &mapping, CStudioHdr *pstudiohdr, int activity, int curSequence );

int SelectWeightedSequence( CStudioHdr *pstudiohdr, int activity, int curSequence )
{
	CVProfScope _( "SelectWeightedSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	VProfIncrementCounter<"Server SelectWeightedSequence">( 1 );

	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	VerifySequenceIndex( pstudiohdr );

	int numSeq = pstudiohdr->GetNumSeq();
	if ( numSeq == 1 )
	{
		return ( GetSequenceActivity( pstudiohdr, 0, nullptr ) == activity ) ? 0 : ACTIVITY_NOT_AVAILABLE;
	}

	return SelectWeightedSequenceFromMapping( *pstudiohdr->ActivityToSequence(), pstudiohdr, activity, curSequence );
}

// Pick a sequence for the given activity. If the current sequence is appropriate for the 
// current activity, and its stored weight is negative (whatever that means), always select
// it. Otherwise perform a weighted selection -- imagine a large roulette wheel, with each
// sequence having a number of spaces corresponding to its weight.
static int SelectWeightedSequenceFromMapping( CStudioHdr::CActivityToSequenceMapping &mapping, CStudioHdr *pstudiohdr, int activity, int curSequence )
{
	using SequenceTuple = CStudioHdr::CActivityToSequenceMapping::SequenceTuple;
	using HashValueType = CStudioHdr::CActivityToSequenceMapping::HashValueType;
	// is the current sequence appropriate?
	if (curSequence >= 0)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( curSequence );

		if (seqdesc.activity == activity && seqdesc.actweight < 0)
			return curSequence;
	}

	if ( !pstudiohdr->SequencesAvailable() )
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	if ( pstudiohdr->GetNumSeq() == 1 )
	{
		contract_assert( false && "Expected single sequence case to be handled in ::SelectWeightedSequence()" );
		return ACTIVITY_NOT_AVAILABLE;
	}

	if (!mapping.ValidateAgainst(pstudiohdr))
	{
		contract_assert( false && "CStudioHdr %s has changed its vmodel pointer without reinitializing its activity mapping! Now performing emergency reinitialization." );
		ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } );
		mapping.Reinitialize(pstudiohdr);
	}

	// a null m_pSequenceTuples just means that this studio header has no activities.
	if (!mapping.m_pSequenceTuples)
		return ACTIVITY_NOT_AVAILABLE;

	// get the data for the given activity
	const HashValueType * __restrict actData = ks::find_ptr( mapping.m_ActToSeqHash, activity );
	if (!actData)
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	contract_assert( ( actData->totalWeight > 0 ) && "Activity %d has total weight of %d!" );
	int weighttotal = actData->totalWeight;

	// failsafe if the weight is 0: assume the artist screwed up and that the first sequence
	// for this activity should be returned.
	int randomValue = 0;
	if ( actData->totalWeight <= 0 )
	{
		Warning( "Activity %s has %d sequences with a total weight of %d!", ActivityList_NameForIndex(activity), actData->count, actData->totalWeight );
		return (mapping.m_pSequenceTuples + actData->startingIdx)->seqnum;
	}
	else if ( actData->totalWeight == 1 )
	{
		randomValue = 0;
	}
	else
	{
		// generate a random number from 0 to the total weight
		if ( IsInPrediction() )
		{
			randomValue = SharedRandomInt( "SelectWeightedSequence", 0, weighttotal - 1 );
		}
		else
		{
			randomValue = RandomInt( 0, weighttotal - 1 );
		}
	}

	// chug through the entries in the list (they are sequential therefore cache-coherent)
	// until we run out of random juice
	SequenceTuple * __restrict sequenceInfo = mapping.m_pSequenceTuples + actData->startingIdx;

	const SequenceTuple *const stopHere = sequenceInfo + actData->count; // this is a backup 
		// in case the weights are somehow miscalculated -- we don't read or write through
		// it (because it aliases the restricted pointer above); it's only here for 
		// the comparison.

	while (randomValue >= sequenceInfo->weight && sequenceInfo < stopHere)
	{
		randomValue -= sequenceInfo->weight;
		++sequenceInfo;
	}

	return sequenceInfo->seqnum;

}

int SelectWeightedSequenceFromModifiers( CStudioHdr *pstudiohdr, int activity, const ActivityModifierSym_t *pActivityModifiers, int iModifierCount )
{
	using SequenceTuple = CStudioHdr::CActivityToSequenceMapping::SequenceTuple;
	using HashValueType = CStudioHdr::CActivityToSequenceMapping::HashValueType;
	CStudioHdr::CActivityToSequenceMapping &mapping = *pstudiohdr->ActivityToSequence();
	if ( !pstudiohdr->SequencesAvailable() )
	{
		return ACTIVITY_NOT_AVAILABLE;
	}

	VerifySequenceIndex( pstudiohdr );

	if ( pstudiohdr->GetNumSeq() == 1 )
	{
		return ( ::GetSequenceActivity( pstudiohdr, 0, nullptr ) == activity ) ? 0 : ACTIVITY_NOT_AVAILABLE;
	}

	if (!mapping.ValidateAgainst(pstudiohdr))
	{
		contract_assert( false && "CStudioHdr %s has changed its vmodel pointer without reinitializing its activity mapping! Now performing emergency reinitialization." );
		ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } );
		mapping.Reinitialize(pstudiohdr);
	}

	// a null m_pSequenceTuples just means that this studio header has no activities.
	if (!mapping.m_pSequenceTuples)
		return ACTIVITY_NOT_AVAILABLE;

	// get the data for the given activity
	const HashValueType * __restrict actData = ks::find_ptr( mapping.m_ActToSeqHash, activity );
	if (!actData)
	{
		return ACTIVITY_NOT_AVAILABLE;
	}
	
	// go through each sequence and give actmod matches preference. LACK of an actmod is a detriment
	int top_score = -1;
	std::vector< int > topScoring; topScoring.reserve( actData->count );
	for ( int i = 0; i < actData->count; i++ )
	{
		SequenceTuple * __restrict sequenceInfo = mapping.m_pSequenceTuples + actData->startingIdx + i;
		int score = 0;

		int num_seq_modifiers = sequenceInfo->iNumActivityModifiers;
		for ( int k = 0; k < num_seq_modifiers; k++ )
		{
			bool bFoundMod = false;

			for ( int m = 0; m < iModifierCount; m++ )
			{
				if ( sequenceInfo->pActivityModifiers[ k ] == pActivityModifiers[ m ] )
				{
					bFoundMod = true;
				}
			}

			if ( bFoundMod )
			{
				score += 2;
			}
			else
			{
				score--;
			}

		}
		
		if ( score >= top_score )
		{
			if ( score > top_score )
			{
				topScoring.clear();
			}
		
			for ( int n=0; n<sequenceInfo->weight; n++ )
			{
				topScoring.push_back( sequenceInfo->seqnum );
			}
		
			top_score = score;
		}
	}

	// randomly pick between the highest scoring sequences ( NOTE: this method of selecting a sequence ignores activity weights )
	//if ( IsInPrediction() )
	//{
	//	return topScoring[ SharedRandomInt( "SelectWeightedSequence", 0, topScoring.Count() - 1 ) ];
	//}
	if ( !topScoring.empty() )
	{
		return topScoring[ RandomInt( 0, std::ssize( topScoring ) - 1 ) ];
	}
	else
	{
		return -1;
	}
}

int SelectHeaviestSequence( CStudioHdr *pstudiohdr, int activity )
{
	if ( !pstudiohdr )
		return 0;

	VerifySequenceIndex( pstudiohdr );

	int maxweight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	int weight = 0;
	for (int i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		int curActivity = GetSequenceActivity( pstudiohdr, i, &weight );
		if (curActivity == activity)
		{
			if ( iabs(weight) > maxweight )
			{
				maxweight = iabs(weight);
				seq = i;
			}
		}
	}

	return seq;
}

void GetEyePosition ( CStudioHdr *pstudiohdr, Vector &vecEyePosition )
{
	if ( !pstudiohdr )
	{
		Warning( "GetEyePosition() Can't get pstudiohdr ptr!\n" );
		return;
	}

	vecEyePosition = pstudiohdr->eyeposition();
}

//-----------------------------------------------------------------------------
// Purpose: Looks up an activity by name.
// Input  : label - Name of the activity to look up, ie "ACT_IDLE"
// Output : Activity index or ACT_INVALID if not found.
//-----------------------------------------------------------------------------
int LookupActivity( CStudioHdr *pstudiohdr, const char *label )
{
	CVProfScope _( "LookupActivity", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if ( !pstudiohdr )
	{
		return 0;
	}

	for ( int i = 0; i < pstudiohdr->GetNumSeq(); i++ )
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( i );
		if ( stricmp( seqdesc.pszActivityName(), label ) == 0 )
		{
			return seqdesc.activity;
		}
	}

	return ACT_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up a sequence by sequence name first, then by activity name.
// Input  : label - The sequence name or activity name to look up.
// Output : Returns the sequence index of the matching sequence, or ACT_INVALID.
//-----------------------------------------------------------------------------
int LookupSequence( CStudioHdr *pstudiohdr, const char *label )
{
	CVProfScope _( "LookupSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	if (! pstudiohdr)
		return 0;

	if (!pstudiohdr->SequencesAvailable())
		return 0;

	//
	// Look up by sequence name.
	//
	int iSequence = pstudiohdr->LookupSequence( label );
	if ( iSequence != -1 )
		return iSequence;

	//
	// Not found, look up by activity name.
	//
	int nActivity = LookupActivity( pstudiohdr, label );
	if (nActivity != ACT_INVALID )
	{
		return SelectWeightedSequence( pstudiohdr, nActivity );
	}

	return ACT_INVALID;
}

void GetSequenceLinearMotion( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec )
{
	if (! pstudiohdr)
	{
		Msg( "Bad pstudiohdr in GetSequenceLinearMotion()!\n" );
		return;
	}

	if (!pstudiohdr->SequencesAvailable())
		return;

	if( iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		// Don't spam on bogus model
		if ( pstudiohdr->GetNumSeq() > 0 )
		{
			static int msgCount = 0;
			while ( ++msgCount < 10 )
			{
				DevMsg( "Bad sequence (%i out of %i max) in GetSequenceLinearMotion() for model '%s'!\n", iSequence, pstudiohdr->GetNumSeq(), pstudiohdr->pszName() );
			}
		}
		pVec->Init();
		return;
	}

	QAngle vecAngles;
	Studio_SeqMovement( pstudiohdr, iSequence, 0, 1.0, poseParameter, (*pVec), vecAngles );
}

float GetSequenceLinearMotionAndDuration( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec )
{
	pVec->Init();
	if ( !pstudiohdr )
	{
		Msg( "Bad pstudiohdr in GetSequenceLinearMotion()!\n" );
		return 0.0f;
	}

	if ( !pstudiohdr->SequencesAvailable() )
		return 0.0f;

	if ( iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		// Don't spam on bogus model
		if ( pstudiohdr->GetNumSeq() > 0 )
		{
			static int msgCount = 0;
			while ( ++msgCount < 10 )
			{
				DevMsg( "Bad sequence (%i out of %i max) in GetSequenceLinearMotion() for model '%s'!\n", iSequence, pstudiohdr->GetNumSeq(), pstudiohdr->pszName() );
			}
		}
		return 0.0f;
	}

	return Studio_SeqMovementAndDuration( pstudiohdr, iSequence, 0, 1.0, poseParameter, (*pVec) );
}

const char *GetSequenceName( CStudioHdr *pstudiohdr, int iSequence )
{
	if( !pstudiohdr || iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		if ( pstudiohdr )
		{
			DevMsg( "Bad sequence in GetSequenceName() for model '%s'!\n", pstudiohdr->pszName() );
		}
		return "Unknown";
	}

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( iSequence );
	return seqdesc.pszLabel();
}

const char *GetSequenceActivityName( CStudioHdr *pstudiohdr, int iSequence )
{
	if( !pstudiohdr || iSequence < 0 || iSequence >= pstudiohdr->GetNumSeq() )
	{
		if ( pstudiohdr )
		{
			DevMsg( "Bad sequence in GetSequenceActivityName() for model '%s'!\n", pstudiohdr->pszName() );
		}
		return "Unknown";
	}

	mstudioseqdesc_t	&seqdesc = pstudiohdr->pSeqdesc( iSequence );
	return seqdesc.pszActivityName( );
}

int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence )
{
	if ( !pstudiohdr || 
		 !pstudiohdr->SequencesAvailable() ||
		sequence < 0 || 
		sequence >= pstudiohdr->GetNumSeq() )
	{
		return 0;
	}

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	return seqdesc.flags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			sequence - 
//			type - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool HasAnimationEventOfType( CStudioHdr *pstudiohdr, int sequence, int type )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return false;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if ( !&seqdesc )
		return false;

	mstudioevent_for_client_server_t *pevent = GetEventIndexForSequence( seqdesc );
	if ( !pevent )
		return false;

	if (seqdesc.numevents == 0 )
		return false;

	int index;
	for ( index = 0; index < (int)seqdesc.numevents; index++ )
	{
		if ( pevent[ index ].Event() == type )
		{
			return true;
		}
	}

	return false;
}

struct animtaglookup_t
{
	int nIndex;
	const char* szName;
};

#define REGISTER_ANIMTAG( _n ) { _n, #_n },
const animtaglookup_t g_AnimTagLookupTable[ANIMTAG_COUNT] = 
{
	REGISTER_ANIMTAG( ANIMTAG_UNINITIALIZED )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_N )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_NE )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_E )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_SE )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_S )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_SW )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_W )
	REGISTER_ANIMTAG( ANIMTAG_STARTCYCLE_NW )

	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_WALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_WALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_RUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_RUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK )

	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_IDLE )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK )
	REGISTER_ANIMTAG( ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK )
	
	REGISTER_ANIMTAG( ANIMTAG_FLASHBANG_PASSABLE )

	REGISTER_ANIMTAG( ANIMTAG_WEAPON_POSTLAYER )
};

int IndexFromAnimTagName( const char* szName )
{
	int i;
	for ( i=1; i<ANIMTAG_COUNT; i++ )
	{
		const animtaglookup_t *pAnimTag = &g_AnimTagLookupTable[i];
		if ( !V_strcmp( szName, pAnimTag->szName ) )
		{
			return pAnimTag->nIndex;
		}
	}
	return ANIMTAG_INVALID;
}

float GetFirstSequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flStart, float flEnd )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return flStart;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numanimtags == 0 )
		return flStart;

	mstudioanimtag_t *panimtag = nullptr;

	int index;
	for ( index = 0; index < (int)seqdesc.numanimtags; index++ )
	{
		panimtag = seqdesc.pAnimTag(index);

		if ( panimtag->tag == ANIMTAG_INVALID )
			continue;

		if ( panimtag->tag == ANIMTAG_UNINITIALIZED )
		{
			panimtag->tag = IndexFromAnimTagName( panimtag->pszTagName() );
		}

		if ( panimtag->tag == nDesiredTag && panimtag->cycle >= flStart && panimtag->cycle < flEnd )
		{
			return panimtag->cycle;
		}
	}

	return flStart;
}

float GetAnySequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flDefault )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() )
		return flDefault;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numanimtags == 0 )
		return flDefault;

	mstudioanimtag_t *panimtag = nullptr;

	int index;
	for ( index = 0; index < (int)seqdesc.numanimtags; index++ )
	{
		panimtag = seqdesc.pAnimTag(index);

		if ( panimtag->tag == ANIMTAG_INVALID )
			continue;

		if ( panimtag->tag == ANIMTAG_UNINITIALIZED )
		{
			panimtag->tag = IndexFromAnimTagName( panimtag->pszTagName() );
		}

		if ( panimtag->tag == nDesiredTag )
		{
			return panimtag->cycle;
		}
	}

	return flDefault;
}

int GetAnimationEvent( CStudioHdr *pstudiohdr, int sequence, animevent_t *pNPCEvent, float flStart, float flEnd, int index )
{
	if ( !pstudiohdr || sequence >= pstudiohdr->GetNumSeq() || !pNPCEvent )
		return 0;

	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );
	if (seqdesc.numevents == 0 || index >= (int)seqdesc.numevents )
		return 0;

	// Msg( "flStart %f flEnd %f (%d) %s\n", flStart, flEnd, seqdesc.numevents, seqdesc.label );
	mstudioevent_for_client_server_t *pevent = GetEventIndexForSequence( seqdesc );
	for (; index < (int)seqdesc.numevents; index++)
	{
		// Don't send client-side events to the server AI
		if ( pevent[index].type & AE_TYPE_NEWEVENTSYSTEM )
		{
			if ( !(pevent[index].type & AE_TYPE_SERVER) )
				 continue;
		}
		else if ( pevent[index].Event_OldSystem() >= EVENT_CLIENT ) //Adrian - Support the old event system
			continue;
	
		bool bOverlapEvent = false;

		if (pevent[index].cycle >= flStart && pevent[index].cycle < flEnd)
		{
			bOverlapEvent = true;
		}
		// FIXME: doesn't work with animations being played in reverse
		else if ((seqdesc.flags & STUDIO_LOOPING) && flEnd < flStart)
		{
			if (pevent[index].cycle >= flStart || pevent[index].cycle < flEnd)
			{
				bOverlapEvent = true;
			}
		}

		if (bOverlapEvent)
		{
			pNPCEvent->pSource = nullptr;
			pNPCEvent->cycle = pevent[index].cycle;
			pNPCEvent->eventtime = gpGlobals->curtime;
			pNPCEvent->type	= pevent[index].type;
			pNPCEvent->Event( pevent[index].Event() );
			pNPCEvent->options = pevent[index].pszOptions();
			return index + 1;
		}
	}
	return 0;
}

int FindTransitionSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, int iGoalSequence, int *piDir )
{
	if ( !pstudiohdr )
		return iGoalSequence;

	if ( !pstudiohdr->SequencesAvailable() )
		return iGoalSequence;

	if ( ( iCurrentSequence < 0 ) || ( iCurrentSequence >= pstudiohdr->GetNumSeq() ) )
		return iGoalSequence;

	if ( ( iGoalSequence < 0 ) || ( iGoalSequence >= pstudiohdr->GetNumSeq() ) )
	{
		// asking for a bogus sequence.  Punt.
		contract_assert( 0 );
		return iGoalSequence;
	}

	// bail if we're going to or from a node 0
	if (pstudiohdr->EntryNode( iCurrentSequence ) == 0 || pstudiohdr->EntryNode( iGoalSequence ) == 0)
	{
		*piDir = 1;
		return iGoalSequence;
	}

	int	iEndNode;

	// Msg( "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	// check to see if we should be going forward or backward through the graph
	if (*piDir > 0)
	{
		iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
	}
	else
	{
		iEndNode = pstudiohdr->EntryNode( iCurrentSequence );
	}

	// if both sequences are on the same node, just go there
	if (iEndNode == pstudiohdr->EntryNode( iGoalSequence ))
	{
		*piDir = 1;
		return iGoalSequence;
	}

	int iInternNode = pstudiohdr->GetTransition( iEndNode, pstudiohdr->EntryNode( iGoalSequence ) );

	// if there is no transitionial node, just go to the goal sequence
	if (iInternNode == 0)
		return iGoalSequence;

	int i;

	// look for someone going from the entry node to next node it should hit
	// this may be the goal sequences node or an intermediate node
	for (i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc(i );
		if (pstudiohdr->EntryNode( i ) == iEndNode && pstudiohdr->ExitNode( i ) == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (seqdesc.nodeflags)
		{
			if (pstudiohdr->ExitNode( i ) == iEndNode && pstudiohdr->EntryNode( i ) == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	// this means that two parts of the node graph are not connected.
	DevMsg( 2, "error in transition graph: %s to %s\n",  pstudiohdr->pszNodeName( iEndNode ), pstudiohdr->pszNodeName( pstudiohdr->EntryNode( iGoalSequence ) ));
	// Go ahead and jump to the goal sequence
	return iGoalSequence;
}

bool GotoSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, float flCurrentCycle, float flCurrentRate, int iGoalSequence, int &nNextSequence, float &flNextCycle, int &iNextDir )
{
	if ( !pstudiohdr )
		return false;

	if ( !pstudiohdr->SequencesAvailable() )
		return false;

	if ( ( iCurrentSequence < 0 ) || ( iCurrentSequence >= pstudiohdr->GetNumSeq() ) )
		return false;

	if ( ( iGoalSequence < 0 ) || ( iGoalSequence >= pstudiohdr->GetNumSeq() ) )
	{
		// asking for a bogus sequence.  Punt.
		contract_assert( 0 );
		return false;
	}

	// bail if we're going to or from a node 0
	if (pstudiohdr->EntryNode( iCurrentSequence ) == 0 || pstudiohdr->EntryNode( iGoalSequence ) == 0)
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int	iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
	// Msg( "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	// if we're in a transition sequence
	if (pstudiohdr->EntryNode( iCurrentSequence ) != pstudiohdr->ExitNode( iCurrentSequence ))
	{
		// are we done with it?
		if (flCurrentRate > 0.0 && flCurrentCycle >= 0.999)
		{
			iEndNode = pstudiohdr->ExitNode( iCurrentSequence );
		}
		else if (flCurrentRate < 0.0 && flCurrentCycle <= 0.001)
		{
			iEndNode = pstudiohdr->EntryNode( iCurrentSequence );
		}
		else
		{
			// nope, exit
			return false;
		}
	}

	// if both sequences are on the same node, just go there
	if (iEndNode == pstudiohdr->EntryNode( iGoalSequence ))
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int iInternNode = pstudiohdr->GetTransition( iEndNode, pstudiohdr->EntryNode( iGoalSequence ) );

	// if there is no transitionial node, just go to the goal sequence
	if (iInternNode == 0)
	{
		iNextDir = 1;
		flNextCycle = 0.0;
		nNextSequence = iGoalSequence;
		return true;
	}

	int i;

	// look for someone going from the entry node to next node it should hit
	// this may be the goal sequences node or an intermediate node
	for (i = 0; i < pstudiohdr->GetNumSeq(); i++)
	{
		mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc(i );
		if (pstudiohdr->EntryNode( i ) == iEndNode && pstudiohdr->ExitNode( i ) == iInternNode)
		{
			iNextDir = 1;
			flNextCycle = 0.0;
			nNextSequence = i;
			return true;
		}
		if (seqdesc.nodeflags)
		{
			if (pstudiohdr->ExitNode( i ) == iEndNode && pstudiohdr->EntryNode( i ) == iInternNode)
			{
				iNextDir = -1;
				flNextCycle = 0.999;	
				nNextSequence = i;
				return true;
			}
		}
	}

	// this means that two parts of the node graph are not connected.
	DevMsg( 2, "error in transition graph: %s to %s\n",  pstudiohdr->pszNodeName( iEndNode ), pstudiohdr->pszNodeName( pstudiohdr->EntryNode( iGoalSequence ) ));
	return false;
}

void SetBodygroupPreset( CStudioHdr *pstudiohdr, int& body, char const *szName )
{
	if (!pstudiohdr)
		return;

	for ( int i=0; i<pstudiohdr->GetNumBodyGroupPresets(); i++ )
	{
		const mstudiobodygrouppreset_t *pBodygroupPreset = pstudiohdr->GetBodyGroupPreset( i );
		if ( !V_strcmp( szName, pBodygroupPreset->pszName() ) )
		{

			for ( int j=0; j<pstudiohdr->numbodyparts(); j++ )
			{
				mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( j );

				int iMask = (pBodygroupPreset->iMask / pbodypart->base) % pbodypart->nummodels;
				if ( iMask == 1 )
				{
					int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;
					int iValCurrent = (pBodygroupPreset->iValue / pbodypart->base) % pbodypart->nummodels;

					body = (body - (iCurrent * pbodypart->base) + (iValCurrent * pbodypart->base));
				}

			}
			break;
		}
	}
}

void SetBodygroup( CStudioHdr *pstudiohdr, int& body, int iGroup, int iValue )
{
	if (! pstudiohdr)
		return;

	if (iGroup >= pstudiohdr->numbodyparts())
		return;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if (iValue >= pbodypart->nummodels)
		return;

	int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;

	body = (body - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}

int GetBodygroup( CStudioHdr *pstudiohdr, int body, int iGroup )
{
	if (! pstudiohdr)
		return 0;

	if (iGroup >= pstudiohdr->numbodyparts())
		return 0;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (body / pbodypart->base) % pbodypart->nummodels;

	return iCurrent;
}

const char *GetBodygroupName( CStudioHdr *pstudiohdr, int iGroup )
{
	if ( !pstudiohdr)
		return "";

	if (iGroup >= pstudiohdr->numbodyparts())
		return "";

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	return pbodypart->pszName();
}

const char *GetBodygroupPartName( CStudioHdr *pstudiohdr, int iGroup, int iPart )
{
	if ( !pstudiohdr)
		return "";

	if (iGroup >= pstudiohdr->numbodyparts())
		return "";

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	if ( iPart < 0 && iPart >= pbodypart->nummodels )
		return "";

	return pbodypart->pModel( iPart )->name;
}

int FindBodygroupByName( CStudioHdr *pstudiohdr, const char *name )
{
	if ( !pstudiohdr )
		return -1;

	int group;
	for ( group = 0; group < pstudiohdr->numbodyparts(); group++ )
	{
		mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( group );
		if ( !V_strcasecmp( name, pbodypart->pszName() ) )
		{
			return group;
		}
	}

	return -1;
}

int GetBodygroupCount( CStudioHdr *pstudiohdr, int iGroup )
{
	if ( !pstudiohdr )
		return 0;

	if (iGroup >= pstudiohdr->numbodyparts())
		return 0;

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );
	return pbodypart->nummodels;
}

int GetNumBodyGroups( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return 0;

	return pstudiohdr->numbodyparts();
}

int GetSequenceActivity( CStudioHdr *pstudiohdr, int sequence, int *pweight )
{
	if (!pstudiohdr || !pstudiohdr->SequencesAvailable() )
	{
		if (pweight)
			*pweight = 0;
		return 0;
	}

	contract_assert( sequence >= 0 && sequence < pstudiohdr->GetNumSeq() );
	mstudioseqdesc_t &seqdesc = pstudiohdr->pSeqdesc( sequence );

	if (!(seqdesc.flags & STUDIO_ACTIVITY))
	{
		SetActivityForSequence( pstudiohdr, sequence );
	}
	if (pweight)
		*pweight = seqdesc.actweight;
	return seqdesc.activity;
}

void GetAttachmentLocalSpace( CStudioHdr *pstudiohdr, int attachIndex, matrix3x4_t &pLocalToWorld )
{
	if ( attachIndex >= 0 )
	{
		const mstudioattachment_t &pAttachment = pstudiohdr->pAttachment(attachIndex);
		MatrixCopy( pAttachment.local, pLocalToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			*name - 
// Output : int
//-----------------------------------------------------------------------------
int FindHitboxSetByName( CStudioHdr *pstudiohdr, const char *name )
{
	if ( !pstudiohdr )
		return -1;

	for ( int i = 0; i < pstudiohdr->numhitboxsets(); i++ )
	{
		mstudiohitboxset_t *set = pstudiohdr->pHitboxSet( i );
		if ( !set )
			continue;

		if ( !stricmp( set->pszName(), name ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
//			setnumber - 
// Output : char const
//-----------------------------------------------------------------------------
const char *GetHitboxSetName( CStudioHdr *pstudiohdr, int setnumber )
{
	if ( !pstudiohdr )
		return "";

	mstudiohitboxset_t *set = pstudiohdr->pHitboxSet( setnumber );
	if ( !set )
		return "";

	return set->pszName();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pstudiohdr - 
// Output : int
//-----------------------------------------------------------------------------
int GetHitboxSetCount( CStudioHdr *pstudiohdr )
{
	if ( !pstudiohdr )
		return 0;

	return pstudiohdr->numhitboxsets();
}

#undef iabs
#undef REGISTER_ANIMTAG

class CAnimationLayer;
class CBaseAnimatingOverlay;

using namespace ::ks::reflect;

	ConVar sv_showanimstate( side_registry(), "sv_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (server) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( side_registry(), "sv_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output sv_showanimstate to Msg(). 2 to store in AnimStateServer.log. 3 for both." );
	ConVar showanimstate_activities( side_registry(), "sv_showanimstate_activities", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show activities in the (server) animation state display." );

// Below this many degrees, slow down turning rate linearly
static constexpr auto FADE_TURN_DEGREES = 15.0f;

// After this, need to start turning feet
static constexpr auto MAX_TORSO_ANGLE = 70.0f;

// Below this amount, don't play a turning animation/perform IK
static constexpr auto MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION = 15.0f;

static constexpr auto POSE_PARAM_DELTA_DAMPEN = 4.0f;

ConVar mp_feetyawrate( 
	side_registry(), "mp_feetyawrate", 
	"400",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"How many degrees per second that we can turn our feet or upper body." );

ConVar mp_facefronttime( 
	side_registry(), "mp_facefronttime", 
	"2", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"After this amount of time of standing in place but aiming to one side, go ahead and move feet to face upper body." );

ConVar mp_ik( side_registry(), "mp_ik", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use IK on in-place turns." );

// Pose parameters stored for debugging.
float g_flLastBodyPitch, g_flLastBodyYaw, m_flLastMoveYaw;

// ------------------------------------------------------------------------------------------------ //
// CBasePlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CBasePlayerAnimState::CBasePlayerAnimState()
{
	m_flEyeYaw = 0.0f;
	m_flEyePitch = 0.0f;
	m_bCurrentFeetYawInitialized = false;
	m_flCurrentTorsoYaw = 0.0f;
	m_flCurrentTorsoYaw = TURN_NONE;
	m_flMaxGroundSpeed = 0.0f;
	m_flStoredCycle = 0.0f;

	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_bForceAimYaw = false;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_angRender.Init();
	m_vLastMovePose.Init();
	m_iCurrent8WayIdleSequence = -1;
	m_iCurrent8WayCrouchIdleSequence = -1;

	m_pOuter = nullptr;
	m_eCurrentMainSequenceActivity = ACT_IDLE;
	m_flLastAnimationStateClearTime = 0.0f;
	
	m_bInFootPlantIdleTurn = false;
	m_flFootPlantIdleTurnCycle = 0.0f;
	m_bFootPlantIdleNeedToLiftFeet = false;

	m_flPoseParamTargetDampenedScaleIdeal = 0.0f;
}

CBasePlayerAnimState::~CBasePlayerAnimState()
{
}

void CBasePlayerAnimState::Init( CBaseAnimatingOverlay *pPlayer, const CModAnimConfig &config )
{
	m_pOuter = pPlayer;
	m_AnimConfig = config;
	ClearAnimationState();
}

void CBasePlayerAnimState::Release()
{
	delete this;
}

void CBasePlayerAnimState::ClearAnimationState()
{
	ClearAnimationLayers();
	m_bCurrentFeetYawInitialized = false;
	m_flLastAnimationStateClearTime = gpGlobals->curtime;
}

float CBasePlayerAnimState::TimeSinceLastAnimationStateClear() const
{
	return gpGlobals->curtime - m_flLastAnimationStateClearTime;
}

void CBasePlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	CVProfScope _( "CBasePlayerAnimState::Update", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Clear animation overlays because we're about to completely reconstruct them.
	ClearAnimationLayers();

	// Some mods don't want to update the player's animation state if they're dead and ragdolled.
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}
	
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();
	// Store these. All the calculations are based on them.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Compute sequences for all the layers.
	ComputeSequences( pStudioHdr );
	
	// Compute all the pose params.
	ComputePoseParam_BodyPitch( pStudioHdr );	// Look up/down.
	ComputePoseParam_BodyYaw();		// Torso rotation.
	ComputePoseParam_MoveYaw( pStudioHdr );		// What direction his legs are running in.

	ComputePlaybackRate();

	if ( sv_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimState( 20 );
	}
}

bool CBasePlayerAnimState::ShouldUpdateAnimState()
{
	// By default, don't update their animation state when they're dead because they're
	// either a ragdoll or they're not drawn.
	return GetOuter()->IsAlive();
}

bool CBasePlayerAnimState::ShouldChangeSequences( void ) const
{
	return true;
}

void CBasePlayerAnimState::SetOuterPoseParameter( int iParam, float flValue )
{
	// Make sure to set all the history values too, otherwise the server can overwrite them.
	GetOuter()->SetPoseParameter( iParam, flValue );
}

void CBasePlayerAnimState::ClearAnimationLayers()
{
	contract_assert( 0 ); // unused?

	CVProfScope _( "CBasePlayerAnimState::ClearAnimationLayers", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
		m_pOuter->GetAnimOverlay( i )->m_fFlags = 0;
	}
}

void CBasePlayerAnimState::RestartMainSequence()
{
	CBaseAnimatingOverlay *pPlayer = GetOuter();

	pPlayer->m_flAnimTime = gpGlobals->curtime;
	pPlayer->SetCycle( 0 );
}

void CBasePlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputeSequences", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	ComputeMainSequence();		// Lower body (walk/run/idle).
	UpdateInterpolators();		// The groundspeed interpolator uses the main sequence info.

	if ( m_AnimConfig.m_bUseAimSequences )
	{
		ComputeAimSequence();		// Upper body, based on weapon type.
	}
}

void CBasePlayerAnimState::ResetGroundSpeed( void )
{
	m_flMaxGroundSpeed = GetCurrentMaxGroundSpeed();
}

void CBasePlayerAnimState::ComputeMainSequence()
{
	CVProfScope _( "CBasePlayerAnimState::ComputeMainSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	CBaseAnimatingOverlay *pPlayer = GetOuter();

	// Have our class or the mod-specific class determine what the current activity is.
	Activity idealActivity = CalcMainActivity();

	Activity oldActivity = m_eCurrentMainSequenceActivity;
	
	// Store our current activity so the aim and fire layers know what to do.
	m_eCurrentMainSequenceActivity = idealActivity;

	// Export to our outer class..
	int animDesired = SelectWeightedSequence( TranslateActivity(idealActivity) );

	ResetCycleAcrossCustomActivityChange( oldActivity, idealActivity );

	if ( animDesired < 0 )
		 animDesired = 0;

	pPlayer->ResetSequence( animDesired );

}

void CBasePlayerAnimState::ResetCycleAcrossCustomActivityChange( Activity iCurrent, Activity iNew )
{
	if ( !GetOuter() )
		return;

	if ( iCurrent == ACT_CROUCHIDLE && iNew != ACT_CROUCHIDLE )
	{
		GetOuter()->SetCycle(0);
	}
}

bool CBasePlayerAnimState::ShouldResetMainSequence( int iCurrentSequence, int iNewSequence )
{
	if ( !GetOuter() )
		return false;

	return GetOuter()->GetSequenceActivity( iCurrentSequence ) == GetOuter()->GetSequenceActivity( iNewSequence );
}

bool CBasePlayerAnimState::ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity )
{
	// If we went from idle to walk, reset the interpolation history.
	return ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE || oldActivity == ACT_TURN || oldActivity == ACT_STEP_FORE ) && 
			 (idealActivity == ACT_WALK || idealActivity == ACT_RUN_CROUCH || idealActivity == ACT_WALK_CROUCH || idealActivity == ACT_RUN ) );
}

void CBasePlayerAnimState::UpdateAimSequenceLayers(
	float flCycle,
	int iFirstLayer,
	bool bForceIdle,
	CSequenceTransitioner *pTransitioner,
	float flWeightScale
	)
{
	float flAimSequenceWeight = 1;
	int iAimSequence = CalcAimLayerSequence( &flCycle, &flAimSequenceWeight, bForceIdle );
	if ( iAimSequence == -1 )
		iAimSequence = 0;

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );

	pLayer->SetSequence( iAimSequence );
	pLayer->SetCycle( flCycle );
	pLayer->SetWeight( clamp( flWeightScale, 0.0f, 1.0f ) );

	pLayer->SetOrder( iFirstLayer );	// should already be set

	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE;

}

void CBasePlayerAnimState::OptimizeLayerWeights( int iFirstLayer, int nLayers )
{
	int i;

	// Find the total weight of the blended layers, not including the idle layer (iFirstLayer)
	float totalWeight = 0.0f;
	for ( i=1; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
		{
			totalWeight += pLayer->GetWeight();
		}
	}

	// Set the idle layer's weight to be 1 minus the sum of other layer weights
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );
	if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
	{
		float flWeight = 1.0f - totalWeight;
		flWeight = std::max( flWeight, 0.0f );
		pLayer->SetWeight( flWeight );
	}

	// This part is just an optimization. Since we have the walk/run animations weighted on top of 
	// the idle animations, all this does is disable the idle animations if the walk/runs are at
	// full weighting, which is whenever a guy is at full speed.
	//
	// So it saves us blending a couple animation layers whenever a guy is walking or running full speed.
	int iLastOne = -1;
	for ( i=0; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.99 )
			iLastOne = i;
	}

	if ( iLastOne != -1 )
	{
		for ( int i=iLastOne-1; i >= 0; i-- )
		{
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
			pLayer->m_nOrder.Set( CBaseAnimatingOverlay::MAX_OVERLAYS );
			pLayer->m_fFlags = 0;
		}
	}
}

bool CBasePlayerAnimState::ShouldBlendAimSequenceToIdle()
{
	Activity act = GetCurrentMainSequenceActivity();

	return (act == ACT_RUN || act == ACT_LEAP || act == ACT_WALK || act == ACT_JUMP || act == ACT_RUNTOIDLE || act == ACT_RUN_CROUCH || act == ACT_WALK_CROUCH );
}

void CBasePlayerAnimState::ComputeAimSequence()
{
	CVProfScope _( "CBasePlayerAnimState::ComputeAimSequence", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Synchronize the lower and upper body cycles.
	float flCycle = m_pOuter->GetCycle();

	//[msmith]
	bool bIsMoving = false;
	float flPlaybackRate = 0.0f;

	if ( ShouldBlendAimSequenceToIdle() )
	{
		// See how much the player should weigh in the motion vs. the idle layers.
		flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );
	}

	// Set the idle layers to full (blend between different idle layers only and do NOT consider movement layers).
	UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER, true, &m_HighAimSequenceTransitioner, 1.0f );

	if ( bIsMoving )
	{
		// Blend the run / walk layers together.
		UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER+1, false, &m_LowAimSequenceTransitioner, flPlaybackRate );
	}

	OptimizeLayerWeights( AIMSEQUENCE_LAYER, NUM_AIMSEQUENCE_LAYERS );
}

int CBasePlayerAnimState::CalcSequenceIndex( const char *pBaseName, ... )
{
	char szFullName[512];
	std::va_list marker;
	__builtin_va_start( marker, pBaseName );
	V_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	__builtin_va_end( marker );
	int iSequence = GetOuter()->LookupSequence( szFullName );
	
	// Show warnings if we can't find anything here.
	if ( iSequence == -1 )
	{
		static std::set< std::string, ks::iless > warned;
		if ( warned.emplace( szFullName ).second )
		{
			Warning( "CalcSequenceIndex: can't find '%s'.\n", szFullName );
		}

		iSequence = 0;
	}

	return iSequence;
}

void CBasePlayerAnimState::UpdateInterpolators()
{
	CVProfScope _( "CBasePlayerAnimState::UpdateInterpolators", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// First, figure out their current max speed based on their current activity.
	float flCurMaxSpeed = GetCurrentMaxGroundSpeed();
	m_flMaxGroundSpeed = flCurMaxSpeed;
}

float CBasePlayerAnimState::GetInterpolatedGroundSpeed()
{
	return m_flMaxGroundSpeed;
}

float CBasePlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();
	bool isMoving = ( speed > MOVING_MINIMUM_SPEED );

	*bIsMoving = false;
	float flReturnValue = 1;

	if ( isMoving && CanThePlayerMove() )
	{
		float flGroundSpeed = GetInterpolatedGroundSpeed();
		if ( flGroundSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else
		{
			flReturnValue = speed / flGroundSpeed;
			flReturnValue = clamp( flReturnValue, 0.01, 10 );	// don't go nuts here.
		}
		*bIsMoving = true;
	}
	
	return flReturnValue;
}

bool CBasePlayerAnimState::CanThePlayerMove()
{
	return true;
}

void CBasePlayerAnimState::ComputePlaybackRate()
{
	CVProfScope _( "CBasePlayerAnimState::ComputePlaybackRate", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
	{
		// When using a 9-way blend, playback rate is always 1 and we just scale the pose params
		// to speed up or slow down the animation.
		bool bIsMoving;
		float flRate = CalcMovementPlaybackRate( &bIsMoving );
		if ( bIsMoving )
			GetOuter()->SetPlaybackRate( flRate );
		else
			GetOuter()->SetPlaybackRate( 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseAnimatingOverlay *CBasePlayerAnimState::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::EstimateYaw()
{
	Vector est_velocity;
	GetOuterAbsVelocity( est_velocity );

	float flLength = est_velocity.Length2D();
	if ( flLength > MOVING_MINIMUM_SPEED )
	{
		m_flGaitYaw = std::atan2( est_velocity[1], est_velocity[0] );
		m_flGaitYaw = RAD2DEG( m_flGaitYaw );
		m_flGaitYaw = AngleNormalize( m_flGaitYaw );
	}
}

static constexpr auto MOVEMENT_MINIMUM_ANIMATED_SPEED = 10.0f;
static constexpr auto MOVEMENT_MAXIMUM_PLAYBACK_RATE = 1.3f;

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_MoveYaw", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	//Matt: Goldsrc style animations need to not rotate the model
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_GOLDSRC )
	{
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );
	}

	// If using goldsrc-style animations where he's moving in the direction that his feet are facing,
	// we don't use move yaw.
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	float ang = m_flEyeYaw;
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		//[msmith]: Since bounding box code uses the entities local angle, we need the local angle to match
		//			the render angle on both the server AND the client.  The server for hit tests, the client
		//			for proper visibility culling.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );

		int iMoveX = GetOuter()->LookupPoseParameter( pStudioHdr, "move_x" );
		int iMoveY = GetOuter()->LookupPoseParameter( pStudioHdr, "move_y" );
		if ( iMoveX < 0 || iMoveY < 0 )
			return;

		bool bIsMoving;
		float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );

		// Setup the 9-way blend parameters based on our speed and direction.
		Vector2D vCurMovePose( 0, 0 );

		m_flPoseParamTargetDampenedScaleIdeal = Approach( flPlaybackRate, m_flPoseParamTargetDampenedScaleIdeal, gpGlobals->frametime * POSE_PARAM_DELTA_DAMPEN );

		if ( bIsMoving )
		{
			vCurMovePose.x = std::cos( DEG2RAD( flYaw ) );
			vCurMovePose.y = -std::sin( DEG2RAD( flYaw ) );
			// movement pose parameters on the diagonals are encoded at 1 instead of 0.707 (cos 45)
			// scale to the outside of a box instead of the outside of a circle.
			float scale = fabs( vCurMovePose.x );
			float scale2 = fabs( vCurMovePose.y );
			if ( scale2 > scale ) scale = scale2;
			if ( scale > 0.01f )
			{
				scale = 1.0f / scale;
				vCurMovePose.x *= scale;
				vCurMovePose.y *= scale;
			}

			vCurMovePose.x *= m_flPoseParamTargetDampenedScaleIdeal;
			vCurMovePose.y *= m_flPoseParamTargetDampenedScaleIdeal;

		}

		GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
		GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

		m_vLastMovePose = vCurMovePose;
	}
	else
	{
		int iMoveYaw = GetOuter()->LookupPoseParameter( pStudioHdr, "move_yaw" );
		if ( iMoveYaw >= 0 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveYaw, flYaw );
			m_flLastMoveYaw = flYaw;

			// Now blend in his idle animation.
			// This makes the 8-way blend act like a 9-way blend by blending to 
			// an idle sequence as he slows down.
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_BodyPitch", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Get pitch from v_angle
	float flPitch = m_flEyePitch;
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "body_pitch" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
	g_flLastBodyPitch = flPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayerAnimState::ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize( anglediff );
	
	float anglediffabs = fabs( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( anglediffabs > maxgap )
	{
		// gap is too big, jump
		//maxmove = (anglediffabs - maxgap);
		float flTooFar = std::min( anglediffabs - maxgap, maxmove * 5 );
		if ( anglediff > 0 )
		{
			current += flTooFar;
		}
		else
		{
			current -= flTooFar;
		}
		current = AngleNormalize( current );
		anglediff = goal - current;
		anglediff = AngleNormalize( anglediff );
		anglediffabs = fabs( anglediff );
	}

	if ( anglediffabs < maxmove )
	{
		// we are close enought, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CBasePlayerAnimState::ComputePoseParam_BodyYaw()
{
	CVProfScope _( "CBasePlayerAnimState::ComputePoseParam_BodyYaw", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	// Find out which way he's running (m_flEyeYaw is the way he's looking).
	Vector vel;
	GetOuterAbsVelocity( vel );
	bool bIsMoving = vel.Length2D() > MOVING_MINIMUM_SPEED;

	// If we just initialized this guy (maybe he just came into the PVS), then immediately
	// set his feet in the right direction, otherwise they'll spin around from 0 to the 
	// right direction every time someone switches spectator targets.
	if ( !m_bCurrentFeetYawInitialized )
	{
		m_bCurrentFeetYawInitialized = true;
		m_flGoalFeetYaw = m_flCurrentFeetYaw = m_flEyeYaw;
		m_flLastTurnTime = 0.0f;
		m_bInFootPlantIdleTurn = false;
	}
	else if ( bIsMoving || m_bForceAimYaw )
	{
		// player is moving, feet yaw = aiming yaw
		if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY || m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
		{
			// His feet point in the direction his eyes are, but they can run in any direction.
			m_flGoalFeetYaw = m_flEyeYaw;
		}
		else
		{
			m_flGoalFeetYaw = RAD2DEG( std::atan2( vel.y, vel.x ) );

			// If he's running backwards, flip his feet backwards.
			Vector vEyeYaw( std::cos( DEG2RAD( m_flEyeYaw ) ), std::sin( DEG2RAD( m_flEyeYaw ) ), 0 );
			Vector vFeetYaw( cos( DEG2RAD( m_flGoalFeetYaw ) ), sin( DEG2RAD( m_flGoalFeetYaw ) ), 0 );
			if ( vEyeYaw.Dot( vFeetYaw ) < -0.01 )
			{
				m_flGoalFeetYaw += 180;
			}
		}
		m_bInFootPlantIdleTurn = false;
	}
	else if ( (gpGlobals->curtime - m_flLastTurnTime) > mp_facefronttime.GetFloat() && m_flGoalFeetYaw != m_flEyeYaw )
	{
		// player didn't move & turn for quite some time
		if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
		{
			m_bInFootPlantIdleTurn = true;
			if ( m_flFootPlantIdleTurnCycle >= 1 )
				m_flFootPlantIdleTurnCycle = 0;
		}

		float flDiff = AngleNormalize(m_flGoalFeetYaw - m_flEyeYaw);
		m_bFootPlantIdleNeedToLiftFeet = (fabs(flDiff) > m_AnimConfig.m_flIdleFootPlantFootLiftDelta);

		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else
	{
		// If he's rotated his view further than the model can turn, make him face forward.
		float flDiff = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

		if ( fabs(flDiff) > m_AnimConfig.m_flMaxBodyYawDegrees )
		{
			if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
			{
				m_bInFootPlantIdleTurn = true;
				if ( m_flFootPlantIdleTurnCycle >= 1 )
					m_flFootPlantIdleTurnCycle = 0;
			}

			m_bFootPlantIdleNeedToLiftFeet = true;

			if ( flDiff  > 0 )
				m_flGoalFeetYaw -= m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
			else
				m_flGoalFeetYaw += m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
		}

		// If current yaw is significantly different from goal, abort idle foot ik to avoid intersecting the legs
		if ( m_bInFootPlantIdleTurn )
		{
			float flDiffYaw = AngleNormalize(m_flCurrentFeetYaw - m_flGoalFeetYaw);
			if ( fabs(flDiffYaw) > m_AnimConfig.m_flIdleFootPlantMaxYaw )
			{
				m_bInFootPlantIdleTurn = false;
				m_flFootPlantIdleTurnCycle = 0;
			}
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );

	if ( m_flCurrentFeetYaw != m_flGoalFeetYaw )
	{
		if ( m_bForceAimYaw )
		{
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		}
		else
		{
			ConvergeAngles( m_flGoalFeetYaw, GetFeetYawRate(), m_AnimConfig.m_flMaxBodyYawDegrees, gpGlobals->frametime, m_flCurrentFeetYaw );
		}

		m_flLastTurnTime = gpGlobals->curtime;
	}

	// Turn off a force aim yaw - either we have already updated or we don't need to.
	m_bForceAimYaw = false;

	float flCurrentTorsoYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );

	// Rotate entire body into position
	m_angRender[YAW] = m_flCurrentFeetYaw;
	m_angRender[PITCH] = m_angRender[ROLL] = 0;
		
	SetOuterBodyYaw( flCurrentTorsoYaw );
	g_flLastBodyYaw = flCurrentTorsoYaw;
}

float CBasePlayerAnimState::SetOuterBodyYaw( float flValue )
{
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( body_yaw < 0 )
	{
		return 0;
	}

	SetOuterPoseParameter( body_yaw, flValue );
	return flValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	contract_assert( 0 );
	return activity;
}

const QAngle& CBasePlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

void CBasePlayerAnimState::SetForceAimYaw( bool bForce )
{
	m_bForceAimYaw = bForce;
}

void CBasePlayerAnimState::GetOuterAbsVelocity( Vector& vel ) const
{
	vel = GetOuter()->GetAbsVelocity();
}

float CBasePlayerAnimState::GetOuterXYSpeed() const
{
	Vector vel;
	GetOuterAbsVelocity( vel );
	return vel.Length2D();
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStateLog( const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	std::va_list marker;
	__builtin_va_start( marker, pMsg );
	V_vsnprintf( str, sizeof( str ), pMsg, marker );
	__builtin_va_end( marker );

	// Log it?	
	if ( showanimstate_log.GetInt() == 1 || showanimstate_log.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( showanimstate_log.GetInt() > 1 )
	{
		const char *fname = "AnimStateServer.log";
		static FileHandle_t hFile = filesystem->Open( fname, "wt" );
		filesystem->FPrintf( hFile, "%s", str );
		filesystem->Flush( hFile );
	}
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStatePrintf( int iLine, const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	std::va_list marker;
	__builtin_va_start( marker, pMsg );
	V_vsnprintf( str, sizeof( str ), pMsg, marker );
	__builtin_va_end( marker );

	// Show it with Con_NPrintf.
	engine->Con_NPrintf( iLine, "%s", str );

	// Log it.
	AnimStateLog( "%s\n", str );
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimState( int iStartLine )
{
	Vector vOuterVel;
	GetOuterAbsVelocity( vOuterVel );

	int iLine = iStartLine;
	AnimStatePrintf( iLine++, "main: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
		GetSequenceName( m_pOuter->GetModelPtr(), m_pOuter->GetSequence() ), 
		m_pOuter->GetSequence(),
		m_pOuter->GetCycle(), 
		m_pOuter->GetSequenceCycleRate(m_pOuter->GetModelPtr(), m_pOuter->GetSequence()),
		m_pOuter->GetPlaybackRate()
		);

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );

		AnimStatePrintf( iLine++, "idle: %s, weight: %.2f\n",
			GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ), 
			(float)pLayer->GetWeight() );
	}

	for ( int i=0; i < m_pOuter->GetNumAnimOverlays()-1; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( AIMSEQUENCE_LAYER + i );
		AnimStatePrintf( iLine++, "%s(%d), flags (%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- " : ( pLayer->GetSequence() == 0 ? "-- " : (showanimstate_activities.GetBool()) ? GetSequenceActivityName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetSequence(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_fFlags,// Doesn't exist on client
			!pLayer->IsActive() ? 0 : (float)pLayer->GetWeight(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetCycle(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nOrder,
			i
			);
	}

	AnimStatePrintf( iLine++, "vel: %.2f, time: %.2f, max: %.2f, animspeed: %.2f", 
		vOuterVel.Length2D(), gpGlobals->curtime, GetInterpolatedGroundSpeed(), m_pOuter->GetSequenceGroundSpeed(m_pOuter->GetSequence()) );
	
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, move_yaw: %.2f, gait_yaw: %.2f, body_pitch: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, m_flLastMoveYaw, m_flGaitYaw, g_flLastBodyPitch );
	}
	else
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, body_pitch: %.2f, move_x: %.2f, move_y: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, g_flLastBodyPitch, m_vLastMovePose.x, m_vLastMovePose.y );
	}

	// Draw a red triangle on the ground for the eye yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetOuter()->GetAbsOrigin() + Vector( 0, 0, 3 );
	QAngle angles( 0, 0, 0 );
	angles[YAW] = m_flEyeYaw;
	Vector vForward, vRight, vUp;
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, gpGlobals->frametime );

	// Draw a blue triangle on the ground for the body yaw.
	angles[YAW] = m_angRender[YAW];
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 0, 255, 255, false, gpGlobals->frametime );

	//left limit
	angles[YAW] = m_flEyeYaw - 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);

	//right limit
	angles[YAW] = m_flEyeYaw + 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);

}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimStateFull( int iStartLine )
{
	AnimStateLog( "----------------- frame %d -----------------\n", gpGlobals->framecount );

	DebugShowAnimState( iStartLine );

	AnimStateLog( "--------------------------------------------\n\n" );
}

// -----------------------------------------------------------------------------
int CBasePlayerAnimState::SelectWeightedSequence( Activity activity ) 
{
	return GetOuter()->SelectWeightedSequence( activity ); 
}

float CBasePlayerAnimState::GetFeetYawRate( void )
{
	return mp_feetyawrate.GetFloat();
}

Activity CBasePlayerAnimState::GetCurrentMainSequenceActivity() const
{
	return m_eCurrentMainSequenceActivity;
}
