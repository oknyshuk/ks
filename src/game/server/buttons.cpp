// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation
// Copyright (c) 1996-2009 Valve Corporation

module;
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "shareddefs.h"
#include "util.h"
#include "declare_class.h"
#include "entitydefs.h"
#include "isaverestore.h"
#include "env_debughistory.h"
#include "igameevents.h"
#include "shake.h"
#include "tier2/resourceprecacher.h"
#include "videocfg/videocfg.h"
#include "worldsize.h"
#include "ai_criteria.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.strtools;
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
import ks.core.platform;
import ks.core.vprof;
import ks.tier1.convar;
import ks.tier1.fixed_pool;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.exprevaluator;
import ks.tier1.iconvar;
import :cstrike15.cs_player;
import :cstrike15.cs_team;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import :cstrike15.cs_playeranimstate;
import :cstrike15.hostage.cs_simple_hostage;
import :econ.econ_item_schema_minimal;

class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements buttons.

void PlayLockSounds( CBaseEntity *pEdict, locksound_t *pls, int flocked, int fbutton );
string_t MakeButtonSound( int sound );				// get string of button sound number

static constexpr auto SF_BUTTON_DONTMOVE = 1;
static constexpr auto SF_ROTBUTTON_NOTSOLID = 1;
static constexpr auto SF_BUTTON_TOGGLE = 32; // button stays pushed until reactivated
static constexpr auto SF_BUTTON_TOUCH_ACTIVATES = 256; // Button fires when touched.
static constexpr auto SF_BUTTON_DAMAGE_ACTIVATES = 512; // Button fires when damaged.
static constexpr auto SF_BUTTON_USE_ACTIVATES = 1024; // Button fires when used.
static constexpr auto SF_BUTTON_LOCKED = 2048; // Whether the button is initially locked.
static constexpr auto SF_BUTTON_SPARK_IF_OFF = 4096; // button sparks in OFF state
static constexpr auto SF_BUTTON_JIGGLE_ON_USE_LOCKED = 8192; // whether to jiggle if someone uses us when we're locked
static constexpr auto SF_BUTTON_NOTSOLID = 16384;

IMPLEMENT_REFLECT_DATAMAP( CBaseButton )

LINK_ENTITY_TO_CLASS( func_button, CBaseButton );

IMPLEMENT_REFLECT_SERVERCLASS( CBaseButton, DT_BaseButton )

void CBaseButton::Precache_Impl( void )
{
	// get door button sounds, for doors which require buttons to open
	if (m_bLockedSound)
	{
		m_ls.sLockedSound = MakeButtonSound( (int)m_bLockedSound );
		PrecacheScriptSound(m_ls.sLockedSound.ToCStr());
	}

	if (m_bUnlockedSound)
	{
		m_ls.sUnlockedSound = MakeButtonSound( (int)m_bUnlockedSound );
		PrecacheScriptSound(m_ls.sUnlockedSound.ToCStr());
	}

	// get sentence group names, for doors which are directly 'touched' to open

	switch (m_bLockedSentence)
	{
		case 1: m_ls.sLockedSentence = MAKE_STRING("NA"); break; // access denied
		case 2: m_ls.sLockedSentence = MAKE_STRING("ND"); break; // security lockout
		case 3: m_ls.sLockedSentence = MAKE_STRING("NF"); break; // blast door
		case 4: m_ls.sLockedSentence = MAKE_STRING("NFIRE"); break; // fire door
		case 5: m_ls.sLockedSentence = MAKE_STRING("NCHEM"); break; // chemical door
		case 6: m_ls.sLockedSentence = MAKE_STRING("NRAD"); break; // radiation door
		case 7: m_ls.sLockedSentence = MAKE_STRING("NCON"); break; // gen containment
		case 8: m_ls.sLockedSentence = MAKE_STRING("NH"); break; // maintenance door
		case 9: m_ls.sLockedSentence = MAKE_STRING("NG"); break; // broken door
		
		default: m_ls.sLockedSentence = NULL_STRING; break;
	}

	switch (m_bUnlockedSentence)
	{
		case 1: m_ls.sUnlockedSentence = MAKE_STRING("EA"); break; // access granted
		case 2: m_ls.sUnlockedSentence = MAKE_STRING("ED"); break; // security door
		case 3: m_ls.sUnlockedSentence = MAKE_STRING("EF"); break; // blast door
		case 4: m_ls.sUnlockedSentence = MAKE_STRING("EFIRE"); break; // fire door
		case 5: m_ls.sUnlockedSentence = MAKE_STRING("ECHEM"); break; // chemical door
		case 6: m_ls.sUnlockedSentence = MAKE_STRING("ERAD"); break; // radiation door
		case 7: m_ls.sUnlockedSentence = MAKE_STRING("ECON"); break; // gen containment
		case 8: m_ls.sUnlockedSentence = MAKE_STRING("EH"); break; // maintenance door
	
		default: m_ls.sUnlockedSentence = NULL_STRING; break;
	}

	if ( m_sNoise != NULL_STRING )
	{
		PrecacheScriptSound( STRING( m_sNoise ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cache user-entity-field values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : Returns true if handled, false if not.
//-----------------------------------------------------------------------------
bool CBaseButton::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "locked_sound"))
	{
		m_bLockedSound = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "locked_sentence"))
	{
		m_bLockedSentence = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "unlocked_sound"))
	{
		m_bUnlockedSound = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "unlocked_sentence"))
	{
		m_bUnlockedSentence = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "min_use_angle"))
	{
		SetUseLookAtAngle( std::atof(szValue) );
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Locks the button. If locked, the button will play the locked sound
//			when the player tries to use it.
//-----------------------------------------------------------------------------
void CBaseButton::Lock()
{
	m_bLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: Unlocks the button, making it able to be pressed again.
//-----------------------------------------------------------------------------
void CBaseButton::Unlock()
{
	m_bLocked = false;
}

//-----------------------------------------------------------------------------
// Purpose: Locks the button. If locked, the button will play the locked sound
//			when the player tries to use it.
//-----------------------------------------------------------------------------
void CBaseButton::InputLock( inputdata_t &inputdata )
{
	Lock();
}

//-----------------------------------------------------------------------------
// Purpose: Unlocks the button, making it able to be pressed again.
//-----------------------------------------------------------------------------
void CBaseButton::InputUnlock( inputdata_t &inputdata )
{
	Unlock();
}

//-----------------------------------------------------------------------------
// Presses or unpresses the button.
//-----------------------------------------------------------------------------
void CBaseButton::Press( CBaseEntity *pActivator, BUTTON_CODE eCode )
{
	if ( ( eCode == BUTTON_PRESS ) && ( m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN ) )
	{
		return;
	}

	if ( ( eCode == BUTTON_ACTIVATE ) && ( m_toggle_state == TS_GOING_UP || m_toggle_state == TS_AT_TOP ) )
	{
		return;
	}

	if ( ( eCode == BUTTON_RETURN ) && ( m_toggle_state == TS_GOING_DOWN || m_toggle_state == TS_AT_BOTTOM ) )
	{
		return;
	}

	// FIXME: consolidate all the button press code into one place!
	if (m_bLocked)
	{
		// play button locked sound
		PlayLockSounds(this, &m_ls, true, true);
		return;
	}

	// Temporarily disable the touch function, until movement is finished.
	SetTouch( nullptr );

	if ( ( eCode == BUTTON_PRESS ) && ( m_toggle_state == TS_AT_TOP ) ||
		 ( ( eCode == BUTTON_RETURN ) && ( m_toggle_state == TS_AT_TOP || m_toggle_state == TS_GOING_UP ) ) )
	{
		if ( m_sNoise != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_VOICE;
			ep.m_pSoundName = (char*)STRING(m_sNoise);
			ep.m_flVolume = 1;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );
		}

		m_OnPressed.FireOutput(pActivator, this);
		ButtonReturn();
	}
	else if ( ( eCode == BUTTON_PRESS ) ||
			  ( ( eCode == BUTTON_ACTIVATE ) && ( m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN ) ) )
	{
		m_OnPressed.FireOutput(pActivator, this);
		ButtonActivate();
	}
}

//-----------------------------------------------------------------------------
// Presses the button.
//-----------------------------------------------------------------------------
void CBaseButton::InputPress( inputdata_t &inputdata )
{
	Press( inputdata.pActivator, BUTTON_PRESS );
}

//-----------------------------------------------------------------------------
// Presses the button, sending it to the top/pressed position.
//-----------------------------------------------------------------------------
void CBaseButton::InputPressIn( inputdata_t &inputdata )
{
	Press( inputdata.pActivator, BUTTON_ACTIVATE );
}

//-----------------------------------------------------------------------------
// Unpresses the button, sending it to the unpressed/bottom position.
//-----------------------------------------------------------------------------
void CBaseButton::InputPressOut( inputdata_t &inputdata )
{
	Press( inputdata.pActivator, BUTTON_RETURN );
}

//-----------------------------------------------------------------------------
// Purpose: We have been damaged. Possibly activate, depending on our flags.
// Input  : pInflictor - 
//			pAttacker - 
//			flDamage - 
//			bitsDamageType - 
// Output : 
//-----------------------------------------------------------------------------
int CBaseButton::OnTakeDamage_Impl( const CTakeDamageInfo &info )
{
	m_OnDamaged.FireOutput(m_hActivator, this);

	// dvsents2: remove obselete health keyvalue from func_button
	if (!HasSpawnFlags(SF_BUTTON_DAMAGE_ACTIVATES) && (m_iHealth == 0))
	{
		return(0);
	}

	BUTTON_CODE code = ButtonResponseToTouch();

	if ( code == BUTTON_NOTHING )
		return 0;

	m_hActivator = info.GetAttacker();

	// dvsents2: why would activator be NULL here?
	if ( m_hActivator == nullptr )
		return 0;

	if (m_bLocked)
	{
		return(0);
	}

	// Temporarily disable the touch function, until movement is finished.
	SetTouch( nullptr );

	if ( code == BUTTON_RETURN )
	{
		if ( m_sNoise != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_VOICE;
			ep.m_pSoundName = (char*)STRING(m_sNoise);
			ep.m_flVolume = 1;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );
		}

		m_OnPressed.FireOutput(m_hActivator, this);
		ButtonReturn();
	}
	else
	{
		// code == BUTTON_ACTIVATE
		m_OnPressed.FireOutput(m_hActivator, this);
		ButtonActivate( );
	}

	return 0;
}

void CBaseButton::Spawn_Impl( )
{ 
	//----------------------------------------------------
	//determine sounds for buttons
	//a sound of 0 should not make a sound
	//----------------------------------------------------
	if ( m_sounds )
	{
		m_sNoise = MakeButtonSound( m_sounds );
		PrecacheScriptSound(m_sNoise.ToCStr());
	}
	else
	{
		m_sNoise = NULL_STRING;
	}

	Precache();

	if ( HasSpawnFlags( SF_BUTTON_SPARK_IF_OFF ) )// this button should spark in OFF state
	{
		SetThink ( &CBaseButton::ButtonSpark );
		SetNextThink( gpGlobals->curtime + 0.5f );// no hurry, make sure everything else spawns
	}

	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle( m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z );
	AngleVectors( angMoveDir, &m_vecMoveDir );

	SetMoveType( MOVETYPE_PUSH );

	if ( HasSpawnFlags( SF_BUTTON_NOTSOLID ) )
	{
		SetSolid( SOLID_NONE );
		AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
		AddSolidFlags( FSOLID_NOT_SOLID );
	}
	else
	{
		SetSolid( SOLID_BSP );
	}

	SetModel( STRING( GetModelName() ) );
	
	if (m_flSpeed == 0)
	{
		m_flSpeed = 40;
	}

	m_takedamage = DAMAGE_YES;

	if (m_flWait == 0)
	{
		m_flWait = 1;
	}

	if (m_flLip == 0)
	{
		m_flLip = 4;
	}

	m_toggle_state = TS_AT_BOTTOM;
	m_vecPosition1 = GetLocalOrigin();

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecButtonOBB = CollisionProp()->OBBSize();
	vecButtonOBB -= Vector( 2, 2, 2 );
	m_vecPosition2	= m_vecPosition1 + (m_vecMoveDir * (DotProductAbs( m_vecMoveDir, vecButtonOBB ) - m_flLip));

	// Is this a non-moving button?
	if ( ((m_vecPosition2 - m_vecPosition1).Length() < 1) || HasSpawnFlags(SF_BUTTON_DONTMOVE) )
	{
		m_vecPosition2 = m_vecPosition1;
	}

	m_fStayPushed = (m_flWait == -1 ? true : false);
	m_fRotating = false;

	if (HasSpawnFlags(SF_BUTTON_LOCKED))
	{
		m_bLocked = true;
	}

	//
	// If using activates the button, set its use function.
	//
	if (HasSpawnFlags(SF_BUTTON_USE_ACTIVATES))
	{
		SetUse(&CBaseButton::ButtonUse);
	}
	else
	{
		SetUse(nullptr);
	}

	//
	// If touching activates the button, set its touch function.
	//
	if (HasSpawnFlags(SF_BUTTON_TOUCH_ACTIVATES))
	{
		SetTouch( &CBaseButton::ButtonTouch );
	}
	else 
	{
		SetTouch ( nullptr );
	}

	CreateVPhysics();
}

//-----------------------------------------------------------------------------

bool CBaseButton::CreateVPhysics_Impl()
{
	VPhysicsInitShadow( false, false );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Button sound table.
//			Also used by CBaseDoor to get 'touched' door lock/unlock sounds
// Input  : sound - index of sound to look up.
// Output : Returns a pointer to the corresponding sound file.
//-----------------------------------------------------------------------------
string_t MakeButtonSound( int sound )
{ 
	char tmp[1024];
	V_snprintf( tmp, sizeof(tmp), "Buttons.snd%d", sound );
	return AllocPooledString(tmp);
}

//-----------------------------------------------------------------------------
// Purpose: Think function that emits sparks at random intervals.
//-----------------------------------------------------------------------------
void CBaseButton::ButtonSpark ( void )
{
	SetThink ( &CBaseButton::ButtonSpark );
	SetNextThink( gpGlobals->curtime + 0.1 + random_valve->RandomFloat ( 0, 1.5 ) );// spark again at random interval

	DoSpark( this, WorldSpaceCenter(), 1, 1, true, vec3_origin );
}

//-----------------------------------------------------------------------------
// Purpose: Called when someone uses us whilst we are locked.
//-----------------------------------------------------------------------------
bool CBaseButton::OnUseLocked( CBaseEntity *pActivator )
{
	PlayLockSounds(this, &m_ls, true, true);

	if ( gpGlobals->curtime > m_flUseLockedTime )
	{
		m_OnUseLocked.FireOutput( pActivator, this );
		m_flUseLockedTime = gpGlobals->curtime + 0.5;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Use function that starts the button moving.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CBaseButton::ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Ignore touches if button is moving, or pushed-in and waiting to auto-come-out.
	// UNDONE: Should this use ButtonResponseToTouch() too?
	if (m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN )
		return;		

	if (m_bLocked)
	{
		OnUseLocked( pActivator );
		return;
	}

	m_hActivator = pActivator;

	if ( m_toggle_state == TS_AT_TOP)
	{
		//
		// If it's a toggle button it can return now. Otherwise, it will either
		// return on its own or will stay pressed indefinitely.
		//
		if ( HasSpawnFlags(SF_BUTTON_TOGGLE))
		{
			if ( m_sNoise != NULL_STRING )
			{
				CPASAttenuationFilter filter( this );

				EmitSound_t ep;
				ep.m_nChannel = CHAN_VOICE;
				ep.m_pSoundName = (char*)STRING(m_sNoise);
				ep.m_flVolume = 1;
				ep.m_SoundLevel = SNDLVL_NORM;

				EmitSound( filter, entindex(), ep );
			}

			m_OnPressed.FireOutput(m_hActivator, this);
			ButtonReturn();
		}
	}
	else
	{
		m_OnPressed.FireOutput(m_hActivator, this);
		ButtonActivate( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a code indicating how the button should respond to being touched.
// Output : Returns one of the following:
//				BUTTON_NOTHING - do nothing
//				BUTTON_RETURN - 
//				BUTTON_ACTIVATE - act as if pressed
//-----------------------------------------------------------------------------
CBaseButton::BUTTON_CODE CBaseButton::ButtonResponseToTouch( void )
{
	// Ignore touches if button is moving, or pushed-in and waiting to auto-come-out.
	if (m_toggle_state == TS_GOING_UP ||
		m_toggle_state == TS_GOING_DOWN ||
		(m_toggle_state == TS_AT_TOP && !m_fStayPushed && !HasSpawnFlags(SF_BUTTON_TOGGLE) ) )
		return BUTTON_NOTHING;

	if (m_toggle_state == TS_AT_TOP)
	{
		if ( HasSpawnFlags(SF_BUTTON_TOGGLE) && !m_fStayPushed)
		{
			return BUTTON_RETURN;
		}
	}
	else
		return BUTTON_ACTIVATE;

	return BUTTON_NOTHING;
}

//-----------------------------------------------------------------------------
// Purpose: Touch function that activates the button if it responds to touch.
// Input  : pOther - The entity that touched us.
//-----------------------------------------------------------------------------
void CBaseButton::ButtonTouch( CBaseEntity *pOther )
{
	// Ignore touches by anything but players
	if ( !pOther->IsPlayer() )
		return;

	m_hActivator = pOther;

	BUTTON_CODE code = ButtonResponseToTouch();

	if ( code == BUTTON_NOTHING )
		return;

	if (!UTIL_IsMasterTriggered(m_sMaster, pOther) || m_bLocked)
	{
		// play button locked sound
		PlayLockSounds(this, &m_ls, true, true);
		return;
	}

	// Temporarily disable the touch function, until movement is finished.
	SetTouch( nullptr );

	if ( code == BUTTON_RETURN )
	{
		if ( m_sNoise != NULL_STRING )
		{
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_VOICE;
			ep.m_pSoundName = (char*)STRING(m_sNoise);
			ep.m_flVolume = 1;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );
		}

		m_OnPressed.FireOutput(m_hActivator, this);
		ButtonReturn();
	}
	else
	{
		// code == BUTTON_ACTIVATE
		m_OnPressed.FireOutput(m_hActivator, this);
		ButtonActivate( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the button moving "in/up".
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseButton::ButtonActivate( void )
{
	if ( m_sNoise != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_VOICE;
		ep.m_pSoundName = (char*)STRING(m_sNoise);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}
	
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator) || m_bLocked)
	{
		// button is locked, play locked sound
		PlayLockSounds(this, &m_ls, true, true);
		return;
	}
	else
	{
		// button is unlocked, play unlocked sound
		PlayLockSounds(this, &m_ls, false, true);
	}

	ASSERT(m_toggle_state == TS_AT_BOTTOM);
	m_toggle_state = TS_GOING_UP;
	
	SetMoveDone( &CBaseButton::TriggerAndWait );
	if (!m_fRotating)
		LinearMove( m_vecPosition2, m_flSpeed);
	else
		AngularMove( m_vecAngle2, m_flSpeed);
}

//-----------------------------------------------------------------------------
// Purpose: Enables or disables the use capability based on our spawnflags.
//-----------------------------------------------------------------------------
int	CBaseButton::ObjectCaps_Impl(void)
{
	return((BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION) |
			(HasSpawnFlags(SF_BUTTON_USE_ACTIVATES) ? (FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS) : 0));
}

//-----------------------------------------------------------------------------
// Purpose: Button has reached the "pressed/top" position. Fire its OnIn output,
//			and pause before returning to "unpressed/bottom".
//-----------------------------------------------------------------------------
void CBaseButton::TriggerAndWait( void )
{
	ASSERT(m_toggle_state == TS_GOING_UP);

	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator) || m_bLocked)
	{
		return;
	}

	m_toggle_state = TS_AT_TOP;
	
	//
	// Re-instate touches if the button is of the toggle variety.
	//
	if (m_fStayPushed || HasSpawnFlags(SF_BUTTON_TOGGLE ) )
	{
		if (HasSpawnFlags(SF_BUTTON_TOUCH_ACTIVATES))
		{
			SetTouch(&CBaseButton::ButtonTouch);
		}
		else
		{
			// BUGBUG: ALL buttons no longer respond to touch
			SetTouch (nullptr);
		}
	}

	//
	// If button automatically comes back out, start it moving out.
	//
	else
	{
		SetNextThink( gpGlobals->curtime + m_flWait );
		SetThink( &CBaseButton::ButtonReturn );
	}
	
	m_nState = 1;			// use alternate textures

	m_OnIn.FireOutput(m_hActivator, this);
}

//-----------------------------------------------------------------------------
// Purpose: Starts the button moving "out/down".
//-----------------------------------------------------------------------------
void CBaseButton::ButtonReturn( void )
{
	ASSERT(m_toggle_state == TS_AT_TOP);
	m_toggle_state = TS_GOING_DOWN;
	
	SetMoveDone( &CBaseButton::ButtonBackHome );
	if (!m_fRotating)
		LinearMove( m_vecPosition1, m_flSpeed);
	else
		AngularMove( m_vecAngle1, m_flSpeed);

	m_nState = 0;			// use normal textures
}

//-----------------------------------------------------------------------------
// Purpose: Button has returned to the "unpressed/bottom" position. Fire its
//			OnOut output and stop moving.
//-----------------------------------------------------------------------------
void CBaseButton::ButtonBackHome( void )
{
	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	m_OnOut.FireOutput(m_hActivator, this);

	//
	// Re-instate touch method, movement cycle is complete.
	//
	if (HasSpawnFlags(SF_BUTTON_TOUCH_ACTIVATES))
	{
		SetTouch( &CBaseButton::ButtonTouch );
	}
	else
	{
		// BUGBUG: ALL buttons no longer respond to touch
		SetTouch ( nullptr );
	}

	// reset think for a sparking button
	if (HasSpawnFlags( SF_BUTTON_SPARK_IF_OFF ) )
	{
		SetThink ( &CBaseButton::ButtonSpark );
		SetNextThink( gpGlobals->curtime + 0.5f );// no hurry
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseButton::DrawDebugTextOverlays_Impl()
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		static const char *pszStates[] =
		{
			"Pressed",
			"Unpressed",
			"Pressing...",
			"Unpressing...",
			"<UNKNOWN STATE>",
		};
	
		char tempstr[255];

		int nState = m_toggle_state;
		if ( ( nState < 0 ) || ( nState > 3 ) )
		{
			nState = 4;
		}

		V_snprintf( tempstr, sizeof(tempstr), "State: %s", pszStates[nState] );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;

		V_snprintf( tempstr, sizeof(tempstr), "%s", m_bLocked ? "Locked" : "Unlocked" );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;
	}
	return text_offset;
}

//
// Rotating button (aka "lever")
//
LINK_ENTITY_TO_CLASS( func_rot_button, CRotButton );

void CRotButton::Spawn_Impl( void )
{
	//----------------------------------------------------
	//determine sounds for buttons
	//a sound of 0 should not make a sound
	//----------------------------------------------------
	if ( m_sounds )
	{
		m_sNoise = MakeButtonSound( m_sounds );
		PrecacheScriptSound(m_sNoise.ToCStr());
	}
	else
	{
		m_sNoise = NULL_STRING;
	}

	// set the axis of rotation
	CBaseToggle::AxisDir();

	// check for clockwise rotation
	if ( HasSpawnFlags( SF_DOOR_ROTATE_BACKWARDS) )
	{
		m_vecMoveAng = m_vecMoveAng * -1;
	}

	SetMoveType( MOVETYPE_PUSH );
	
	SetSolid( SOLID_VPHYSICS );
	if ( HasSpawnFlags( SF_ROTBUTTON_NOTSOLID ) )
	{
		AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	SetModel( STRING( GetModelName() ) );
	
	if (m_flSpeed == 0)
		m_flSpeed = 40;

	if (m_flWait == 0)
		m_flWait = 1;

	if (m_iHealth > 0)
	{
		m_takedamage = DAMAGE_YES;
	}

	m_toggle_state = TS_AT_BOTTOM;
	m_vecAngle1	= GetLocalAngles();
	m_vecAngle2	= GetLocalAngles() + m_vecMoveAng * m_flMoveDistance;
	ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating button start/end positions are equal\n");

	m_fStayPushed = (m_flWait == -1 ? true : false);
	m_fRotating = true;

	SetUse(&CRotButton::ButtonUse);

	//
	// If touching activates the button, set its touch function.
	//
	if (!HasSpawnFlags(SF_BUTTON_TOUCH_ACTIVATES))
	{
		SetTouch ( nullptr );
	}
	else
	{
		SetTouch( &CRotButton::ButtonTouch );
	}

	CreateVPhysics();
}

bool CRotButton::CreateVPhysics_Impl( void )
{
	VPhysicsInitShadow( false, false );
	return true;
}

//-----------------------------------------------------------------------------
// CMomentaryRotButton spawnflags
//-----------------------------------------------------------------------------
static constexpr auto SF_MOMENTARY_DOOR = 1;
static constexpr auto SF_MOMENTARY_NOT_USABLE = 2;
static constexpr auto SF_MOMENTARY_AUTO_RETURN = 16;

IMPLEMENT_REFLECT_DATAMAP( CMomentaryRotButton )

LINK_ENTITY_TO_CLASS( momentary_rot_button, CMomentaryRotButton );

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Spawn_Impl( void )
{
	CBaseToggle::AxisDir();

	m_bUpdateTarget = true;

	if ( m_flSpeed == 0 )
	{
		m_flSpeed = 100;
	}

	// Clamp start position and issue bounds warning
	if (m_flStartPosition < 0.0 || m_flStartPosition > 1.0)
	{
		Warning("WARNING: Momentary door (%s) start position not between 0 and 1.  Clamping.\n",GetDebugName());
		m_flStartPosition = clamp(m_IdealYaw, 0, 1);
	}

	// Check direction fields (for backward compatibility)
	if (m_direction != 1 && m_direction != -1)
	{
		m_direction = 1;
	}

	if (m_flMoveDistance < 0)
	{
		m_vecMoveAng = m_vecMoveAng * -1;
		m_flMoveDistance = -m_flMoveDistance;
	}

	m_start = GetLocalAngles() - m_vecMoveAng * m_flMoveDistance * m_flStartPosition;
	m_end	= GetLocalAngles() + m_vecMoveAng * m_flMoveDistance * (1-m_flStartPosition);

	m_IdealYaw			= m_flStartPosition;

	// Force start direction at end points
	if (m_flStartPosition == 0.0)
	{
		m_direction = -1;
	}
	else if (m_flStartPosition == 1.0)
	{
		m_direction = 1;
	}

	if (HasSpawnFlags(SF_BUTTON_LOCKED))
	{
		m_bLocked = true;
	}

	if ( HasSpawnFlags( SF_BUTTON_USE_ACTIVATES ) )
	{
		if ( m_sounds )
		{
			m_sNoise = MakeButtonSound( m_sounds );
			PrecacheScriptSound(m_sNoise.ToCStr());
		}
		else
		{
			m_sNoise = NULL_STRING;
		}

		m_lastUsed	= 0;
		UpdateTarget(0,this);
	}

	SetSolid( SOLID_VPHYSICS );
	if (HasSpawnFlags(SF_ROTBUTTON_NOTSOLID))
	{
		AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING( GetModelName() ) );
	
	CreateVPhysics();

	// Slam the object back to solid - if we really want it to be solid.
	if ( m_bSolidBsp )
	{
		SetSolid( SOLID_BSP );
	}

	m_bDisabled = false;
}

int	CMomentaryRotButton::ObjectCaps_Impl( void ) 
{ 
	int flags = BaseClass::ObjectCaps_Impl();
	if (!HasSpawnFlags(SF_BUTTON_USE_ACTIVATES))
	{
		return flags;
	}
	else
	{	
		return (flags | FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMomentaryRotButton::CreateVPhysics_Impl( void )
{
	VPhysicsInitShadow( false, false );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMomentaryRotButton::PlaySound( void )
{
	if ( m_sNoise == NULL_STRING )
		return;

	CPASAttenuationFilter filter( this );

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName = (char*)STRING(m_sNoise);
	ep.m_flVolume = 1;
	ep.m_SoundLevel = SNDLVL_NORM;

	EmitSound( filter, entindex(), ep );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a given angular position as a value along our motion from 0 to 1.
// Input  : vecAngles - 
//-----------------------------------------------------------------------------
float CMomentaryRotButton::GetPos( const QAngle &vecAngles )
{
	float flScale = 1;
	if (( m_vecMoveAng[0] < 0 ) || ( m_vecMoveAng[1] < 0 ) || ( m_vecMoveAng[2] < 0 ))
	{
		flScale = -1;
	}

	float flPos = flScale * CBaseToggle::AxisDelta( m_spawnflags, vecAngles, m_start ) / m_flMoveDistance;
	return( clamp( flPos, 0, 1 ));
}

//------------------------------------------------------------------------------
// Purpose :
// Input   : flPosition 
//------------------------------------------------------------------------------
void CMomentaryRotButton::InputSetPosition( inputdata_t &inputdata )
{
	m_IdealYaw = clamp( inputdata.value.Float(), 0, 1 );

	float flCurPos = GetPos( GetLocalAngles() );
	if ( flCurPos < m_IdealYaw )
	{
		// Moving forward (from start to end).
		SetLocalAngularVelocity( m_flSpeed * m_vecMoveAng );
		m_direction = 1;
	}
	else if ( flCurPos > m_IdealYaw )
	{
		// Moving backward (from end to start).
		SetLocalAngularVelocity( -m_flSpeed * m_vecMoveAng );
		m_direction = -1;
	}
	else
	{
		// We're there already; nothing to do.
		SetLocalAngularVelocity( vec3_angle );
		return;
	}

	SetMoveDone( &CMomentaryRotButton::SetPositionMoveDone );

	SetThink( &CMomentaryRotButton::UpdateThink );
	SetNextThink( gpGlobals->curtime );

	//
	// Think again in 0.1 seconds or the time that it will take us to reach our movement goal,
	// whichever is the shorter interval. This prevents us from overshooting and stuttering when we
	// are told to change position in very small increments.
	//
	QAngle vecNewAngles = m_start + m_vecMoveAng * ( m_IdealYaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( m_spawnflags, vecNewAngles, GetLocalAngles() ));
	float dt = flAngleDelta / m_flSpeed;
	if ( dt < TICK_INTERVAL )
	{
		dt = TICK_INTERVAL;
		float speed = flAngleDelta / TICK_INTERVAL;
		SetLocalAngularVelocity( speed * m_vecMoveAng * m_direction );
	}
	dt = clamp( dt, TICK_INTERVAL, TICK_INTERVAL * 6);

	SetMoveDoneTime( dt );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   : flPosition 
//------------------------------------------------------------------------------
void CMomentaryRotButton::InputSetPositionImmediately( inputdata_t &inputdata )
{
	m_IdealYaw = clamp( inputdata.value.Float(), 0, 1 );
	SetLocalAngles( m_start + m_vecMoveAng * ( m_IdealYaw * m_flMoveDistance ) );
}

//------------------------------------------------------------------------------
// Purpose: Turns off target updates so that we can change the wheel's position
//			without changing the target's position. Used for jiggling when locked.
//------------------------------------------------------------------------------
void CMomentaryRotButton::InputDisableUpdateTarget( inputdata_t &inputdata )
{
	m_bUpdateTarget = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns target updates back on (after jiggling).
//------------------------------------------------------------------------------
void CMomentaryRotButton::InputEnableUpdateTarget( inputdata_t &inputdata )
{
	m_bUpdateTarget = true;
}

//-----------------------------------------------------------------------------
// Purpose: Locks the button. If locked, the button will play the locked sound
//			when the player tries to use it.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Lock()
{
	BaseClass::Lock();

	SetLocalAngularVelocity( vec3_angle );
	SetMoveDoneTime( -1 );
	SetMoveDone( nullptr );

	SetNextThink( TICK_NEVER_THINK );
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: Unlocks the button, making it able to be pressed again.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Unlock()
{
	BaseClass::Unlock();

	SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );

	// Delay before autoreturn.
	SetMoveDoneTime( 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Fires the appropriate outputs at the extremes of motion.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::OutputMovementComplete( void )
{
	if (m_IdealYaw == 1.0)
	{
		m_OnFullyClosed.FireOutput(this, this);
	}
	else if (m_IdealYaw == 0.0)
	{
		m_OnFullyOpen.FireOutput(this, this);
	}

	m_OnReachedPosition.FireOutput( this, this );
}

//------------------------------------------------------------------------------
// Purpose: MoveDone function for the SetPosition input handler. Tracks our
//			progress toward a movement goal and updates our outputs.
//------------------------------------------------------------------------------
void CMomentaryRotButton::SetPositionMoveDone(void)
{
	float flCurPos = GetPos( GetLocalAngles() );

	if ((( flCurPos >= m_IdealYaw ) && ( m_direction == 1 )) ||
		(( flCurPos <= m_IdealYaw ) && ( m_direction == -1 )))
	{
		//
		// We reached or surpassed our movement goal.
		//
		SetLocalAngularVelocity( vec3_angle );
		// BUGBUG: Won't this get the player stuck?
		SetLocalAngles( m_start + m_vecMoveAng * ( m_IdealYaw * m_flMoveDistance ) );
		SetNextThink( TICK_NEVER_THINK );
		SetMoveDoneTime( -1 );
		UpdateTarget( m_IdealYaw, this );
		OutputMovementComplete();
		return;
	}

	// TODO: change this to use a Think function like ReturnThink.
	QAngle vecNewAngles = m_start + m_vecMoveAng * ( m_IdealYaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( m_spawnflags, vecNewAngles, GetLocalAngles() ));
	float dt = flAngleDelta / m_flSpeed;
	if ( dt < TICK_INTERVAL )
	{
		dt = TICK_INTERVAL;
		float speed = flAngleDelta / TICK_INTERVAL;
		SetLocalAngularVelocity( speed * m_vecMoveAng * m_direction );
	}
	dt = clamp( dt, TICK_INTERVAL, TICK_INTERVAL * 6);

	SetMoveDoneTime( dt );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bDisabled == true )
		return;

	if (m_bLocked)
	{
		if ( OnUseLocked( pActivator ) && HasSpawnFlags( SF_BUTTON_JIGGLE_ON_USE_LOCKED ) )
		{
			// Jiggle two degrees.
			float flDist = 2.0 / m_flMoveDistance;

			// Must be first!
			g_EventQueue.AddEvent( this, "_DisableUpdateTarget", 0, this, this );

			variant_t value;
			value.SetFloat( flDist );
			g_EventQueue.AddEvent( this, "SetPosition", value, 0.01, this, this );

			value.SetFloat( 0.0 );
			g_EventQueue.AddEvent( this, "SetPosition", value, 0.1, this, this );

			value.SetFloat( 0.5 * flDist );
			g_EventQueue.AddEvent( this, "SetPosition", value, 0.2, this, this );

			value.SetFloat( 0.0 );
			g_EventQueue.AddEvent( this, "SetPosition", value, 0.3, this, this );

			// Must be last! And must be late enough to cover the settling time.
			g_EventQueue.AddEvent( this, "_EnableUpdateTarget", 0.5, this, this );
		}

		return;
	}

	//
	// Reverse our direction and play movement sound every time the player
	// pauses between uses.
	//
	bool bPlaySound = false;
	
	if ( !m_lastUsed )
	{
		bPlaySound = true;
		m_direction = -m_direction;

		//Alert that we've been pressed
		m_OnPressed.FireOutput( m_hActivator, this );
	}

	m_lastUsed = 1;

	float flPos = GetPos( GetLocalAngles() );
	UpdateSelf( flPos, bPlaySound );

	//
	// Think every frame while we are moving.
	// HACK: Don't reset the think time if we already have a pending think.
	// This works around an issue with host_thread_mode > 0 when the player's
	// clock runs ahead of the server.
	//
	if ( !m_pfnThink )
	{
		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles changing direction at the extremes of our range of motion
//			and updating our avelocity while being used by the player.
// Input  : value - Number from 0 to 1 indicating our desired position within
//				our range of motion, 0 = start, 1 = end.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::UpdateSelf( float value, bool bPlaySound )
{
	//
	// Set our move clock to 0.1 seconds in the future so we stop spinning unless we are
	// used again before then.
	//
	SetMoveDoneTime( 0.1 );

	//
	// If we hit the end, zero our avelocity and snap to the end angles.
	//
	if ( m_direction > 0 && value >= 1.0 )
	{
		SetLocalAngularVelocity( vec3_angle );
		SetLocalAngles( m_end );

		m_OnFullyClosed.FireOutput(this, this);
		return;
	}
	//
	// If we returned to the start, zero our avelocity and snap to the start angles.
	//
	else if ( m_direction < 0 && value <= 0 )
	{
		SetLocalAngularVelocity( vec3_angle );
		SetLocalAngles( m_start );

		m_OnFullyOpen.FireOutput(this, this);
		return;
	}
	
	if ( bPlaySound )
	{
		PlaySound();
	}

	SetLocalAngularVelocity( ( m_direction * m_flSpeed ) * m_vecMoveAng );
	SetMoveDone( &CMomentaryRotButton::UseMoveDone );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the value of our position, firing any targets.
// Input  : value - New position, from 0 - 1.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::UpdateTarget( float value, CBaseEntity *pActivator )
{
	if ( !m_bUpdateTarget )
		return;

	if (m_Position.Get() != value)
	{
		m_Position.Set(value, pActivator, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the end of motion caused by player use.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::UseMoveDone( void )
{
	SetLocalAngularVelocity( vec3_angle );

	// Make sure our targets stop where we stopped.
	float flPos = GetPos( GetLocalAngles() );
	UpdateTarget( flPos, this );

	// Alert that we've been unpressed
	m_OnUnpressed.FireOutput( m_hActivator, this );

	m_lastUsed = 0;

	if ( !HasSpawnFlags( SF_BUTTON_TOGGLE ) && m_returnSpeed > 0 )
	{
		SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );
		m_direction = -1;

		// Delay before autoreturn.
		SetMoveDoneTime( 0.1f );
	}
	else
	{
		SetThink( nullptr );
		SetMoveDone( nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: MoveDone function for rotating back to the start position.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::ReturnMoveDone( void )
{
	float value = GetPos( GetLocalAngles() );
	if ( value <= 0 )
	{
		//
		// Got back to the start, stop spinning.
		//
		SetLocalAngularVelocity( vec3_angle );
		SetLocalAngles( m_start );

		UpdateTarget( 0, nullptr );

		SetMoveDoneTime( -1 );
		SetMoveDone( nullptr );

		SetNextThink( TICK_NEVER_THINK );
		SetThink( nullptr );
	}
	else
	{
		SetLocalAngularVelocity( -m_returnSpeed * m_vecMoveAng );
		SetMoveDoneTime( 0.1f );

		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( gpGlobals->curtime + 0.01f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function for updating target as we move.
//-----------------------------------------------------------------------------
void CMomentaryRotButton::UpdateThink( void )
{
	float value = GetPos( GetLocalAngles() );
	UpdateTarget( value, nullptr );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CMomentaryRotButton::DrawDebugTextOverlays_Impl(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[255];
		
		V_snprintf(tempstr,sizeof(tempstr),"QAngle: %.2f %.2f %.2f", GetLocalAngles()[0], GetLocalAngles()[1], GetLocalAngles()[2]);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"AVelocity: %.2f %.2f %.2f", GetLocalAngularVelocity()[0], GetLocalAngularVelocity()[1], GetLocalAngularVelocity()[2]);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"Target Pos:   %3.3f",m_IdealYaw);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		float flCurPos = GetPos(GetLocalAngles());
		V_snprintf(tempstr,sizeof(tempstr),"Current Pos:   %3.3f",flCurPos);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		V_snprintf(tempstr,sizeof(tempstr),"Direction: %s",(m_direction == 1) ? "Forward" : "Backward");
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Input hander that starts the spawner
//-----------------------------------------------------------------------------
void CMomentaryRotButton::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: Input hander that stops the spawner
//-----------------------------------------------------------------------------
void CMomentaryRotButton::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// Purpose: Start the spawner
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Enable( void )
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Stop the spawner
//-----------------------------------------------------------------------------
void CMomentaryRotButton::Disable( void )
{
	m_bDisabled = true;
}

class CBaseEntity;
class CEntityList;
class CEventAction;

using namespace ::ks::reflect;

/*
Entity Data Descriptions

Each entity has an array which defines it's data in way that is useful for
entity communication, parsing initial values from the map, and save/restore.

each entity has to have the following line in it's class declaration:

	DECLARE_DATADESC();

this line defines that it has an m_DataDesc[] array, and declares functions through
which various subsystems can iterate the data.

In it's implementation, each entity has to have:

	typedescription_t CBaseEntity::m_DataDesc[] = { ... }
	
in which all it's data is defined (see below), followed by:

which implements the functions necessary for iterating through an entities data desc.

There are several types of data:
	
	FIELD		: this is a variable which gets saved & loaded from disk
	KEY			: this variable can be read in from the map file
	GLOBAL		: a global field is actually local; it is saved/restored, but is actually
				  unique to the entity on each level.
	CUSTOM		: the save/restore parsing functions are described by the user.
	ARRAY		: an array of values
	OUTPUT		: a variable or event that can be connected to other entities (see below)
	INPUTFUNC	: maps a string input to a function pointer. Outputs connected to this input
				  will call the notify function when fired.
	INPUT		: maps a string input to a member variable. Outputs connected to this input
				  will update the input data value when fired.
	INPUTNOTIFY	: maps a string input to a member variable/function pointer combo. Outputs
				  connected to this input will update the data value and call the notify
				  function when fired.

some of these can overlap.  all the data descriptions usable are:

	DEFINE_KEYFIELD(	name,	fieldtype,	mapname )
	DEFINE_KEYFIELD_NOTSAVED(	name,	fieldtype,	mapname )
	DEFINE_GLOBAL_KEYFIELD(name,	fieldtype,	mapname )

where:
	type is the name of the class (eg. CBaseEntity)
	name is the name of the variable in the class (eg. m_iHealth)
	fieldtype is the type of data (FIELD_STRING, FIELD_INTEGER, etc)
	mapname is the string by which this variable is associated with map file data
	count is the number of items in the array
	datafuncs is a struct containing function pointers for a custom-defined save/restore/parse

OUTPUTS:

 	DEFINE_OUTPUT(		outputvar,	outputname )

	This maps the string 'outputname' to the COutput-derived member variable outputvar.  In the VMF
	file these outputs can be hooked up to inputs (see above).  Whenever the internal state
	of an entity changes it will often fire off outputs so that map makers can hook up behaviors.
	e.g.  A door entity would have OnDoorOpen, OnDoorClose, OnTouched, etc outputs.
*/
//=============================================================================

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CEventAction )

// ID Stamp used to uniquely identify every output
int CEventAction::s_iNextIDStamp = 0;

//-----------------------------------------------------------------------------
// Purpose: Creates an event action and assigns it an unique ID stamp.
// Input  : ActionData - the map file data block descibing the event action.
//-----------------------------------------------------------------------------
CEventAction::CEventAction( const char *ActionData )
{
	m_pNext = nullptr;
	m_iIDStamp = ++s_iNextIDStamp;

	m_flDelay = 0;
	m_iTarget = NULL_STRING;
	m_iParameter = NULL_STRING;
	m_iTargetInput = NULL_STRING;
	m_nTimesToFire = EVENT_FIRE_ALWAYS;

	if (ActionData == nullptr)
		return;

	char szToken[256];
	
	char chDelim = VMF_IOPARAM_STRING_DELIMITER;
	if (!std::strchr(ActionData, VMF_IOPARAM_STRING_DELIMITER))
	{
		chDelim = ',';
	}

	//
	// Parse the target name.
	//
	const char *psz = nexttoken(szToken, ActionData, chDelim);
	if (szToken[0] != '\0')
	{
		m_iTarget = AllocPooledString(szToken);
	}

	//
	// Parse the input name.
	//
	psz = nexttoken(szToken, psz, chDelim);
	if (szToken[0] != '\0')
	{
		m_iTargetInput = AllocPooledString(szToken);
	}
	else
	{
		m_iTargetInput = AllocPooledString("Use");
	}

	//
	// Parse the parameter override.
	//
	psz = nexttoken(szToken, psz, chDelim);
	if (szToken[0] != '\0')
	{
		m_iParameter = AllocPooledString(szToken);
	}

	//
	// Parse the delay.
	//
	psz = nexttoken(szToken, psz, chDelim);
	if (szToken[0] != '\0')
	{
		m_flDelay = std::atof(szToken);
	}

	//
	// Parse the number of times to fire.
	//
	nexttoken(szToken, psz, chDelim);
	if (szToken[0] != '\0')
	{
		m_nTimesToFire = std::atoi(szToken);
		if (m_nTimesToFire == 0)
		{
			m_nTimesToFire = EVENT_FIRE_ALWAYS;
		}
	}
}

CEventAction::CEventAction( const CEventAction &p_EventAction )
{
	m_pNext = nullptr;
	m_iIDStamp = ++s_iNextIDStamp;

	m_flDelay = p_EventAction.m_flDelay;
	m_iTarget = p_EventAction.m_iTarget;
	m_iParameter = p_EventAction.m_iParameter;
	m_iTargetInput = p_EventAction.m_iTargetInput;
	m_nTimesToFire = p_EventAction.m_nTimesToFire;
}

// this memory pool stores blocks around the size of CEventAction/inputitem_t structs
// can be used for other blocks; will error if to big a block is tried to be allocated
ks::fixed_pool g_EntityListPool( std::max(sizeof(CEventAction),sizeof(CMultiInputVar::inputitem_t)), 512, ks::fixed_pool::grow::fast, "g_EntityListPool", std::max(alignof(CEventAction),alignof(CMultiInputVar::inputitem_t)) );

void *CEventAction::operator new( std::size_t stAllocateBlock )
{
	return g_EntityListPool.alloc( stAllocateBlock );
}

void *CEventAction::operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntityListPool.alloc( stAllocateBlock );
}

void CEventAction::operator delete( void *pMem )
{
	g_EntityListPool.free( pMem );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the highest-valued delay in our list of event actions.
//-----------------------------------------------------------------------------
float CBaseEntityOutput::GetMaxDelay(void)
{
	float flMaxDelay = 0;
	CEventAction *ev = m_ActionList;

	while (ev != nullptr)
	{
		if (ev->m_flDelay > flMaxDelay)
		{
			flMaxDelay = ev->m_flDelay;
		}
		ev = ev->m_pNext;
	}

	return(flMaxDelay);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CBaseEntityOutput::~CBaseEntityOutput()
{
	CEventAction *ev = m_ActionList;
	while (ev != nullptr)
	{
		CEventAction *pNext = ev->m_pNext;	
		delete ev;
		ev = pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fires the event, causing a sequence of action to occur in other ents.
// Input  : pActivator - Entity that initiated this sequence of actions.
//			pCaller - Entity that is actually causing the event.
//-----------------------------------------------------------------------------
void CBaseEntityOutput::FireOutput(variant_t Value, CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay)
{
	//
	// Iterate through all eventactions and fire them off.
	//
	CEventAction *ev = m_ActionList;
	CEventAction *prev = nullptr;
	
	while (ev != nullptr)
	{
		if (ev->m_iParameter == NULL_STRING)
		{
			//
			// Post the event with the default parameter.
			//
			g_EventQueue.AddEvent( STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), Value, ev->m_flDelay + fDelay, pActivator, pCaller, ev->m_iIDStamp );
		}
		else
		{
			//
			// Post the event with a parameter override.
			//
			variant_t ValueOverride;
			ValueOverride.SetString( ev->m_iParameter );
			g_EventQueue.AddEvent( STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), ValueOverride, ev->m_flDelay, pActivator, pCaller, ev->m_iIDStamp );
		}

		if ( developer.GetBool() )
		{
			if ( ev->m_flDelay )
			{
				char szBuffer[256];
				V_snprintf( szBuffer, sizeof(szBuffer), "(%0.2f) output: (%s,%s) -> (%s,%s,%.1f)(%s)\n", gpGlobals->curtime, pCaller ? STRING(pCaller->m_iClassname) : "NULL", pCaller ? STRING(pCaller->GetEntityName()) : "NULL", STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), ev->m_flDelay, STRING(ev->m_iParameter) );
				DevMsg( 2, "%s", szBuffer );
				ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
			}
			else
			{
				char szBuffer[256];
				V_snprintf( szBuffer, sizeof(szBuffer), "(%0.2f) output: (%s,%s) -> (%s,%s)(%s)\n", gpGlobals->curtime, pCaller ? STRING(pCaller->m_iClassname) : "NULL", pCaller ? STRING(pCaller->GetEntityName()) : "NULL", STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), STRING(ev->m_iParameter) );
				DevMsg( 2, "%s", szBuffer );
				ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
			}
		}

		if ( pCaller && pCaller->m_debugOverlays & OVERLAY_MESSAGE_BIT)
		{
			pCaller->DrawOutputOverlay(ev);
		}

		//
		// Remove the event action from the list if it was set to be fired a finite
		// number of times (and has been).
		//
		bool bRemove = false;
		if (ev->m_nTimesToFire != EVENT_FIRE_ALWAYS)
		{
			ev->m_nTimesToFire--;
			if (ev->m_nTimesToFire == 0)
			{
				char szBuffer[256];
				V_snprintf( szBuffer, sizeof(szBuffer), "Removing from action list: (%s,%s) -> (%s,%s)\n", pCaller ? STRING(pCaller->m_iClassname) : "NULL", pCaller ? STRING(pCaller->GetEntityName()) : "NULL", STRING(ev->m_iTarget), STRING(ev->m_iTargetInput));
				DevMsg( 2, "%s", szBuffer );
				ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
				bRemove = true;
			}
		}

		if (!bRemove)
		{
			prev = ev;
			ev = ev->m_pNext;
		}
		else
		{
			if (prev != nullptr)
			{
				prev->m_pNext = ev->m_pNext;
			}
			else
			{
				m_ActionList = ev->m_pNext;
			}

			CEventAction *next = ev->m_pNext;
			delete ev;
			ev = next;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parameterless firing of an event
// Input  : pActivator - 
//			pCaller - 
//-----------------------------------------------------------------------------
void COutputEvent::FireOutput(CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay)
{
	variant_t Val;
	Val.Set( FIELD_VOID, nullptr );
	CBaseEntityOutput::FireOutput(Val, pActivator, pCaller, fDelay);
}

void CBaseEntityOutput::ParseEventAction( const char *EventData )
{
	AddEventAction( new CEventAction( EventData ) );
}

void CBaseEntityOutput::AddEventAction( CEventAction *pEventAction )
{
	pEventAction->m_pNext = m_ActionList;
	m_ActionList = pEventAction;
}

void CBaseEntityOutput::RemoveEventAction( CEventAction *pEventAction )
{
	CEventAction *pAction = GetFirstAction();
	CEventAction *pPrevAction = nullptr;
	while ( pAction )
	{
		if ( pAction == pEventAction )
		{
			if ( !pPrevAction )
			{
				m_ActionList = nullptr;
			}
			else
			{
				pPrevAction->m_pNext = pAction->m_pNext;
			}
			return;
		}
		pAction = pAction->m_pNext;
	}
}

// save data description for the event queue
IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CBaseEntityOutput )

const CEventAction *CBaseEntityOutput::GetActionForTarget( string_t iSearchTarget ) const
{
	for ( CEventAction *ev = m_ActionList; ev != nullptr; ev = ev->m_pNext )
	{
		if ( ev->m_iTarget == iSearchTarget )
			return ev;
	}

	return nullptr;
}

int CBaseEntityOutput::NumberOfElements( void )
{
	int count = 0;
	for ( CEventAction *ev = m_ActionList; ev != nullptr; ev = ev->m_pNext )
	{
		count++;
	}
	return count;
}

/// Delete every single action in the action list. 
void CBaseEntityOutput::DeleteAllElements( void ) 
{
	// walk front to back, deleting as we go. We needn't fix up pointers because
	// EVERYTHING will die.

	CEventAction *pNext = m_ActionList;
	// wipe out the head
	m_ActionList = nullptr;
	while (pNext)
	{
		CEventAction *strikeThis = pNext;
		pNext = pNext->m_pNext;
		delete strikeThis;
	}

}

/// EVENTS save/restore parsing wrapper

class CEventsSaveDataOps : public ISaveRestoreOps
{
	virtual bool Parse( const SaveRestoreFieldInfo_t &fieldInfo, char const* szValue )
	{
		CBaseEntityOutput *ev = (CBaseEntityOutput*)fieldInfo.pField;
		ev->ParseEventAction( szValue );
		return true;
	}
};

CEventsSaveDataOps g_EventsSaveDataOps;
ISaveRestoreOps *eventFuncs = &g_EventsSaveDataOps;

//-----------------------------------------------------------------------------
//			CMultiInputVar implementation
//
// Purpose: holds a list of inputs and their ID tags
//			used for entities that hold inputs from a set of other entities
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: destructor, frees the data list
//-----------------------------------------------------------------------------
CMultiInputVar::~CMultiInputVar()
{
	if ( m_InputList )
	{
		while ( m_InputList->next != nullptr )
		{
			inputitem_t *input = m_InputList->next;
			m_InputList->next = input->next;
			delete input;
		}
		delete m_InputList;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the data set with a new value
// Input  : newVal - the new value to add to or update in the list
//			outputID - the source of the value
//-----------------------------------------------------------------------------
void CMultiInputVar::AddValue( variant_t newVal, int outputID )
{
	// see if it's already in the list
	inputitem_t *inp;
	for ( inp = m_InputList; inp != nullptr; inp = inp->next )
	{
		// already in list, so just update this link
		if ( inp->outputID == outputID )
		{
			inp->value = newVal;
			return;
		}
	}

	// add to start of list
	inp = new inputitem_t;
	inp->value = newVal;
	inp->outputID = outputID;
	if ( !m_InputList )
	{
		m_InputList = inp;
		inp->next = nullptr;
	}
	else
	{
		inp->next = m_InputList;
		m_InputList = inp;
	}
}

//-----------------------------------------------------------------------------
// Purpose: allocates memory from the entitylist pool
//-----------------------------------------------------------------------------
void *CMultiInputVar::inputitem_t::operator new( std::size_t stAllocateBlock )
{
	return g_EntityListPool.alloc( stAllocateBlock );
}

void *CMultiInputVar::inputitem_t::operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntityListPool.alloc( stAllocateBlock );
}

//-----------------------------------------------------------------------------
// Purpose: frees memory from the entitylist pool
//-----------------------------------------------------------------------------
void CMultiInputVar::inputitem_t::operator delete( void *pMem )
{
	g_EntityListPool.free( pMem );
}

//-----------------------------------------------------------------------------
//			CEventQueue implementation
//
// Purpose: holds and executes a global prioritized queue of entity actions
//-----------------------------------------------------------------------------
ks::fixed_pool EventQueuePrioritizedEvent_t::s_Allocator( sizeof(EventQueuePrioritizedEvent_t), 128, ks::fixed_pool::grow::slow, "EventQueuePrioritizedEvent_t pool", alignof(EventQueuePrioritizedEvent_t) );

CEventQueue g_EventQueue;

CEventQueue::CEventQueue()
{
	m_Events.m_flFireTime = -std::numeric_limits<float>::max();
	m_Events.m_pNext = nullptr;

	Init();
}

CEventQueue::~CEventQueue()
{
	Clear();
}

// Robin: Left here for backwards compatability.
class CEventQueueSaveLoadProxy : public CLogicalEntity
{
	DECLARE_CLASS( CEventQueueSaveLoadProxy, CLogicalEntity );

};

LINK_ENTITY_TO_CLASS(event_queue_saveload_proxy, CEventQueueSaveLoadProxy);

//-----------------------------------------------------------------------------
// EVENT QUEUE SAVE / RESTORE
//-----------------------------------------------------------------------------
static short EVENTQUEUE_SAVE_RESTORE_VERSION = 1;

//-----------------------------------------------------------------------------

//-------------------------------------

void CEventQueue::Init( void )
{
	Clear();
}

void CEventQueue::Clear( void )
{
	// delete all the events in the queue
	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;
	
	while ( pe != nullptr )
	{
		EventQueuePrioritizedEvent_t *next = pe->m_pNext;
		delete pe;
		pe = next;
	}

	m_Events.m_pNext = nullptr;
}

void CEventQueue::Dump( void )
{
	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;

	Msg("Dumping event queue. Current time is: %.2f\n", gpGlobals->curtime );

	while ( pe != nullptr )
	{
		EventQueuePrioritizedEvent_t *next = pe->m_pNext;

		Msg("   (%.2f) Target: '%s', Input: '%s', Parameter '%s'. Activator: '%s', Caller '%s'.  \n", 
			pe->m_flFireTime, 
			STRING(pe->m_iTarget), 
			STRING(pe->m_iTargetInput), 
			pe->m_VariantValue.String(),
			pe->m_pActivator ? pe->m_pActivator->GetDebugName() : "None", 
			pe->m_pCaller ? pe->m_pCaller->GetDebugName() : "None"  );

		pe = next;
	}

	Msg("Finished dump.\n");
}

//-----------------------------------------------------------------------------
// Purpose: adds the action into the correct spot in the priority queue, targeting entity via string name
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( const char *target, const char *targetInput, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	// build the new event
	EventQueuePrioritizedEvent_t *newEvent = new EventQueuePrioritizedEvent_t;
	newEvent->m_flFireTime = gpGlobals->curtime + fireDelay;	// priority key in the priority queue
	newEvent->m_iTarget = MAKE_STRING( target );
	newEvent->m_pEntTarget = nullptr;
	newEvent->m_iTargetInput = MAKE_STRING( targetInput );
	newEvent->m_pActivator = pActivator;
	newEvent->m_pCaller = pCaller;
	newEvent->m_VariantValue = Value;
	newEvent->m_iOutputID = outputID;

	AddEvent( newEvent );
}

//-----------------------------------------------------------------------------
// Purpose: adds the action into the correct spot in the priority queue, targeting entity via pointer
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( CBaseEntity *target, const char *targetInput, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	// build the new event
	EventQueuePrioritizedEvent_t *newEvent = new EventQueuePrioritizedEvent_t;
	newEvent->m_flFireTime = gpGlobals->curtime + fireDelay;	// primary priority key in the priority queue
	newEvent->m_iTarget = NULL_STRING;
	newEvent->m_pEntTarget = target;
	newEvent->m_iTargetInput = MAKE_STRING( targetInput );
	newEvent->m_pActivator = pActivator;
	newEvent->m_pCaller = pCaller;
	newEvent->m_VariantValue = Value;
	newEvent->m_iOutputID = outputID;

	AddEvent( newEvent );
}

void CEventQueue::AddEvent( CBaseEntity *target, const char *action, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	variant_t Value;
	Value.Set( FIELD_VOID, nullptr );
	AddEvent( target, action, Value, fireDelay, pActivator, pCaller, outputID );
}

//-----------------------------------------------------------------------------
// Purpose: private function, adds an event into the list
// Input  : *newEvent - the (already built) event to add
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( EventQueuePrioritizedEvent_t *newEvent )
{
	// loop through the actions looking for a place to insert
	EventQueuePrioritizedEvent_t *pe;
	for ( pe = &m_Events; pe->m_pNext != nullptr; pe = pe->m_pNext )
	{
		if ( pe->m_pNext->m_flFireTime > newEvent->m_flFireTime )
		{
			break;
		}
	}

	contract_assert( pe );

	// insert
	newEvent->m_pNext = pe->m_pNext;
	newEvent->m_pPrev = pe;
	pe->m_pNext = newEvent;
	if ( newEvent->m_pNext )
	{
		newEvent->m_pNext->m_pPrev = newEvent;
	}
}

void CEventQueue::RemoveEvent( EventQueuePrioritizedEvent_t *pe )
{
	contract_assert( pe->m_pPrev );
	pe->m_pPrev->m_pNext = pe->m_pNext;
	if ( pe->m_pNext )
	{
		pe->m_pNext->m_pPrev = pe->m_pPrev;
	}
}

//-----------------------------------------------------------------------------
// Purpose: fires off any events in the queue who's fire time is (or before) the present time
//-----------------------------------------------------------------------------
void CEventQueue::ServiceEvents( void )
{
	if (!CBaseEntity::Debug_ShouldStep())
	{
		return;
	}

	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;

	while ( pe != nullptr && pe->m_flFireTime <= gpGlobals->curtime )
	{
		MDLCACHE_CRITICAL_SECTION();

		bool targetFound = false;

		// find the targets
		if ( pe->m_iTarget != NULL_STRING )
		{
			// In the context the event, the searching entity is also the caller
			CBaseEntity *pSearchingEntity = pe->m_pCaller;
			CBaseEntity *target = nullptr;
			while ( 1 )
			{
				target = gEntList.FindEntityByName( target, pe->m_iTarget, pSearchingEntity, pe->m_pActivator, pe->m_pCaller );
				if ( !target )
					break;

				// pump the action into the target
				target->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
				targetFound = true;
			}
		}

		// direct pointer
		if ( pe->m_pEntTarget != nullptr )
		{
			pe->m_pEntTarget->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
			targetFound = true;
		}

		if ( !targetFound )
		{
			// See if we can find a target if we treat the target as a classname
			if ( pe->m_iTarget != NULL_STRING )
			{
				CBaseEntity *target = nullptr;
				while ( 1 )
				{
					target = gEntList.FindEntityByClassname( target, STRING(pe->m_iTarget) );
					if ( !target )
						break;

					// pump the action into the target
					target->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
					targetFound = true;
				}
			}
		}

		if ( !targetFound )
		{
			const char *pClass ="", *pName = "";
			
			// might be NULL
			if ( pe->m_pCaller )
			{
				pClass = STRING(pe->m_pCaller->m_iClassname);
				pName = STRING(pe->m_pCaller->GetEntityName());
			}
			
			char szBuffer[256];
			V_snprintf( szBuffer, sizeof(szBuffer), "unhandled input: (%s) -> (%s), from (%s,%s); target entity not found\n", STRING(pe->m_iTargetInput), STRING(pe->m_iTarget), pClass, pName );
			DevMsg( 2, "%s", szBuffer );
			ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
		}

		// remove the event from the list (remembering that the queue may have been added to)
		RemoveEvent( pe );
		delete pe;

		//
		// If we are in debug mode, exit the loop if we have fired the correct number of events.
		//
		if (CBaseEntity::Debug_IsPaused())
		{
			if (!CBaseEntity::Debug_Step())
			{
				break;
			}
		}

		// restart the list (to catch any new items have probably been added to the queue)
		pe = m_Events.m_pNext;	
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dumps the contents of the Entity I/O event queue to the console.
//-----------------------------------------------------------------------------
void CC_DumpEventQueue()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_EventQueue.Dump();
}
static ConCommand dumpeventqueue( side_registry(), "dumpeventqueue", CC_DumpEventQueue, "Dump the contents of the Entity I/O event queue to the console." );

//-----------------------------------------------------------------------------
// Purpose: Removes all pending events from the I/O queue that were added by the
//			given caller.
//
//			TODO: This is only as reliable as callers are in passing the correct
//				  caller pointer when they fire the outputs. Make more foolproof.
//-----------------------------------------------------------------------------
void CEventQueue::CancelEvents( CBaseEntity *pCaller )
{
	if (!pCaller)
		return;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != nullptr)
	{
		bool bDelete = false;
		if (pCur->m_pCaller == pCaller)
		{
			// Pointers match; make sure everything else matches.
			if (!stricmp(STRING(pCur->m_pCaller->GetEntityName()), STRING(pCaller->GetEntityName())) &&
				!stricmp(pCur->m_pCaller->GetClassname(), pCaller->GetClassname()))
			{
				// Found a matching event; delete it from the queue.
				bDelete = true;
			}
		}

		EventQueuePrioritizedEvent_t *pCurSave = pCur;
		pCur = pCur->m_pNext;

		if (bDelete)
		{
			RemoveEvent( pCurSave );
			delete pCurSave;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all pending events of the specified type from the I/O queue of the specified target
//
//			TODO: This is only as reliable as callers are in passing the correct
//				  caller pointer when they fire the outputs. Make more foolproof.
//-----------------------------------------------------------------------------
void CEventQueue::CancelEventOn( CBaseEntity *pTarget, const char *sInputName )
{
	if (!pTarget)
		return;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != nullptr)
	{
		bool bDelete = false;
		if (pCur->m_pEntTarget == pTarget)
		{
			if ( StringHasPrefixCaseSensitive( STRING(pCur->m_iTargetInput), sInputName ) )
			{
				// Found a matching event; delete it from the queue.
				bDelete = true;
			}
		}

		EventQueuePrioritizedEvent_t *pCurSave = pCur;
		pCur = pCur->m_pNext;

		if (bDelete)
		{
			RemoveEvent( pCurSave );
			delete pCurSave;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the target has any pending inputs.
// Input  : *pTarget - 
//			*sInputName - NULL for any input, or a specified one
//-----------------------------------------------------------------------------
bool CEventQueue::HasEventPending( CBaseEntity *pTarget, const char *sInputName )
{
	if (!pTarget)
		return false;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != nullptr)
	{
		if (pCur->m_pEntTarget == pTarget)
		{
			if ( !sInputName )
				return true;

			if ( StringHasPrefixCaseSensitive( STRING(pCur->m_iTargetInput), sInputName ) )
				return true;
		}

		pCur = pCur->m_pNext;
	}

	return false;
}

void ServiceEventQueue( void )
{
	CVProfScope _( "ServiceEventQueue()", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );

	g_EventQueue.ServiceEvents();
}

// save data description for the event queue
IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CEventQueue )

// save data for a single event in the queue
IMPLEMENT_REFLECT_DATAMAP_SIMPLE( EventQueuePrioritizedEvent_t )

////////////////////////// variant_t implementation //////////////////////////

// BUGBUG: Add support for function pointer save/restore to variants
// BUGBUG: Must pass datamap_t to read/write fields 
void variant_t::Set( fieldtype_t ftype, void *data )
{
	fieldType = ftype;

	switch ( ftype )
	{
	case FIELD_BOOLEAN:		bVal = *((bool *)data);				break;
	case FIELD_CHARACTER:	iVal = *((char *)data);				break;
	case FIELD_SHORT:		iVal = *((short *)data);			break;
	case FIELD_INTEGER:		iVal = *((int *)data);				break;
	case FIELD_STRING:		iszVal = *((string_t *)data);		break;
	case FIELD_FLOAT:		flVal = *((float *)data);			break;
	case FIELD_COLOR32:		rgbaVal = *((color32 *)data);		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		vecVal[0] = ((float *)data)[0];
		vecVal[1] = ((float *)data)[1];
		vecVal[2] = ((float *)data)[2];
		break;
	}

	case FIELD_EHANDLE:		eVal = *((EHANDLE *)data);			break;
	case FIELD_CLASSPTR:	eVal = *((CBaseEntity **)data);		break;
	case FIELD_VOID:		
	default:
		iVal = 0; fieldType = FIELD_VOID;	
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies the value in the variant into a block of memory
// Input  : *data - the block to write into
//-----------------------------------------------------------------------------
void variant_t::SetOther( void *data )
{
	switch ( fieldType )
	{
	case FIELD_BOOLEAN:		*((bool *)data) = bVal != 0;		break;
	case FIELD_CHARACTER:	*((char *)data) = iVal;				break;
	case FIELD_SHORT:		*((short *)data) = iVal;			break;
	case FIELD_INTEGER:		*((int *)data) = iVal;				break;
	case FIELD_STRING:		*((string_t *)data) = iszVal;		break;
	case FIELD_FLOAT:		*((float *)data) = flVal;			break;
	case FIELD_COLOR32:		*((color32 *)data) = rgbaVal;		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		((float *)data)[0] = vecVal[0];
		((float *)data)[1] = vecVal[1];
		((float *)data)[2] = vecVal[2];
		break;
	}

	case FIELD_EHANDLE:		*((EHANDLE *)data) = eVal;			break;
	case FIELD_CLASSPTR:	*((CBaseEntity **)data) = eVal;		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Converts the variant to a new type. This function defines which I/O
//			types can be automatically converted between. Connections that require
//			an unsupported conversion will cause an error message at runtime.
// Input  : newType - the type to convert to
// Output : Returns true on success, false if the conversion is not legal
//-----------------------------------------------------------------------------
bool variant_t::Convert( fieldtype_t newType )
{
	if ( newType == fieldType )
	{
		return true;
	}

	//
	// Converting to a null value is easy.
	//
	if ( newType == FIELD_VOID )
	{
		Set( FIELD_VOID, nullptr );
		return true;
	}

	//
	// FIELD_INPUT accepts the variant type directly.
	//
	if ( newType == FIELD_INPUT )
	{
		return true;
	}

	switch ( fieldType )
	{
		case FIELD_INTEGER:
		{
			switch ( newType )
			{
				case FIELD_FLOAT:
				{
					SetFloat( (float) iVal );
					return true;
				}

				case FIELD_BOOLEAN:
				{
					SetBool( iVal != 0 );
					return true;
				}
			}
			break;
		}

		case FIELD_FLOAT:
		{
			switch ( newType )
			{
				case FIELD_INTEGER:
				{
					SetInt( (int) flVal );
					return true;
				}

				case FIELD_BOOLEAN:
				{
					SetBool( flVal != 0 );
					return true;
				}
			}
			break;
		}

		//
		// Everyone must convert from FIELD_STRING if possible, since
		// parameter overrides are always passed as strings.
		//
		case FIELD_STRING:
		{
			switch ( newType )
			{
				case FIELD_INTEGER:
				{
					if (iszVal != NULL_STRING)
					{
						SetInt(std::atoi(STRING(iszVal)));
					}
					else
					{
						SetInt(0);
					}
					return true;
				}

				case FIELD_FLOAT:
				{
					if (iszVal != NULL_STRING)
					{
						SetFloat(std::atof(STRING(iszVal)));
					}
					else
					{
						SetFloat(0);
					}
					return true;
				}

				case FIELD_BOOLEAN:
				{
					if (iszVal != NULL_STRING)
					{
						SetBool( std::atoi(STRING(iszVal)) != 0 );
					}
					else
					{
						SetBool(false);
					}
					return true;
				}

				case FIELD_VECTOR:
				{
					Vector tmpVec = vec3_origin;
					if (std::sscanf(STRING(iszVal), "[%f %f %f]", &tmpVec[0], &tmpVec[1], &tmpVec[2]) == 0)
					{
						// Try sucking out 3 floats with no []s
						sscanf(STRING(iszVal), "%f %f %f", &tmpVec[0], &tmpVec[1], &tmpVec[2]);
					}
					SetVector3D( tmpVec );
					return true;
				}

				case FIELD_COLOR32:
				{
					int nRed = 0;
					int nGreen = 0;
					int nBlue = 0;
					int nAlpha = 255;

					std::sscanf(STRING(iszVal), "%d %d %d %d", &nRed, &nGreen, &nBlue, &nAlpha);
					SetColor32( nRed, nGreen, nBlue, nAlpha );
					return true;
				}

				case FIELD_EHANDLE:
				{
					// convert the string to an entity by locating it by classname
					CBaseEntity *ent = nullptr;
					if ( iszVal != NULL_STRING )
					{
						// FIXME: do we need to pass an activator in here?
						ent = gEntList.FindEntityByName( nullptr, iszVal );
					}
					SetEntity( ent );
					return true;
				}
			}
		
			break;
		}

		case FIELD_EHANDLE:
		{
			switch ( newType )
			{
				case FIELD_STRING:
				{
					// take the entities targetname as the string
					string_t iszStr = NULL_STRING;
					if ( eVal != nullptr )
					{
						SetString( eVal->GetEntityName() );
					}
					return true;
				}
			}
			break;
		}
	}

	// invalid conversion
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: All types must be able to display as strings for debugging purposes.
// Output : Returns a pointer to the string that represents this value.
//
//			NOTE: The returned pointer should not be stored by the caller as
//				  subsequent calls to this function will overwrite the contents
//				  of the buffer!
//-----------------------------------------------------------------------------
const char *variant_t::ToString( void ) const
{
	static char szBuf[512];

	switch (fieldType)
	{
	case FIELD_STRING:
		{
			return(STRING(iszVal));
		}

	case FIELD_BOOLEAN:
		{
			if (bVal == 0)
			{
				V_strncpy(szBuf, "false",sizeof(szBuf));
			}
			else
			{
				V_strncpy(szBuf, "true",sizeof(szBuf));
			}
			return(szBuf);
		}

	case FIELD_INTEGER:
		{
			V_snprintf( szBuf, sizeof( szBuf ), "%i", iVal );
			return(szBuf);
		}

	case FIELD_FLOAT:
		{
			V_snprintf(szBuf,sizeof(szBuf), "%g", flVal);
			return(szBuf);
		}

	case FIELD_COLOR32:
		{
			V_snprintf(szBuf,sizeof(szBuf), "%d %d %d %d", (int)rgbaVal.r, (int)rgbaVal.g, (int)rgbaVal.b, (int)rgbaVal.a);
			return(szBuf);
		}

	case FIELD_VECTOR:
		{
			V_snprintf(szBuf,sizeof(szBuf), "[%g %g %g]", (double)vecVal[0], (double)vecVal[1], (double)vecVal[2]);
			return(szBuf);
		}

	case FIELD_VOID:
		{
			szBuf[0] = '\0';
			return(szBuf);
		}

	case FIELD_EHANDLE:
		{
			const char *pszName = (Entity()) ? STRING(Entity()->GetEntityName()) : "<<null entity>>";
			V_strncpy( szBuf, pszName, 512 );
			return (szBuf);
		}
	}

	return("No conversion to string");
}

/////////////////////// entitylist /////////////////////

ks::fixed_pool g_EntListMemPool( sizeof(entitem_t), 256, ks::fixed_pool::grow::none, "g_EntListMemPool", alignof(entitem_t) );

void *entitem_t::operator new( std::size_t stAllocateBlock )
{
	return g_EntListMemPool.alloc( stAllocateBlock );
}

void *entitem_t::operator new( std::size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntListMemPool.alloc( stAllocateBlock );
}

void entitem_t::operator delete( void *pMem )
{
	g_EntListMemPool.free( pMem );
}

CEntityList::CEntityList()
{
	m_pItemList = nullptr;
	m_iNumItems = 0;
}

CEntityList::~CEntityList()
{
	// remove all items from the list
	entitem_t *next, *e = m_pItemList;
	while ( e != nullptr )
	{
		next = e->pNext;
		delete e;
		e = next;
	}
	m_pItemList = nullptr;
}

void CEntityList::AddEntity( CBaseEntity *pEnt )
{
	// check if it's already in the list; if not, add it
	entitem_t *e = m_pItemList;
	while ( e != nullptr )
	{
		if ( e->hEnt == pEnt )
		{
			// it's already in the list
			return;
		}

		if ( e->pNext == nullptr )
		{
			// we've hit the end of the list, so tack it on
			e->pNext = new entitem_t;
			e->pNext->hEnt = pEnt;
			e->pNext->pNext = nullptr;
			m_iNumItems++;
			return;
		}

		e = e->pNext;
	}
	
	// empty list
	m_pItemList = new entitem_t;
	m_pItemList->hEnt = pEnt;
	m_pItemList->pNext = nullptr;
	m_iNumItems = 1;
}

void CEntityList::DeleteEntity( CBaseEntity *pEnt )
{
	// find the entry in the list and delete it
	entitem_t *prev = nullptr, *e = m_pItemList;
	while ( e != nullptr )
	{
		// delete the link if it's the matching entity OR if the link is NULL
		if ( e->hEnt == pEnt || e->hEnt == nullptr )
		{
			if ( prev )
			{
				prev->pNext = e->pNext;
			}
			else
			{
				m_pItemList = e->pNext;
			}

			delete e;
			m_iNumItems--;

			// REVISIT: Is this correct?  Is this just here to clean out dead EHANDLEs?
			// restart the loop
			e = m_pItemList;
			prev = nullptr;
			continue;
		}

		prev = e;
		e = e->pNext;
	}
}

class CBaseEntity;
class CTakeDamageInfo;

using namespace ::ks::reflect;

// Purpose: Base class for simple projectiles that use studio models

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// This file was made before I
// realized Portal had already
// made the same thing 
// (probably because it was 
// squirreled away in the PORTAL
// source folder). Anyway, this
// needs to be reconciled with that
// file since clearly there is
// demand for this feature (sjb) 11/27/07
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

LINK_ENTITY_TO_CLASS( baseprojectile, CBaseProjectile );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Spawn(	char *pszModel,
								  const Vector &vecOrigin,
								  const Vector &vecVelocity,
								  edict_t *pOwner,
								  MoveType_t	iMovetype,
								  MoveCollide_t nMoveCollide,
								  int	iDamage,
								  int iDamageType,
								  CBaseEntity *pIntendedTarget )
{
	Precache();

	SetModel( pszModel );

	m_iDmg = iDamage;
	m_iDmgType = iDamageType;

	SetMoveType( iMovetype, nMoveCollide );
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );

	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	UTIL_SetOrigin( this, vecOrigin );
	SetAbsVelocity( vecVelocity );

	SetOwnerEntity( Instance( pOwner ) );

	m_hIntendedTarget.Set( pIntendedTarget );

	QAngle angles;

	VectorAngles( vecVelocity, angles );

	SetAbsAngles( angles );

	// Call think for free the first time. It's up to derived classes to rethink.
	SetNextThink( gpGlobals->curtime );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Touch_Impl( CBaseEntity *pOther )
{
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER) )
	{
			return;
	}

	HandleTouch( pOther );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::HandleTouch( CBaseEntity *pOther )
{
	CBaseEntity *pOwner;

	pOwner = GetOwnerEntity();

	if( !pOwner )
	{
		pOwner = this;
	}

	trace_t	tr;
	tr = BaseClass::GetTouchTrace( );

	CTakeDamageInfo info( this, pOwner, m_iDmg, m_iDmgType );
	GuessDamageForce( &info, (tr.endpos - tr.startpos), tr.endpos );
	pOther->TakeDamage( info );

	UTIL_Remove( this );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::Think_Impl()
{
	HandleThink();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CBaseProjectile::HandleThink()
{
}

class CBaseCombatWeapon;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CEconQuestDefinition;
class CEffectData;
class CTakeDamageInfo;
class CTeam;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: client/server game specific stuff

extern int giPrecacheGrunt;

ConVar  *sv_cheats = nullptr;
static ConVar tv_relaytextchat( side_registry(), "tv_relaytextchat", "1", FCVAR_RELEASE, "Relay text chat data: 0=off, 1=say, 2=say+say_team" );

void ClientKill( edict_t *pEdict, const Vector &vecForce, bool bExplode = false )
{
	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( GetContainingEntity( pEdict ) );
	pPlayer->CommitSuicide( vecForce, bExplode );
}

char * CheckChatText( CBasePlayer *pPlayer, char *text )
{
	char *p = text;

	// invalid if NULL or empty
	if ( !text || !text[0] )
		return nullptr;

	int length = V_strlen( text );

	// remove quotes (leading & trailing) if present
	if (*p == '"')
	{
		p++;
		length -=2;
		p[length] = 0;
	}

	// cut off after 127 chars
	if ( length > 127 )
		text[127] = 0;

	GameRules()->CheckChatText( pPlayer, p );

	return p;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEdict, const CCommand &args, bool teamonly )
{
	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[256];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = args[0];
	bool bSenderDead = false;

	// We can get a raw string now, without the "say " prepended
	if ( args.ArgC() == 0 )
		return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( args.ArgC() >= 2 )
		{
			p = (char *)args.ArgS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( args.ArgC() >= 2 )
		{
			V_snprintf( szTemp,sizeof(szTemp), "%s %s", ( char * )pcmd, (char *)args.ArgS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			V_snprintf( szTemp,sizeof(szTemp), "%s", ( char * )pcmd );
		}
		p = szTemp;
	}

	CBasePlayer *pPlayer = nullptr;
	if ( pEdict )
	{
		pPlayer = ((CBasePlayer *)CBaseEntity::Instance( pEdict ));
		contract_assert( pPlayer );

		// make sure the text has valid content
		p = CheckChatText( pPlayer, p );
	}

	if ( !p )
		return;

	if ( pEdict )
	{
		if ( !pPlayer->CanSpeak() )
			return;

		// See if the player wants to modify of check the text
		pPlayer->CheckChatText( p, 127 );	// though the buffer szTemp that p points to is 256, 
											// chat text is capped to 127 in CheckChatText above

		// make sure the text has valid content
		p = CheckChatText( pPlayer, p );

		if ( !p )
			return;

		contract_assert( std::strlen( pPlayer->GetPlayerName() ) > 0 );

		bSenderDead = ( pPlayer->m_lifeState != LIFE_ALIVE );
	}
	else
	{
		bSenderDead = false;
	}

	const char *pszFormat = nullptr;
	const char *pszPrefix = nullptr;
	const char *pszLocation = nullptr;
	if ( g_pGameRules )
	{
		pszFormat = g_pGameRules->GetChatFormat( teamonly, pPlayer );
		pszPrefix = g_pGameRules->GetChatPrefix( teamonly, pPlayer );	
		pszLocation = g_pGameRules->GetChatLocation( teamonly, pPlayer );
	}

	const char *pszPlayerName = pPlayer ? pPlayer->GetPlayerName():"Console";

	if ( pszPrefix && std::strlen( pszPrefix ) > 0 )
	{
		if ( pszLocation && strlen( pszLocation ) )
		{
			V_snprintf( text, sizeof(text), "%s %s @ %s: ", pszPrefix, pszPlayerName, pszLocation );
		}
		else
		{
			V_snprintf( text, sizeof(text), "%s %s: ", pszPrefix, pszPlayerName );
		}
	}
	else
	{
		V_snprintf( text, sizeof(text), "%s: ", pszPlayerName );
	}

	j = sizeof(text) - 2 - std::strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	V_strncat( text, p, sizeof( text ), COPY_ALL_CHARACTERS );
	V_strncat( text, "\n", sizeof( text ), COPY_ALL_CHARACTERS );
 
	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = nullptr;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		client = ToBaseMultiplayerPlayer( UTIL_PlayerByIndex( i ) );
		if ( !client || !client->edict() )
			continue;
		
		if ( client->edict() == pEdict )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		if ( client->IsHLTV() )
		{
			if ( !tv_relaytextchat.GetInt() )
				continue; // chat is not relayed to TV
			else if ( teamonly && ( tv_relaytextchat.GetInt() < 2 ) )
				continue; // team-only chat, in mode that TV doesn't relay
		}
		else
		{
			if ( pPlayer && !g_pGameRules->PlayerCanHearChat( client, pPlayer, teamonly ) )
				continue;

			if ( pPlayer && !client->CanHearAndReadChatFrom( pPlayer ) )
				continue;

			if ( pPlayer && GetVoiceGameMgr() && GetVoiceGameMgr()->IsPlayerIgnoringPlayer( pPlayer->entindex(), i ) )
				continue;
		}

		CSingleUserRecipientFilter user( client );
		user.MakeReliable();

		if ( pszFormat )
		{
			UTIL_SayText2Filter( user, pPlayer,
				teamonly ? kEUtilSayTextMessageType_TeamonlyChat : kEUtilSayTextMessageType_AllChat,
				pszFormat, pszPlayerName, p, pszLocation );
		}
		else
		{
			UTIL_SayTextFilter( user, text, pPlayer,
				teamonly ? kEUtilSayTextMessageType_TeamonlyChat : kEUtilSayTextMessageType_AllChat );
		}
	}

	if ( pPlayer )
	{
		// print to the sending client
		CSingleUserRecipientFilter user( pPlayer );
		user.MakeReliable();

		if ( pszFormat )
		{
			UTIL_SayText2Filter( user, pPlayer,
				teamonly ? kEUtilSayTextMessageType_TeamonlyChat : kEUtilSayTextMessageType_AllChat,
				pszFormat, pszPlayerName, p, pszLocation );
		}
		else
		{
			UTIL_SayTextFilter( user, text, pPlayer,
				teamonly ? kEUtilSayTextMessageType_TeamonlyChat : kEUtilSayTextMessageType_AllChat );
		}
	}

	// echo to server console
	// Adrian: Only do this if we're running a dedicated server since we already print to console on the client.
	if ( engine->IsDedicatedServer() )
		 Msg( "%s", text );

	contract_assert( p );

	int userid = 0;
	const char *networkID = "Console";
	const char *playerName = "Console";
	const char *playerTeam = "Console";
	if ( pPlayer )
	{
		userid = pPlayer->GetUserID();
		networkID = pPlayer->GetNetworkIDString();
		playerName = pPlayer->GetPlayerName();
		CTeam *team = pPlayer->GetTeam();
		if ( team )
		{
			playerTeam = team->GetName();
		}
	}
		
	if ( teamonly )
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say_team \"%s\"\n", playerName, userid, networkID, playerTeam, p );
	else
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" say \"%s\"\n", playerName, userid, networkID, playerTeam, p );

// 	Vitaliy 1/24/2013 -- converting text chat and voice chat to encrypted data
// 	for TV implies that no unencrypted messages would contain the same text/voice.
// 	Even though nothing in CS:GO codebase listens for "player_say" event there
//	are plugins that might want it. Broadcast the event if we are not encrypting
//	text data (3/22/2013)
	static ConVarRef tv_encryptdata_key( "tv_encryptdata_key" );
	static ConVarRef tv_encryptdata_key_pub( "tv_encryptdata_key_pub" );
	if ( *tv_encryptdata_key.GetString() || *tv_encryptdata_key_pub.GetString() )
	{
		// Do nothing, since we are encrypting text
	}
	else if ( IGameEvent * event = gameeventmanager->CreateEvent( "player_say" ) )	// will be null if there are no listeners!
	{
		event->SetInt("userid", userid );
		event->SetString("text", p );
		event->SetInt( "priority", 1 );	// player_say
		gameeventmanager->FireEvent( event );
	}
}

PRECACHE_REGISTER_BEGIN( GLOBAL, ClientPrecache )
	// Precache cable textures.
	PRECACHE( MODEL, "cable/phonecable.vmt" )
	PRECACHE( MODEL, "cable/phonecable_red.vmt" )
	PRECACHE( MODEL, "cable/cable.vmt" )
	PRECACHE( MODEL, "cable/cable_lit.vmt" )
	PRECACHE( MODEL, "cable/chain.vmt" )
	PRECACHE( MODEL, "cable/rope.vmt" )
	PRECACHE( MODEL, "sprites/blueglow1.vmt" )
	PRECACHE( MODEL, "sprites/purpleglow1.vmt" )
	PRECACHE( MODEL, "sprites/purplelaser1.vmt" )

	PRECACHE_CONDITIONAL( MODEL, "models/germangibs.mdl", g_Language.GetInt() == LANGUAGE_GERMAN )
	PRECACHE_CONDITIONAL( MODEL, "models/gibs/hgibs.mdl", g_Language.GetInt() != LANGUAGE_GERMAN )

	PRECACHE( GAMESOUND, "Error" )
	PRECACHE( GAMESOUND, "Hud.Hint" )
	PRECACHE( GAMESOUND, "Player.FallDamage" )
	PRECACHE( GAMESOUND, "Player.Swim" )

	// General HUD sounds
	PRECACHE( GAMESOUND, "Player.PickupWeapon" )
	PRECACHE( GAMESOUND, "Player.DenyWeaponSelection" )
	PRECACHE( GAMESOUND, "Player.WeaponSelected" )
	PRECACHE( GAMESOUND, "Player.WeaponSelected_CT")
	PRECACHE( GAMESOUND, "Player.WeaponSelected_T")
	PRECACHE( GAMESOUND, "Player.WeaponSelectionClose" )
	PRECACHE( GAMESOUND, "Player.WeaponSelectionMoveSlot" )

	// General legacy temp ents sounds
	PRECACHE( GAMESOUND, "Bounce.Glass" )
	PRECACHE( GAMESOUND, "Bounce.Metal" )
	PRECACHE( GAMESOUND, "Bounce.Flesh" )
	PRECACHE( GAMESOUND, "Bounce.Wood" )
	PRECACHE( GAMESOUND, "Bounce.Shrapnel" )
	PRECACHE( GAMESOUND, "Bounce.ShotgunShell" )
	PRECACHE( GAMESOUND, "Bounce.Shell" )
	PRECACHE( GAMESOUND, "Bounce.Concrete" )

	PRECACHE( GAMESOUND, "BaseEntity.EnterWater" )
	PRECACHE( GAMESOUND, "BaseEntity.ExitWater" )

	// Game Instructor sounds
	PRECACHE( GAMESOUND, "Instructor.LessonStart" )
	PRECACHE( GAMESOUND, "Instructor.ImportantLessonStart" )
PRECACHE_REGISTER_END()

void ClientPrecache( void )
{
	ClientGamePrecache();

	// Force levels
	char pBuf[MAX_PATH];
	for ( int i = 0; i < CPU_LEVEL_PC_COUNT; ++i )
	{
		V_snprintf( pBuf, sizeof(pBuf), "cfg/cpu_level_%d_pc.ekv", i );
		engine->ForceExactFile( pBuf );
		V_snprintf( pBuf, sizeof(pBuf), "cfg/cpu_level_%d_pc_ss.ekv", i );
		engine->ForceExactFile( pBuf );
	}

	for ( int i = 0; i < GPU_LEVEL_PC_COUNT; ++i )
	{
		V_snprintf( pBuf, sizeof(pBuf), "cfg/gpu_level_%d_pc.ekv", i );
		engine->ForceExactFile( pBuf );
	}

	for ( int i = 0; i < MEM_LEVEL_PC_COUNT; ++i )
	{
		V_snprintf( pBuf, sizeof(pBuf), "cfg/mem_level_%d_pc.ekv", i );
		engine->ForceExactFile( pBuf );
	}

	for ( int i = 0; i < GPU_MEM_LEVEL_PC_COUNT; ++i )
	{
		V_snprintf( pBuf, sizeof(pBuf), "cfg/gpu_mem_level_%d_pc.ekv", i );
		engine->ForceExactFile( pBuf );
	}

	// Game Instructor lessons - don't want people making simple scripted wall hacks
	engine->ForceExactFile( "scripts/instructor_lessons.txt" );
	engine->ForceExactFile( "scripts/mod_lessons.txt" );
}

static void cast_ray( const CCommand &args );
static ConCommand cast_ray_command( side_registry(), "cast_ray", cast_ray, "Tests collision detection", FCVAR_CHEAT );
static void cast_ray( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	
	Vector forward;
	trace_t tr;

	pPlayer->EyeVectors( &forward );
	Vector start = pPlayer->EyePosition();
	UTIL_TraceLine(start, start + forward * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHit() )
	{
		DevMsg(1, "Hit %s\nposition %.2f, %.2f, %.2f\nangles %.2f, %.2f, %.2f\n", tr.Ent<CBaseEntity>()->GetClassname(),
			tr.Ent<CBaseEntity>()->GetAbsOrigin().x, tr.Ent<CBaseEntity>()->GetAbsOrigin().y, tr.Ent<CBaseEntity>()->GetAbsOrigin().z,
			tr.Ent<CBaseEntity>()->GetAbsAngles().x, tr.Ent<CBaseEntity>()->GetAbsAngles().y, tr.Ent<CBaseEntity>()->GetAbsAngles().z );
		DevMsg(1, "Hit: hitbox %d, hitgroup %d, physics bone %d, solid %d, surface %s, surfaceprop %s, contents %08x\n", tr.hitbox, tr.hitgroup, tr.physicsbone, tr.Ent<CBaseEntity>()->GetSolid(), tr.surface.name, physprops->GetPropName( tr.surface.surfaceProps ), tr.contents );
		NDebugOverlay::Line( start, tr.endpos, 0, 255, 0, false, 10 );
		NDebugOverlay::Line( tr.endpos, tr.endpos + tr.plane.normal * 12, 255, 255, 0, false, 10 );
	}
}

static void cast_hull( const CCommand &args );
static ConCommand cast_hull_command( side_registry(), "cast_hull", cast_hull, "Tests hull collision detection", FCVAR_CHEAT );
static void cast_hull( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	
	Vector forward;
	trace_t tr;

	Vector extents;
	extents.Init(16,16,16);
	pPlayer->EyeVectors( &forward );
	Vector start = pPlayer->EyePosition();
	UTIL_TraceHull(start, start + forward * MAX_COORD_RANGE, -extents, extents, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHit() )
	{
		DevMsg(1, "Hit %s\nposition %.2f, %.2f, %.2f\nangles %.2f, %.2f, %.2f\n", tr.Ent<CBaseEntity>()->GetClassname(),
			tr.Ent<CBaseEntity>()->GetAbsOrigin().x, tr.Ent<CBaseEntity>()->GetAbsOrigin().y, tr.Ent<CBaseEntity>()->GetAbsOrigin().z,
			tr.Ent<CBaseEntity>()->GetAbsAngles().x, tr.Ent<CBaseEntity>()->GetAbsAngles().y, tr.Ent<CBaseEntity>()->GetAbsAngles().z );
		DevMsg(1, "Hit: hitbox %d, hitgroup %d, physics bone %d, solid %d, surface %s, surfaceprop %s\n", tr.hitbox, tr.hitgroup, tr.physicsbone, tr.Ent<CBaseEntity>()->GetSolid(), tr.surface.name, physprops->GetPropName( tr.surface.surfaceProps ) );
		NDebugOverlay::SweptBox( start, tr.endpos, -extents, extents, vec3_angle, 0, 0, 255, 0, 10 );
		Vector end = tr.endpos;// - tr.plane.normal * DotProductAbs( tr.plane.normal, extents );
		NDebugOverlay::Line( end, end + tr.plane.normal * 24, 255, 255, 64, false, 10 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to find targets for ent_* commands
//			Without a name, returns the entity under the player's crosshair.
//			With a name it finds entities via name/classname/index
//-----------------------------------------------------------------------------
CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent )
{
	if ( !pPlayer )
		return nullptr;

	// If no name was given set bits based on the picked
	if (FStrEq(name,"")) 
	{
		// If we've already found an entity, return NULL. 
		// Makes it easier to write code using this func.
		if ( ent )
			return nullptr;

		return pPlayer ? pPlayer->FindPickerEntity() : nullptr;
	}

	int index = std::atoi( name );
	if ( index )
	{
		// If we've already found an entity, return NULL. 
		// Makes it easier to write code using this func.
		if ( ent )
			return nullptr;

		return CBaseEntity::Instance( index );
	}
		
	// Loop through all entities matching, starting from the specified previous
	while ( (ent = gEntList.NextEnt(ent)) != nullptr )
	{
		if (  (ent->GetEntityName() != NULL_STRING	&& ent->NameMatches(name))	|| 
			  (ent->m_iClassname != NULL_STRING && ent->ClassMatches(name)) )
		{
			return ent;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: called each time a player uses a "cmd" command
// Input  : pPlayer - the player who issued the command
//-----------------------------------------------------------------------------
void SetDebugBits( CBasePlayer* pPlayer, const char *name, int bit )
{
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = nullptr;
	while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != nullptr )
	{
		if (pEntity->m_debugOverlays & bit)
		{
			pEntity->m_debugOverlays &= ~bit;
		}
		else
		{
			pEntity->m_debugOverlays |= bit;

		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pKillTargetName - 
//-----------------------------------------------------------------------------
void KillTargets( const char *pKillTargetName )
{
	CBaseEntity *pentKillTarget = nullptr;

	DevMsg( 2, "KillTarget: %s\n", pKillTargetName );
	pentKillTarget = gEntList.FindEntityByName( nullptr, pKillTargetName );
	while ( pentKillTarget )
	{
		UTIL_Remove( pentKillTarget );

		DevMsg( 2, "killing %s\n", STRING( pentKillTarget->m_iClassname ) );
		pentKillTarget = gEntList.FindEntityByName( pentKillTarget, pKillTargetName );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void ConsoleKillTarget( CBasePlayer *pPlayer, const char *name )
{
	// If no name was given use the picker
	if (FStrEq(name,"")) 
	{
		CBaseEntity *pEntity = pPlayer ? pPlayer->FindPickerEntity() : nullptr;
		if ( pEntity )
		{
			UTIL_Remove( pEntity );
			Msg( "killing %s\n", pEntity->GetDebugName() );
			return;
		}
	}
	// Otherwise use name or classname
	KillTargets( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointClientCommand : public CPointEntity
{
public:
	DECLARE_CLASS( CPointClientCommand, CPointEntity );
	DECLARE_DATADESC();

	[[= ks::reflect::Input{ .name = "Command", .type = FIELD_STRING } ]] void InputCommand( inputdata_t& inputdata );
};

void CPointClientCommand::InputCommand( inputdata_t& inputdata )
{
	if ( !inputdata.value.String()[0] )
		return;

	edict_t *pClient = nullptr;
	if ( gpGlobals->maxClients == 1 )
	{
		pClient = INDEXENT( 1 );
	}
	else
	{
		// In multiplayer, send it back to the activator
		CBasePlayer *player = entity_cast<CBasePlayer>( inputdata.pActivator );
		if ( player )
		{
			pClient = player->edict();
		}
	}

	if ( !pClient || !pClient->GetUnknown() )
		return;

	engine->ClientCommand( pClient, "%s\n", inputdata.value.String() );
}

IMPLEMENT_REFLECT_DATAMAP( CPointClientCommand )

LINK_ENTITY_TO_CLASS( point_clientcommand, CPointClientCommand );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointServerCommand : public CPointEntity
{
public:
	DECLARE_CLASS( CPointServerCommand, CPointEntity );
	DECLARE_DATADESC();
	[[= ks::reflect::Input{ .name = "Command", .type = FIELD_STRING } ]] void InputCommand( inputdata_t& inputdata );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CPointServerCommand::InputCommand( inputdata_t& inputdata )
{
	if ( !inputdata.value.String()[0] )
		return;

	CBasePlayer *player = UTIL_GetListenServerHost();
	// if we're on a dedicated server or a non-listen server, only accept whitelisted commands
	if ( engine->IsDedicatedServer() || player == nullptr )
	{
		// Parse the text into distinct commands
		const char *pCurrentCommand = inputdata.value.String();
		int nOffsetToNextCommand;
		int	nLen = V_strlen( inputdata.value.String() );
		for( ; nLen > 0; nLen -= nOffsetToNextCommand+1, pCurrentCommand += nOffsetToNextCommand+1 )
		{
			// find a \n or ; line break
			int nCommandLength;
			UTIL_GetNextCommandLength( pCurrentCommand, nLen, &nCommandLength, &nOffsetToNextCommand );
			if ( nCommandLength <= 0 )
				continue;

			engine->ServerCommand( UTIL_VarArgs( "whitelistcmd %.*s\n", nCommandLength, pCurrentCommand ) );
		}
		return;
	}

	engine->ServerCommand( UTIL_VarArgs( "%s\n", inputdata.value.String() ) );
}

IMPLEMENT_REFLECT_DATAMAP( CPointServerCommand )

LINK_ENTITY_TO_CLASS( point_servercommand, CPointServerCommand );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointBroadcastClientCommand : public CPointEntity
{
public:
	DECLARE_CLASS( CPointBroadcastClientCommand, CPointEntity );
	DECLARE_DATADESC();
	[[= ks::reflect::Input{ .name = "Command", .type = FIELD_STRING } ]] void InputCommand( inputdata_t& inputdata );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CPointBroadcastClientCommand::InputCommand( inputdata_t& inputdata )
{
	if ( !inputdata.value.String()[0] )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pl = UTIL_PlayerByIndex( i );
		if ( !pl )
			continue;

		edict_t *pClient = pl->edict();
		if ( !pClient || !pClient->GetUnknown() )
			continue;

		engine->ClientCommand( pClient, "%s\n", inputdata.value.String() );
	}
}

IMPLEMENT_REFLECT_DATAMAP( CPointBroadcastClientCommand )

LINK_ENTITY_TO_CLASS( point_broadcastclientcommand, CPointBroadcastClientCommand );

//------------------------------------------------------------------------------
// Purpose : Draw a line betwen two points.  White if no world collisions, red if collisions
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_DrawLine( const CCommand &args )
{
	Vector startPos;
	Vector endPos;

	startPos.x = clamp( std::atof(args[1]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	startPos.y = clamp( atof(args[2]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	startPos.z = clamp( atof(args[3]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	endPos.x = clamp( atof(args[4]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	endPos.y = clamp( atof(args[5]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	endPos.z = clamp( atof(args[6]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );

	UTIL_AddDebugLine(startPos,endPos,true,true);
}
static ConCommand drawline(side_registry(), "drawline", CC_DrawLine, "Draws line between two 3D Points.\n\tGreen if no collision\n\tRed is collides with something\n\tArguments: x1 y1 z1 x2 y2 z2", FCVAR_CHEAT);

//------------------------------------------------------------------------------
// Purpose : Draw a cross at a points.  
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_DrawCross( const CCommand &args )
{
	Vector vPosition;

	vPosition.x = clamp( std::atof(args[1]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	vPosition.y = clamp( atof(args[2]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	vPosition.z = clamp( atof(args[3]), MIN_COORD_FLOAT, MAX_COORD_FLOAT );

	// Offset since min and max z in not about center
	Vector mins = Vector(-5,-5,-5);
	Vector maxs = Vector(5,5,5);

	Vector start = mins + vPosition;
	Vector end   = maxs + vPosition;
	UTIL_AddDebugLine(start,end,true,true);

	start.x += (maxs.x - mins.x);
	end.x	-= (maxs.x - mins.x);
	UTIL_AddDebugLine(start,end,true,true);

	start.y += (maxs.y - mins.y);
	end.y	-= (maxs.y - mins.y);
	UTIL_AddDebugLine(start,end,true,true);

	start.x -= (maxs.x - mins.x);
	end.x	+= (maxs.x - mins.x);
	UTIL_AddDebugLine(start,end,true,true);
}
static ConCommand drawcross(side_registry(), "drawcross", CC_DrawCross, "Draws a cross at the given location\n\tArguments: x y z", FCVAR_CHEAT);

//------------------------------------------------------------------------------
// helper function for kill and explode
//------------------------------------------------------------------------------
void kill_helper( const CCommand &args, bool bVector, bool bExplode )
{
	bool bKillOther = args.ArgC() > ( bVector ? 4 : 1 );

	CBasePlayer *pPlayer = nullptr;

	if ( bKillOther && sv_cheats->GetBool() )
	{
		// Find the matching netname
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToBasePlayer( UTIL_PlayerByIndex(i) );
			if ( pPlayer && V_strstr( pPlayer->GetPlayerName(), args[1] ) )
				break;
			pPlayer = nullptr;
		}
	}
	else
	{
		pPlayer = UTIL_GetCommandClient();
	}

	if ( !pPlayer || g_pGameRules->IgnorePlayerKillCommand() )
	{
		return;
	}

	// If we're doing global assassination targets, we have a known assassinate quest and the player who is the target is on the correct team
	// then don't let them suicide. 
	if ( CSGameRules() && CSGameRules()->GetActiveAssassinationQuest() )
	{
		CEconQuestDefinition *pQuest = CSGameRules()->GetActiveAssassinationQuest();
		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pQuest && pCSPlayer 
			&& ( int( pQuest->GetTargetTeam() ) == pPlayer->GetTeamNumber() )
			&& ( pCSPlayer->IsAssassinationTarget() ) )
			return;
	}

	if ( bVector && sv_cheats->GetBool() )
	{
		int i = bKillOther ? 2 : 1;
		Vector vecForce;
		vecForce.x = std::atof( args[i++] );
		vecForce.y = atof( args[i++] );
		vecForce.z = atof( args[i++] );

		pPlayer->CommitSuicide( vecForce, bExplode );
	}
	else
	{
		pPlayer->CommitSuicide( bExplode );
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void kill( const CCommand &args );
static ConCommand kill_command( side_registry(), "kill", kill, "Kills the player with generic damage", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void kill( const CCommand &args )
{
	kill_helper( args, false, false );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void explode( const CCommand &args );
static ConCommand explode_command( side_registry(), "explode", explode, "Kills the player with explosive damage", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void explode( const CCommand &args )
{
	kill_helper( args, false, true );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void killvector( const CCommand &args );
static ConCommand killvector_command( side_registry(), "killvector", killvector, "Kills a player applying force. Usage: killvector <player> <x value> <y value> <z value>", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void killvector( const CCommand &args )
{
	kill_helper( args, true, false );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void explodevector( const CCommand &args );
static ConCommand explodevector_command( side_registry(), "explodevector", explodevector, "Kills a player applying an explosive force. Usage: explodevector <player> <x value> <y value> <z value>", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void explodevector( const CCommand &args )
{
	kill_helper( args, true, true );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void buddha( const CCommand &args );
static ConCommand buddha_command( side_registry(), "buddha", buddha, "Toggle.  Player takes damage but won't die. (Shows red cross when health is zero)", FCVAR_CHEAT );
static void buddha( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer )
	{
		if (pPlayer->m_debugOverlays & OVERLAY_BUDDHA_MODE)
		{
			pPlayer->m_debugOverlays &= ~OVERLAY_BUDDHA_MODE;
			Msg("Buddha Mode off...\n");
		}
		else
		{
			pPlayer->m_debugOverlays |= OVERLAY_BUDDHA_MODE;
			Msg("Buddha Mode on...\n");
		}
	}
}

static constexpr auto TALK_INTERVAL = 0.66; // min time between say commands from a client
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void say( const CCommand &args );
static ConCommand say_command( side_registry(), "say", say, "Display player message", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void say( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer )
	{
		if (( pPlayer->LastTimePlayerTalked() + TALK_INTERVAL ) < gpGlobals->curtime) 
		{
			Host_Say( pPlayer->edict(), args, 0 );
			pPlayer->NotePlayerTalked();
		}
	}
	else if ( UTIL_IsCommandIssuedByServerAdmin() )
	{
		Host_Say( nullptr, args, 0 );
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void say_team( const CCommand &args );
static ConCommand say_team_command( side_registry(), "say_team", say_team, "Display player message to team", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void say_team( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if (pPlayer)
	{
		if (( pPlayer->LastTimePlayerTalked() + TALK_INTERVAL ) < gpGlobals->curtime) 
		{
			Host_Say( pPlayer->edict(), args, 1 );
			pPlayer->NotePlayerTalked();
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void give( const CCommand &args );
static ConCommand give_command( side_registry(), "give", give, "Give item to player.\n\tArguments: <item_name>", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );
static void give( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer 
		&& (gpGlobals->maxClients == 1 || sv_cheats->GetBool()) 
		&& args.ArgC() >= 2 )
	{
		char item_to_give[ 256 ];
		V_strncpy( item_to_give, args[1], sizeof( item_to_give ) );
		V_strlower( item_to_give );

		// Dirty hack to avoid suit playing it's pickup sound
		if ( !V_stricmp( item_to_give, "item_suit" ) )
		{
			pPlayer->EquipSuit( false );
			return;
		}

		string_t iszItem = AllocPooledString( item_to_give );	// Make a copy of the classname
		pPlayer->GiveNamedItem( STRING(iszItem) );
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_Player_SetModel( const CCommand &args )
{
	if ( gpGlobals->deathmatch )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( pPlayer && args.ArgC() == 2)
	{
		static char szName[256];
		V_snprintf( szName, sizeof( szName ), "models/%s.mdl", args[1] );
		pPlayer->SetModel( szName );
		UTIL_SetSize(pPlayer, VEC_HULL_MIN, VEC_HULL_MAX);
	}
}
static ConCommand setmodel(side_registry(), "setmodel", CC_Player_SetModel, "Changes's player's model", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_Player_TestDispatchEffect( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( !pPlayer)
		return;
	
	if ( args.ArgC() < 2 )
	{
		Msg(" Usage: test_dispatcheffect <effect name> <distance away> <flags> <magnitude> <scale>\n " );
		Msg("		 defaults are: <distance 1024> <flags 0> <magnitude 0> <scale 0>\n" );
		return;
	}

	// Optional distance
	float flDistance = 1024;
	if ( args.ArgC() >= 3 )
	{
		flDistance = std::atoi( args[ 2 ] );
	}

	// Optional flags
	float flags = 0;
	if ( args.ArgC() >= 4 )
	{
		flags = std::atoi( args[ 3 ] );
	}

	// Optional magnitude
	float magnitude = 0;
	if ( args.ArgC() >= 5 )
	{
		magnitude = std::atof( args[ 4 ] );
	}

	// Optional scale
	float scale = 0;
	if ( args.ArgC() >= 6 )
	{
		scale = std::atof( args[ 5 ] );
	}

	Vector vecForward;
	QAngle vecAngles = pPlayer->EyeAngles();
	AngleVectors( vecAngles, &vecForward );

	// Trace forward
	trace_t tr;
	Vector vecSrc = pPlayer->EyePosition();
	Vector vecEnd = vecSrc + (vecForward * flDistance);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_ALL, pPlayer, COLLISION_GROUP_NONE, &tr );

	// Fill out the generic data
	CEffectData data;
	// If we hit something, use that data
	if ( tr.fraction < 1.0 )
	{
		data.m_vOrigin = tr.endpos;
		VectorAngles( tr.plane.normal, data.m_vAngles );
		data.m_vNormal = tr.plane.normal;
	}
	else
	{
		data.m_vOrigin = vecEnd;
		data.m_vAngles = vecAngles;
		AngleVectors( vecAngles, &data.m_vNormal );
	}
	data.m_nEntIndex = pPlayer->entindex();
	data.m_fFlags = flags;
	data.m_flMagnitude = magnitude;
	data.m_flScale = scale;
	PrecacheEffect( (char *)args[1] );
	DispatchEffect( (char *)args[1], data );
}

static ConCommand test_dispatcheffect(side_registry(), "test_dispatcheffect", CC_Player_TestDispatchEffect, "Test a clientside dispatch effect.\n\tUsage: test_dispatcheffect <effect name> <distance away> <flags> <magnitude> <scale>\n\tDefaults are: <distance 1024> <flags 0> <magnitude 0> <scale 0>\n", FCVAR_CHEAT);

//-----------------------------------------------------------------------------
// Purpose: Quickly switch to the bug bait, or back to previous item
//-----------------------------------------------------------------------------
void CC_Player_BugBaitSwap( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	
	if ( pPlayer )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

		if ( pWeapon )
		{
			// Tell the client to stop selecting weapons
			engine->ClientCommand( UTIL_GetCommandClient()->edict(), "cancelselect" );

			const char *strWeaponName = pWeapon->GetName();

			if ( !V_stricmp( strWeaponName, "weapon_bugbait" ) )
			{
				pPlayer->SelectLastItem();
			}
			else
			{
				pPlayer->SelectItem( "weapon_bugbait" );
			}
		}
	}
}
static ConCommand bugswap(side_registry(), "bug_swap", CC_Player_BugBaitSwap, "Automatically swaps the current weapon for the bug bait and back again.", FCVAR_CHEAT );

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_Player_Use( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer)
	{
		pPlayer->SelectItem((char *)args[1]);
	}
}
static ConCommand use(side_registry(), "use", CC_Player_Use, "Use a particular weapon\t\nArguments: <weapon_name>", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );

class SimplePhysicsTraceFilter : public IPhysicsTraceFilter
{
	CBaseEntity *m_pEntity;
	int m_mask;

public:
	SimplePhysicsTraceFilter( CBaseEntity *pEntity, int mask )
	{
		m_pEntity = pEntity;
		m_mask = mask;
	}

	virtual bool ShouldHitObject( IPhysicsObject *pObject, int contentsMask )
	{
		if ( m_pEntity->VPhysicsGetObject() == pObject )
			return false;

		if ( (m_mask & contentsMask) == 0 )
			return false;

		return true;
	}

	virtual PhysicsTraceType_t	GetTraceType() const
	{
		return VPHYSICS_TRACE_STATIC_AND_MOVING;
	}
};

//------------------------------------------------------------------------------
// A small wrapper around SV_Move that never clips against the supplied entity.
//------------------------------------------------------------------------------
bool TestEntityPosition ( CBaseEntity *pEntity, unsigned int mask )
{
	trace_t	trace;
	IPhysicsObject *physObject = pEntity->VPhysicsGetObject();
	const Vector &origin = pEntity->GetAbsOrigin();
	if ( physObject )
	{
		QAngle angles = pEntity->GetAbsAngles();

		//Vector obbMins, obbMaxs;
		Vector mins, maxs;
		//obbMins = pEntity->CollisionProp()->OBBMins();
		//obbMaxs = pEntity->CollisionProp()->OBBMaxs();
		//pEntity->CollisionProp()->CollisionAABBToWorldAABB( obbMins, obbMaxs, &mins, &maxs );
		pEntity->CollisionProp()->WorldSpaceSurroundingBounds( &mins, &maxs );

		UTIL_TraceHull( vec3_origin, vec3_origin, mins, maxs, mask, pEntity, COLLISION_GROUP_NONE, &trace );
		//SimplePhysicsTraceFilter filter( pEntity, (int)mask );
		//physenv->SweepCollideable( physObject->GetCollide(), origin, origin, angles, mask, &filter, &trace );
	}
	else
	{
		UTIL_TraceEntity( pEntity, origin, origin, mask, &trace );
	}
	return (trace.startsolid == 0);
}

//------------------------------------------------------------------------------
// Searches along the direction ray in steps of "step" to see if 
// the entity position is passible.
// Used for putting the player in valid space when toggling off noclip mode.
//------------------------------------------------------------------------------
static int FindPassableSpace( CBaseEntity *pEntity, unsigned int mask, const Vector& direction, float step, Vector& oldorigin )
{
	int i;
	for ( i = 0; i < 100; i++ )
	{
		Vector origin = pEntity->GetAbsOrigin();
		VectorMA( origin, step, direction, origin );
		pEntity->SetAbsOrigin( origin );
		if ( TestEntityPosition( pEntity, mask ) )
		{
			VectorCopy( pEntity->GetAbsOrigin(), oldorigin );
			return 1;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Test various directions for empty space -- for debugging only; this is slow and
// meant for finding a place to put a noclipped player who goes solid again.
//------------------------------------------------------------------------------
bool FindEmptySpace( CBaseEntity *pEntity, unsigned int mask, const Vector &forward, const Vector &right, const Vector &up, Vector *testOrigin )
{
	return	FindPassableSpace( pEntity, mask, forward, 1, *testOrigin )	||  // forward
			FindPassableSpace( pEntity, mask, right, 1, *testOrigin )	||  // right
			FindPassableSpace( pEntity, mask, right, -1, *testOrigin )	||  // left
			FindPassableSpace( pEntity, mask, up, 1, *testOrigin )		||  // up
			FindPassableSpace( pEntity, mask, up, -1, *testOrigin )		||  // down
			FindPassableSpace( pEntity, mask, forward, -1, *testOrigin ) ;  // back
}

//------------------------------------------------------------------------------
// Noclip
//------------------------------------------------------------------------------
ConVar noclip_fixup( side_registry(), "noclip_fixup", "1", FCVAR_CHEAT );
void EnableNoClip( CBasePlayer *pPlayer )
{
	// Disengage from hierarchy
	pPlayer->SetParent( nullptr );
	pPlayer->SetMoveType( MOVETYPE_NOCLIP );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "noclip ON\n");
	pPlayer->AddEFlags( EFL_NOCLIP_ACTIVE );
	pPlayer->NoClipStateChanged();

	engine->SetNoClipEnabled( true );

	UTIL_LogPrintf( "%s entered NOCLIP mode\n", GameLogSystem()->FormatPlayer( pPlayer ) );
}

void DisableNoClip( CBasePlayer *pPlayer )
{
	CPlayerState *pl = pPlayer->PlayerData();
	contract_assert( pl );

	pPlayer->RemoveEFlags( EFL_NOCLIP_ACTIVE );
	pPlayer->SetMoveType( MOVETYPE_WALK );

	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "noclip OFF\n");
	Vector oldorigin = pPlayer->GetAbsOrigin();
	unsigned int mask = MASK_PLAYERSOLID;
	if ( noclip_fixup.GetBool() && !TestEntityPosition( pPlayer, mask ) )
	{
		Vector forward, right, up;

		AngleVectors ( pl->v_angle, &forward, &right, &up);

		if ( !FindEmptySpace( pPlayer, mask, forward, right, up, &oldorigin ) )
		{
			Msg( "Can't find the world\n" );
		}

		pPlayer->SetAbsOrigin( oldorigin );
	}

	pPlayer->NoClipStateChanged();

	engine->SetNoClipEnabled( false );

	UTIL_LogPrintf( "%s left NOCLIP mode\n", GameLogSystem()->FormatPlayer( pPlayer ) );
}

static void noclip( const CCommand &args );
static ConCommand noclip_command( side_registry(), "noclip", noclip, "Toggle. Player becomes non-solid and flies.  Optional argument of 0 or 1 to force enable/disable", FCVAR_CHEAT );
static void noclip( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() >= 2 )
	{
		bool bEnable = V_atoi( args.Arg( 1 ) ) ? true : false;
		if ( bEnable && pPlayer->GetMoveType() != MOVETYPE_NOCLIP )
		{
			EnableNoClip( pPlayer );
		}
		else if ( !bEnable && pPlayer->GetMoveType() == MOVETYPE_NOCLIP )
		{
			DisableNoClip( pPlayer );
		}
	}
	else
	{
		// Toggle the noclip state if there aren't any arguments.
		if ( pPlayer->GetMoveType() != MOVETYPE_NOCLIP )
		{
			EnableNoClip( pPlayer );
		}
		else
		{
			DisableNoClip( pPlayer );
		}
	}
}

//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
void CC_God_f (void)
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

// 	if ( gpGlobals->deathmatch )
// 		return;

	pPlayer->ToggleFlag( FL_GODMODE );
	if (!(pPlayer->GetFlags() & FL_GODMODE ) )
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "godmode OFF\n");
	else
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "godmode ON\n");
}

//------------------------------------------------------------------------------
// Sets all players to godmode
//------------------------------------------------------------------------------
void CC_Gods_f (void)
{
	if ( !sv_cheats->GetBool() )
		return;
	
	// Decide how to toggle based on the state of the local player.
	bool turnOn = false;
	CBasePlayer *pLocalPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( pLocalPlayer )
	{
		turnOn = !(pLocalPlayer->GetFlags() & FL_GODMODE);
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex( i ) ); 
		if ( !pPlayer )
			continue;

		if ( turnOn != ( ( pPlayer->GetFlags() & FL_GODMODE ) > 0 ) )
		{
			pPlayer->ToggleFlag( FL_GODMODE );
		}
	}

	ClientPrint( pLocalPlayer, HUD_PRINTCONSOLE, turnOn ? "godsmode ON\n" : "godsmode OFF\n");
}

static ConCommand god(side_registry(), "god", CC_God_f, "Toggle. Player becomes invulnerable.", FCVAR_CHEAT );
static ConCommand gods(side_registry(), "gods", CC_Gods_f, "Toggle. All players become invulnerable.", FCVAR_CHEAT );

static void ent_setpos( const CCommand &args );
static ConCommand ent_setpos_command( side_registry(), "ent_setpos", ent_setpos, "Move entity to position", FCVAR_CHEAT );
static void ent_setpos( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 4 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  ent_setpos index x y <optional z>\n");
		return;
	}

	int nIndex = V_atoi( args[ 1 ] );
	CBaseEntity *ent = CBaseEntity::Instance( nIndex );
	if ( !ent )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "ent_setpos no entity %d\n", nIndex ) );
		return;
	}

	Vector oldorigin = ent->GetAbsOrigin();

	Vector newpos;
	newpos.x = std::atof( args[2] );
	newpos.y = atof( args[3] );
	newpos.z = args.ArgC() == 5 ? atof( args[4] ) : oldorigin.z;

	ent->SetAbsOrigin( newpos );
}

static void ent_setang( const CCommand &args );
static ConCommand ent_setang_command( side_registry(), "ent_setang", ent_setang, "Set entity angles", FCVAR_CHEAT );
static void ent_setang( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 4 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  ent_setang index pitch yaw <optional roll>\n");
		return;
	}

	int nIndex = V_atoi( args[ 1 ] );
	CBaseEntity *ent = CBaseEntity::Instance( nIndex );
	if ( !ent )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "ent_setang no entity %d\n", nIndex ) );
		return;
	}

	QAngle old = ent->GetAbsAngles();

	QAngle newAng;
	newAng.x = std::atof( args[2] );
	newAng.y = atof( args[3] );
	newAng.z = args.ArgC() == 5 ? atof( args[4] ) : old.z;

	ent->SetAbsAngles( newAng );
}

//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
static void setpos( const CCommand &args );
static ConCommand setpos_command( side_registry(), "setpos", setpos, "Move player to specified origin (must have sv_cheats).", FCVAR_CHEAT );
static void setpos( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setpos x y <z optional>\n");
		return;
	}

	Vector oldorigin = pPlayer->GetAbsOrigin();

	Vector newpos;
	newpos.x = clamp( std::atof( args[1] ), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	newpos.y = clamp( atof( args[2] ), MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	newpos.z = args.ArgC() == 4 ?  clamp( atof( args[3] ), MIN_COORD_FLOAT, MAX_COORD_FLOAT ) : oldorigin.z;

	pPlayer->SetAbsOrigin( newpos );

	if ( !TestEntityPosition( pPlayer, MASK_PLAYERSOLID ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "setpos into world, use noclip to unstick yourself!\n");
	}
}

//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
static void setpos_player( const CCommand &args );
static ConCommand setpos_player_command( side_registry(), "setpos_player", setpos_player, "Move specified player to specified origin (must have sv_cheats).", FCVAR_CHEAT );
static void setpos_player( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pCommandPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pCommandPlayer )
		return;

	if ( args.ArgC() < 4 )
	{
		ClientPrint( pCommandPlayer, HUD_PRINTCONSOLE, "Usage:  setpos player_index x y <z optional>\n");
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex( std::atoi( args[1] ) ) ); 
	if ( !pPlayer )
		return;

	Vector oldorigin = pPlayer->GetAbsOrigin();

	Vector newpos;
	newpos.x = std::atof( args[2] );
	newpos.y = atof( args[3] );
	newpos.z = args.ArgC() == 5 ? atof( args[4] ) : oldorigin.z;

	pPlayer->SetAbsOrigin( newpos );

	if ( !TestEntityPosition( pPlayer, MASK_PLAYERSOLID ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "setpos into world, use noclip to unstick yourself!\n");
	}
}

//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
void CC_setang_f (const CCommand &args)
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setang pitch yaw <roll optional>\n");
		return;
	}

	QAngle oldang = pPlayer->GetAbsAngles();

	QAngle newang;
	newang.x = std::atof( args[1] );
	newang.y = atof( args[2] );
	newang.z = args.ArgC() == 4 ? atof( args[3] ) : oldang.z;

	pPlayer->SnapEyeAngles( newang );
}

static ConCommand setang(side_registry(), "setang", CC_setang_f, "Snap player eyes to specified pitch yaw <roll:optional> (must have sv_cheats).", FCVAR_CHEAT );

//------------------------------------------------------------------------------
// Move position
//------------------------------------------------------------------------------
static void setpos_exact( const CCommand &args );
static ConCommand setpos_exact_command( side_registry(), "setpos_exact", setpos_exact, "Move player to an exact specified origin (must have sv_cheats).", FCVAR_CHEAT );
static void setpos_exact( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setpos_exact x y <z optional>\n");
		return;
	}

	Vector oldorigin = pPlayer->GetAbsOrigin();

	Vector newpos;
	newpos.x = std::atof( args[1] );
	newpos.y = atof( args[2] );
	newpos.z = args.ArgC() == 4 ? atof( args[3] ) : oldorigin.z;

	pPlayer->Teleport( &newpos, nullptr, nullptr );

	if ( !TestEntityPosition( pPlayer, MASK_PLAYERSOLID ) )
	{
		if ( pPlayer->GetMoveType() != MOVETYPE_NOCLIP )
		{
			EnableNoClip( pPlayer );
			return;
		}
	}
}

static void setang_exact( const CCommand &args );
static ConCommand setang_exact_command( side_registry(), "setang_exact", setang_exact, "Snap player eyes and orientation to specified pitch yaw <roll:optional> (must have sv_cheats).", FCVAR_CHEAT );
static void setang_exact( const CCommand &args )
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setang_exact pitch yaw <roll optional>\n");
		return;
	}

	QAngle oldang = pPlayer->GetAbsAngles();

	QAngle newang;
	newang.x = std::atof( args[1] );
	newang.y = atof( args[2] );
	newang.z = args.ArgC() == 4 ? atof( args[3] ) : oldang.z;

	pPlayer->Teleport( nullptr, &newang, nullptr );
	pPlayer->SnapEyeAngles( newang );

}

//------------------------------------------------------------------------------
// Sets client to notarget mode.
//------------------------------------------------------------------------------
void CC_Notarget_f (void)
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( gpGlobals->deathmatch )
		return;

	pPlayer->ToggleFlag( FL_NOTARGET );
	if ( !(pPlayer->GetFlags() & FL_NOTARGET ) )
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "notarget OFF\n");
	else
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "notarget ON\n");
}

ConCommand notarget(side_registry(), "notarget", CC_Notarget_f, "Toggle. Player becomes hidden to NPCs.", FCVAR_CHEAT);

//------------------------------------------------------------------------------
// Damage the client the specified amount
//------------------------------------------------------------------------------
void CC_HurtMe_f(const CCommand &args)
{
	if ( !sv_cheats->GetBool() )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	int iDamage = 10;
	if ( args.ArgC() >= 2 )
	{
		iDamage = std::atoi( args[ 1 ] );
	}

	pPlayer->TakeDamage( CTakeDamageInfo( pPlayer, pPlayer, iDamage, DMG_PREVENT_PHYSICS_FORCE ) );
}

static ConCommand hurtme(side_registry(), "hurtme", CC_HurtMe_f, "Hurts the player.\n\tArguments: <health to lose>", FCVAR_CHEAT);

static bool IsInGroundList( CBaseEntity *ent, CBaseEntity *ground )
{
	if ( !ground || !ent )
		return false;

	groundlink_t *root = ( groundlink_t * )ground->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			CBaseEntity *other = link->entity;
			if ( other == ent )
				return true;
			link = link->nextLink;
		}
	}

	return false;

}

static int DescribeGroundList( CBaseEntity *ent )
{
	if ( !ent )
		return 0;

	int c = 1;

	Msg( "%i : %s (ground %i %s)\n", ent->entindex(), ent->GetClassname(), 
		ent->GetGroundEntity() ? ent->GetGroundEntity()->entindex() : -1,
		ent->GetGroundEntity() ? ent->GetGroundEntity()->GetClassname() : "NULL" );
	groundlink_t *root = ( groundlink_t * )ent->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			CBaseEntity *other = link->entity;
			if ( other )
			{
				Msg( "  %02i:  %i %s\n", c++, other->entindex(), other->GetClassname() );

				if ( other->GetGroundEntity() != ent )
				{
					contract_assert( 0 );
					Msg( "   mismatched!!!\n" );
				}
			}
			else
			{
				contract_assert( 0 );
				Msg( "  %02i:  NULL link\n", c++ );
			}
			link = link->nextLink;
		}
	}

	if ( ent->GetGroundEntity() != nullptr )
	{
		contract_assert( IsInGroundList( ent, ent->GetGroundEntity() ) );
	}

	return c - 1;
}

extern int groundlinksallocated;
void CC_GroundList_f(const CCommand &args)
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() == 2 )
	{
		int idx = std::atoi( args[1] );

		CBaseEntity *ground = CBaseEntity::Instance( idx );
		if ( ground )
		{
			DescribeGroundList( ground );
		}
	}
	else
	{
		CBaseEntity *ent = nullptr;
		int linkCount = 0;
		while ( (ent = gEntList.NextEnt(ent)) != nullptr )
		{
			linkCount += DescribeGroundList( ent );
		}

		contract_assert( linkCount == groundlinksallocated );

		Msg( "--- %i links\n", groundlinksallocated );
	}
}

static ConCommand groundlist(side_registry(), "groundlist", CC_GroundList_f, "Display ground entity list <index>" );

//-----------------------------------------------------------------------------
// Purpose: called each time a player uses a "cmd" command
// Input  : *pEdict - the player who issued the command
//-----------------------------------------------------------------------------
void ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	const char *pCmd = args[0];

	// Is the client spawned yet?
	if ( !pPlayer )
		return;

	MDLCACHE_CRITICAL_SECTION();

	/*
	const char *pstr;

	if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		// Subtype may be specified
		if ( args.ArgC() == 2 )
		{
			pPlayer->SelectItem( pcmd, atoi( args[1] ) );
		}
		else
		{
			pPlayer->SelectItem(pcmd);
		}
	}
	*/
	
	if ( FStrEq( pCmd, "killtarget" ) )
	{
		if ( g_pDeveloper->GetBool() && sv_cheats->GetBool() && UTIL_IsCommandIssuedByServerAdmin() )
		{
			ConsoleKillTarget( pPlayer, args[1] );
		}
	}
	else if ( FStrEq( pCmd, "demorestart" ) ) 
	{
		pPlayer->ForceClientDllUpdate(); 
	}
	else if ( FStrEq( pCmd, "fade" ) )
	{
		color32 black = {32,63,100,200};
		UTIL_ScreenFade( pPlayer, black, 3, 3, FFADE_OUT  );
	} 
	else if ( FStrEq( pCmd, "te" ) )
	{
		if ( sv_cheats->GetBool() && UTIL_IsCommandIssuedByServerAdmin() )
		{
			if ( FStrEq( args[1], "stop" ) )
			{
				// Destroy it
				//
				CBaseEntity *ent = gEntList.FindEntityByClassname( nullptr, "te_tester" );
				while ( ent )
				{
					CBaseEntity *next = gEntList.FindEntityByClassname( ent, "te_tester" );
					UTIL_Remove( ent );
					ent = next;
				}
			}
			else
			{
				CTempEntTester::Create( pPlayer->WorldSpaceCenter(), pPlayer->EyeAngles(), args[1], args[2] );
			}
		}
	}
	else 
	{
		if ( !g_pGameRules->ClientCommand( pPlayer, args ) )
		{
			if ( V_strlen( pCmd ) > 128 )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Console command too long.\n" );
			}
			else
			{
				// tell the user they entered an unknown command
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", pCmd ) );
			}
		}
	}
}
