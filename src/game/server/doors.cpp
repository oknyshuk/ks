// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include <math.h>
#include "declare_class.h"
#include "eiface.h"
#include "igameevents.h"
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
#include "util.h"
#include "bspfile.h"
#include "reflect_annotations.h"
#include "string_t.h"
#include "dlight.h"
#include "dt_send.h"
#include "tier2/resourceprecacher.h"
#include "ai_criteria.h"
#include "basecombatweapon_shared.h"
#include "edict.h"
#include "shareddefs.h"
#include "collisionutils.h"
#include "datacache/imdlcache.h"
#include "worldsize.h"
#include "basehandle.h"
module ks.server;

import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.platform;
import ks.simd;
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
import ks.core.wchartypes;
import ks.tier1.strtools;
import ks.tier1.createinterface;
import ks.core.color;
import ks.core.types;
import ks.core.vprof;
import ks.game.precipitation_shared;
import ks.math.ssemath;
import ks.tier1.convar;
import ks.tier1.small_vector;
import :cstrike15.cs_player;
import :cstrike15.cs_playeranimstate;
import :econ.econ_item_schema_minimal;
import :cstrike15.bot.cs_bot;
import :cstrike15.cs_player_2;
import ks.tier1.registry;
import ks.tier1.iconvar;
import ks.core.platform;
import ks.core.scratcharena;
import ks.game.entity_cast;
import ks.math.vector2d;
import ks.tier1.generichash;
import ks.tier1.vector_ops;

class CBaseDoor;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements two types of doors: linear and rotating.

inline constexpr const char *CLOSE_AREAPORTAL_THINK_CONTEXT = "CloseAreaportalThink";

IMPLEMENT_REFLECT_DATAMAP( CBaseDoor )

LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );

//
// func_water is implemented as a linear door so we can raise/lower the water level.
//
LINK_ENTITY_TO_CLASS_ALIAS( func_water, CBaseDoor );

// SendTable stuff.
IMPLEMENT_REFLECT_SERVERCLASS( CBaseDoor, DT_BaseDoor )

static constexpr auto DOOR_SENTENCEWAIT = 6;
static constexpr auto DOOR_SOUNDWAIT = 1;
static constexpr auto BUTTON_SOUNDWAIT = 0.5;

//-----------------------------------------------------------------------------
// Purpose: play door or button locked or unlocked sounds. 
//			NOTE: this routine is shared by doors and buttons
// Input  : pEdict - 
//			pls - 
//			flocked - if true, play 'door is locked' sound, otherwise play 'door
//				is unlocked' sound.
//			fbutton - 
//-----------------------------------------------------------------------------
void PlayLockSounds(CBaseEntity *pEdict, locksound_t *pls, int flocked, int fbutton)
{
	if ( pEdict->HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		return;
	}
	float flsoundwait = ( fbutton ) ? BUTTON_SOUNDWAIT : DOOR_SOUNDWAIT;

	if ( flocked )
	{
		int		fplaysound = (pls->sLockedSound != NULL_STRING && gpGlobals->curtime > pls->flwaitSound);
		int		fplaysentence = (pls->sLockedSentence != NULL_STRING && !pls->bEOFLocked && gpGlobals->curtime > pls->flwaitSentence);
		float	fvol = ( fplaysound && fplaysentence ) ? 0.25f : 1.0f;

		// if there is a locked sound, and we've debounced, play sound
		if (fplaysound)
		{
			// play 'door locked' sound
			CPASAttenuationFilter filter( pEdict );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_ITEM;
			ep.m_pSoundName = (char*)STRING(pls->sLockedSound);
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = SNDLVL_NORM;

			CBaseEntity::EmitSound( filter, pEdict->entindex(), ep );
			pls->flwaitSound = gpGlobals->curtime + flsoundwait;
		}

		// if there is a sentence, we've not played all in list, and we've debounced, play sound
		if (fplaysentence)
		{
			// play next 'door locked' sentence in group
			int iprev = pls->iLockedSentence;
			
			pls->iLockedSentence = SENTENCEG_PlaySequentialSz(	pEdict->edict(), 
																STRING(pls->sLockedSentence), 
																0.85f, 
																SNDLVL_NORM, 
																0, 
																100, 
																pls->iLockedSentence, 
																false);
			pls->iUnlockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFLocked = (iprev == pls->iLockedSentence);
		
			pls->flwaitSentence = gpGlobals->curtime + DOOR_SENTENCEWAIT;
		}
	}
	else
	{
		// UNLOCKED SOUND

		int fplaysound = (pls->sUnlockedSound != NULL_STRING && gpGlobals->curtime > pls->flwaitSound);
		int fplaysentence = (pls->sUnlockedSentence != NULL_STRING && !pls->bEOFUnlocked && gpGlobals->curtime > pls->flwaitSentence);
		float fvol;

		// if playing both sentence and sound, lower sound volume so we hear sentence
		fvol = ( fplaysound && fplaysentence ) ? 0.25f : 1.0f;

		// play 'door unlocked' sound if set
		if (fplaysound)
		{
			CPASAttenuationFilter filter( pEdict );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_ITEM;
			ep.m_pSoundName = (char*)STRING(pls->sUnlockedSound);
			ep.m_flVolume = fvol;
			ep.m_SoundLevel = SNDLVL_NORM;

			CBaseEntity::EmitSound( filter, pEdict->entindex(), ep );
			pls->flwaitSound = gpGlobals->curtime + flsoundwait;
		}

		// play next 'door unlocked' sentence in group
		if (fplaysentence)
		{
			int iprev = pls->iUnlockedSentence;
			
			pls->iUnlockedSentence = SENTENCEG_PlaySequentialSz(pEdict->edict(), STRING(pls->sUnlockedSentence), 
					  0.85, SNDLVL_NORM, 0, 100, pls->iUnlockedSentence, false);
			pls->iLockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFUnlocked = (iprev == pls->iUnlockedSentence);
			pls->flwaitSentence = gpGlobals->curtime + DOOR_SENTENCEWAIT;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cache user-entity-field values until spawn is called.
// Input  : szKeyName - 
//			szValue - 
// Output : Returns true.
//-----------------------------------------------------------------------------
bool CBaseDoor::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "locked_sentence"))
	{
		m_bLockedSentence = std::atof(szValue);
	}
	else if (FStrEq(szKeyName, "unlocked_sentence"))
	{
		m_bUnlockedSentence = std::atof(szValue);
	}
	else
		return BaseClass::KeyValue_Impl( szKeyName, szValue );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Spawn_Impl()
{
	Precache();

	if ( GetMoveParent() && GetRootMoveParent()->GetSolid() == SOLID_BSP )
	{
		SetSolid( SOLID_BSP );
	}
	else
	{
		SetSolid( SOLID_VPHYSICS );
	}

	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle( m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z );
	AngleVectors( angMoveDir, &m_vecMoveDir );

	SetModel( STRING( GetModelName() ) );
	m_vecPosition1	= GetLocalOrigin();

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecOBB = CollisionProp()->OBBSize();
	if ( GetLocalAngles() != vec3_angle )
	{	// this door must have been rotated from via an instance, so make sure we rotate our bounds
		Vector		vMinsIn, vMaxsIn, vMinsOut, vMaxsOut;
		matrix3x4_t	mat;

		vMinsIn = CollisionProp()->OBBMins();
		vMaxsIn = CollisionProp()->OBBMaxs();
		AngleMatrix( GetLocalAngles(), mat );
		RotateAABB( mat, vMinsIn, vMaxsIn, vMinsOut, vMaxsOut );

		vecOBB = ( vMaxsOut - vMinsOut );
	}
	vecOBB -= Vector( 2, 2, 2 );
	m_vecPosition2	= m_vecPosition1 + (m_vecMoveDir * (DotProductAbs( m_vecMoveDir, vecOBB ) - m_flLip));

	if ( !IsRotatingDoor() )
	{
		if ( ( m_eSpawnPosition == FUNC_DOOR_SPAWN_OPEN ) || HasSpawnFlags( SF_DOOR_START_OPEN_OBSOLETE ) )
		{	// swap pos1 and pos2, put door at pos2
			UTIL_SetOrigin( this, m_vecPosition2);
			m_toggle_state = TS_AT_TOP;
		}
		else
		{
			m_toggle_state = TS_AT_BOTTOM;
		}
	}

	if (HasSpawnFlags(SF_DOOR_LOCKED))
	{
		m_bLocked = true;
	}

	SetMoveType( MOVETYPE_PUSH );
	
	if (m_flSpeed == 0)
	{
		m_flSpeed = 100;
	}
	
	SetTouch( &CBaseDoor::DoorTouch );

	if ( !FClassnameIs( this, "func_water" ) )
	{
		if ( HasSpawnFlags(SF_DOOR_PASSABLE) )
		{
			//normal door
			AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
			AddSolidFlags( FSOLID_NOT_SOLID );
		}

		if ( HasSpawnFlags( SF_DOOR_NONSOLID_TO_PLAYER ) )
		{
			SetCollisionGroup( COLLISION_GROUP_PASSABLE_DOOR );
			// HACKHACK: Set this hoping that any children of the door that get blocked by the player
			// will get fixed up by vphysics
			// NOTE: We could decouple this as a separate behavior, but managing player collisions is already complex enough.
			// NOTE: This is necessary to prevent the player from blocking the wrecked train car in ep2_outland_01
			AddFlag( FL_UNBLOCKABLE_BY_PLAYER );
		}
		if ( m_bIgnoreDebris )
		{
			// both of these flags want to set the collision group and 
			// there isn't a combo group
			contract_assert( !HasSpawnFlags( SF_DOOR_NONSOLID_TO_PLAYER ) );
			if ( HasSpawnFlags( SF_DOOR_NONSOLID_TO_PLAYER ) )
			{
				Warning("Door %s with conflicting collision settings, removing ignoredebris\n", GetDebugName() );
			}
			else
			{
				SetCollisionGroup( COLLISION_GROUP_INTERACTIVE );
			}
		}
	}

	if ( ( m_eSpawnPosition == FUNC_DOOR_SPAWN_OPEN ) && HasSpawnFlags( SF_DOOR_START_OPEN_OBSOLETE ) )
	{
		Warning("Door %s using obsolete 'Start Open' spawnflag with 'Spawn Position' set to 'Open'. Reverting to old behavior.\n", GetDebugName() );
	}

	CreateVPhysics();
}

void CBaseDoor::MovingSoundThink( void )
{
	CPASAttenuationFilter filter( this );
	filter.MakeReliable();

	EmitSound_t ep;
	ep.m_nChannel = CHAN_STATIC;
	if ( m_NoiseMovingClosed == NULL_STRING || m_toggle_state == TS_GOING_DOWN || m_toggle_state == TS_AT_BOTTOM )
	{
		ep.m_pSoundName = (char*)STRING(m_NoiseMoving);
	}
	else
	{
		ep.m_pSoundName = (char*)STRING(m_NoiseMovingClosed);
	}
	ep.m_flVolume = 1;
	ep.m_SoundLevel = SNDLVL_NORM;

	EmitSound( filter, entindex(), ep );

	//Only loop sounds in HL1 to maintain HL2 behavior
	if( ShouldLoopMoveSound() )
	{
		float duration = enginesound->GetSoundDuration( ep.m_pSoundName );
		SetContextThink( &CBaseDoor::MovingSoundThink, gpGlobals->curtime + duration, "MovingSound" );
	}
}

void CBaseDoor::StartMovingSound( void )
{
	MovingSoundThink();

	CBasePlayer *player = ToBasePlayer(m_hActivator);
	IGameEvent * event = gameeventmanager->CreateEvent( "door_moving" );
	if( event )
	{
		event->SetInt( "entindex", entindex() );
		event->SetInt( "userid", (player)?player->GetUserID():0 );
		gameeventmanager->FireEvent( event );
	}

}

void CBaseDoor::StopMovingSound(void)
{
	SetContextThink( nullptr, gpGlobals->curtime, "MovingSound" );
	char *pSoundName;
	if ( m_NoiseMovingClosed == NULL_STRING || m_toggle_state == TS_GOING_UP || m_toggle_state == TS_AT_TOP )
	{
		pSoundName = (char*)STRING(m_NoiseMoving);
	}
	else
	{
		pSoundName = (char*)STRING(m_NoiseMovingClosed);
	}
	StopSound( entindex(), CHAN_STATIC, pSoundName );
}
 
bool CBaseDoor::ShouldSavePhysics_Impl()
{
	// don't save physics if you're func_water
	return !FClassnameIs( this, "func_water" );
}

//-----------------------------------------------------------------------------
bool CBaseDoor::CreateVPhysics_Impl( )
{
	if ( !FClassnameIs( this, "func_water" ) )
	{
		//normal door
		// NOTE: Create this even when the door is not solid to support constraints.
		VPhysicsInitShadow( false, false );
	}
	else
	{
		// special contents
		AddSolidFlags( FSOLID_VOLUME_CONTENTS );
		SETBITS( m_spawnflags, SF_DOOR_SILENT );	// water is silent for now

		IPhysicsObject *pPhysics = VPhysicsInitShadow( false, false );
		fluidparams_t fluid;
		
		contract_assert( CollisionProp()->GetCollisionAngles() == vec3_angle );
		fluid.damping = 0.01f;
		fluid.surfacePlane[0] = 0;
		fluid.surfacePlane[1] = 0;
		fluid.surfacePlane[2] = 1;
		fluid.surfacePlane[3] = CollisionProp()->GetCollisionOrigin().z + CollisionProp()->OBBMaxs().z - 1;
		fluid.currentVelocity.Init(0,0,0);
		fluid.torqueFactor = 0.1f;
		fluid.viscosityFactor = 0.01f;
		fluid.pGameData = static_cast<void *>(this);
		
		//FIXME: Currently there's no way to specify that you want slime
		fluid.contents = CONTENTS_WATER;
		
		physenv->CreateFluidController( pPhysics, &fluid );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CBaseDoor *pDoorList[64];
	m_bDoorGroup = true;

	// force movement groups to sync!!!
	int doorCount = GetDoorMovementGroup( pDoorList, std::size( pDoorList ) );
	for ( int i = 0; i < doorCount; i++ )
	{
		if ( pDoorList[i]->m_vecMoveDir == m_vecMoveDir )
		{
			bool error = false;
			if ( pDoorList[i]->IsRotatingDoor() )
			{
				error = ( pDoorList[i]->GetLocalAngles() != GetLocalAngles() ) ? true : false;
			}
			else 
			{
				error = ( pDoorList[i]->GetLocalOrigin() != GetLocalOrigin() ) ? true : false;
			}
			if ( error )
			{
				// don't do group blocking
				m_bDoorGroup = false;
			}
		}
	}
	
	switch ( m_toggle_state )
	{
	case TS_AT_TOP:
		UpdateAreaPortals( true );
		break;
	case TS_AT_BOTTOM:
		UpdateAreaPortals( false );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
// This is ONLY used by the node graph to test movement through a door
void CBaseDoor::InputSetToggleState( inputdata_t &inputdata )
{
	SetToggleState( inputdata.value.Int() );
}

void CBaseDoor::SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		UTIL_SetOrigin( this, m_vecPosition2 );
	else
		UTIL_SetOrigin( this, m_vecPosition1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::Precache_Impl( void )
{
	//Fill in a default value if necessary
	if ( IsRotatingDoor() )
	{
		UTIL_ValidateSoundName( m_NoiseMoving,		"RotDoorSound.DefaultMove" );
		UTIL_ValidateSoundName( m_NoiseArrived,		"RotDoorSound.DefaultArrive" );
		UTIL_ValidateSoundName( m_ls.sLockedSound,	"RotDoorSound.DefaultLocked" );
		UTIL_ValidateSoundName( m_ls.sUnlockedSound,"DoorSound.Null" );
	}
	else
	{
		UTIL_ValidateSoundName( m_NoiseMoving,		"DoorSound.DefaultMove" );
		UTIL_ValidateSoundName( m_NoiseArrived,		"DoorSound.DefaultArrive" );
		UTIL_ValidateSoundName( m_ls.sLockedSound,	"DoorSound.DefaultLocked" );
		UTIL_ValidateSoundName( m_ls.sUnlockedSound,"DoorSound.Null" );
	}

	//Precache them all
	PrecacheScriptSound( (char *) STRING(m_NoiseMoving) );
	PrecacheScriptSound( (char *) STRING(m_NoiseArrived) );
	PrecacheScriptSound( (char *) STRING(m_NoiseMovingClosed) );
	PrecacheScriptSound( (char *) STRING(m_NoiseArrivedClosed) );
	PrecacheScriptSound( (char *) STRING(m_ls.sLockedSound) );
	PrecacheScriptSound( (char *) STRING(m_ls.sUnlockedSound) );

	//Get sentence group names, for doors which are directly 'touched' to open
	switch (m_bLockedSentence)
	{
		case 1: m_ls.sLockedSentence = AllocPooledString("NA"); break; // access denied
		case 2: m_ls.sLockedSentence = AllocPooledString("ND"); break; // security lockout
		case 3: m_ls.sLockedSentence = AllocPooledString("NF"); break; // blast door
		case 4: m_ls.sLockedSentence = AllocPooledString("NFIRE"); break; // fire door
		case 5: m_ls.sLockedSentence = AllocPooledString("NCHEM"); break; // chemical door
		case 6: m_ls.sLockedSentence = AllocPooledString("NRAD"); break; // radiation door
		case 7: m_ls.sLockedSentence = AllocPooledString("NCON"); break; // gen containment
		case 8: m_ls.sLockedSentence = AllocPooledString("NH"); break; // maintenance door
		case 9: m_ls.sLockedSentence = AllocPooledString("NG"); break; // broken door
		
		default: m_ls.sLockedSentence = NULL_STRING; break;
	}

	switch (m_bUnlockedSentence)
	{
		case 1: m_ls.sUnlockedSentence = AllocPooledString("EA"); break; // access granted
		case 2: m_ls.sUnlockedSentence = AllocPooledString("ED"); break; // security door
		case 3: m_ls.sUnlockedSentence = AllocPooledString("EF"); break; // blast door
		case 4: m_ls.sUnlockedSentence = AllocPooledString("EFIRE"); break; // fire door
		case 5: m_ls.sUnlockedSentence = AllocPooledString("ECHEM"); break; // chemical door
		case 6: m_ls.sUnlockedSentence = AllocPooledString("ERAD"); break; // radiation door
		case 7: m_ls.sUnlockedSentence = AllocPooledString("ECON"); break; // gen containment
		case 8: m_ls.sUnlockedSentence = AllocPooledString("EH"); break; // maintenance door
		
		default: m_ls.sUnlockedSentence = NULL_STRING; break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Doors not tied to anything (e.g. button, another door) can be touched,
//			to make them activate.
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	if( m_ChainTarget != NULL_STRING )
		ChainTouch( pOther );

	// Ignore touches by anything but players.
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// If door is not opened by touch, do nothing.
	if ( !HasSpawnFlags(SF_DOOR_PTOUCH) )
	{
		return; 
	}
	
	// If door has master, and it's not ready to trigger, 
	// play 'locked' sound.
	if (m_sMaster != NULL_STRING && !UTIL_IsMasterTriggered(m_sMaster, pOther))
	{
		PlayLockSounds(this, &m_ls, true, false);
	}

	if (m_bLocked)
	{
		m_OnLockedUse.FireOutput( pOther, pOther );
		PlayLockSounds(this, &m_ls, true, false);
		return; 
	}
	
	// Remember who activated the door.
	m_hActivator = pOther;

	if (DoorActivate( ))
	{
		// Temporarily disable the touch function, until movement is finished.
		SetTouch( nullptr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Delays turning off area portals when closing doors to prevent visual artifacts
//-----------------------------------------------------------------------------
void CBaseDoor::CloseAreaPortalsThink( void )
{
	UpdateAreaPortals( false );
	SetContextThink( nullptr, gpGlobals->curtime, CLOSE_AREAPORTAL_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : isOpen - 
//-----------------------------------------------------------------------------
void CBaseDoor::UpdateAreaPortals( bool isOpen )
{
	// cancel pending close
	SetContextThink( nullptr, gpGlobals->curtime, CLOSE_AREAPORTAL_THINK_CONTEXT );

	if ( IsRotatingDoor() && HasSpawnFlags(SF_DOOR_START_OPEN_OBSOLETE) ) // logic inverted when using rot doors that start open
		isOpen = !isOpen;

	string_t name = GetEntityName();
	if ( !name )
		return;
	
	CBaseEntity *pPortal = nullptr;
	while ( ( pPortal = gEntList.FindEntityByClassname( pPortal, "func_areaportal" ) ) != nullptr )
	{
		if ( pPortal->HasTarget( name ) )
		{
			// USE_ON means open the portal, off means close it
			pPortal->Use( this, this, isOpen?USE_ON:USE_OFF, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player uses the door.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CBaseDoor::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( m_ChainTarget != NULL_STRING )
		ChainUse();

	// We can't +use this if it can't be +used
	if ( m_hActivator != nullptr && m_hActivator->IsPlayer() && HasSpawnFlags( SF_DOOR_PUSE ) == false )
	{
		PlayLockSounds( this, &m_ls, true, false );
		return;
	}

	bool bAllowUse = false;

	// if not ready to be used, ignore "use" command.
	if( HasSpawnFlags(SF_DOOR_NEW_USE_RULES) )
	{
		//New behavior:
		// If not ready to be used, ignore "use" command.
		// Allow use in these cases:
		//		- when the door is closed/closing
		//		- when the door is open/opening and can be manually closed
		if ( ( m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN ) || ( HasSpawnFlags(SF_DOOR_NO_AUTO_RETURN) && ( m_toggle_state == TS_AT_TOP || m_toggle_state == TS_GOING_UP ) ) )
			bAllowUse = true;
	}
	else
	{
		// Legacy behavior:
		if (m_toggle_state == TS_AT_BOTTOM || (HasSpawnFlags(SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP) )
			bAllowUse = true;
	}

	if( bAllowUse )
	{
		if (m_bLocked)
		{
			m_OnLockedUse.FireOutput( pActivator, pCaller );
			PlayLockSounds(this, &m_ls, true, false);
		}
		else
		{
			DoorActivate();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Passes Use along to certain named doors.
//-----------------------------------------------------------------------------
void CBaseDoor::ChainUse( void )
{
	if ( m_isChaining )
		return;

	CBaseEntity *ent = nullptr;
	while ( ( ent = gEntList.FindEntityByName( ent, m_ChainTarget, nullptr ) ) != nullptr )
	{
		if ( ent == this )
			continue;

		CBaseDoor *door = dynamic_cast< CBaseDoor * >( ent );
		if ( door )
		{
			door->SetChaining( true );
			door->Use( m_hActivator, nullptr, USE_TOGGLE, 0.0f ); // only the first param is used
			door->SetChaining( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Passes Touch along to certain named doors.
//-----------------------------------------------------------------------------
void CBaseDoor::ChainTouch( CBaseEntity *pOther )
{
	if ( m_isChaining )
		return;

	CBaseEntity *ent = nullptr;
	while ( ( ent = gEntList.FindEntityByName( ent, m_ChainTarget, nullptr ) ) != nullptr )
	{
		if ( ent == this )
			continue;

		CBaseDoor *door = dynamic_cast< CBaseDoor * >( ent );
		if ( door )
		{
			door->SetChaining( true );
			door->Touch( pOther );
			door->SetChaining( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Closes the door if it is not already closed.
//-----------------------------------------------------------------------------
void CBaseDoor::InputClose( inputdata_t &inputdata )
{
	if ( m_toggle_state != TS_AT_BOTTOM )
	{	
		DoorGoDown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that locks the door.
//-----------------------------------------------------------------------------
void CBaseDoor::InputLock( inputdata_t &inputdata )
{
	Lock();
}

//-----------------------------------------------------------------------------
// Purpose: Opens the door if it is not already open.
//-----------------------------------------------------------------------------
void CBaseDoor::InputOpen( inputdata_t &inputdata )
{
	if (m_toggle_state != TS_AT_TOP && m_toggle_state != TS_GOING_UP )
	{	
		// I'm locked, can't open
		if (m_bLocked)
			return; 

		// Play door unlock sounds.
		PlayLockSounds(this, &m_ls, false, false);
		DoorGoUp();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the door if it is not already open.
//-----------------------------------------------------------------------------
void CBaseDoor::InputToggle( inputdata_t &inputdata )
{
	// I'm locked, can't open
	if (m_bLocked)
		return; 

	if (m_toggle_state == TS_AT_BOTTOM)
	{	
		DoorGoUp();
	}
	else if (m_toggle_state == TS_AT_TOP)
	{
		DoorGoDown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that unlocks the door.
//-----------------------------------------------------------------------------
void CBaseDoor::InputUnlock( inputdata_t &inputdata )
{
	Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDoor::InputSetSpeed( inputdata_t &inputdata )
{
	m_flSpeed = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Locks the door so that it cannot be opened.
//-----------------------------------------------------------------------------
void CBaseDoor::Lock( void )
{
	m_bLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose: Unlocks the door so that it can be opened.
//-----------------------------------------------------------------------------
void CBaseDoor::Unlock( void )
{
	m_bLocked = false;
}

//-----------------------------------------------------------------------------
// Purpose: Causes the door to "do its thing", i.e. start moving, and cascade activation.
// Output : int
//-----------------------------------------------------------------------------
int CBaseDoor::DoorActivate( )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return 0;

	if (HasSpawnFlags(SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP)
	{
		// door should close
		DoorGoDown();
	}
	else
	{
		// door should open
		// play door unlock sounds
		PlayLockSounds(this, &m_ls, false, false);

		if ( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_GOING_UP )
		{
			DoorGoUp();
		}
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//-----------------------------------------------------------------------------
void CBaseDoor::DoorGoUp( void )
{
	edict_t	*pevActivator;

	UpdateAreaPortals( true );
	// It could be going-down, if blocked.
	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if ( !HasSpawnFlags(SF_DOOR_SILENT ) )
	{
		// If we're not moving already, start the moving noise
		if ( m_toggle_state != TS_GOING_UP && m_toggle_state != TS_GOING_DOWN )
		{
			StartMovingSound();
		}
	}

	m_toggle_state = TS_GOING_UP;
	
	SetMoveDone( &CBaseDoor::DoorHitTop );
	if ( IsRotatingDoor() )		// !!! BUGBUG Triggered doors don't work with this yet
	{
		float	sign = 1.0;

		if ( m_hActivator != nullptr )
		{
			pevActivator = m_hActivator->edict();
			
			if ( !HasSpawnFlags( SF_DOOR_ONEWAY ) && m_vecMoveAng.y ) 		// Y axis rotation, move away from the player
			{
				// Positive is CCW, negative is CW, so make 'sign' 1 or -1 based on which way we want to open.
				// Important note:  All doors face East at all times, and twist their local angle to open.
				//					So you can't look at the door's facing to determine which way to open.

				Vector origin = GetAbsOrigin();

				Vector nearestPoint;
				CollisionProp()->CalcNearestPoint( m_hActivator->GetAbsOrigin(), &nearestPoint );
				Vector activatorToNearestPoint = nearestPoint - m_hActivator->GetAbsOrigin();
				activatorToNearestPoint.z = 0;

				Vector activatorToOrigin = origin - m_hActivator->GetAbsOrigin();
				activatorToOrigin.z = 0;

				float nearestPointDistToOrigin = activatorToOrigin.DistTo( activatorToNearestPoint );
				activatorToOrigin.NormalizeInPlace();
				activatorToNearestPoint.NormalizeInPlace();
				float dot = activatorToOrigin.Dot( activatorToNearestPoint );
				if ( nearestPointDistToOrigin < 5.0f && dot > 0.99f )
				{
					// the nearest point and the origin are nearly the same, and we're looking at them in a
					// line.  This would give us unpredictable results, so let's pull the origin out from
					// the center some.
					origin += (origin - WorldSpaceCenter()); // pull the origin out from the center some
					activatorToOrigin = origin - m_hActivator->GetAbsOrigin();
					activatorToOrigin.z = 0;
					activatorToOrigin.NormalizeInPlace();
				}

				// Point right hand at door hinge, curl hand towards closest spot on door, if thumb
				// is up, open door CW.  -- Department of Basic Cross Product Understanding for Noobs
				Vector cross = activatorToOrigin.Cross( activatorToNearestPoint );

				if( cross.z > 0.0f )
				{
					sign = -1.0f;	
				}
			}
		}
		AngularMove(m_vecAngle2*sign, m_flSpeed);
	}
	else
	{
		LinearMove(m_vecPosition2, m_flSpeed);
	}

	//Fire our open ouput
	m_OnOpen.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: The door has reached the "up" position.  Either go back down, or
//			wait for another activation.
//-----------------------------------------------------------------------------
void CBaseDoor::DoorHitTop( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();
		StopMovingSound();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = (char*)STRING(m_NoiseArrived);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}

	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;
	
	// toggle-doors don't come down automatically, they wait for refire.
	if (HasSpawnFlags( SF_DOOR_NO_AUTO_RETURN))
	{
		// Re-instate touch method, movement is complete
		SetTouch( &CBaseDoor::DoorTouch );
	}
	else
	{
		// In flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		SetMoveDoneTime( m_flWait );
		SetMoveDone( &CBaseDoor::DoorGoDown );

		if ( m_flWait == -1 )
		{
			SetNextThink( TICK_NEVER_THINK );
		}
	}

	if (HasSpawnFlags(SF_DOOR_START_OPEN_OBSOLETE) )
	{
		m_OnFullyClosed.FireOutput(this, this);
	}
	else
	{
		m_OnFullyOpen.FireOutput(this, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//-----------------------------------------------------------------------------
void CBaseDoor::DoorGoDown( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		// If we're not moving already, start the moving noise
		if ( m_toggle_state != TS_GOING_UP && m_toggle_state != TS_GOING_DOWN )
		{
			StartMovingSound();
		}
	}
	
	m_toggle_state = TS_GOING_DOWN;

	SetMoveDone( &CBaseDoor::DoorHitBottom );
	if ( IsRotatingDoor() )//rotating door
		AngularMove( m_vecAngle1, m_flSpeed);
	else
		LinearMove( m_vecPosition1, m_flSpeed);

	//Fire our closed output
	m_OnClose.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: The door has reached the "down" position.  Back to quiescence.
//-----------------------------------------------------------------------------
void CBaseDoor::DoorHitBottom( void )
{
	if ( !HasSpawnFlags( SF_DOOR_SILENT ) )
	{
		CPASAttenuationFilter filter( this );
		filter.MakeReliable();

		StopMovingSound();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		if ( m_NoiseArrivedClosed == NULL_STRING )
			ep.m_pSoundName = (char*)STRING(m_NoiseArrived);
		else
			ep.m_pSoundName = (char*)STRING(m_NoiseArrivedClosed);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}

	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	// Re-instate touch method, cycle is complete
	SetTouch( &CBaseDoor::DoorTouch );

	if (HasSpawnFlags(SF_DOOR_START_OPEN_OBSOLETE))
	{
		m_OnFullyOpen.FireOutput(m_hActivator, this);
	}
	else
	{
		m_OnFullyClosed.FireOutput(m_hActivator, this);
	}

	// Close the area portals just after the door closes, to prevent visual artifacts in multiplayer games
	SetContextThink( &CBaseDoor::CloseAreaPortalsThink, gpGlobals->curtime + 0.5f, CLOSE_AREAPORTAL_THINK_CONTEXT );
}

// Lists all doors in the same movement group as this one
int CBaseDoor::GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax )
{
	int count = 0;
	CBaseEntity	*pTarget = nullptr;

	// Block all door pieces with the same targetname here.
	if ( GetEntityName() != NULL_STRING )
	{
		for (;;)
		{
			pTarget = gEntList.FindEntityByName( pTarget, GetEntityName(), nullptr );

			if ( pTarget != this )
			{
				if ( !pTarget )
					break;

				CBaseDoor *pDoor = dynamic_cast<CBaseDoor *>(pTarget);

				if ( pDoor && count < listMax )
				{
					pDoorList[count] = pDoor;
					count++;
				}
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Called the first frame that the door is blocked while opening or closing.
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CBaseDoor::StartBlocked_Impl( CBaseEntity *pOther )
{
	//
	// Fire whatever events we need to due to our blocked state.
	//
	if (m_toggle_state == TS_GOING_DOWN)
	{
		m_OnBlockedClosing.FireOutput(pOther, this);
	}
	else
	{
		m_OnBlockedOpening.FireOutput(pOther, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame when the door is blocked while opening or closing.
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CBaseDoor::Blocked_Impl( CBaseEntity *pOther )
{
	// Hurt the blocker a little.
	if ( m_flBlockDamage )
	{
		// if the door is marked "force closed" or it has a negative wait, then there's nothing to do but 
		// push/damage the object.
		// If block damage is set, but this object is a physics prop that can't be damaged, just
		// give up and disable collisions
		if ( (m_bForceClosed || m_flWait < 0) && pOther->GetMoveType() == MOVETYPE_VPHYSICS && 
		   (pOther->m_takedamage == DAMAGE_NO || pOther->m_takedamage == DAMAGE_EVENTS_ONLY) )
		{
			EntityPhysics_CreateSolver( this, pOther, true, 4.0f );
		}
		else
		{
			pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );
		}
	}

	// If we're set to force ourselves closed, keep going
	if ( m_bForceClosed )
		return;

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (m_flWait >= 0)
	{
		if (m_toggle_state == TS_GOING_DOWN)
		{
			DoorGoUp();
		}
		else
		{
			DoorGoDown();
		}
	}

	// Block all door pieces with the same targetname here.
	if ( GetEntityName() != NULL_STRING )
	{
		CBaseDoor *pDoorList[64];
		int doorCount = GetDoorMovementGroup( pDoorList, std::size( pDoorList ) );

		for ( int i = 0; i < doorCount; i++ )
		{
			CBaseDoor *pDoor = pDoorList[i];

			if ( pDoor->m_flWait >= 0)
			{
				if (m_bDoorGroup && pDoor->m_vecMoveDir == m_vecMoveDir && pDoor->GetAbsVelocity() == GetAbsVelocity() && pDoor->GetLocalAngularVelocity() == GetLocalAngularVelocity())
				{
					pDoor->m_nSimulationTick = m_nSimulationTick;	// don't run simulation this frame if you haven't run yet

					// this is the most hacked, evil, bastardized thing I've ever seen. kjb
					if ( !pDoor->IsRotatingDoor() )
					{// set origin to realign normal doors
						pDoor->SetLocalOrigin( GetLocalOrigin() );
						pDoor->SetAbsVelocity( vec3_origin );// stop!

					}
					else
					{// set angles to realign rotating doors
						pDoor->SetLocalAngles( GetLocalAngles() );
						pDoor->SetLocalAngularVelocity( vec3_angle );
					}
				}
			
				if ( pDoor->m_toggle_state == TS_GOING_DOWN)
					pDoor->DoorGoUp();
				else
					pDoor->DoorGoDown();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called the first frame that the door is unblocked while opening or closing.
//-----------------------------------------------------------------------------
void CBaseDoor::EndBlocked_Impl( void )
{
	//
	// Fire whatever events we need to due to our unblocked state.
	//
	if (m_toggle_state == TS_GOING_DOWN)
	{
		m_OnUnblockedClosing.FireOutput(this, this);
	}
	else
	{
		m_OnUnblockedOpening.FireOutput(this, this);
	}
}

/*func_door_rotating

TOGGLE causes the door to wait in both the start and end states for  
a trigger event.

START_OPEN causes the door to move to its destination when spawned,  
and operate in reverse.  It is used to temporarily or permanently  
close off an area when triggered (not usefull for touch or  
takedamage doors).

You need to have an origin brush as part of this entity.  The  
center of that brush will be
the point around which it is rotated. It will rotate around the Z  
axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote  
button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
*/

//==================================================
// CRotDoor 
//==================================================

class CRotDoor : public CBaseDoor
{
public:
	DECLARE_CLASS( CRotDoor, CBaseDoor );

	void Spawn_Impl( void );
	bool CreateVPhysics_Impl();
	// This is ONLY used by the node graph to test movement through a door
	virtual void SetToggleState( int state );
	virtual bool IsRotatingDoor() { return true; }

	[[= ks::reflect::Key{ .name = "solidbsp" } ]] bool m_bSolidBsp;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );

IMPLEMENT_REFLECT_DATAMAP( CRotDoor )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRotDoor::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	// set the axis of rotation
	CBaseToggle::AxisDir();

	// check for clockwise rotation
	if ( HasSpawnFlags(SF_DOOR_ROTATE_BACKWARDS) )
		m_vecMoveAng = m_vecMoveAng * -1;
	
	//m_flWait			= 2; who the hell did this? (sjb)
	m_vecAngle1	= GetLocalAngles();
	m_vecAngle2	= GetLocalAngles() + m_vecMoveAng * m_flMoveDistance;

	ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal\n");

	// Starting open allows a func_door to be lighted in the closed position but
	// spawn in the open position
	//
	// SF_DOOR_START_OPEN_OBSOLETE is an old broken way of spawning open that has
	// been deprecated.
	if ( HasSpawnFlags(SF_DOOR_START_OPEN_OBSOLETE) )
	{	
		// swap pos1 and pos2, put door at pos2, invert movement direction
		QAngle vecNewAngles = m_vecAngle2;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecNewAngles;
		m_vecMoveAng = -m_vecMoveAng;

		// We've already had our physics setup in BaseClass::Spawn, so teleport to our
		// current position. If we don't do this, our vphysics shadow will not update.
		Teleport( nullptr, &m_vecAngle1, nullptr );

		m_toggle_state = TS_AT_BOTTOM;
	}
	else if ( m_eSpawnPosition == FUNC_DOOR_SPAWN_OPEN )
	{	
		// We've already had our physics setup in BaseClass::Spawn, so teleport to our
		// current position. If we don't do this, our vphysics shadow will not update.
		Teleport( nullptr, &m_vecAngle2, nullptr );
		m_toggle_state = TS_AT_TOP;
	}
	else
	{
		m_toggle_state = TS_AT_BOTTOM;
	}

	// Slam the object back to solid - if we really want it to be solid.
	if ( m_bSolidBsp )
	{
		SetSolid( SOLID_BSP );
	}
}

//-----------------------------------------------------------------------------

bool CRotDoor::CreateVPhysics_Impl()
{
	if ( !IsSolidFlagSet( FSOLID_NOT_SOLID ) )
	{
		VPhysicsInitShadow( false, false );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
// This is ONLY used by the node graph to test movement through a door
void CRotDoor::SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		SetLocalAngles( m_vecAngle2 );
	else
		SetLocalAngles( m_vecAngle1 );
}

int CBaseDoor::ObjectCaps_Impl( void)
{
	int flags = BaseClass::ObjectCaps_Impl();
	if ( HasSpawnFlags( SF_DOOR_PUSE ) )
		return flags | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS;

	return flags;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Dynamic light.

static constexpr auto NUM_DL_EXPONENT_BITS = 8;
static constexpr auto MIN_DL_EXPONENT_VALUE = -((1 << (NUM_DL_EXPONENT_BITS-1)) - 1);
static constexpr auto MAX_DL_EXPONENT_VALUE = ((1 << (NUM_DL_EXPONENT_BITS-1)) - 1);

class CDynamicLight : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_DynamicLight" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CDynamicLight, CBaseEntity );

	void Spawn_Impl( void );
	void DynamicLightThink( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	// Turn on and off the light
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );

public:
	unsigned char m_ActualFlags;
	CNetworkVar( unsigned char, m_Flags, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( unsigned char, m_LightStyle, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "style", .input = true } ]] );
	bool	m_On;
	CNetworkVar( float, m_Radius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "distance", .input = true } ]] );
	CNetworkVar( int, m_Exponent, [[= ks::reflect::Net{ .bits = NUM_DL_EXPONENT_BITS } ]] [[= ks::reflect::Key{ .name = "brightness", .input = true } ]] );
	CNetworkVar( float, m_InnerAngle, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 360.0f } ]] [[= ks::reflect::Key{ .name = "_inner_cone", .input = true } ]] );
	CNetworkVar( float, m_OuterAngle, [[= ks::reflect::Net{ .bits = 8, .low = 0.0, .high = 360.0f } ]] [[= ks::reflect::Key{ .name = "_cone", .input = true } ]] );
	CNetworkVar( float, m_SpotRadius, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "spotlight_radius", .input = true } ]] );
};

LINK_ENTITY_TO_CLASS(light_dynamic, CDynamicLight);

IMPLEMENT_REFLECT_DATAMAP( CDynamicLight )

IMPLEMENT_REFLECT_SERVERCLASS( CDynamicLight, DT_DynamicLight )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDynamicLight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "_light" ) )
	{
		color32 tmp;
		V_StringToColor32( &tmp, szValue );
		SetRenderColor( tmp.r, tmp.g, tmp.b );
	}
	else if ( FStrEq( szKeyName, "pitch" ) )
	{
		float angle = std::atof(szValue);
		if ( angle )
		{
			QAngle angles = GetAbsAngles();
			angles[PITCH] = -angle;
			SetAbsAngles( angles );
		}
	}
	else if ( FStrEq( szKeyName, "spawnflags" ) )
	{
		m_ActualFlags = m_Flags = std::atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

//------------------------------------------------------------------------------
// Turn on and off the light
//------------------------------------------------------------------------------
void CDynamicLight::InputTurnOn( inputdata_t &inputdata )
{
	m_Flags = m_ActualFlags;
	m_On = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CDynamicLight::InputTurnOff( inputdata_t &inputdata )
{
	// This basically shuts it off
	m_Flags = DLIGHT_NO_MODEL_ILLUMINATION | DLIGHT_NO_WORLD_ILLUMINATION;
	m_On = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CDynamicLight::InputToggle( inputdata_t &inputdata )
{
	if (m_On)
	{
		InputTurnOff( inputdata );
	}
	else
	{
		InputTurnOn( inputdata );
	}
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CDynamicLight::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	m_On = true;
	UTIL_SetSize( this, vec3_origin, vec3_origin );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// If we have a target, think so we can orient towards it
	if ( m_target != NULL_STRING )
	{
		SetThink( &CDynamicLight::DynamicLightThink );
		SetNextThink( gpGlobals->curtime + 0.1 );
	}
	
	int clampedExponent = clamp( m_Exponent.Get(), MIN_DL_EXPONENT_VALUE, MAX_DL_EXPONENT_VALUE );
	if ( m_Exponent != clampedExponent )
	{
		Warning( "light_dynamic at [%d %d %d] has invalid exponent value (%d must be between %d and %d).\n",
			(int)GetAbsOrigin().x, (int)GetAbsOrigin().x, (int)GetAbsOrigin().x, 
			m_Exponent.Get(),
			MIN_DL_EXPONENT_VALUE,
			MAX_DL_EXPONENT_VALUE );
		
		m_Exponent = clampedExponent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDynamicLight::DynamicLightThink( void )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pEntity = GetNextTarget();
	if ( pEntity )
	{
		Vector vecToTarget = (pEntity->GetAbsOrigin() - GetAbsOrigin());
		QAngle vecAngles;
		VectorAngles( vecToTarget, vecAngles );
		SetAbsAngles( vecAngles );
	}
	
	SetNextThink( gpGlobals->curtime + 0.1 );
}

class CEffectData;

using namespace ::ks::reflect;

static constexpr auto SUBINCH_PRECISION = 3;

// MAX_MODEL_INDEX_BITS moved to effect_dispatch_data.h: an annotation on CEffectData has to be a
// constant expression where the class is declared.

	IMPLEMENT_REFLECT_TABLE( CEffectData, DT_EffectData );

CEffectData::CEffectData()
{
	m_vOrigin.Init();
	m_vStart.Init();
	m_vNormal.Init();
	m_vAngles.Init();

	m_fFlags = 0;
	m_nEntIndex = 0;
	m_flScale = 1.f;
	m_nAttachmentIndex = 0;
	m_nSurfaceProp = 0;

	m_flMagnitude = 0.0f;
	m_flRadius = 0.0f;

	m_nMaterial = 0;
	m_nDamageType = 0;
	m_nHitBox = 0;

	m_nColor = 0;

	m_nOtherEntIndex = 0;

	m_bPositionsAreRelativeToEntity = false;
}

class CBaseAnimating;
class CBaseCombatCharacter;
class CBaseEntity;
class CBasePlayer;
class CCSPlayer;
class CCSWeaponInfo;
class CEconItemDefinition;
class CEffectData;
class CSprite;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements a grab bag of visual effects entities.

static constexpr auto SF_FUNNEL_REVERSE = 1; // funnel effect repels particles instead of attracting them.

static constexpr auto SF_GIBSHOOTER_REPEATABLE = (1<<0); // allows a gibshooter to be refired
static constexpr auto SF_SHOOTER_FLAMING = (1<<1); // gib is on fire
static constexpr auto SF_SHOOTER_STRICT_REMOVE = (1<<2); // remove this gib even if it is in the player's view

// UNDONE: This should be client-side and not use TempEnts
class CBubbling : public CBaseEntity
{
	[[= ks::reflect::KeyFrom<"m_flSpeed", ks::reflect::Key{ .name = "current" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CBubbling, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	void	FizzThink( void );

	// Input handlers.
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void	InputActivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Deactivate", .type = FIELD_VOID } ]] void	InputDeactivate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetCurrent", .type = FIELD_INTEGER } ]] void	InputSetCurrent( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetDensity", .type = FIELD_INTEGER } ]] void	InputSetDensity( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetFrequency", .type = FIELD_INTEGER } ]] void	InputSetFrequency( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	void TurnOn();
	void TurnOff();
	void Toggle();

	[[= ks::reflect::Key{ .name = "density" } ]] int		m_density;
	[[= ks::reflect::Key{ .name = "frequency" } ]] int		m_frequency;
	int		m_bubbleModel;
	int		m_state;
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

IMPLEMENT_REFLECT_DATAMAP( CBubbling )

static constexpr auto SF_BUBBLES_STARTOFF = 0x0001;

void CBubbling::Spawn_Impl( void )
{
	Precache( );
	SetModel( STRING( GetModelName() ) );		// Set size

	// Make it invisible to client
	SetRenderAlpha( 0 );

	SetSolid( SOLID_NONE );						// Remove model & collisions

	if ( !HasSpawnFlags(SF_BUBBLES_STARTOFF) )
	{
		SetThink( &CBubbling::FizzThink );
		SetNextThink( gpGlobals->curtime + 2.0 );
		m_state = 1;
	}
	else
	{
		m_state = 0;
	}
}

void CBubbling::Precache_Impl( void )
{
	m_bubbleModel = PrecacheModel("sprites/bubble.vmt");			// Precache bubble sprite
}

void CBubbling::Toggle()
{
	if (!m_state)
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CBubbling::TurnOn()
{
	m_state = 1;
	SetThink( &CBubbling::FizzThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBubbling::TurnOff()
{
	m_state = 0;
	SetThink( nullptr );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputActivate( inputdata_t &inputdata )
{
	TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputDeactivate( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetCurrent( inputdata_t &inputdata )
{
	m_flSpeed = (float)inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetDensity( inputdata_t &inputdata )
{
	m_density = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetFrequency( inputdata_t &inputdata )
{
	m_frequency = inputdata.value.Int();

	// Reset think time
	if ( m_state )
	{
		if ( m_frequency > 19 )
		{
			SetNextThink( gpGlobals->curtime + 0.5f );
		}
		else
		{
			SetNextThink( gpGlobals->curtime + 2.5 - (0.1 * m_frequency) );
		}
	}
}

void CBubbling::FizzThink( void )
{
	Vector center = WorldSpaceCenter();
	CPASFilter filter( center );
	te->Fizz( filter, 0.0, this, m_bubbleModel, m_density, (int)m_flSpeed );

	if ( m_frequency > 19 )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 2.5 - (0.1 * m_frequency) );
	}
}

// ENV_TRACER
// Fakes a tracer
class CEnvTracer : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvTracer, CPointEntity );

	void Spawn_Impl( void );
	void TracerThink( void );
	void Activate_Impl( void );

	DECLARE_DATADESC();

	Vector m_vecEnd;
	[[= ks::reflect::Key{ .name = "delay" } ]] float  m_flDelay;
};

LINK_ENTITY_TO_CLASS( env_tracer, CEnvTracer );

IMPLEMENT_REFLECT_DATAMAP( CEnvTracer )

//-----------------------------------------------------------------------------
// Purpose: Called after keyvalues are parsed.
//-----------------------------------------------------------------------------
void CEnvTracer::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	if (!m_flDelay)
		m_flDelay = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Called after all the entities have been loaded.
//-----------------------------------------------------------------------------
void CEnvTracer::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CBaseEntity *pEnd = gEntList.FindEntityByName( nullptr, m_target );
	if (pEnd != nullptr)
	{
		m_vecEnd = pEnd->GetLocalOrigin();
		SetThink( &CEnvTracer::TracerThink );
		SetNextThink( gpGlobals->curtime + m_flDelay );
	}
	else
	{
		Msg( "env_tracer: unknown entity \"%s\"\n", STRING(m_target) );
	}
}

// Think
void CEnvTracer::TracerThink( void )
{
	UTIL_Tracer( GetAbsOrigin(), m_vecEnd );

	SetNextThink( gpGlobals->curtime + m_flDelay );
}

//#################################################################################
//  >> CGibShooter
//#################################################################################
enum GibSimulation_t
{
	GIB_SIMULATE_POINT,
	GIB_SIMULATE_PHYSICS,
	GIB_SIMULATE_RAGDOLL,
};

class CGibShooter : public CBaseEntity
{
public:
	DECLARE_CLASS( CGibShooter, CBaseEntity );

	void Spawn_Impl( void );
	void Precache_Impl( void );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual CGib *CreateGib( void );

protected:
	// Purpose:
	CBaseEntity *SpawnGib( const Vector &vecShootDir, float flSpeed );

	DECLARE_DATADESC();
private:
	void InitPointGib( CGib *pGib, const Vector &vecShootDir, float flSpeed );
	void ShootThink( void );

protected:
	[[= ks::reflect::Key{ .name = "m_iGibs" } ]] int		m_iGibs;
	int		m_iGibCapacity;
	int		m_iGibMaterial;
	int		m_iGibModelIndex;
	[[= ks::reflect::Key{ .name = "m_flVelocity" } ]] float	m_flGibVelocity;
	[[= ks::reflect::Key{ .name = "gibangles" } ]] QAngle	m_angGibRotation;
	[[= ks::reflect::Key{ .name = "gibanglevelocity" } ]] float	m_flGibAngVelocity;
	[[= ks::reflect::Key{ .name = "m_flVariance" } ]] float	m_flVariance;
	[[= ks::reflect::Key{ .name = "m_flGibLife" } ]] float	m_flGibLife;
	[[= ks::reflect::Key{ .name = "Simulation" } ]] int		m_nSimulationType;
	int		m_nMaxGibModelFrame;
	[[= ks::reflect::Key{ .name = "delay" } ]] float	m_flDelay;

	[[= ks::reflect::Key{ .name = "nogibshadows" } ]] bool	m_bNoGibShadows;

	bool	m_bIsSprite;
	[[= ks::reflect::Key{ .name = "LightingOrigin" } ]] string_t m_iszLightingOrigin;

	// ----------------
	//	Inputs
	// ----------------
	[[= ks::reflect::Input{ .name = "Shoot", .type = FIELD_VOID } ]] void InputShoot( inputdata_t &inputdata );
};

IMPLEMENT_REFLECT_DATAMAP( CGibShooter )

LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );

void CGibShooter::Precache_Impl ( void )
{
	if ( g_Language.GetInt() == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PrecacheModel ("models/germanygibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PrecacheModel ("models/gibs/hgibs.mdl");
	}
}

void CGibShooter::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for shooting gibs.
//-----------------------------------------------------------------------------
void CGibShooter::InputShoot( inputdata_t &inputdata )
{
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( gpGlobals->curtime );
}

void CGibShooter::Spawn_Impl( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	if ( m_flDelay < 0 )
	{
		m_flDelay = 0.0;
	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	m_iGibCapacity = m_iGibs;

	m_nMaxGibModelFrame = modelinfo->GetModelFrameCount( modelinfo->GetModel( m_iGibModelIndex ) );
}

CGib *CGibShooter::CreateGib ( void )
{
	ConVarRef violence_hgibs( "violence_hgibs" );
	if ( violence_hgibs.IsValid() && !violence_hgibs.GetInt() )
		return nullptr;

	CGib *pGib = CREATE_ENTITY( CGib, "gib" );
	pGib->Spawn( "models/gibs/hgibs.mdl" );
	pGib->SetBloodColor( BLOOD_COLOR_RED );

	if ( m_nMaxGibModelFrame <= 1 )
	{
		DevWarning( 2, "GibShooter Body is <= 1!\n" );
	}

	pGib->m_nBody = random_valve->RandomInt ( 1, m_nMaxGibModelFrame - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	if ( m_iszLightingOrigin != NULL_STRING )
	{
		// Make the gibs use the lighting origin
		pGib->SetLightingOrigin( m_iszLightingOrigin );
	}

	return pGib;
}

void CGibShooter::InitPointGib( CGib *pGib, const Vector &vecShootDir, float flSpeed )
{
	if ( pGib )
	{
		pGib->SetLocalOrigin( GetAbsOrigin() );
		pGib->SetAbsVelocity( vecShootDir * flSpeed );

		QAngle angVel( random_valve->RandomFloat ( 100, 200 ), random_valve->RandomFloat ( 100, 300 ), 0 );
		pGib->SetLocalAngularVelocity( angVel );

		float thinkTime = ( pGib->GetNextThink() - gpGlobals->curtime );

		pGib->m_lifeTime = (m_flGibLife * random_valve->RandomFloat( 0.95, 1.05 ));	// +/- 5%

		// HL1 gibs always die after a certain time, other games have to opt-in
		if( HasSpawnFlags( SF_SHOOTER_STRICT_REMOVE ) )
		{
			pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
			pGib->SetThink ( &CGib::DieThink );
		}

		if ( pGib->m_lifeTime < thinkTime )
		{
			pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
			pGib->m_lifeTime = 0;
		}

		if ( m_bIsSprite == true )
		{
			pGib->SetSprite( CSprite::SpriteCreate( STRING( GetModelName() ), pGib->GetAbsOrigin(), false ) );

			CSprite *pSprite = (CSprite*)pGib->GetSprite();

			if ( pSprite )
			{
				pSprite->SetAttachment( pGib, 0 );
				pSprite->SetOwnerEntity( pGib );

				pSprite->SetScale( 1 );
				pSprite->SetTransparency( m_nRenderMode, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, m_nRenderFX );
				pSprite->AnimateForTime( 5, m_flGibLife + 1 ); //This framerate is totally wrong
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CGibShooter::SpawnGib( const Vector &vecShootDir, float flSpeed )
{
	switch (m_nSimulationType)
	{
		case GIB_SIMULATE_RAGDOLL:
		{
			// UNDONE: Assume a mass of 200 for now
			Vector force = vecShootDir * flSpeed * 200;
			return CreateRagGib( STRING( GetModelName() ), GetAbsOrigin(), GetAbsAngles(), force, m_flGibLife );
		}

		case GIB_SIMULATE_PHYSICS:
		{
			CGib *pGib = CreateGib();

			if ( pGib )
			{
				pGib->SetAbsOrigin( GetAbsOrigin() );
				pGib->SetAbsAngles( m_angGibRotation );

				pGib->m_lifeTime = (m_flGibLife * random_valve->RandomFloat( 0.95, 1.05 ));	// +/- 5%

				pGib->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				IPhysicsObject *pPhysicsObject = pGib->VPhysicsInitNormal( SOLID_VPHYSICS, pGib->GetSolidFlags(), false );
				pGib->SetMoveType( MOVETYPE_VPHYSICS );

				if ( pPhysicsObject )
				{
					// Set gib velocity
					Vector vVel		= vecShootDir * flSpeed;
					pPhysicsObject->AddVelocity(&vVel, nullptr);

					AngularImpulse torque;
					torque.x = m_flGibAngVelocity * random_valve->RandomFloat( 0.1f, 1.0f );
					torque.y = m_flGibAngVelocity * random_valve->RandomFloat( 0.1f, 1.0f );
					torque.z = 0.0f;
					torque *= pPhysicsObject->GetMass();

					pPhysicsObject->ApplyTorqueCenter( torque );

					if( HasSpawnFlags( SF_SHOOTER_STRICT_REMOVE ) )
					{
						pGib->m_bForceRemove = true;
						pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
						pGib->SetThink ( &CGib::DieThink );
					}

				}
				else
				{
					InitPointGib( pGib, vecShootDir, flSpeed );
				}
			}
			return pGib;
		}

		case GIB_SIMULATE_POINT:
		{
			CGib *pGib = CreateGib();

			if ( pGib )
			{
				pGib->SetAbsAngles( m_angGibRotation );

				InitPointGib( pGib, vecShootDir, flSpeed );
				return pGib;
			}
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGibShooter::ShootThink ( void )
{
	SetNextThink( gpGlobals->curtime + m_flDelay );

	Vector vecShootDir, vForward,vRight,vUp;
	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	vecShootDir = vForward;
	vecShootDir = vecShootDir + vRight * random_valve->RandomFloat( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + vForward * random_valve->RandomFloat( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + vUp * random_valve->RandomFloat( -1, 1) * m_flVariance;

	VectorNormalize( vecShootDir );

	SpawnGib( vecShootDir, m_flGibVelocity );

	if ( --m_iGibs <= 0 )
	{
		if ( HasSpawnFlags(SF_GIBSHOOTER_REPEATABLE) )
		{
			m_iGibs = m_iGibCapacity;
			SetThink ( nullptr );
			SetNextThink( gpGlobals->curtime );
		}
		else
		{
			SetThink ( &CGibShooter::SUB_Remove );
			SetNextThink( gpGlobals->curtime );
		}
	}
}

class CEnvShooter : public CGibShooter
{
public:
	DECLARE_CLASS( CEnvShooter, CGibShooter );

	CEnvShooter() { m_flGibGravityScale = 1.0f; }
	void		Precache_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool		KeyValue_Impl( const char *szKeyName, const char *szValue );

	CGib		*CreateGib( void );

	DECLARE_DATADESC();

public:

	[[= ks::reflect::Key{ .name = "skin" } ]] int m_nSkin;
	[[= ks::reflect::Key{ .name = "scale" } ]] float m_flGibScale;
	[[= ks::reflect::Key{ .name = "gibgravityscale" } ]] float m_flGibGravityScale;

};

IMPLEMENT_REFLECT_DATAMAP( CEnvShooter )

LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

bool CEnvShooter::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "shootmodel"))
	{
		m_bIsSprite = false;
		SetModelName( AllocPooledString(szValue) );

		//Adrian - not pretty...
		if ( V_stristr( szValue, ".vmt" ) )
			 m_bIsSprite = true;
	}

	else if (FStrEq(szKeyName, "shootsounds"))
	{
		int iNoise = std::atoi(szValue);
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;

		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

void CEnvShooter::Precache_Impl ( void )
{
	m_iGibModelIndex = PrecacheModel( STRING( GetModelName() ) );
}

CGib *CEnvShooter::CreateGib ( void )
{
	CGib *pGib = CREATE_ENTITY( CGib, "gib" );

	if ( m_bIsSprite == true )
	{
		//HACK HACK
		pGib->Spawn( "" );
	}
	else
	{
		pGib->Spawn( STRING( GetModelName() ) );
	}

	int bodyPart = 0;

	if ( m_nMaxGibModelFrame > 1 )
	{
		bodyPart = random_valve->RandomInt( 0, m_nMaxGibModelFrame-1 );
	}

	pGib->m_nBody = bodyPart;
	pGib->SetBloodColor( DONT_BLEED );
	pGib->m_material = m_iGibMaterial;

	pGib->m_nRenderMode = m_nRenderMode;
	pGib->m_clrRender = m_clrRender;
	pGib->m_nRenderFX = m_nRenderFX;
	pGib->m_nSkin = m_nSkin;
	pGib->m_lifeTime = gpGlobals->curtime + m_flGibLife;

	pGib->SetGravity( m_flGibGravityScale );

	// Spawn a flaming gib
	if ( HasSpawnFlags( SF_SHOOTER_FLAMING ) )
	{
		// Tag an entity flame along with us
		CEntityFlame *pFlame = CEntityFlame::Create( pGib, pGib->m_lifeTime );
		if ( pFlame != nullptr )
		{
			pGib->SetFlame( pFlame );
		}
	}

	if ( m_iszLightingOrigin != NULL_STRING )
	{
		// Make the gibs use the lighting origin
		pGib->SetLightingOrigin( m_iszLightingOrigin );
	}

	if( m_bNoGibShadows )
	{
		pGib->AddEffects( EF_NOSHADOW );
	}

	return pGib;
}

//-----------------------------------------------------------------------------
// An entity that shoots out junk when hit by a rotor wash
//-----------------------------------------------------------------------------
class CRotorWashShooter : public CEnvShooter, public IRotorWashShooter
{
public:
	DECLARE_CLASS( CRotorWashShooter, CEnvShooter );
	DECLARE_DATADESC();

	void Spawn_Impl();

public:
	// Inherited from IRotorWashShooter
	virtual CBaseEntity *DoWashPush( float flTimeSincePushStarted, const Vector &vecForce );

private:
	// Amount of time we need to spend under the rotor before we shoot
	[[= ks::reflect::Key{ .name = "rotortime" } ]] float m_flTimeUnderRotor;
	[[= ks::reflect::Key{ .name = "rotortimevariance" } ]] float m_flTimeUnderRotorVariance;

	// Last time we were hit with a wash...
	float m_flLastWashStartTime;
	float m_flNextGibTime;
};

LINK_ENTITY_TO_CLASS( env_rotorshooter, CRotorWashShooter );

//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CRotorWashShooter )

//-----------------------------------------------------------------------------
// Gets at the interface if the entity supports it
//-----------------------------------------------------------------------------
IRotorWashShooter *GetRotorWashShooter( CBaseEntity *pEntity )
{
	CRotorWashShooter *pShooter = dynamic_cast<CRotorWashShooter*>(pEntity);
	return pShooter;
}

//-----------------------------------------------------------------------------
// Inherited from IRotorWashShooter
//-----------------------------------------------------------------------------
void CRotorWashShooter::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	m_flLastWashStartTime = -1;
}

//-----------------------------------------------------------------------------
// Inherited from IRotorWashShooter
//-----------------------------------------------------------------------------
CBaseEntity *CRotorWashShooter::DoWashPush( float flWashStartTime, const Vector &vecForce )
{
	if ( flWashStartTime == m_flLastWashStartTime )
	{
		if ( m_flNextGibTime > gpGlobals->curtime )
			return nullptr;
	}

	m_flLastWashStartTime = flWashStartTime;
	m_flNextGibTime	= gpGlobals->curtime + m_flTimeUnderRotor + random_valve->RandomFloat( -1, 1) * m_flTimeUnderRotorVariance;
	if ( m_flNextGibTime <= gpGlobals->curtime )
	{
		m_flNextGibTime = gpGlobals->curtime + 0.01f;
	}

	// Set the velocity to be what the force would cause it to accelerate to
	// after one tick
	Vector vecShootDir = vecForce;
	VectorNormalize( vecShootDir );

	vecShootDir.x += random_valve->RandomFloat( -1, 1 ) * m_flVariance;
	vecShootDir.y += random_valve->RandomFloat( -1, 1 ) * m_flVariance;
	vecShootDir.z += random_valve->RandomFloat( -1, 1 ) * m_flVariance;

	VectorNormalize( vecShootDir );

	CBaseEntity *pGib = SpawnGib( vecShootDir, m_flGibVelocity /*flLength*/ );

	if ( --m_iGibs <= 0 )
	{
		if ( HasSpawnFlags(SF_GIBSHOOTER_REPEATABLE) )
		{
			m_iGibs = m_iGibCapacity;
		}
		else
		{
			SetThink ( &CGibShooter::SUB_Remove );
			SetNextThink( gpGlobals->curtime );
		}
	}

	return pGib;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTestEffect : public CBaseEntity
{
public:
	DECLARE_CLASS( CTestEffect, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	// bool	KeyValue( const char *szKeyName, const char *szValue );
	void Think_Impl( void );
	void Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iLoop;
	int		m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};

LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

void CTestEffect::Spawn_Impl( void )
{
	Precache( );
}

void CTestEffect::Precache_Impl( void )
{
	PrecacheModel( "sprites/lgtning.vmt" );
}

void CTestEffect::Think_Impl( void )
{
	int i;
	float t = (gpGlobals->curtime - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.vmt", 10 );

		trace_t	tr;

		Vector vecSrc = GetAbsOrigin();
		Vector vecDir = Vector( random_valve->RandomFloat( -1.0, 1.0 ), random_valve->RandomFloat( -1.0, 1.0 ),random_valve->RandomFloat( -1.0, 1.0 ) );
		VectorNormalize( vecDir );
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		pbeam->PointsInit( vecSrc, tr.endpos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 10.0 );
		pbeam->SetScrollRate( 12 );

		m_flBeamTime[m_iBeam] = gpGlobals->curtime;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;

	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->curtime - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->curtime;
		m_iBeam = 0;
		SetNextThink( TICK_NEVER_THINK );
	}
}

void CTestEffect::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_flStartTime = gpGlobals->curtime;
}

// Blood effects
class CBlood : public CPointEntity
{
public:
	DECLARE_CLASS( CBlood, CPointEntity );

	void	Precache_Impl();
	void	Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	inline	int		Color( void ) { return m_Color; }
	inline	float 	BloodAmount( void ) { return m_flAmount; }

	inline	void SetColor( int color ) { m_Color = color; }

	// Input handlers
	[[= ks::reflect::Input{ .name = "EmitBlood", .type = FIELD_VOID } ]] void InputEmitBlood( inputdata_t &inputdata );

	Vector	Direction( void );
	Vector	BloodPosition( CBaseEntity *pActivator );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "spraydir" } ]] Vector m_vecSprayDir;
	[[= ks::reflect::Key{ .name = "amount" } ]] float m_flAmount;
	int m_Color;

private:
};

LINK_ENTITY_TO_CLASS( env_blood, CBlood );

IMPLEMENT_REFLECT_DATAMAP( CBlood )

static constexpr auto SF_BLOOD_RANDOM = 0x0001;
static constexpr auto SF_BLOOD_STREAM = 0x0002;
static constexpr auto SF_BLOOD_PLAYER = 0x0004;
static constexpr auto SF_BLOOD_DECAL = 0x0008;
static constexpr auto SF_BLOOD_CLOUD = 0x0010;
static constexpr auto SF_BLOOD_DROPS = 0x0020;
static constexpr auto SF_BLOOD_GORE = 0x0040;

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CBlood::Precache_Impl()
{
	BaseClass::Precache_Impl();
	UTIL_BloodSprayPrecache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBlood::Spawn_Impl( void )
{
	Precache();

	// Convert spraydir from angles to a vector
	QAngle angSprayDir = QAngle( m_vecSprayDir.x, m_vecSprayDir.y, m_vecSprayDir.z );
	AngleVectors( angSprayDir, &m_vecSprayDir );

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetColor( BLOOD_COLOR_RED );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : szKeyName -
//			szValue -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBlood::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "color"))
	{
		int color = std::atoi(szValue);
		switch ( color )
		{
			case 1:
			{
				SetColor( BLOOD_COLOR_YELLOW );
				break;
			}
		}
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

Vector CBlood::Direction( void )
{
	if ( HasSpawnFlags( SF_BLOOD_RANDOM ) )
		return UTIL_RandomBloodVector();

	return m_vecSprayDir;
}

Vector CBlood::BloodPosition( CBaseEntity *pActivator )
{
	if ( HasSpawnFlags( SF_BLOOD_PLAYER ) )
	{
		CBasePlayer *player;

		if ( pActivator && pActivator->IsPlayer() )
		{
			player = ToBasePlayer( pActivator );
		}
		else
		{
			player = UTIL_GetLocalPlayer();
		}

		if ( player )
		{
			return (player->EyePosition()) + Vector( random_valve->RandomFloat(-10,10), random_valve->RandomFloat(-10,10), random_valve->RandomFloat(-10,10) );
		}
	}

	return GetLocalOrigin();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void UTIL_BloodSprayPrecache()
{
	PrecacheEffect( "bloodspray" );
}

void UTIL_BloodSpray( const Vector &pos, const Vector &dir, int color, int amount, int flags )
{
	if( color == DONT_BLEED )
		return;

	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_fFlags = flags;
	data.m_nColor = color;

	DispatchEffect( "bloodspray", data );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for triggering the blood effect.
//-----------------------------------------------------------------------------
void CBlood::InputEmitBlood( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_BLOOD_STREAM ) )
	{
		UTIL_BloodStream( BloodPosition(inputdata.pActivator), Direction(), Color(), BloodAmount() );
	}
	else
	{
		UTIL_BloodDrips( BloodPosition(inputdata.pActivator), Direction(), Color(), BloodAmount() );
	}

	if ( HasSpawnFlags( SF_BLOOD_DECAL ) )
	{
		Vector forward = Direction();
		Vector start = BloodPosition( inputdata.pActivator );
		trace_t tr;

		UTIL_TraceLine( start, start + forward * BloodAmount() * 2, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &tr, Color() );
		}
	}

	//
	// New-fangled blood effects.
	//
	if ( HasSpawnFlags( SF_BLOOD_CLOUD | SF_BLOOD_DROPS | SF_BLOOD_GORE ) )
	{
		int nFlags = 0;
		if (HasSpawnFlags(SF_BLOOD_CLOUD))
		{
			nFlags |= FX_BLOODSPRAY_CLOUD;
		}

		if (HasSpawnFlags(SF_BLOOD_DROPS))
		{
			nFlags |= FX_BLOODSPRAY_DROPS;
		}

		if (HasSpawnFlags(SF_BLOOD_GORE))
		{
			nFlags |= FX_BLOODSPRAY_GORE;
		}

		UTIL_BloodSpray(GetAbsOrigin(), Direction(), Color(), BloodAmount(), nFlags);
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CEnvFunnel : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvFunnel, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iSprite;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------

void CEnvFunnel::Precache_Impl ( void )
{
	m_iSprite = PrecacheModel ( "sprites/flare6.vmt" );
}

void CEnvFunnel::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBroadcastRecipientFilter filter;
	te->LargeFunnel( filter, 0.0,
		&GetAbsOrigin(), m_iSprite, HasSpawnFlags( SF_FUNNEL_REVERSE ) ? 1 : 0 );

	SetThink( &CEnvFunnel::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

void CEnvFunnel::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
}

//=========================================================
// Beverage Dispenser
// overloaded m_iHealth, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvBeverage, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );

	// Input handlers.
	[[= ks::reflect::Input{ .name = "Activate", .type = FIELD_VOID } ]] void	InputActivate( inputdata_t &inputdata );

	DECLARE_DATADESC();

public:
	bool	m_CanInDispenser;
	int		m_nBeverageType;
};

void CEnvBeverage::Precache_Impl ( void )
{
	PrecacheModel( "models/can.mdl" );
}

IMPLEMENT_REFLECT_DATAMAP( CEnvBeverage )

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );

bool CEnvBeverage::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "beveragetype"))
	{
		m_nBeverageType = std::atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

void CEnvBeverage::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_CanInDispenser || m_iHealth <= 0 )
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	CBaseAnimating *pCan = (CBaseAnimating *)CBaseEntity::Create( "item_sodacan", GetLocalOrigin(), GetLocalAngles(), this );

	if ( m_nBeverageType == 6 )
	{
		// random
		pCan->m_nSkin = random_valve->RandomInt( 0, 5 );
	}
	else
	{
		pCan->m_nSkin = m_nBeverageType;
	}

	m_CanInDispenser = true;
	m_iHealth -= 1;

	//SetThink (SUB_Remove);
	//SetNextThink( gpGlobals->curtime );
}

void CEnvBeverage::InputActivate( inputdata_t &inputdata )
{
	Use( inputdata.pActivator, inputdata.pCaller, USE_ON, 0 );
}

void CEnvBeverage::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	m_CanInDispenser = false;

	if ( m_iHealth == 0 )
	{
		m_iHealth = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseAnimating
{
public:
	DECLARE_CLASS( CItemSoda, CBaseAnimating );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	CanThink ( void );
	void	CanTouch ( CBaseEntity *pOther );

};

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Precache_Impl ( void )
{
	PrecacheModel( "models/can.mdl" );

	PrecacheScriptSound( "ItemSoda.Bounce" );
}

void CItemSoda::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_FLYGRAVITY );

	SetModel ( "models/can.mdl" );
	UTIL_SetSize ( this, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );

	SetThink (&CItemSoda::CanThink);
	SetNextThink( gpGlobals->curtime + 0.5f );
}

void CItemSoda::CanThink ( void )
{
	EmitSound( "ItemSoda.Bounce" );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	UTIL_SetSize ( this, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );

	SetThink ( nullptr );
	SetTouch ( &CItemSoda::CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( GetOwnerEntity() )
	{
		// tell the machine the can was taken
		CEnvBeverage *bev = (CEnvBeverage *)GetOwnerEntity();
		bev->m_CanInDispenser = false;
	}

	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetTouch ( nullptr );
	SetThink ( &CItemSoda::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

//=========================================================
// func_precipitation - temporary snow solution for first HL2
// technology demo
//=========================================================

class CPrecipitation : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Precipitation" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPrecipitation, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPrecipitation();
	int UpdateTransmitState_Impl();
	void	Spawn_Impl( void );

	// bits: Q_log2( NUM_PRECIPITATION_TYPES ) + 1, spelled out because Q_log2 is a runtime call.
	CNetworkVar( PrecipitationType_t, m_nPrecipType, [[= ks::reflect::Net{ .bits = 4, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "preciptype" } ]] );
};

LINK_ENTITY_TO_CLASS( func_precipitation, CPrecipitation );

IMPLEMENT_REFLECT_DATAMAP( CPrecipitation )

// Just send the normal entity crap
IMPLEMENT_REFLECT_SERVERCLASS( CPrecipitation, DT_Precipitation )

CPrecipitation::CPrecipitation()
{
	m_nPrecipType = PRECIPITATION_TYPE_RAIN; // default to rain.
}

int CPrecipitation::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CPrecipitation::Spawn_Impl( void )
{
	//SetTransmitState( FL_EDICT_ALWAYS );
	SetTransmitState( FL_EDICT_PVSCHECK );

	PrecacheMaterial( "effects/fleck_ash1" );
	PrecacheMaterial( "effects/fleck_ash2" );
	PrecacheMaterial( "effects/fleck_ash3" );
	PrecacheMaterial( "effects/ember_swirling001" );
	Precache();

	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );		// Set size
	if ( m_nPrecipType == PRECIPITATION_TYPE_PARTICLERAIN )
	{
		SetSolid( SOLID_VPHYSICS );
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddSolidFlags( FSOLID_FORCE_WORLD_ALIGNED );
		VPhysicsInitStatic();
	}
	else
	{
		SetSolid( SOLID_NONE );							// Remove model & collisions
	}

	// Default to rain.
	if ( m_nPrecipType < 0 || m_nPrecipType > NUM_PRECIPITATION_TYPES )
		m_nPrecipType = PRECIPITATION_TYPE_RAIN;

	m_nRenderMode = kRenderEnvironmental;
}

//=========================================================
// func_precipitation_blocker - prevents precipitation from happening in this volume
//=========================================================

class CPrecipitationBlocker : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_PrecipitationBlocker" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CPrecipitationBlocker, CBaseEntity );
	DECLARE_SERVERCLASS();

	CPrecipitationBlocker();
	void	Spawn_Impl( void );
	int		UpdateTransmitState_Impl( void );
};

LINK_ENTITY_TO_CLASS( func_precipitation_blocker, CPrecipitationBlocker );

// Just send the normal entity crap
IMPLEMENT_REFLECT_SERVERCLASS( CPrecipitationBlocker, DT_PrecipitationBlocker )

CPrecipitationBlocker::CPrecipitationBlocker()
{
}

int CPrecipitationBlocker::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CPrecipitationBlocker::Spawn_Impl( void )							   
{
	SetTransmitState( FL_EDICT_ALWAYS );
	Precache();
	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );		// Set size

	m_nRenderMode = kRenderEnvironmental;
}

//--------------------------------------------------------------------------------------------------------
class CDetailBlocker : public CServerOnlyEntity
{
	DECLARE_CLASS( CDetailBlocker, CServerOnlyEntity );
public:

	CDetailBlocker() : CServerOnlyEntity() {}
	virtual ~CDetailBlocker() {}
};

LINK_ENTITY_TO_CLASS( func_detail_blocker, CDetailBlocker );

//-----------------------------------------------------------------------------
// EnvWind - global wind info
//-----------------------------------------------------------------------------
class CEnvWind : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvWind", .base = false } ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_iMinWind", ks::reflect::Key{ .name = "minwind" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_iMaxWind", ks::reflect::Key{ .name = "maxwind" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_iMinGust", ks::reflect::Key{ .name = "mingust" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_iMaxGust", ks::reflect::Key{ .name = "maxgust" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_flMinGustDelay", ks::reflect::Key{ .name = "mingustdelay" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_flMaxGustDelay", ks::reflect::Key{ .name = "maxgustdelay" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_iGustDirChange", ks::reflect::Key{ .name = "gustdirchange" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_flGustDuration", ks::reflect::Key{ .name = "gustduration" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_OnGustStart", ks::reflect::Key{ .name = "OnGustStart" } >{} ]]
	[[= ks::reflect::KeyFrom<"m_EnvWindShared.m_OnGustEnd", ks::reflect::Key{ .name = "OnGustEnd" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvWind, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	WindThink( void );
	int		UpdateTransmitState_Impl( void );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

private:
	[[= ks::reflect::Net{} ]] CEnvWindShared m_EnvWindShared; // FIXME - fails to compile as networked var due to operator= problem
};

LINK_ENTITY_TO_CLASS( env_wind, CEnvWind );

IMPLEMENT_REFLECT_DATAMAP( CEnvWind )

IMPLEMENT_REFLECT_TABLE( CEnvWindShared, DT_EnvWindShared );

// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CEnvWind, DT_EnvWind )

void CEnvWind::Precache_Impl ( void )
{
//	if (m_iszGustSound)
//	{
//		PrecacheScriptSound( STRING( m_iszGustSound ) );
//	}
}

void CEnvWind::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	m_EnvWindShared.m_iInitialWindDir = (int)( anglemod( m_EnvWindShared.m_iInitialWindDir ) );
	m_EnvWindShared.Init( entindex(), 0, gpGlobals->curtime, GetLocalAngles().y, 0 );

	SetThink( &CEnvWind::WindThink );
	SetNextThink( gpGlobals->curtime );
}

int CEnvWind::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CEnvWind::WindThink( void )
{
	SetNextThink( m_EnvWindShared.WindThink( gpGlobals->curtime ) );
}

//==================================================
// CEmbers
//==================================================

static constexpr auto bitsSF_EMBERS_START_ON = 0x00000001;
static constexpr auto bitsSF_EMBERS_TOGGLE = 0x00000002;

// UNDONE: This is a brush effect-in-volume entity, move client side.
class CEmbers : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_Embers" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEmbers, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	void	EmberUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CNetworkVar( int, m_nDensity, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "density" } ]] );
	CNetworkVar( int, m_nLifetime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "lifetime" } ]] );
	CNetworkVar( int, m_nSpeed, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "speed" } ]] );

	CNetworkVar( bool, m_bEmit, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};

LINK_ENTITY_TO_CLASS( env_embers, CEmbers );

//Data description
IMPLEMENT_REFLECT_DATAMAP( CEmbers )

//Data table
IMPLEMENT_REFLECT_SERVERCLASS( CEmbers, DT_Embers )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEmbers::Spawn_Impl( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	SetSolid( SOLID_NONE );
	SetRenderAlpha( 0 );
	m_nRenderMode	= kRenderTransTexture;

	SetUse( &CEmbers::EmberUse );

	//Start off if we're targetted (unless flagged)
	m_bEmit = ( HasSpawnFlags( bitsSF_EMBERS_START_ON ) || ( !GetEntityName() ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEmbers::Precache_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pActivator -
//			*pCaller -
//			useType -
//			value -
//-----------------------------------------------------------------------------
void CEmbers::EmberUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//If we're not toggable, only allow one use
	if ( !HasSpawnFlags( bitsSF_EMBERS_TOGGLE ) )
	{
		SetUse( nullptr );
	}

	//Handle it
	switch ( useType )
	{
	case USE_OFF:
		m_bEmit = false;
		break;

	case USE_ON:
		m_bEmit = true;
		break;

	case USE_SET:
		m_bEmit = !!(int)value;
		break;

	default:
	case USE_TOGGLE:
		m_bEmit = !m_bEmit;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CPhysicsWire : public CBaseEntity
{
public:
	DECLARE_CLASS( CPhysicsWire, CBaseEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

	DECLARE_DATADESC();

protected:

	bool SetupPhysics( void );

	[[= ks::reflect::Key{ .name = "Density" } ]] int		m_nDensity;
};

LINK_ENTITY_TO_CLASS( env_physwire, CPhysicsWire );

IMPLEMENT_REFLECT_DATAMAP( CPhysicsWire )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysicsWire::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	Precache();

//	if ( SetupPhysics() == false )
//		return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysicsWire::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
}

class CPhysBallSocket;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPhysicsWire::SetupPhysics( void )
{
/*
	CPointEntity	*anchorEnt, *freeEnt;
	CPhysBallSocket *socket;

	char	anchorName[256];
	char	freeName[256];

	int		iAnchorName, iFreeName;

	anchorEnt = (CPointEntity *) CreateEntityByName( "info_target" );

	if ( anchorEnt == NULL )
		return false;

	//Create and connect all segments
	for ( int i = 0; i < m_nDensity; i++ )
	{
		// Create other end of our link
		freeEnt = (CPointEntity *) CreateEntityByName( "info_target" );

		// Create a ballsocket and attach the two
		//socket = (CPhysBallSocket *) CreateEntityByName( "phys_ballsocket" );

		V_snprintf( anchorName,sizeof(anchorName), "__PWIREANCHOR%d", i );
		V_snprintf( freeName,sizeof(freeName), "__PWIREFREE%d", i+1 );

		iAnchorName = MAKE_STRING( anchorName );
		iFreeName	= MAKE_STRING( freeName );

		//Fake the names
		//socket->m_nameAttach1 = anchorEnt->m_iGlobalname	= iAnchorName;
		//socket->m_nameAttach2 = freeEnt->m_iGlobalname	= iFreeName

		//socket->Activate();

		//The free ent is now the anchor for the next link
		anchorEnt = freeEnt;
	}
*/

	return true;
}

//
// Muzzle flash
//

class CEnvMuzzleFlash : public CPointEntity
{
	DECLARE_CLASS( CEnvMuzzleFlash, CPointEntity );

public:
	void Spawn_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Fire", .type = FIELD_VOID } ]] void	InputFire( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "scale" } ]] float	m_flScale;
	[[= ks::reflect::Key{ .name = "parentattachment" } ]] string_t m_iszParentAttachment;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvMuzzleFlash )

LINK_ENTITY_TO_CLASS( env_muzzleflash, CEnvMuzzleFlash );

//-----------------------------------------------------------------------------
// Spawn!
//-----------------------------------------------------------------------------
void CEnvMuzzleFlash::Spawn_Impl()
{
	if ( (m_iszParentAttachment != NULL_STRING) && GetParent() && GetParent()->GetBaseAnimating() )
	{
		CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
		int nParentAttachment = pAnim->LookupAttachment( STRING(m_iszParentAttachment) );
		if ( nParentAttachment != 0 )
		{
			SetParent( GetParent(), nParentAttachment );
			SetLocalOrigin( vec3_origin );
			SetLocalAngles( vec3_angle );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CEnvMuzzleFlash::InputFire( inputdata_t &inputdata )
{
	g_pEffects->MuzzleFlash( GetAbsOrigin(), GetAbsAngles(), m_flScale, MUZZLEFLASH_TYPE_DEFAULT );
}

//=========================================================
// Splash!
//=========================================================
static constexpr auto SF_ENVSPLASH_FINDWATERSURFACE = 0x00000001;
static constexpr auto SF_ENVSPLASH_DIMINISH = 0x00000002;
class CEnvSplash : public CPointEntity
{
	DECLARE_CLASS( CEnvSplash, CPointEntity );

public:
	void Precache_Impl();
	void Spawn_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "Splash", .type = FIELD_VOID } ]] void	InputSplash( inputdata_t &inputdata );

protected:

	[[= ks::reflect::Key{ .name = "scale" } ]] float	m_flScale;

	DECLARE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP( CEnvSplash )

LINK_ENTITY_TO_CLASS( env_splash, CEnvSplash );

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------

void CEnvSplash::Precache_Impl()
{
	BaseClass::Precache_Impl();
	PrecacheEffect( "watersplash" );
}

void CEnvSplash::Spawn_Impl()
{
	Precache();
	BaseClass::Spawn_Impl();
}

static constexpr auto SPLASH_MAX_DEPTH = 120.0f;
void CEnvSplash::InputSplash( inputdata_t &inputdata )
{
	CEffectData	data;

	data.m_fFlags = 0;

	float scale = m_flScale;

	if( HasSpawnFlags( SF_ENVSPLASH_FINDWATERSURFACE ) )
	{
		if( UTIL_PointContents(GetAbsOrigin(), MASK_WATER) & MASK_WATER )
		{
			// No splash if I'm supposed to find the surface of the water, but I'm underwater.
			return;
		}

		// Trace down and find the water's surface. This is designed for making
		// splashes on the surface of water that can change water level.
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 4096 ), (MASK_WATER|MASK_SOLID_BRUSHONLY), this, COLLISION_GROUP_NONE, &tr );
		data.m_vOrigin = tr.endpos;

		if ( tr.contents & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}
	}
	else
	{
		data.m_vOrigin = GetAbsOrigin();
	}

	if( HasSpawnFlags( SF_ENVSPLASH_DIMINISH ) )
	{
		// Get smaller if I'm in deeper water.
		float depth = 0.0f;

		trace_t tr;
		UTIL_TraceLine( data.m_vOrigin, data.m_vOrigin - Vector( 0, 0, 4096 ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		depth = fabs( tr.startpos.z - tr.endpos.z );

		float factor = 1.0f - (depth / SPLASH_MAX_DEPTH);

		if( factor < 0.1 )
		{
			// Don't bother making one this small.
			return;
		}

		scale *= factor;
	}

	data.m_vNormal = Vector( 0, 0, 1 );
	data.m_flScale = scale;

	DispatchEffect( "watersplash", data );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CEnvGunfire : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvGunfire, CPointEntity );

	CEnvGunfire()
	{
		// !!!HACKHACK
		// These fields came along kind of late, so they get
		// initialized in the constructor for now. (sjb)
		m_flBias = 1.0f;
		m_bCollide = false;
	}

	void Precache_Impl();
	void Spawn_Impl();
	void Activate_Impl();
	void StartShooting();
	void StopShooting();
	void ShootThink();
	void UpdateTarget();
	void FireBullet(
		Vector vecSrc,	// shooting postion
		const QAngle &shootAngles,  //shooting angle
		float flDistance, // max distance 
		float flPenetration, // the power of the penetration
		int nPenetrationCount,
		int iBulletType, // ammo type
		int iDamage, // base damage
		float flRangeModifier, // damage range modifier
		CBaseEntity *pevAttacker, // shooter
		bool bDoEffects,
		float xSpread, float ySpread,
		const char *pszTracerName
		);

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "minburstsize" } ]] int	m_iMinBurstSize;
	[[= ks::reflect::Key{ .name = "maxburstsize" } ]] int m_iMaxBurstSize;

	[[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Key{ .name = "minburstdelay" } ]] float m_flMinBurstDelay;
	[[= ks::reflect::As{ FIELD_TIME } ]] [[= ks::reflect::Key{ .name = "maxburstdelay" } ]] float m_flMaxBurstDelay;

	[[= ks::reflect::Key{ .name = "rateoffire" } ]] float m_flRateOfFire;

	[[= ks::reflect::Key{ .name = "shootsound" } ]] string_t	m_iszShootSound;
	[[= ks::reflect::Key{ .name = "tracertype" } ]] string_t	m_iszTracerType;
	[[= ks::reflect::Key{ .name = "weaponname" } ]] string_t	m_iszWeaponName;

	[[= ks::reflect::Key{ .name = "startdisabled" } ]] bool m_bDisabled;

	int	m_iShotsRemaining;

	[[= ks::reflect::Key{ .name = "spread" } ]] int		m_iSpread;
	Vector	m_vecSpread;
	Vector	m_vecTargetPosition;
	float	m_flTargetDist;

	[[= ks::reflect::Key{ .name = "bias" } ]] float	m_flBias;
	[[= ks::reflect::Key{ .name = "collisions" } ]] bool	m_bCollide;

	EHANDLE m_hTarget;

	DECLARE_DATADESC();
};

IMPLEMENT_REFLECT_DATAMAP( CEnvGunfire )
LINK_ENTITY_TO_CLASS( env_gunfire, CEnvGunfire );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Precache_Impl()
{
	PrecacheScriptSound( STRING( m_iszShootSound ) );
	PrecacheEffect( "impact_wallbang_heavy" );
	PrecacheEffect( "impact_wallbang_light" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Spawn_Impl()
{
	Precache();

	m_iShotsRemaining = 0;
	m_flRateOfFire = 1.0f / m_flRateOfFire;

	switch( m_iSpread )
	{
	case 1:
		m_vecSpread = VECTOR_CONE_1DEGREES;
		break;
	case 5:
		m_vecSpread = VECTOR_CONE_5DEGREES;
		break;
	case 10:
		m_vecSpread = VECTOR_CONE_10DEGREES;
		break;
	case 15:
		m_vecSpread = VECTOR_CONE_15DEGREES;
		break;

	default:
		m_vecSpread = vec3_origin;
		break;
	}

	if( !m_bDisabled )
	{
		StartShooting();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Activate_Impl( void )
{
	// Find my target
	if (m_target != NULL_STRING)
	{
		m_hTarget = gEntList.FindEntityByName( nullptr, m_target );
	}

	BaseClass::Activate_Impl();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::StartShooting()
{
	m_iShotsRemaining = random_valve->RandomInt( m_iMinBurstSize, m_iMaxBurstSize );

	SetThink( &CEnvGunfire::ShootThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::UpdateTarget()
{
	if( m_hTarget )
	{
		if( m_hTarget->WorldSpaceCenter() != m_vecTargetPosition )
		{
			// Target has moved.
			// Locate my target and cache the position and distance.
			m_vecTargetPosition = m_hTarget->WorldSpaceCenter();
			m_flTargetDist = (GetAbsOrigin() - m_vecTargetPosition).Length();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::StopShooting()
{
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::ShootThink()
{
	if( !m_hTarget )
	{
		StopShooting();
	}

	UpdateTarget();

	Vector vecDir = m_vecTargetPosition - GetAbsOrigin();
	VectorNormalize( vecDir );

	CShotManipulator manipulator( vecDir );

	vecDir = manipulator.ApplySpread( m_vecSpread, m_flBias );

	Vector vecEnd;

	CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( STRING(m_iszWeaponName) );
	if ( pItemDef && pItemDef->GetDefinitionIndex() != 0 )
	{
		CSWeaponID wid = WeaponIdFromString( STRING(m_iszWeaponName) );
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( wid );
		
		uint16 nItemDefIndex = pItemDef->GetDefinitionIndex();
	
		QAngle angDir;
		VectorAngles( vecDir, angDir );

		FX_FireBullets(
			entindex(),
			nItemDefIndex,
			GetAbsOrigin(),
			angDir,
			wid,
			0,
			CBaseEntity::GetPredictionRandomSeed(  ) & 255,
			0,
			0,
			0,
			0,
			SINGLE,
			0 );

		int nPenetrationCount = 4;

		int		iDamage = pWeaponInfo->GetDamage( nullptr );
		float	flRange = pWeaponInfo->GetRange( nullptr );
		float	flPenetration = pWeaponInfo->GetPenetration( nullptr );
		float	flRangeModifier = pWeaponInfo->GetRangeModifier( nullptr );
		int		iAmmoType = pWeaponInfo->GetPrimaryAmmoType( nullptr );

		FireBullet(
			GetAbsOrigin(),
			angDir,
			flRange,
			flPenetration,
			nPenetrationCount,
			iAmmoType,
			iDamage,
			flRangeModifier,
			this,
			true,
			0, 0,
			pWeaponInfo->GetTracerEffectName() );

		m_iShotsRemaining--;
		float flNextShot = gpGlobals->curtime + pWeaponInfo->GetCycleTime();
		SetNextThink( flNextShot );

		if( m_iShotsRemaining == 0 )
		{
			StartShooting();
			SetNextThink( gpGlobals->curtime + random_valve->RandomFloat( m_flMinBurstDelay, m_flMaxBurstDelay ) );	
		}
	}

}

static const int kMaxNumPenetrationsSupported = 4;
struct DelayedDamageInfoData_t
{
	CTakeDamageInfo m_info;
	trace_t m_tr;

	typedef ks::small_vector< DelayedDamageInfoData_t, kMaxNumPenetrationsSupported > Array;
};

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask,
											 const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if ( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

extern ConVar sv_showbullethits;
extern ConVar sv_penetration_type;
#define	CS_MASK_SHOOT (MASK_SOLID|CONTENTS_DEBRIS)

void CEnvGunfire::FireBullet(
	Vector vecSrc,	// shooting postion
	const QAngle &shootAngles,  //shooting angle
	float flDistance, // max distance 
	float flPenetration, // the power of the penetration
	int nPenetrationCount,
	int iBulletType, // ammo type
	int iDamage, // base damage
	float flRangeModifier, // damage range modifier
	CBaseEntity *pevAttacker, // shooter
	bool bDoEffects,
	float xSpread, float ySpread,
	const char *pszTracerName
	)
{
	CCSPlayer *pPlayer = nullptr;
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
			break;
	}

	float fCurrentDamage = iDamage;   // damage of the bullet at it's current trajectory
	float flCurrentDistance = 0.0;  //distance that the bullet has traveled so far

	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( shootAngles, &vecDirShooting, &vecRight, &vecUp );

	// MIKETODO: put all the ammo parameters into a script file and allow for CS-specific params.
	float flPenetrationPower = 35;		// thickness of a wall that this bullet can penetrate
	float flPenetrationDistance = 3000;	// distance at which the bullet is capable of penetrating a wall
	float flDamageModifier = 0.5f;		// default modification of bullets power after they go through a wall.
	float flPenetrationModifier = 1.0f;

	// we use the max penetrations on this gun to figure out how much penetration it's capable of
	if ( sv_penetration_type.GetInt() == 1 )
		flPenetrationPower = flPenetration;

	if ( !pevAttacker )
		pevAttacker = this;  // the default attacker is ourselves

	// add the spray 
	Vector vecDir = vecDirShooting + xSpread * vecRight + ySpread * vecUp;

	VectorNormalize( vecDir );

	bool bFirstHit = true;

	const CBaseCombatCharacter *lastPlayerHit = nullptr;	// this includes players, bots, and hostages

//#ifdef CLIENT_DLL
	Vector vecWallBangHitStart, vecWallBangHitEnd;
	vecWallBangHitStart.Init();
	vecWallBangHitEnd.Init();
	bool bWallBangStarted = false;
	bool bWallBangEnded = false;
	bool bWallBangHeavyVersion = false;
//#endif

	bool bBulletHitPlayer = false;

	MDLCACHE_CRITICAL_SECTION();

//#ifndef CLIENT_DLL
	DelayedDamageInfoData_t::Array arrPendingDamage;
//#endif

	bool bShotHitTeammate = false;

	float flDist_aim = 0;
	while ( fCurrentDamage > 0 )
	{
		Vector vecEnd = vecSrc + vecDir * ( flDistance - flCurrentDistance );

		trace_t tr; // main enter bullet trace

		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEnd, CS_MASK_SHOOT | CONTENTS_HITBOX, this, lastPlayerHit, COLLISION_GROUP_NONE, &tr );
		{
			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );

			// Check for player hitboxes extending outside their collision bounds
			const float rayExtension = 40.0f;
			UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vecDir * rayExtension, CS_MASK_SHOOT | CONTENTS_HITBOX, &filter, &tr );
		}

		if ( !flDist_aim )
		{
			flDist_aim = ( tr.fraction != 1.0 ) ? ( tr.startpos - tr.endpos ).Length() : 0;
		}

		lastPlayerHit = dynamic_cast< const CBaseCombatCharacter * >( tr.Ent<CBaseEntity>() );

		if ( lastPlayerHit )
		{
			if ( lastPlayerHit->GetTeamNumber() == GetTeamNumber() )
			{
				bShotHitTeammate = true;
			}

			bBulletHitPlayer = true;
		}

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

//#ifdef CLIENT_DLL
		if ( !bWallBangStarted && !bBulletHitPlayer )
		{
			vecWallBangHitStart = tr.endpos;
			vecWallBangHitEnd = tr.endpos;
			bWallBangStarted = true;

			if ( fCurrentDamage > 20 )
				bWallBangHeavyVersion = true;
		}
		else if ( !bWallBangEnded )
		{
			vecWallBangHitEnd = tr.endpos;

			if ( bBulletHitPlayer )
				bWallBangEnded = true;
		}
//#endif

		bFirstHit = false;

		/************* MATERIAL DETECTION ***********/
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		int iEnterMaterial = pSurfaceData->game.material;

		flPenetrationModifier = pSurfaceData->game.penetrationModifier;
		flDamageModifier = pSurfaceData->game.damageModifier;

		bool hitGrate = ( tr.contents & CONTENTS_GRATE ) != 0;

		//calculate the damage based on the distance the bullet travelled.
		flCurrentDistance += tr.fraction * ( flDistance - flCurrentDistance );
		fCurrentDamage *= std::pow( flRangeModifier, ( flCurrentDistance / 500 ) );

		// the value of iPenetration when the round reached its max penetration distance
		int nPenetrationAtMaxDistance = 0;

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifyer is super low, just stop the bullet
		if ( ( flCurrentDistance > flPenetrationDistance && flPenetration > 0 ) ||
			 flPenetrationModifier < 0.1 )
		{
			nPenetrationAtMaxDistance = 0;
			nPenetrationCount = 0;
		}

		int iDamageType = DMG_BULLET | DMG_NEVERGIB;
		//CWeaponCSBase* pActiveWeapon = NULL;

		if ( bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos, MASK_WATER ) & ( CONTENTS_WATER | CONTENTS_SLIME ) )
			{
				trace_t waterTrace;
				UTIL_TraceLine( vecSrc, tr.endpos, ( MASK_SHOT | CONTENTS_WATER | CONTENTS_SLIME ), this, COLLISION_GROUP_NONE, &waterTrace );

				if ( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random_valve->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & ( SURF_SKY | SURF_NODRAW | SURF_HINT | SURF_SKIP ) ) )
				{
					//CBaseEntity *pEntity = tr.Ent<CBaseEntity>();
					UTIL_ImpactTrace( &tr, iDamageType );
				}
			}
		}

		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( tr.DidHit() && tr.Ent<CBaseEntity>() && tr.Ent<CBaseEntity>()->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}

// #ifdef CLIENT_DLL
// 		// create the tracer
// 		CreateWeaponTracer( vecSrc, tr.endpos );
// #endif
		if ( bWallBangStarted == false )
			UTIL_ParticleTracer( pszTracerName, vecSrc, tr.endpos, entindex() );

		// add damage to entity that we hit

		// CBaseEntity *pEntity = tr.Ent<CBaseEntity>();

		//Shooting dropped grenades detonates them
		//
		// DAMAGE MUST BE DEFERRED TILL LATER IF WE DECIDE TO SHIP IT
		//
		// 		if ( sv_shoot_dropped_grenades.GetBool() )
		// 		{
		// 			CBaseCSGrenade* pWeapon = dynamic_cast<CBaseCSGrenade*>( pEntity );
		// 			//Only detonate shot grenades if they have been dropped in the world longer than the grace period.
		// 			//This prevents shooting at players and they miraculously explode - because you shot their grenade the instant they died
		// 			if ( pWeapon && gpGlobals->curtime > (pWeapon->m_flDroppedAtTime + sv_shoot_dropped_grenades_grace_time.GetFloat()) )
		// 			{
		// 				pWeapon->ShotDetonate( this, pWeapon->GetCSWpnData() );
		// 				pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
		// 				pWeapon->AddEffects( EF_NODRAW );
		// 				UTIL_Remove( pWeapon );
		// 			}
		// 		}

		// [pfreese] Check if enemy players were killed by this bullet, and if so,
		// add them to the iPenetrationKills count

		DelayedDamageInfoData_t &delayedDamage = arrPendingDamage.emplace_back();
		delayedDamage.m_tr = tr;

		int nObjectsPenetrated = kMaxNumPenetrationsSupported - ( nPenetrationCount + nPenetrationAtMaxDistance );
		CTakeDamageInfo &info = delayedDamage.m_info;
		info.Set( pevAttacker, pevAttacker, nullptr/*GetActiveWeapon()*/, fCurrentDamage, iDamageType, 0, nObjectsPenetrated );

		// Set the bullet ID so that we can later track all the enemies that are damage by the same bullet
		info.SetAmmoType( iBulletType );
		CalculateBulletDamageForce( &info, iBulletType, vecDir, tr.endpos );

		//bool bWasAlive = pEntity->IsAlive();

		// === Damage applied later ===

		// [dkorus] note: values are changed inside of HandleBulletPenetration
		bool bulletStopped = pPlayer->HandleBulletPenetration( flPenetration, iEnterMaterial, hitGrate, tr, vecDir, pSurfaceData, flPenetrationModifier,
													  flDamageModifier, bDoEffects, iDamageType, flPenetrationPower, nPenetrationCount, vecSrc, flDistance,
													  flCurrentDistance, fCurrentDamage );

		// [dkorus] bulletStopped is true if the bullet can no longer continue penetrating materials
		if ( bulletStopped )
			break;
	}

	for ( auto &pendingDamage : arrPendingDamage )
	{
		ClearMultiDamage();

		CTakeDamageInfo &info = pendingDamage.m_info;
		trace_t &tr = pendingDamage.m_tr;

		CBaseEntity *pEntity = tr.Ent<CBaseEntity>();
		//bool bWasAlive = pEntity->IsAlive();

		pEntity->DispatchTraceAttack( info, vecDir, &tr );
		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );
		ApplyMultiDamage();

// 		if ( bWasAlive && !pEntity->IsAlive() && pEntity->IsPlayer() && IsOtherEnemy( pEntity->entindex() ) )
// 		{
// 			++iPenetrationKills;
// 		}
	}

//#ifdef CLIENT_DLL
	if ( bWallBangStarted )
	{
		float flWallBangLength = ( vecWallBangHitEnd - vecWallBangHitStart ).Length();
		if ( flWallBangLength > 0 && flWallBangLength < 800 )
		{
			QAngle temp;
			VectorAngles( vecWallBangHitEnd - vecWallBangHitStart, temp );

// 			CEffectData	data;
// 			data.m_vOrigin = vecWallBangHitStart;
// 			data.m_vStart = vecWallBangHitEnd;
// 			data.m_vAngles = temp;
// 			//data.m_vNormal = vecWallBangHitStart - vecWallBangHitEnd;
// 			data.m_flScale = 1.0f;

			//why is particle system index stored on m_nHitBox?
			if ( bWallBangHeavyVersion )
			{
				//data.m_nHitBox = GetParticleSystemIndex( "impact_wallbang_heavy" );
				//UTIL_Tracer( vecWallBangHitStart, vecWallBangHitEnd, 0, TRACER_DONT_USE_ATTACHMENT, 0, true, "impact_wallbang_heavy"/*, data.m_nHitBox */);
				UTIL_ParticleTracer( "impact_wallbang_heavy", vecWallBangHitStart, vecWallBangHitEnd, entindex() );
			}
			else
			{
				//data.m_nHitBox = GetParticleSystemIndex( "impact_wallbang_light" );
				//UTIL_Tracer( vecWallBangHitStart, vecWallBangHitEnd, 0, TRACER_DONT_USE_ATTACHMENT, 0, true, "impact_wallbang_light"/*, data.m_nHitBox*/ );
				UTIL_ParticleTracer( "impact_wallbang_light", vecWallBangHitStart, vecWallBangHitEnd, entindex() );
			}

			//StartParticleEffect( data );

			//debugoverlay->AddLineOverlay( vecWallBangHitStart, vecWallBangHitEnd, 0, 255, 0, false, 3 );
		}
	}
//#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
	StartShooting();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
// Quadratic spline beam effect
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( env_quadraticbeam, CEnvQuadraticBeam );

IMPLEMENT_REFLECT_SERVERCLASS( CEnvQuadraticBeam, DT_QuadraticBeam )

void CEnvQuadraticBeam::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	m_nRenderMode = kRenderTransAdd;
	SetRenderColor( 255, 255, 255 );
}

CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner )
{
	CEnvQuadraticBeam *pBeam = (CEnvQuadraticBeam *)CBaseEntity::Create( "env_quadraticbeam", start, vec3_angle, pOwner );
	UTIL_SetModel( pBeam, pSpriteName );
	pBeam->SetSpline( control, end );
	pBeam->SetScrollRate( 0.0 );
	pBeam->SetWidth(width);
	return pBeam;
}

PRECACHE_REGISTER_BEGIN( GLOBAL, EffectsPrecache )
	PRECACHE( GAMESOUND, "Underwater.BulletImpact" )
	PRECACHE( GAMESOUND, "FX_RicochetSound.Ricochet" )
	PRECACHE(GAMESOUND, "FX_RicochetSound.Ricochet_Legacy")
	PRECACHE( GAMESOUND, "Physics.WaterSplash" )
	PRECACHE( GAMESOUND, "BaseExplosionEffect.Sound" )
	PRECACHE( GAMESOUND, "Splash.SplashSound" )
	PRECACHE_CONDITIONAL( GAMESOUND, "HudChat.Message", gpGlobals->maxClients > 1 )
PRECACHE_REGISTER_END()

class CEnvViewPunch : public CPointEntity
{
public:

	DECLARE_CLASS( CEnvViewPunch, CPointEntity );

	void Spawn_Impl();

	// Input handlers
	[[= ks::reflect::Input{ .name = "ViewPunch", .type = FIELD_VOID } ]] void InputViewPunch( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "radius" } ]] float m_flRadius;
	[[= ks::reflect::Key{ .name = "punchangle" } ]] QAngle m_angViewPunch;

	void DoViewPunch();

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_viewpunch, CEnvViewPunch );

IMPLEMENT_REFLECT_DATAMAP( CEnvViewPunch )

static constexpr auto SF_PUNCH_EVERYONE = 0x0001; // Don't check radius
static constexpr auto SF_PUNCH_IN_AIR = 0x0002; // Punch players in air

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	if ( GetSpawnFlags() & SF_PUNCH_EVERYONE )
	{
		m_flRadius = 0;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::DoViewPunch()
{
	bool bAir = (GetSpawnFlags() & SF_PUNCH_IN_AIR) ? true : false;
	UTIL_ViewPunch( GetAbsOrigin(), m_angViewPunch, m_flRadius, bAir );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::InputViewPunch( inputdata_t &inputdata )
{
	DoViewPunch();
}

void CEnvQuadraticBeam::SetSpline( const Vector &control, const Vector &target)
{
	m_targetPosition = target;
	m_controlPosition = control;
}

#undef CS_MASK_SHOOT

class CBasePlayer;
class CRecipientFilter;

using namespace ::ks::reflect;

// Purpose: Utility code.

extern int		g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud
extern int		g_sModelIndexBloodDrop;		// (in combatweapon.cpp) holds the sprite index for the initial blood
extern int		g_sModelIndexBloodSpray;	// (in combatweapon.cpp) holds the sprite index for splattered blood

//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
class CEffectsServer : public IEffects
{
public:
	CEffectsServer();
	virtual ~CEffectsServer();

	// Members of the IEffect interface
	virtual void Beam( const Vector &Start, const Vector &End, int nModelIndex, 
		int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
		float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
		unsigned char noise, unsigned char red, unsigned char green,
		unsigned char blue, unsigned char brightness, unsigned char speed);
	virtual void Smoke( const Vector &origin, int mModel, float flScale, float flFramerate );
	virtual void Sparks( const Vector &position, int nMagnitude = 1, int nTrailLength = 1, const Vector *pvecDir = nullptr );
	virtual void Dust( const Vector &pos, const Vector &dir, float size, float speed );
	virtual void MuzzleFlash( const Vector &origin, const QAngle &angles, float scale, int type );
	virtual void MetalSparks( const Vector &position, const Vector &direction ); 
	virtual void EnergySplash( const Vector &position, const Vector &direction, bool bExplosive = false );
	virtual void Ricochet( const Vector &position, const Vector &direction );

	// FIXME: Should these methods remain in this interface? Or go in some 
	// other client-server neutral interface?
	virtual float Time();
	virtual bool IsServer();
	virtual void SuppressEffectsSounds( bool bSuppress ) { contract_assert( 0 ); }

private:
	//-----------------------------------------------------------------------------
	// Purpose: Returning true means don't even call TE func
	// Input  : filter - 
	//			*suppress_host - 
	// Output : static bool
	//-----------------------------------------------------------------------------
	bool SuppressTE( CRecipientFilter& filter )
	{
		if ( GetSuppressHost() )
		{
			if ( !filter.IgnorePredictionCull() )
			{
				filter.RemoveRecipient( (CBasePlayer *)GetSuppressHost()  );
			}

			if ( !filter.GetRecipientCount() )
			{
				// Suppress it
				return true;
			}
		}

		// There's at least one recipient
		return false;
	}
};

//-----------------------------------------------------------------------------
// Client-server neutral effects interface accessor
//-----------------------------------------------------------------------------
static CEffectsServer s_EffectServer;
static InterfaceReg s_Reg_CEffectsServer_IEffects( side_registry(), []() -> void * { return static_cast<IEffects *>( &s_EffectServer ); }, IEFFECTS_INTERFACE_VERSION );;
IEffects *g_pEffects = &s_EffectServer;

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEffectsServer::CEffectsServer()
{
}

CEffectsServer::~CEffectsServer()
{
}

//-----------------------------------------------------------------------------
// Generates a beam
//-----------------------------------------------------------------------------
void CEffectsServer::Beam( const Vector &vecStart, const Vector &vecEnd, int nModelIndex, 
	int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
	float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
	unsigned char noise, unsigned char red, unsigned char green,
	unsigned char blue, unsigned char brightness, unsigned char speed)
{
	CBroadcastRecipientFilter filter;
	if ( !SuppressTE( filter ) )
	{
		te->BeamPoints( filter, 0.0,
			&vecStart, &vecEnd, nModelIndex, nHaloIndex, frameStart, frameRate, flLife,  
			width, endWidth, fadeLength, noise, red, green, blue, brightness, speed );
	}
}

//-----------------------------------------------------------------------------
// Generates various tempent effects
//-----------------------------------------------------------------------------
void CEffectsServer::Smoke( const Vector &origin, int mModel, float flScale, float flFramerate )
{
	CPVSFilter filter( origin );
	if ( !SuppressTE( filter ) )
	{
		te->Smoke( filter, 0.0, &origin, mModel, flScale * 0.1f, flFramerate );
	}
}

void CEffectsServer::Sparks( const Vector &position, int nMagnitude, int nTrailLength, const Vector *pvecDir )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		te->Sparks( filter, 0.0, &position, nMagnitude, nTrailLength, pvecDir );
	}
}

void CEffectsServer::Dust( const Vector &pos, const Vector &dir, float size, float speed )
{
	CPVSFilter filter( pos );
	if ( !SuppressTE( filter ) )
	{
		te->Dust( filter, 0.0, pos, dir, size, speed );
	}
}

void CEffectsServer::MuzzleFlash( const Vector &origin, const QAngle &angles, float scale, int type )
{
	CPVSFilter filter( origin );
	if ( !SuppressTE( filter ) )
	{
		te->MuzzleFlash( filter, 0.0f, origin, angles, scale, type );
	}
}

void CEffectsServer::MetalSparks( const Vector &position, const Vector &direction )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		te->MetalSparks( filter, 0.0, &position, &direction );
	}
}

void CEffectsServer::EnergySplash( const Vector &position, const Vector &direction, bool bExplosive )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		te->EnergySplash( filter, 0.0, &position, &direction, bExplosive );
	}
}

void CEffectsServer::Ricochet( const Vector &position, const Vector &direction )
{
	CPVSFilter filter( position );
	if ( !SuppressTE( filter ) )
	{
		te->ArmorRicochet( filter, 0.0, &position, &direction );
	}
}

//-----------------------------------------------------------------------------
// FIXME: Should these methods remain in this interface? Or go in some 
// other client-server neutral interface?
//-----------------------------------------------------------------------------
float CEffectsServer::Time()
{
	return gpGlobals->curtime;
}

bool CEffectsServer::IsServer()
{
	return true;
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: This module implements functions to support ehandles.

extern bool g_bDisableEhandleAccess;
	void DebugCheckEHandleAccess( void *pEnt )
	{

		if ( g_bDisableEhandleAccess )
		{
			Msg( "Access of EHANDLE/CHandle for class %s:%p in destructor!\n",
				STRING(((CBaseEntity*)pEnt)->m_iClassname ), pEnt );
		}
	}

class CBaseEntity;
class CBasePlayer;
class CEntityBlocker;

using namespace ::ks::reflect;

LINK_ENTITY_TO_CLASS( entity_blocker, CEntityBlocker );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&mins - 
//			&maxs - 
//			NULL - 
// Output : CEntityBlocker
//-----------------------------------------------------------------------------
CEntityBlocker *CEntityBlocker::Create( const Vector &origin, const Vector &mins, const Vector &maxs, CBaseEntity *pOwner, bool bBlockPhysics )
{
	CEntityBlocker *pBlocker = (CEntityBlocker *) CBaseEntity::Create( "entity_blocker", origin, vec3_angle, pOwner );
	
	if ( pBlocker != nullptr )
	{
		pBlocker->SetSize( mins, maxs );
		if ( bBlockPhysics )
		{
			pBlocker->VPhysicsInitStatic();
		}
	}

	return pBlocker;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityBlocker::Spawn_Impl( void )
{
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST );
}

//-----------------------------------------------------------------------------
// Purpose: Entity blockers don't block tracelines so they don't screw up weapon fire, etc
//-----------------------------------------------------------------------------
bool CEntityBlocker::TestCollision_Impl( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	return false;
}

//------------------------------------------------------------------------------
// Purpose : 
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_Test_Entity_Blocker( void )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	Vector vecForward;
	pPlayer->GetVectors( &vecForward, nullptr, nullptr );

	trace_t tr;
	Vector vecOrigin = pPlayer->GetAbsOrigin() + (vecForward * 256);
	UTIL_TraceHull( vecOrigin + Vector(0,0,256), vecOrigin - Vector(0,0,256), VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( !tr.allsolid && !tr.startsolid )
	{
		CEntityBlocker::Create( tr.endpos, VEC_HULL_MIN, VEC_HULL_MAX, nullptr, true );
		NDebugOverlay::Box( tr.endpos, VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 64, 1000.0 );
	}
}
static ConCommand test_entity_blocker(side_registry(), "test_entity_blocker", CC_Test_Entity_Blocker, "Test command that drops an entity blocker out in front of the player.", FCVAR_CHEAT );

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Dissolve entity to be attached to target entity. Serves two purposes:
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//			2) An entity that can be created at runtime to ignite a target entity.

static const char *s_pElectroThinkContext = "ElectroThinkContext";

//-----------------------------------------------------------------------------
// Lifetime 
//-----------------------------------------------------------------------------
static constexpr auto DISSOLVE_FADE_IN_START_TIME = 0.0f;
static constexpr auto DISSOLVE_FADE_IN_END_TIME = 1.0f;
static constexpr auto DISSOLVE_FADE_OUT_MODEL_START_TIME = 1.9f;
static constexpr auto DISSOLVE_FADE_OUT_MODEL_END_TIME = 2.0f;
static constexpr auto DISSOLVE_FADE_OUT_START_TIME = 2.0f;
static constexpr auto DISSOLVE_FADE_OUT_END_TIME = 2.0f;

//-----------------------------------------------------------------------------
// Model 
//-----------------------------------------------------------------------------
inline constexpr const char *DISSOLVE_SPRITE_NAME = "sprites/blueglow1.vmt";

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CEntityDissolve )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CEntityDissolve, DT_EntityDissolve )

LINK_ENTITY_TO_CLASS( env_entity_dissolver, CEntityDissolve );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityDissolve::CEntityDissolve( void )
{
	m_flStartTime	= 0.0f;
	m_nMagnitude = 250;
}

CEntityDissolve::~CEntityDissolve( void )
{
}

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEntityDissolve::Precache_Impl()
{
	if ( NULL_STRING == GetModelName() )
	{
		PrecacheModel( DISSOLVE_SPRITE_NAME );
	}
	else
	{
		PrecacheModel( STRING( GetModelName() ) );
	}
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEntityDissolve::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	Precache();
	UTIL_SetModel( this, STRING( GetModelName() ) );

	if ( (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		if ( dynamic_cast< CRagdollProp* >( GetMoveParent() ) )
		{
			SetContextThink( &CEntityDissolve::ElectrocuteThink, gpGlobals->curtime + 0.01f, s_pElectroThinkContext );
		}
	}
	
	// Setup our times
	m_flFadeInStart = DISSOLVE_FADE_IN_START_TIME;
	m_flFadeInLength = DISSOLVE_FADE_IN_END_TIME - DISSOLVE_FADE_IN_START_TIME;
	
	m_flFadeOutModelStart = DISSOLVE_FADE_OUT_MODEL_START_TIME;
	m_flFadeOutModelLength = DISSOLVE_FADE_OUT_MODEL_END_TIME - DISSOLVE_FADE_OUT_MODEL_START_TIME;
	
	m_flFadeOutStart = DISSOLVE_FADE_OUT_START_TIME;
	m_flFadeOutLength = DISSOLVE_FADE_OUT_END_TIME - DISSOLVE_FADE_OUT_START_TIME;

	if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
	{
		m_flFadeInStart = 0.0f;
		m_flFadeOutStart = CORE_DISSOLVE_FADE_START;
		m_flFadeOutModelStart = CORE_DISSOLVE_MODEL_FADE_START;
		m_flFadeOutModelLength = CORE_DISSOLVE_MODEL_FADE_LENGTH;
		m_flFadeInLength = CORE_DISSOLVE_FADEIN_LENGTH;
	}

	m_nRenderMode = kRenderTransColor;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	m_nRenderFX = kRenderFxNone;

	SetThink( &CEntityDissolve::DissolveThink );
	if ( gpGlobals->curtime > m_flStartTime )
	{
		// Necessary for server-side ragdolls
		DissolveThink();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.01f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CEntityDissolve::InputDissolve( inputdata_t &inputdata )
{
	string_t strTarget = inputdata.value.StringID();

	if (strTarget == NULL_STRING)
	{
		strTarget = m_target;
	}

	CBaseEntity *pTarget = nullptr;
	while ((pTarget = gEntList.FindEntityGeneric(pTarget, STRING(strTarget), this, inputdata.pActivator)) != nullptr)
	{
		CBaseAnimating *pBaseAnim = pTarget->GetBaseAnimating();
		if (pBaseAnim)
		{
			pBaseAnim->Dissolve( nullptr, gpGlobals->curtime, false, m_nDissolveType, GetAbsOrigin(), m_nMagnitude );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityDissolve *CEntityDissolve::Create( CBaseEntity *pTarget, const char *pMaterialName, 
	float flStartTime, int nDissolveType, bool *pRagdollCreated )
{
	if ( pRagdollCreated )
	{
		*pRagdollCreated = false;
	}

	if ( !pMaterialName )
	{
		pMaterialName = DISSOLVE_SPRITE_NAME;
	}

	if ( pTarget->IsPlayer() )
	{
		// Simply immediately kill the player.
		CBasePlayer *pPlayer = static_cast< CBasePlayer* >( pTarget );
		pPlayer->SetArmorValue( 0 );
		CTakeDamageInfo info( pPlayer, pPlayer, pPlayer->GetHealth(), DMG_GENERIC | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );
		pPlayer->TakeDamage( info );
		return nullptr;
	}

	CEntityDissolve *pDissolve = (CEntityDissolve *) CreateEntityByName( "env_entity_dissolver" );

	if ( pDissolve == nullptr )
		return nullptr;

	pDissolve->m_nDissolveType = nDissolveType;
	if ( (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		
	}

	pDissolve->SetModelName( AllocPooledString(pMaterialName) );
	pDissolve->AttachToEntity( pTarget );
	pDissolve->SetStartTime( flStartTime );
	pDissolve->Spawn();

	// Send to the client even though we don't have a model
	pDissolve->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// Play any appropriate noises when we start to dissolve
	if ( (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		pTarget->DispatchResponse( "TLK_ELECTROCUTESCREAM" );
	}
	else
	{
		pTarget->DispatchResponse( "TLK_DISSOLVESCREAM" );
	}
	return pDissolve;
}

//-----------------------------------------------------------------------------
// What type of dissolve?
//-----------------------------------------------------------------------------
CEntityDissolve *CEntityDissolve::Create( CBaseEntity *pTarget, CBaseEntity *pSource )
{
	// Look for other boogies on the ragdoll + kill them
	for ( CBaseEntity *pChild = pSource->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
	{
		CEntityDissolve *pDissolve = dynamic_cast<CEntityDissolve*>(pChild);
		if ( !pDissolve )
			continue;

		return Create( pTarget, STRING( pDissolve->GetModelName() ), pDissolve->m_flStartTime, pDissolve->m_nDissolveType );
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CEntityDissolve::AttachToEntity( CBaseEntity *pTarget )
{
	// So our dissolver follows the entity around on the server.
	SetParent( pTarget );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lifetime - 
//-----------------------------------------------------------------------------
void CEntityDissolve::SetStartTime( float flStartTime )
{
	m_flStartTime = flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CEntityDissolve::DissolveThink( void )
{
	CBaseAnimating *pTarget = ( GetMoveParent() ) ? GetMoveParent()->GetBaseAnimating() : nullptr;

	if ( GetModelName() == NULL_STRING && pTarget == nullptr )
		 return;
	
	if ( pTarget == nullptr )
	{
		UTIL_Remove( this );
		return;
	}

	// Turn them into debris
	pTarget->SetCollisionGroup( COLLISION_GROUP_DISSOLVING );

	if ( pTarget && pTarget->GetFlags() & FL_TRANSRAGDOLL )
	{
		SetRenderAlpha( 0 );
	}

	float dt = gpGlobals->curtime - m_flStartTime;

	if ( dt < m_flFadeInStart )
	{
		SetNextThink( m_flStartTime + m_flFadeInStart );
		return;
	}

	// If we're done fading, then kill our target entity and us
	if ( dt >= m_flFadeOutStart + m_flFadeOutLength )
	{
		// Necessary to cause it to do the appropriate death cleanup
		// Yeah, the player may have nothing to do with it, but
		// passing NULL to TakeDamage causes bad things to happen
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		int iNoPhysicsDamage = g_pGameRules->Damage_GetNoPhysicsForce();
		CTakeDamageInfo info( pPlayer, pPlayer, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL | iNoPhysicsDamage );
		pTarget->TakeDamage( info );

		if ( pTarget != pPlayer )
		{
			UTIL_Remove( pTarget );
		}
		
		UTIL_Remove( this );
		
		return;
	}

	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CEntityDissolve::ElectrocuteThink( void )
{
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( GetMoveParent() );
	if ( !pRagdoll )
		return;

	ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );
	for ( int j = 0; j < pRagdollPhys->listCount; ++j )
	{
		Vector vecForce;
		vecForce = RandomVector( -2400.0f, 2400.0f );
		pRagdollPhys->list[j].pObject->ApplyForceCenter( vecForce ); 
	}

	SetContextThink( &CEntityDissolve::ElectrocuteThink, gpGlobals->curtime + random_valve->RandomFloat( 0.1, 0.2f ), 
		s_pElectroThinkContext );
}

Vector CEntityDissolve::GetDissolverOrigin( void)
{
	Vector vReturn = m_vDissolverOrigin; 
	return vReturn;	
}

class CBaseCombatCharacter;
class CBaseEntity;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Flame entity to be attached to target entity. Serves two purposes:
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//			2) An entity that can be created at runtime to ignite a target entity.

IMPLEMENT_REFLECT_SERVERCLASS( CEntityFlame, DT_EntityFlame )

LINK_ENTITY_TO_CLASS( entityflame, CEntityFlame );
PRECACHE_REGISTER(entityflame);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityFlame::CEntityFlame( void )
{
	m_flSize			= 0.0f;
	m_flLifetime		= gpGlobals->curtime;
	m_bPlayingSound		= false;
	m_iDangerSound		= SOUNDLIST_EMPTY;
	m_bCheapEffect		= false;
}

void CEntityFlame::UpdateOnRemove_Impl()
{
	// Sometimes the entity I'm burning gets destroyed by other means,
	// which kills me. Make sure to stop the burning sound.
	if ( m_bPlayingSound )
	{
		EmitSound( "General.StopBurning" );
		m_bPlayingSound = false;
	}

	if ( m_iDangerSound != SOUNDLIST_EMPTY )
	{
		CSoundEnt::FreeSound( m_iDangerSound );
		m_iDangerSound = SOUNDLIST_EMPTY;
	}

	BaseClass::UpdateOnRemove_Impl();
}

void CEntityFlame::Precache_Impl()
{
	BaseClass::Precache_Impl();

	PrecacheParticleSystem( "burning_character" );
	PrecacheParticleSystem( "burning_gib_01" );

	PrecacheScriptSound( "General.StopBurning" );
	PrecacheScriptSound( "General.BurningFlesh" );
	PrecacheScriptSound( "General.BurningObject" );
}

void CEntityFlame::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	m_flLifetime = gpGlobals->curtime;

	SetThink( &CEntityFlame::FlameThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	//Send to the client even though we don't have a model
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

}
							    
//-----------------------------------------------------------------------------
// Since we don't save/load danger links, we need to reacquire them here
//-----------------------------------------------------------------------------
void CEntityFlame::Activate_Impl()
{
	BaseClass::Activate_Impl();

}

void CEntityFlame::UseCheapEffect( bool bCheap )
{
	m_bCheapEffect = bCheap;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityFlame *CEntityFlame::Create( CBaseEntity *pTarget, float flLifetime, float flSize /*= 0.0f*/, bool bUseHitboxes /*= true*/ )
{
	CEntityFlame *pFlame = (CEntityFlame *)CreateEntityByName( "entityflame" );

	if ( pFlame == nullptr )
		return nullptr;

	if ( flSize <= 0.0f )
	{
		float xSize = pTarget->CollisionProp()->OBBMaxs().x - pTarget->CollisionProp()->OBBMins().x;
		float ySize = pTarget->CollisionProp()->OBBMaxs().y - pTarget->CollisionProp()->OBBMins().y;
		flSize = ( xSize + ySize ) * 0.5f;
		if ( flSize < 16.0f )
		{
			flSize = 16.0f;
		}
	}

	if ( flLifetime <= 0.0f )
	{
		flLifetime = 2.0f;
	}

	pFlame->m_flSize = flSize;
	pFlame->Spawn();
	UTIL_SetOrigin( pFlame, pTarget->GetAbsOrigin() );
	pFlame->AttachToEntity( pTarget );
	pFlame->SetLifetime( flLifetime );
	pFlame->Activate();

	return pFlame;
}

//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CEntityFlame::AttachToEntity( CBaseEntity *pTarget )
{
	// For networking to the client.
	m_hEntAttached = pTarget;

	{
		EmitSound( "General.BurningObject" );
	}

	m_bPlayingSound = true;

	// So our heat emitter follows the entity around on the server.
	SetParent( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lifetime - 
//-----------------------------------------------------------------------------
void CEntityFlame::SetLifetime( float lifetime )
{
	m_flLifetime = gpGlobals->curtime + lifetime;
}

float CEntityFlame::GetRemainingLife( void ) const
{
	return m_flLifetime - gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CEntityFlame::FlameThink( void )
{
	// Assure that this function will be ticked again even if we early-out in the if below.
	SetNextThink( gpGlobals->curtime + FLAME_DAMAGE_INTERVAL );

	if ( !m_hEntAttached.Get() )
	{
		UTIL_Remove( this );
		return;
	}

	if ( m_hEntAttached->GetFlags() & FL_TRANSRAGDOLL )
	{
		SetRenderAlpha( 0 );
		return;
	}

	if ( m_hEntAttached->GetWaterLevel() > WL_NotInWater )
	{
		Vector mins, maxs;

		mins = m_hEntAttached->WorldSpaceCenter();
		maxs = mins;

		maxs.z = m_hEntAttached->WorldSpaceCenter().z;
		maxs.x += 32;
		maxs.y += 32;
		
		mins.z -= 32;
		mins.x -= 32;
		mins.y -= 32;

		UTIL_Bubbles( mins, maxs, 12 );
	}

	// See if we're done burning, or our attached ent has vanished
	if ( m_flLifetime < gpGlobals->curtime || m_hEntAttached == nullptr )
	{
		EmitSound( "General.StopBurning" );
		m_bPlayingSound = false;
		SetThink( &CEntityFlame::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.5f );

		// Notify anything we're attached to
		if ( m_hEntAttached )
		{
			CBaseCombatCharacter *pAttachedCC = m_hEntAttached->MyCombatCharacterPointer();

			if( pAttachedCC )
			{
				// Notify the NPC that it's no longer burning!
				pAttachedCC->Extinguish();
			}
		}

		return;
	}

	if ( m_hEntAttached )
	{
		// Do radius damage ignoring the entity I'm attached to. This will harm things around me.
		RadiusDamage( CTakeDamageInfo( this, this, 4.0f, DMG_BURN ), GetAbsOrigin(), m_flSize/2, CLASS_NONE, m_hEntAttached );

		// Directly harm the entity I'm attached to. This is so we can precisely control how much damage the entity
		// that is on fire takes without worrying about the flame's position relative to the bodytarget (which is the
		// distance that the radius damage code uses to determine how much damage to inflict)
		m_hEntAttached->TakeDamage( CTakeDamageInfo( this, this, FLAME_DIRECT_DAMAGE, DMG_BURN | DMG_DIRECT ) );

		if( !nullptr && hl2_episodic.GetBool() )
		{
			const float ENTITYFLAME_MOVE_AWAY_DIST = 24.0f;
			// Make a sound near my origin, and up a little higher (in case I'm on the ground, so NPC's still hear it)
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin(), ENTITYFLAME_MOVE_AWAY_DIST, 0.1f, this, SOUNDENT_CHANNEL_REPEATED_DANGER );
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin() + Vector( 0, 0, 48.0f ), ENTITYFLAME_MOVE_AWAY_DIST, 0.1f, this, SOUNDENT_CHANNEL_REPEATING );
		}
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, FLAME_RADIUS_DAMAGE, DMG_BURN ), GetAbsOrigin(), m_flSize/2, CLASS_NONE, nullptr );
	}

	FireSystem_AddHeatInRadius( GetAbsOrigin(), m_flSize/2, 2.0f );

}  

//-----------------------------------------------------------------------------
// Igniter
//-----------------------------------------------------------------------------
class CEnvEntityIgniter : public CBaseEntity 
{
public:
	DECLARE_CLASS( CEnvEntityIgniter, CBaseEntity );
	DECLARE_DATADESC();

	void Precache_Impl();

protected:
	[[= ks::reflect::Input{ .name = "Ignite", .type = FIELD_VOID } ]] void InputIgnite( inputdata_t &inputdata );
	[[= ks::reflect::Key{ .name = "lifetime" } ]] float m_flLifetime;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvEntityIgniter )

LINK_ENTITY_TO_CLASS( env_entity_igniter, CEnvEntityIgniter );

//-----------------------------------------------------------------------------
// Purpose: Ignites entities
//-----------------------------------------------------------------------------
void CEnvEntityIgniter::Precache_Impl()
{
	BaseClass::Precache_Impl();
	UTIL_PrecacheOther( "entityflame" );
}

//-----------------------------------------------------------------------------
// Purpose: Ignites entities
//-----------------------------------------------------------------------------
void CEnvEntityIgniter::InputIgnite( inputdata_t &inputdata )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pTarget = nullptr;
	while ( (pTarget = gEntList.FindEntityGeneric(pTarget, STRING(m_target), this, inputdata.pActivator)) != nullptr )
	{
		// Combat characters know how to catch themselves on fire.
		CBaseCombatCharacter *pBCC = pTarget->MyCombatCharacterPointer();
		if (pBCC)
		{
			// DVS TODO: consider promoting Ignite to CBaseEntity and doing everything here
			pBCC->Ignite( m_flLifetime );
			continue;
		}

		// Everything else, we handle here.
		CEntityFlame::Create( pTarget, m_flLifetime );
	}
}

class CBaseAnimating;
class CBaseEntity;
class CBasePlayer;
class CTakeDamageInfo;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Dissolve entity to be attached to target entity. Serves two purposes:
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//			2) An entity that can be created at runtime to ignite a target entity.


//-----------------------------------------------------------------------------
// Model 
//-----------------------------------------------------------------------------
inline constexpr const char *FREEZING_SPRITE_NAME = "sprites/blueglow1.vmt";

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CEntityFreezing )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CEntityFreezing, DT_EntityFreezing )

LINK_ENTITY_TO_CLASS( env_entity_freezing, CEntityFreezing );
PRECACHE_REGISTER( env_entity_freezing );

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEntityFreezing::Precache_Impl()
{
	if ( NULL_STRING != GetModelName() )
	{
		PrecacheModel( STRING( GetModelName() ) );
	}

#ifdef USE_BLOBULATOR
	PrecacheMaterial( "models/weapons/w_icegun/ice_surface" );
#endif
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEntityFreezing::Spawn_Impl()
{
	BaseClass::Spawn_Impl();
	Precache();
	UTIL_SetModel( this, STRING( GetModelName() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CEntityFreezing::InputFreeze( inputdata_t &inputdata )
{
	string_t strTarget = inputdata.value.StringID();

	if (strTarget == NULL_STRING)
	{
		strTarget = m_target;
	}

	CBaseEntity *pTarget = nullptr;
	while ((pTarget = gEntList.FindEntityGeneric(pTarget, STRING(strTarget), this, inputdata.pActivator)) != nullptr)
	{
		CBaseAnimating *pBaseAnim = pTarget->GetBaseAnimating();
		if ( pBaseAnim )
		{
			pBaseAnim->Freeze( m_flFrozen );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityFreezing *CEntityFreezing::Create( CBaseAnimating *pTarget )
{
	if ( pTarget )
	{
		if ( pTarget->IsPlayer() )
		{
			// Simply immediately kill the player.
			CBasePlayer *pPlayer = static_cast< CBasePlayer* >( pTarget );
			pPlayer->SetArmorValue( 0 );
			CTakeDamageInfo info( pPlayer, pPlayer, pPlayer->GetHealth(), DMG_GENERIC | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );
			pPlayer->TakeDamage( info );
			return nullptr;
		}
	}

	CEntityFreezing *pFreezing = (CEntityFreezing *) CreateEntityByName( "env_entity_freezing" );

	if ( pFreezing == nullptr )
		return nullptr;

	if ( pTarget )
	{
		pFreezing->AttachToEntity( pTarget );
		pFreezing->SetModelName( pTarget->GetModelName() );
		pFreezing->SetFrozen( pTarget->GetFrozenAmount() );
	}

	DispatchSpawn( pFreezing );

	// Send to the client even though we don't have a model
	pFreezing->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	return pFreezing;
}

//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CEntityFreezing::AttachToEntity( CBaseEntity *pTarget )
{
	// So our dissolver follows the entity around on the server.
	SetParent( pTarget );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
}

class CBaseEntity;
struct notify_system_event_params_t;

using namespace ::ks::reflect;

// Purpose: Drops particles where the entity was.

//-----------------------------------------------------------------------------
// Used to retire the entity
//-----------------------------------------------------------------------------
static const char *s_pRetireContext = "RetireContext";

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CEntityParticleTrail, DT_EntityParticleTrail )

LINK_ENTITY_TO_CLASS( env_particle_trail, CEntityParticleTrail );

//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityParticleTrail *CEntityParticleTrail::Create( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info, CBaseEntity *pConstraintEntity )
{
	int iMaterialName = GetMaterialIndex( STRING(info.m_strMaterialName) );

	// Look for other particle trails on the entity + copy state to the new entity
	CEntityParticleTrail *pTrail;
	CBaseEntity *pNext;
	for ( CBaseEntity *pChild = pTarget->FirstMoveChild(); pChild; pChild = pNext )
	{
		pNext = pChild->NextMovePeer();
		pTrail = dynamic_cast<CEntityParticleTrail*>(pChild);
		if ( pTrail && (pTrail->m_iMaterialName == iMaterialName) )
		{
			// Prevent destruction if it re-enters the field
			pTrail->IncrementRefCount();
			return pTrail;
		}
	}

	pTrail = (CEntityParticleTrail *)CreateEntityByName( "env_particle_trail" );
	if ( pTrail == nullptr )
		return nullptr;

	pTrail->m_hConstraintEntity = pConstraintEntity;
	pTrail->m_iMaterialName = iMaterialName;
	pTrail->m_Info.CopyFrom(info);
	pTrail->m_nRefCount = 1;
	pTrail->AttachToEntity( pTarget );
	pTrail->Spawn();
	return pTrail;
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEntityParticleTrail::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

	/*
	SetThink( &CEntityParticleTrail::BoogieThink );
	SetNextThink( gpGlobals->curtime + 0.01f );

	if ( HasSpawnFlags( SF_RAGDOLL_BOOGIE_ELECTRICAL ) )
	{
		SetContextThink( ZapThink, gpGlobals->curtime + random->RandomFloat( 0.1f, 0.3f ), s_pZapContext ); 
	}
	*/
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEntityParticleTrail::UpdateOnRemove_Impl()
{
	g_pNotify->ClearEntity( this );

	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------------------------------------------
// Force our constraint entity to be trasmitted
//-----------------------------------------------------------------------------
void CEntityParticleTrail::SetTransmit_Impl( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( (*pInfo->m_pTransmitEdict)[ entindex() ] )
		return;

	BaseClass::SetTransmit_Impl( pInfo, bAlways );
	
	// Force our constraint entity to be sent too.
	if ( m_hConstraintEntity )
	{
		m_hConstraintEntity->SetTransmit( pInfo, bAlways );
	}
}

//-----------------------------------------------------------------------------
// Retire
//-----------------------------------------------------------------------------
void CEntityParticleTrail::IncrementRefCount()
{
	if ( m_nRefCount == 0 )
	{
		SetContextThink( nullptr, gpGlobals->curtime, s_pRetireContext );
	}
	++m_nRefCount;
}

void CEntityParticleTrail::DecrementRefCount()
{
	--m_nRefCount;
	contract_assert( m_nRefCount >= 0 );
	if ( m_nRefCount == 0 )
	{
		FollowEntity( nullptr );
		g_pNotify->ClearEntity( this );
		SetContextThink( &CEntityParticleTrail::SUB_Remove, gpGlobals->curtime + m_Info.m_flLifetime, s_pRetireContext );
	}
}

//-----------------------------------------------------------------------------
// Clean up when the entity goes away.
//-----------------------------------------------------------------------------
void CEntityParticleTrail::NotifySystemEvent_Impl( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	BaseClass::NotifySystemEvent_Impl( pNotify, eventType, params );
	if ( eventType == NOTIFY_EVENT_DESTROY )
	{
		FollowEntity( nullptr );
		g_pNotify->ClearEntity( this );
		if ( m_nRefCount != 0 )
		{
			m_nRefCount = 0;
			SetContextThink( &CEntityParticleTrail::SUB_Remove, gpGlobals->curtime + m_Info.m_flLifetime, s_pRetireContext );
		}
	}
}

//-----------------------------------------------------------------------------
// Suppression count
//-----------------------------------------------------------------------------
void CEntityParticleTrail::Destroy( CBaseEntity *pTarget, const EntityParticleTrailInfo_t &info )
{
	int iMaterialName = GetMaterialIndex( STRING(info.m_strMaterialName) );

	// Look for the particle trail attached to this entity + decrease refcount
	CBaseEntity *pNext;
	for ( CBaseEntity *pChild = pTarget->FirstMoveChild(); pChild; pChild = pNext )
	{
		pNext = pChild->NextMovePeer();
		CEntityParticleTrail *pTrail = dynamic_cast<CEntityParticleTrail*>(pChild);
		if ( !pTrail || (pTrail->m_iMaterialName != iMaterialName) )
			continue;

		pTrail->DecrementRefCount();
	}
}

//-----------------------------------------------------------------------------
// Attach to an entity
//-----------------------------------------------------------------------------
void CEntityParticleTrail::AttachToEntity( CBaseEntity *pTarget )
{
	FollowEntity( pTarget );
	g_pNotify->AddEntity( this, pTarget );
}

using namespace ::ks::reflect;

// Purpose: Drops particles where the entity was.

//-----------------------------------------------------------------------------
// Default values
//-----------------------------------------------------------------------------
EntityParticleTrailInfo_t::EntityParticleTrailInfo_t()
{
	m_strMaterialName = NULL_STRING;
	m_flLifetime = 4.0f;
	m_flStartSize = 2.0f;
	m_flEndSize = 3.0f;
}

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( EntityParticleTrailInfo_t )

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_TABLE( EntityParticleTrailInfo_t, DT_EntityParticleTrailInfo );

class CBaseEntity;
class CBasePlayer;
class IEntityFindFilter;
class IEntityListener;
struct notify_system_event_params_t;
struct touchlink_t;

using namespace ::ks::reflect;

void SceneManager_ClientActive( CBasePlayer *player );

static std::vector< IServerNetworkable* > g_DeleteList;

CGlobalEntityList gEntList;
CBaseEntityList *g_pEntityList = &gEntList;

class CAimTargetManager : public IEntityListener
{
public:
	// Called by CEntityListSystem
	void LevelInitPreEntity() 
	{ 
		gEntList.AddListenerEntity( this );
		Clear(); 
	}
	void LevelShutdownPostEntity()
	{
		gEntList.RemoveListenerEntity( this );
		Clear();
	}

	void Clear() 
	{ 
		m_targetList.clear(); m_targetList.shrink_to_fit(); 
	}

	void ForceRepopulateList()
	{
		Clear();

		CBaseEntity *pEnt = gEntList.FirstEnt();

		while( pEnt )
		{
			if( ShouldAddEntity(pEnt) )
				AddEntity(pEnt);

			pEnt = gEntList.NextEnt( pEnt );
		}
	}
	
	bool ShouldAddEntity( CBaseEntity *pEntity )
	{
		return ((pEntity->GetFlags() & FL_AIMTARGET) != 0);
	}

	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntitySpawned( CBaseEntity *pEntity )
	{
		if ( ShouldAddEntity( pEntity ) )
		{
			RemoveEntity( pEntity );
			AddEntity( pEntity );
		}
	}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
		if ( !(pEntity->GetFlags() & FL_AIMTARGET) )
			return;
		RemoveEntity(pEntity);
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		if ( pEntity->IsMarkedForDeletion() )
			return;
		m_targetList.push_back( pEntity );
	}
	void RemoveEntity( CBaseEntity *pEntity )
	{
		int index = ks::find_index( m_targetList, pEntity );
		if ( ks::is_valid_index( m_targetList, index ) )
		{
			ks::swap_remove( m_targetList, index );
		}
	}

	int ListCount() { return std::ssize( m_targetList ); }
	int ListCopy( CBaseEntity *pList[], int listMax )
	{
		int count = std::min(listMax, ListCount() );
		std::memcpy( pList, m_targetList.data(), sizeof(CBaseEntity *) * count );
		return count;
	}
	CBaseEntity *ListElement( int iIndex )
	{
		return m_targetList[iIndex];
	}

private:
	std::vector< CBaseEntity * >	m_targetList;
};

static CAimTargetManager g_AimManager;

int AimTarget_ListCount()
{
	return g_AimManager.ListCount();
}
int AimTarget_ListCopy( CBaseEntity *pList[], int listMax )
{
	return g_AimManager.ListCopy( pList, listMax );
}
CBaseEntity *AimTarget_ListElement( int iIndex )
{
	return g_AimManager.ListElement( iIndex );
}
void AimTarget_ForceRepopulateList()
{
	g_AimManager.ForceRepopulateList();
}

// Manages a list of all entities currently doing game simulation or thinking
// NOTE: This is usually a small subset of the global entity list, so it's
// an optimization to maintain this list incrementally rather than polling each
// frame.
struct simthinkentry_t
{
	unsigned short	entEntry;
	unsigned short	unused0;
	int				nextThinkTick;
};
class CSimThinkManager : public IEntityListener
{
public:
	CSimThinkManager()
	{
		Clear();
	}
	void Clear()
	{
		m_simThinkList.clear(); m_simThinkList.shrink_to_fit();
		for ( int i = 0; i < std::size( m_entinfoIndex ); i++ )
		{
			m_entinfoIndex[i] = 0xFFFF;
		}
	}
	void LevelInitPreEntity()
	{
		gEntList.AddListenerEntity( this );
	}

	void LevelShutdownPostEntity()
	{
		gEntList.RemoveListenerEntity( this );
		Clear();
	}

	void OnEntityCreated( CBaseEntity *pEntity )
	{
	}
	void OnEntityDeleted( CBaseEntity *pEntity )
	{
		RemoveEntinfoIndex( pEntity->GetRefEHandle().GetEntryIndex() );
	}

	void RemoveEntinfoIndex( int index )
	{
		int listHandle = m_entinfoIndex[index];
		// If this guy is in the active list, remove him
		if ( listHandle != 0xFFFF )
		{
			contract_assert( m_simThinkList[listHandle].entEntry == index );
			ks::swap_remove( m_simThinkList, listHandle );
			m_entinfoIndex[index] = 0xFFFF;
			
			// fast remove shifted someone, update that someone
			if ( listHandle < std::ssize( m_simThinkList ) )
			{
				m_entinfoIndex[m_simThinkList[listHandle].entEntry] = listHandle;
			}
		}
	}
	int ListCount()
	{
		return std::ssize( m_simThinkList );
	}

	int ListCopy( CBaseEntity *pList[], int listMax )
	{
		int count = std::min(listMax, ListCount());
		int out = 0;
		for ( int i = 0; i < count; i++ )
		{
			// only copy out entities that will simulate or think this frame
			if ( m_simThinkList[i].nextThinkTick <= gpGlobals->tickcount )
			{
				contract_assert( m_simThinkList[i].nextThinkTick>=0 );
				int entinfoIndex = m_simThinkList[i].entEntry;
				const CEntInfo *pInfo = gEntList.GetEntInfoPtrByIndex( entinfoIndex );
				if ( !pInfo->m_pEntity )
					continue;
				pList[out] = (CBaseEntity *)pInfo->m_pEntity;
				contract_assert( m_simThinkList[i].nextThinkTick==0 || pList[out]->GetFirstThinkTick()==m_simThinkList[i].nextThinkTick );
				out++;
			}
		}

		return out;
	}

	void EntityChanged( CBaseEntity *pEntity )
	{
		// might change after deletion, don't put back into the list
		if ( pEntity->IsMarkedForDeletion() )
			return;
		
		const CBaseHandle &eh = pEntity->GetRefEHandle();
		if ( !eh.IsValid() )
			return;

		int index = eh.GetEntryIndex();
		if ( pEntity->IsEFlagSet( EFL_NO_THINK_FUNCTION ) && pEntity->IsEFlagSet( EFL_NO_GAME_PHYSICS_SIMULATION ) )
		{
			RemoveEntinfoIndex( index );
		}
		else
		{
			// already in the list? (had think or sim last time, now has both - or had both last time, now just one)
			if ( m_entinfoIndex[index] == 0xFFFF )
			{
				m_entinfoIndex[index] = std::ssize( m_simThinkList ); m_simThinkList.emplace_back();
				m_simThinkList[m_entinfoIndex[index]].entEntry = (unsigned short)index;
				m_simThinkList[m_entinfoIndex[index]].nextThinkTick = 0;
				if ( pEntity->IsEFlagSet(EFL_NO_GAME_PHYSICS_SIMULATION) )
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = pEntity->GetFirstThinkTick();
					contract_assert( m_simThinkList[m_entinfoIndex[index]].nextThinkTick>=0 );
				}
			}
			else
			{
				contract_assert( m_simThinkList[m_entinfoIndex[index]].entEntry == index );
				// updating existing entry - if no sim, reset think time
				if ( pEntity->IsEFlagSet(EFL_NO_GAME_PHYSICS_SIMULATION) )
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = pEntity->GetFirstThinkTick();
					contract_assert( m_simThinkList[m_entinfoIndex[index]].nextThinkTick>=0 );
				}
				else
				{
					m_simThinkList[m_entinfoIndex[index]].nextThinkTick = 0;
				}
			}
		}
	}

private:
	unsigned short m_entinfoIndex[NUM_ENT_ENTRIES];
	std::vector< simthinkentry_t >	m_simThinkList;
};

CSimThinkManager g_SimThinkManager;

int SimThink_ListCount()
{
	return g_SimThinkManager.ListCount();
}

int SimThink_ListCopy( CBaseEntity *pList[], int listMax )
{
	return g_SimThinkManager.ListCopy( pList, listMax );
}

void SimThink_EntityChanged( CBaseEntity *pEntity )
{
	g_SimThinkManager.EntityChanged( pEntity );
}

// This manages a list of entities queued up to receive PostClientMessages callbacks
class CPostClientMessageManager
{
public:
	void LevelShutdownPostEntity()
	{
		m_list.clear(); m_list.shrink_to_fit();
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		m_list.push_back( (unsigned short)pEntity->GetRefEHandle().GetEntryIndex() );
	}
	void PostClientMessagesSent()
	{
		for ( int i = std::ssize( m_list )-1; i >= 0; --i )
		{
			CBaseEntity *pEntity = (CBaseEntity *)gEntList.GetEntInfoPtrByIndex( m_list[i] )->m_pEntity;

			if ( pEntity )
			{
				pEntity->PostClientMessagesSent();
			}
		}
		m_list.clear();
	}
private:
	// UNDONE: Need to use ehandles instead?
	std::vector< unsigned short >	m_list;
};

static CPostClientMessageManager g_PostClientManager;

static CBaseEntityClassList *s_pClassLists = nullptr;
CBaseEntityClassList::CBaseEntityClassList()
{
	m_pNextClassList = s_pClassLists;
	s_pClassLists = this;
}
CBaseEntityClassList::~CBaseEntityClassList()
{
}

//-----------------------------------------------------------------------------
// Entity hash tables
//-----------------------------------------------------------------------------

struct CEntsByStringHashFuncs
{
	std::size_t operator()( string_t iszStr ) const
	{
		static_assert( sizeof( char * ) == sizeof( intp ) );
		return HashIntp( ( intp )iszStr.ToCStr() );
	}
};

// Pooled classname -> head of that class's entity list; entities keep a pointer to their head slot
static std::unordered_map< string_t, CBaseEntity *, CEntsByStringHashFuncs > g_EntsByClassname( 512 );

//-----------------------------------------------------------------------------
CGlobalEntityList::CGlobalEntityList()
{
	m_iHighestEnt = m_iNumEnts = m_iNumEdicts = 0;
	m_bClearingEntities = false;
}

// removes the entity from the global list
// only called from with the CBaseEntity destructor
static bool g_fInCleanupDelete;

void CGlobalEntityList::UpdateName( IHandleEntity *pHandleEnt, CBaseHandle hEnt )
{
	CEntInfo *pEntInfo = const_cast<CEntInfo *>( GetEntInfoPtr( hEnt ) );
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	CBaseEntity *pEnt = EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
	pEntInfo->m_iClassName = MAKE_STRING( pEnt->GetClassname() );
	pEntInfo->m_iName = pEnt->GetEntityName();
}

void CGlobalEntityList::UpdateName( IHandleEntity *pEnt )
{
	UpdateName( pEnt, pEnt->GetRefEHandle() );
}

// mark an entity as deleted
void CGlobalEntityList::AddToDeleteList( IServerNetworkable *ent )
{
	if ( ent && ent->GetEntityHandle()->GetRefEHandle() != CBaseHandle( INVALID_EHANDLE ) )
	{
		g_DeleteList.push_back( ent );
	}
}

extern bool g_bDisableEhandleAccess;
// call this before and after each frame to delete all of the marked entities.
void CGlobalEntityList::CleanupDeleteList( void )
{
	CVProfScope _( "CGlobalEntityList::CleanupDeleteList", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	g_fInCleanupDelete = true;
	// clean up the vphysics delete list as well
	PhysOnCleanupDeleteList();

	g_bDisableEhandleAccess = true;
	for ( int i = 0; i < std::ssize( g_DeleteList ); i++ )
	{
		g_DeleteList[i]->Release();
	}
	g_bDisableEhandleAccess = false;
	g_DeleteList.clear();

	g_fInCleanupDelete = false;
}

int CGlobalEntityList::ResetDeleteList( void )
{
	int result = std::ssize( g_DeleteList );
	g_DeleteList.clear();
	return result;
}

	// add a class that gets notified of entity events
void CGlobalEntityList::AddListenerEntity( IEntityListener *pListener )
{
	if ( ks::find_index( m_entityListeners, pListener ) >= 0 )
	{
		contract_assert( false && "Can't add listeners multiple times\n" );
		return;
	}
	m_entityListeners.push_back( pListener );
}

void CGlobalEntityList::RemoveListenerEntity( IEntityListener *pListener )
{
	ks::erase_first( m_entityListeners, pListener );
}

void CGlobalEntityList::Clear( void )
{
	m_bClearingEntities = true;

	// Add all remaining entities in the game to the delete list and call appropriate UpdateOnRemove
	CBaseHandle hCur = FirstHandle();
	while ( hCur != InvalidHandle() )
	{
		IServerNetworkable *ent = GetServerNetworkable( hCur );
		if ( ent )
		{
			MDLCACHE_CRITICAL_SECTION();
			// Force UpdateOnRemove to be called
			UTIL_Remove( ent );
		}
		hCur = NextHandle( hCur );
	}
		
	CleanupDeleteList();
	// free the memory
	g_DeleteList.clear(); g_DeleteList.shrink_to_fit();

	g_EntsByClassname.clear();

	CBaseEntity::m_nDebugPlayer = -1;
	CBaseEntity::m_bInDebugSelect = false; 
	m_iHighestEnt = 0;
	m_iNumEnts = 0;

	m_bClearingEntities = false;
}

int CGlobalEntityList::NumberOfEntities( void )
{
	return m_iNumEnts;
}

int CGlobalEntityList::NumberOfEdicts( void )
{
	return m_iNumEdicts;
}

CBaseEntity *CGlobalEntityList::NextEnt( CBaseEntity *pCurrentEnt ) 
{ 
	if ( !pCurrentEnt )
	{
		const CEntInfo *pInfo = FirstEntInfo();
		if ( !pInfo )
			return nullptr;

		return (CBaseEntity *)pInfo->m_pEntity;
	}

	// Run through the list until we get a CBaseEntity.
	const CEntInfo *pList = GetEntInfoPtr( pCurrentEnt->GetRefEHandle() );
	if ( pList )
		pList = NextEntInfo(pList);

	while ( pList )
	{
		return (CBaseEntity *)pList->m_pEntity;
		pList = pList->m_pNext;
	}
	
	return nullptr; 

}

void CGlobalEntityList::ReportEntityFlagsChanged( CBaseEntity *pEntity, unsigned int flagsOld, unsigned int flagsNow )
{
	if ( pEntity->IsMarkedForDeletion() )
		return;
	// UNDONE: Move this into IEntityListener instead?
	unsigned int flagsChanged = flagsOld ^ flagsNow;
	if ( flagsChanged & FL_AIMTARGET )
	{
		unsigned int flagsAdded = flagsNow & flagsChanged;
		unsigned int flagsRemoved = flagsOld & flagsChanged;

		if ( flagsAdded & FL_AIMTARGET )
		{
			g_AimManager.AddEntity( pEntity );
		}
		if ( flagsRemoved & FL_AIMTARGET )
		{
			g_AimManager.RemoveEntity( pEntity );
		}
	}
}

void CGlobalEntityList::AddPostClientMessageEntity( CBaseEntity *pEntity )
{
	g_PostClientManager.AddEntity( pEntity );
}
void CGlobalEntityList::PostClientMessagesSent()
{
	g_PostClientManager.PostClientMessagesSent();
}

//-----------------------------------------------------------------------------
// Purpose: Used to confirm a pointer is a pointer to an entity, useful for
//			asserts.
//-----------------------------------------------------------------------------
bool CGlobalEntityList::IsEntityPtr( void *pTest )
{
	if ( pTest )
	{
		const CEntInfo *pInfo = FirstEntInfo();
		for ( ;pInfo; pInfo = pInfo->m_pNext )
		{
			if ( pTest == (void *)pInfo->m_pEntity )
				return true;
		}
	}

	return false; 
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given classname.
// Input  : pStartEntity - Last entity found, NULL to start a new iteration.
//			szName - Classname to search for.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *pEntity = (CBaseEntity *)pInfo->m_pEntity;
		if ( !pEntity )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( EntityNamesMatch( szName, pInfo->m_iClassName ) )
			return pEntity;
	}

	return nullptr;
}

CBaseEntity *CGlobalEntityList::FindEntityByClassnameFast( CBaseEntity *pStartEntity, string_t iszClassname )
{
	if ( pStartEntity )
	{
		return pStartEntity->m_pNextByClass;
	}

	auto it = g_EntsByClassname.find( iszClassname );
	if ( it != g_EntsByClassname.end() )
	{
		return it->second;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finds an entity given a procedural name.
// Input  : szName - The procedural name to search for, should start with '!'.
//			pSearchingEntity - 
//			pActivator - The activator entity if this was called from an input
//				or Use handler.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityProcedural( const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	//
	// Check for the name escape character.
	//
	if ( szName[0] == '!' )
	{
		const char *pName = szName + 1;

		//
		// It is a procedural name, look for the ones we understand.
		//
		if ( FStrEq( pName, "player" ) )
		{
			return (CBaseEntity *)UTIL_PlayerByIndex( 1 );
		}
		else if ( FStrEq( pName, "pvsplayer" ) )
		{
			if ( pSearchingEntity )
			{
				return CBaseEntity::Instance( UTIL_FindClientInPVS( pSearchingEntity->edict() ) );
			}
			else if ( pActivator )
			{
				// FIXME: error condition?
				return CBaseEntity::Instance( UTIL_FindClientInPVS( pActivator->edict() ) );
			}
			else
			{
				// FIXME: error condition?
				return (CBaseEntity *)UTIL_PlayerByIndex( 1 );
			}

		}
		else if ( FStrEq( pName, "activator" ) )
		{
			return pActivator;
		}
		else if ( FStrEq( pName, "caller" ) )
		{
			return pCaller;
		}
		else if ( FStrEq( pName, "picker" ) )
		{
			return UTIL_PlayerByIndex(1) ? UTIL_PlayerByIndex(1)->FindPickerEntity() : nullptr;
		}
		else if ( FStrEq( pName, "self" ) )
		{
			return pSearchingEntity;
		}
		else 
		{
			Warning( "Invalid entity search name %s\n", szName );
			contract_assert( 0 );
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given name.
// Input  : pStartEntity - Last entity found, NULL to start a new iteration.
//			szName - Name to search for.
//			pActivator - Activator entity if this was called from an input
//				handler or Use handler.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByName( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller, IEntityFindFilter *pFilter )
{
	if ( !szName || szName[0] == 0 )
		return nullptr;

	if ( szName[0] == '!' )
	{
		//
		// Avoid an infinite loop, only find one match per procedural search!
		//
		if (pStartEntity == nullptr)
			return FindEntityProcedural( szName, pSearchingEntity, pActivator, pCaller );

		return nullptr;
	}
	
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( pInfo->m_iName == NULL_STRING )
			continue;

		if ( EntityNamesMatch( szName, pInfo->m_iName ) )
		{
			if ( pFilter && !pFilter->ShouldFindEntity(ent) )
				continue;

			return ent;
		}
	}

	return nullptr;
}

CBaseEntity *CGlobalEntityList::FindEntityByNameFast( CBaseEntity *pStartEntity, string_t iszName )
{
	if ( iszName == NULL_STRING || STRING(iszName)[0] == 0 )
		return nullptr;

	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( pInfo->m_iName == NULL_STRING )
			continue;

		if ( pInfo->m_iName == iszName )
		{
			return ent;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pStartEntity - 
//			szModelName - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByModel( CBaseEntity *pStartEntity, const char *szModelName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->edict() || !ent->GetModelName() )
			continue;

		if ( FStrEq( STRING(ent->GetModelName()), szModelName ) )
			return ent;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given target.
// Input  : pStartEntity - 
//			szName - 
//-----------------------------------------------------------------------------
// FIXME: obsolete, remove
CBaseEntity	*CGlobalEntityList::FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->m_target )
			continue;

		if ( FStrEq( STRING(ent->m_target), szName ) )
			return ent;
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the entities with a given target.
// Input  : pStartEntity - 
//			szName - 
//-----------------------------------------------------------------------------
// FIXME: obsolete, remove
CBaseEntity	*CGlobalEntityList::FindEntityByOutputTarget( CBaseEntity *pStartEntity, string_t iTarget )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		datamap_t *dmap = ent->GetDataDescMap();
		while ( dmap )
		{
			int fields = dmap->dataNumFields;
			for ( int i = 0; i < fields; i++ )
			{
				typedescription_t *dataDesc = &dmap->dataDesc[i];
				if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((intp)ent + (intp)dataDesc->fieldOffset);
					if ( pOutput->GetActionForTarget( iTarget ) )
						return ent;
				}
			}

			dmap = dmap->baseMap;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Used to iterate all the entities within a sphere.
// Input  : pStartEntity - 
//			vecCenter - 
//			flRadius - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius )
{
	const CEntInfo *pInfo = pStartEntity ? GetEntInfoPtr( pStartEntity->GetRefEHandle() )->m_pNext : FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		if ( !ent->edict() )
			continue;

		Vector vecRelativeCenter;
		ent->CollisionProp()->WorldToCollisionSpace( vecCenter, &vecRelativeCenter );
		if ( !IsBoxIntersectingSphere( ent->CollisionProp()->OBBMins(),	ent->CollisionProp()->OBBMaxs(), vecRelativeCenter, flRadius ) )
			continue;

		return ent;
	}

	// nothing found
	return nullptr; 
}

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by name within a radius
// Input  : szName - Entity name to search for.
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByNameNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByName( pSearch, szName, pSearchingEntity, pActivator, pCaller )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the first entity by name within a radius
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for.
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByNameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		return gEntList.FindEntityByName( pEntity, szName, pSearchingEntity, pActivator, pCaller );
	}

	while ((pEntity = gEntList.FindEntityByName( pEntity, szName, pSearchingEntity, pActivator, pCaller )) != nullptr)
	{
		if ( !pEntity->edict() )
			continue;

		float flDist2 = (pEntity->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			return pEntity;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by class name withing given search radius.
// Input  : szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearest( const char *szName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassname( pSearch, szName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}

CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearestFast( string_t iszName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassnameFast( pSearch, iszName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by class name withing given search radius.
// Input  : szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameNearest2D( const char *szName, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = nullptr;

	//
	// Check for matching class names within the search radius.
	//
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		flMaxDist2 = MAX_TRACE_LENGTH * MAX_TRACE_LENGTH;
	}

	CBaseEntity *pSearch = nullptr;
	while ((pSearch = gEntList.FindEntityByClassname( pSearch, szName )) != nullptr)
	{
		if ( !pSearch->edict() )
			continue;

		float flDist2 = (pSearch->GetAbsOrigin().AsVector2D() - vecSrc.AsVector2D()).LengthSqr();

		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}

	return pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the first entity within radius distance by class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;
	float flMaxDist2 = flRadius * flRadius;
	if (flMaxDist2 == 0)
	{
		return gEntList.FindEntityByClassname( pEntity, szName );
	}

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szName )) != nullptr)
	{
		if ( !pEntity->edict() && !pEntity->IsEFlagSet( EFL_SERVER_ONLY ) )
			continue;

		// Instead of checking absorigin vs sphere, check if the obb intersects the sphere.
		Vector vecRelativeCenter;
		pEntity->CollisionProp()->WorldToCollisionSpace( vecSrc, &vecRelativeCenter );
		if ( IsBoxIntersectingSphere( pEntity->CollisionProp()->OBBMins(),	pEntity->CollisionProp()->OBBMaxs(), vecRelativeCenter, flRadius ) )
		{
			return pEntity;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the first entity within an extent by class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity class name, ie "info_target".
//			vecMins - Search mins.
//			vecMaxs - Search maxs.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityByClassnameWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecMins, const Vector &vecMaxs )
{
	//
	// Check for matching class names within the search radius.
	//
	CBaseEntity *pEntity = pStartEntity;

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szName )) != nullptr)
	{
		if ( !pEntity->edict() && !pEntity->IsEFlagSet( EFL_SERVER_ONLY ) )
			continue;

		// check if the aabb intersects the search aabb.
		Vector entMins, entMaxs;
		pEntity->CollisionProp()->WorldSpaceAABB( &entMins, &entMaxs );
		if ( IsBoxIntersectingBox( vecMins, vecMaxs, entMins, entMaxs ) )
		{
			return pEntity;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finds an entity by target name or class name.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGeneric( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByName( pStartEntity, szName, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassname( pStartEntity, szName );
	}

	return pEntity;
} 

//-----------------------------------------------------------------------------
// Purpose: Finds the first entity by target name or class name within a radius
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGenericWithin( CBaseEntity *pStartEntity, const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByNameWithin( pStartEntity, szName, vecSrc, flRadius, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassnameWithin( pStartEntity, szName, vecSrc, flRadius );
	}

	return pEntity;
} 

//-----------------------------------------------------------------------------
// Purpose: Finds the nearest entity by target name or class name within a radius.
// Input  : pStartEntity - The entity to start from when doing the search.
//			szName - Entity name to search for. Treated as a target name first,
//				then as an entity class name, ie "info_target".
//			vecSrc - Center of search radius.
//			flRadius - Search radius for classname search, 0 to search everywhere.
//			pSearchingEntity - The entity that is doing the search.
//			pActivator - The activator entity if this was called from an input
//				or Use handler, NULL otherwise.
// Output : Returns a pointer to the found entity, NULL if none.
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityGenericNearest( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pSearchingEntity, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	CBaseEntity *pEntity = nullptr;

	pEntity = gEntList.FindEntityByNameNearest( szName, vecSrc, flRadius, pSearchingEntity, pActivator, pCaller );
	if (!pEntity)
	{
		pEntity = gEntList.FindEntityByClassnameNearest( szName, vecSrc, flRadius );
	}

	return pEntity;
} 

//-----------------------------------------------------------------------------
// Purpose: Find the nearest entity along the facing direction from the given origin
//			within the angular threshold (ignores worldspawn) with the
//			given classname.
// Input  : origin - 
//			facing - 
//			threshold - 
//			classname - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityClassNearestFacing( const Vector &origin, const Vector &facing, float threshold, char *classname)
{
	float bestDot = threshold;
	CBaseEntity *best_ent = nullptr;

	const CEntInfo *pInfo = FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		// FIXME: why is this skipping pointsize entities?
		if (ent->IsPointSized() )
			continue;

		// Make vector to entity
		Vector	to_ent = (ent->GetAbsOrigin() - origin);

		VectorNormalize( to_ent );
		float dot = DotProduct (facing , to_ent );
		if (dot > bestDot) 
		{
			if (FClassnameIs(ent,classname))
			{
				// Ignore if worldspawn
				if (!FClassnameIs( ent, "worldspawn" )  && !FClassnameIs( ent, "soundent")) 
				{
					bestDot	= dot;
					best_ent = ent;
				}
			}
		}
	}
	return best_ent;
}

//-----------------------------------------------------------------------------
// Purpose: Find the nearest entity along the facing direction from the given origin
//			within the angular threshold (ignores worldspawn)
// Input  : origin - 
//			facing - 
//			threshold - 
//-----------------------------------------------------------------------------
CBaseEntity *CGlobalEntityList::FindEntityNearestFacing( const Vector &origin, const Vector &facing, float threshold)
{
	float bestDot = threshold;
	CBaseEntity *best_ent = nullptr;

	const CEntInfo *pInfo = FirstEntInfo();

	for ( ;pInfo; pInfo = pInfo->m_pNext )
	{
		CBaseEntity *ent = (CBaseEntity *)pInfo->m_pEntity;
		if ( !ent )
		{
			DevWarning( "NULL entity in global entity list!\n" );
			continue;
		}

		// Ignore logical entities
		if (!ent->edict())
			continue;

		// Make vector to entity
		Vector	to_ent = ent->WorldSpaceCenter() - origin;
		VectorNormalize(to_ent);

		float dot = DotProduct( facing, to_ent );
		if (dot <= bestDot) 
			continue;

		// Ignore if worldspawn
		if (!FStrEq( STRING(ent->m_iClassname), "worldspawn")  && !FStrEq( STRING(ent->m_iClassname), "soundent")) 
		{
			bestDot	= dot;
			best_ent = ent;
		}
	}
	return best_ent;
}

void CGlobalEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
	int i = handle.GetEntryIndex();

	// record current list details
	m_iNumEnts++;
	if ( i > m_iHighestEnt )
		m_iHighestEnt = i;

	// If it's a CBaseEntity, notify the listeners.
	CBaseEntity *pBaseEnt = EntityCast<CBaseEntity>( static_cast<IServerUnknown*>(pEnt)->GetBaseEntity() );
	if ( pBaseEnt->edict() )
		m_iNumEdicts++;
	
	// NOTE: Must be a CBaseEntity on server
	contract_assert( pBaseEnt );
	//DevMsg(2,"Created %s\n", pBaseEnt->GetClassname() );
	for ( i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityCreated( pBaseEnt );
	}
}

void CGlobalEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
#ifdef DEBUG
	if ( !g_fInCleanupDelete )
	{
		int i;
		for ( i = 0; i < std::ssize( g_DeleteList ); i++ )
		{
			if ( g_DeleteList[i]->GetEntityHandle() == pEnt )
			{
				ks::swap_remove( g_DeleteList, i );
				Msg( "ERROR: Entity being destroyed but previously threaded on g_DeleteList\n" );
				break;
			}
		}
	}
#endif

	CBaseEntity *pBaseEnt = EntityCast<CBaseEntity>( static_cast<IServerUnknown*>(pEnt)->GetBaseEntity() );
	if ( pBaseEnt->edict() )
		m_iNumEdicts--;

	m_iNumEnts--;
}

void CGlobalEntityList::NotifyCreateEntity( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return;

	// Make sure no one is trying to build an entity without game strings.
	contract_assert( MAKE_STRING( pEnt->GetClassname() ) == FindPooledString( pEnt->GetClassname() ) && 
		( pEnt->GetEntityName() == NULL_STRING || pEnt->GetEntityName() == FindPooledString( pEnt->GetEntityName().ToCStr() ) ) );

	contract_assert( pEnt->m_pPrevByClass == nullptr && pEnt->m_pNextByClass == nullptr && pEnt->m_ppListByClassHead == nullptr );

	CBaseEntity *&pHead = g_EntsByClassname.try_emplace( MAKE_STRING( pEnt->GetClassname() ), nullptr ).first->second;
	pEnt->m_ppListByClassHead = &pHead;
	if ( pHead )
	{
		pHead->m_pPrevByClass = pEnt;
		pEnt->m_pNextByClass = pHead;
		contract_assert( pEnt->m_pPrevByClass == nullptr );
	}
	pHead = pEnt;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityCreated( pEnt );
	}
}

void CGlobalEntityList::NotifySpawn( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntitySpawned( pEnt );
	}
}

// NOTE: This doesn't happen in OnRemoveEntity() specifically because 
// listeners may want to reference the object as it's being deleted
// OnRemoveEntity isn't called until the destructor and all data is invalid.
void CGlobalEntityList::NotifyRemoveEntity( CBaseEntity *pBaseEnt )
{
	if ( !pBaseEnt )
		return;

	//DevMsg(2,"Deleted %s\n", pBaseEnt->GetClassname() );
	for ( int i = std::ssize( m_entityListeners )-1; i >= 0; i-- )
	{
		m_entityListeners[i]->OnEntityDeleted( pBaseEnt );
	}

	if ( pBaseEnt->m_ppListByClassHead )
	{
		CBaseEntity *&pHead = *pBaseEnt->m_ppListByClassHead;
		if ( pHead == pBaseEnt )
		{
			pHead = pBaseEnt->m_pNextByClass;
			// Don't remove empty list, on the assumption that the number of classes that are not referenced again is small
			// Plus during map load we get a lot of precache others that hit this [8/8/2008 tom]
		}

		contract_assert( pHead != pBaseEnt );
		if ( pBaseEnt->m_pNextByClass )
		{
			pBaseEnt->m_pNextByClass->m_pPrevByClass = pBaseEnt->m_pPrevByClass;
		}

		if ( pBaseEnt->m_pPrevByClass )
		{
			pBaseEnt->m_pPrevByClass->m_pNextByClass = pBaseEnt->m_pNextByClass;
		}

		pBaseEnt->m_pPrevByClass = pBaseEnt->m_pNextByClass = nullptr;
		pBaseEnt->m_ppListByClassHead = nullptr;
	}
}

//-----------------------------------------------------------------------------
// NOTIFY LIST
// 
// Allows entities to get events fired when another entity changes
//-----------------------------------------------------------------------------
struct entitynotify_t
{
	CBaseEntity	*pNotify;
	CBaseEntity	*pWatched;
};
class CNotifyList : public INotify, public IEntityListener
{
public:
	// INotify
	void AddEntity( CBaseEntity *pNotify, CBaseEntity *pWatched );
	void RemoveEntity( CBaseEntity *pNotify, CBaseEntity *pWatched );
	void ReportNamedEvent( CBaseEntity *pEntity, const char *pEventName );
	void ClearEntity( CBaseEntity *pNotify );
	void ReportSystemEvent( CBaseEntity *pEntity, notify_system_event_t eventType, const notify_system_event_params_t &params );

	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity );
	virtual void OnEntityDeleted( CBaseEntity *pEntity );

	// Called from CEntityListSystem
	void LevelInitPreEntity();
	void LevelShutdownPreEntity();

private:
	std::vector< entitynotify_t >	m_notifyList;
};

void CNotifyList::AddEntity( CBaseEntity *pNotify, CBaseEntity *pWatched )
{
	// OPTIMIZE: Also flag pNotify for faster "RemoveAllNotify" ?
	pWatched->AddEFlags( EFL_NOTIFY );
	int index = std::ssize( m_notifyList ); m_notifyList.emplace_back();
	entitynotify_t &notify = m_notifyList[index];
	notify.pNotify = pNotify;
	notify.pWatched = pWatched;
}

// Remove noitfication for an entity
void CNotifyList::RemoveEntity( CBaseEntity *pNotify, CBaseEntity *pWatched )
{
	for ( int i = std::ssize( m_notifyList ); --i >= 0; )
	{
		if ( m_notifyList[i].pNotify == pNotify && m_notifyList[i].pWatched == pWatched)
		{
			ks::swap_remove( m_notifyList, i );
		}
	}
}

void CNotifyList::ReportNamedEvent( CBaseEntity *pEntity, const char *pInputName )
{
	variant_t emptyVariant;

	if ( !pEntity->IsEFlagSet(EFL_NOTIFY) )
		return;

	for ( int i = 0; i < std::ssize( m_notifyList ); i++ )
	{
		if ( m_notifyList[i].pWatched == pEntity )
		{
			m_notifyList[i].pNotify->AcceptInput( pInputName, pEntity, pEntity, emptyVariant, 0 );
		}
	}
}

void CNotifyList::LevelInitPreEntity()
{
	gEntList.AddListenerEntity( this );
}

void CNotifyList::LevelShutdownPreEntity( void )
{
	gEntList.RemoveListenerEntity( this );
	m_notifyList.clear(); m_notifyList.shrink_to_fit();
}

void CNotifyList::OnEntityCreated( CBaseEntity *pEntity )
{
}

void CNotifyList::OnEntityDeleted( CBaseEntity *pEntity )
{
	ReportDestroyEvent( pEntity );
	ClearEntity( pEntity );
}

// UNDONE: Slow linear search?
void CNotifyList::ClearEntity( CBaseEntity *pNotify )
{
	for ( int i = std::ssize( m_notifyList ); --i >= 0; )
	{
		if ( m_notifyList[i].pNotify == pNotify || m_notifyList[i].pWatched == pNotify)
		{
			ks::swap_remove( m_notifyList, i );
		}
	}
}

void CNotifyList::ReportSystemEvent( CBaseEntity *pEntity, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	if ( !pEntity->IsEFlagSet(EFL_NOTIFY) )
		return;

	for ( int i = 0; i < std::ssize( m_notifyList ); i++ )
	{
		if ( m_notifyList[i].pWatched == pEntity )
		{
			m_notifyList[i].pNotify->NotifySystemEvent( pEntity, eventType, params );
		}
	}
}

static CNotifyList g_NotifyList;
INotify *g_pNotify = &g_NotifyList;

class CEntityTouchManager : public IEntityListener
{
public:
	// called by CEntityListSystem
	void LevelInitPreEntity() 
	{ 
		gEntList.AddListenerEntity( this );
		Clear(); 
	}
	void LevelShutdownPostEntity() 
	{ 
		gEntList.RemoveListenerEntity( this );
		Clear(); 
	}
	void FrameUpdatePostEntityThink();

	void Clear()
	{
		m_updateList.clear(); m_updateList.shrink_to_fit();
	}
	
	// IEntityListener
	virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
		if ( !pEntity->GetCheckUntouch() )
			return;
		int index = ks::find_index( m_updateList, pEntity );
		if ( ks::is_valid_index( m_updateList, index ) )
		{
			ks::swap_remove( m_updateList, index );
		}
	}
	void AddEntity( CBaseEntity *pEntity )
	{
		if ( pEntity->IsMarkedForDeletion() )
			return;

		contract_assert( !ks::is_valid_index( m_updateList, ks::find_index( m_updateList, pEntity ) ) );
		m_updateList.push_back( pEntity );
	}

	void RemoveEntity( CBaseEntity *pEntity )
	{
		ks::swap_erase_first( m_updateList, pEntity );
	}

private:
	std::vector< CBaseEntity * >	m_updateList;
};

static CEntityTouchManager g_TouchManager;

void EntityTouch_Add( CBaseEntity *pEntity )
{
	g_TouchManager.AddEntity( pEntity );
}

void EntityTouch_Remove( CBaseEntity *pEntity )
{
	g_TouchManager.RemoveEntity( pEntity );
}

void CEntityTouchManager::FrameUpdatePostEntityThink()
{
	ks::ScratchArena<> scratch;
	CVProfScope _( "CEntityTouchManager::FrameUpdatePostEntityThink", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	// Loop through all entities again, checking their untouch if flagged to do so
	
	int count = std::ssize( m_updateList );
	if ( count )
	{
		// copy off the list
		CBaseEntity **ents = scratch.Alloc<CBaseEntity *>( count );
		std::memcpy( ents, m_updateList.data(), sizeof(CBaseEntity *) * count );
		// clear it
		m_updateList.clear();
		
		// now update those ents
		for ( int i = 0; i < count; i++ )
		{
			//Assert( ents[i]->GetCheckUntouch() );
			if ( ents[i]->GetCheckUntouch() )
			{
				ents[i]->PhysicsCheckForEntityUntouch();
			}
		}
	}
}

class CRespawnEntitiesFilter : public IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname )
	{
		// Create everything but the world
		return V_stricmp( pClassname, "worldspawn" ) != 0;
	}

	virtual CBaseEntity* CreateNextEntity( const char *pClassname )
	{
		return CreateEntityByName( pClassname );
	}
};

// One hook to rule them all...
// Since most of the little list managers in here only need one or two of the game
// system callbacks, this hook is a game system that passes them the appropriate callbacks
class CEntityListSystem : public CAutoGameSystemPerFrame
{
public:
	CEntityListSystem( char const *name ) : CAutoGameSystemPerFrame( name )
	{
		m_bRespawnAllEntities = false;
	}
	void LevelInitPreEntity()
	{
		g_NotifyList.LevelInitPreEntity();
		g_TouchManager.LevelInitPreEntity();
		g_AimManager.LevelInitPreEntity();
		g_SimThinkManager.LevelInitPreEntity();
	}
	void LevelShutdownPreEntity()
	{
		g_NotifyList.LevelShutdownPreEntity();
	}
	void LevelShutdownPostEntity()
	{
		g_TouchManager.LevelShutdownPostEntity();
		g_AimManager.LevelShutdownPostEntity();
		g_PostClientManager.LevelShutdownPostEntity();
		g_SimThinkManager.LevelShutdownPostEntity();
		CBaseEntityClassList *pClassList = s_pClassLists;
		while ( pClassList )
		{
			pClassList->LevelShutdownPostEntity();
			pClassList = pClassList->m_pNextClassList;
		}
	}

	void FrameUpdatePostEntityThink()
	{
		g_TouchManager.FrameUpdatePostEntityThink();

		if ( m_bRespawnAllEntities )
		{
			m_bRespawnAllEntities = false;

			// Don't change globalstate owing to deletion here
			GlobalEntity_EnableStateUpdates( false );

			// Remove all entities
			int nPlayerIndex = -1;
			CBaseEntity *pEnt = gEntList.FirstEnt();
			while ( pEnt )
			{
				CBaseEntity *pNextEnt = gEntList.NextEnt( pEnt );
				if ( pEnt->IsPlayer() )
				{
					nPlayerIndex = pEnt->entindex();
				}
				if ( !pEnt->IsEFlagSet( EFL_KEEP_ON_RECREATE_ENTITIES ) )
				{
					UTIL_Remove( pEnt );
				}
				pEnt = pNextEnt;
			}
			
			gEntList.CleanupDeleteList();

			GlobalEntity_EnableStateUpdates( true );

			// Allows us to immediately re-use the edict indices we just freed to avoid edict overflow
			engine->AllowImmediateEdictReuse();

			CRespawnEntitiesFilter filter;
			MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );

			// Allocate a CBasePlayer for pev, and call spawn
			if ( nPlayerIndex >= 0 )
			{
				edict_t *pEdict = INDEXENT( nPlayerIndex );
				ClientPutInServer( pEdict, "unnamed" );
				ClientActive( pEdict, false );

				CBasePlayer *pPlayer = ( CBasePlayer * )CBaseEntity::Instance( pEdict );
				SceneManager_ClientActive( pPlayer );
			}
		}
	}

	bool m_bRespawnAllEntities;
};

static CEntityListSystem g_EntityListSystem( "CEntityListSystem" );

//-----------------------------------------------------------------------------
// Respawns all entities in the level
//-----------------------------------------------------------------------------
void RespawnEntities()
{
	g_EntityListSystem.m_bRespawnAllEntities = true;
}

static ConCommand restart_entities( side_registry(), "respawn_entities", RespawnEntities, "Respawn all the entities in the map.", FCVAR_CHEAT | FCVAR_SPONLY );

class CSortedEntityList
{
public:
	CSortedEntityList() : m_sortedList(), m_emptyCount(0) {}

	struct CEntityReportLess
	{
		bool operator()( CBaseEntity *src1, CBaseEntity *src2 ) const
		{
			return stricmp( src1->GetClassname(), src2->GetClassname() ) < 0;
		}
	};

	void AddEntityToList( CBaseEntity *pEntity )
	{
		if ( !pEntity )
		{
			m_emptyCount++;
		}
		else
		{
			m_sortedList.insert( pEntity );
		}
	}
	void ReportEntityList()
	{
		const char *pLastClass = "";
		int count = 0;
		int edicts = 0;
		for ( CBaseEntity *pEntity : m_sortedList )
		{
			if ( !pEntity )
				continue;

			if ( pEntity->edict() )
				edicts++;

			const char *pClassname = pEntity->GetClassname();
			if ( !FStrEq( pClassname, pLastClass ) )
			{
				if ( count )
				{
					Msg("Class: %s (%d)\n", pLastClass, count );
				}

				pLastClass = pClassname;
				count = 1;
			}
			else
				count++;
		}
		if ( pLastClass[0] != 0 && count )
		{
			Msg("Class: %s (%d)\n", pLastClass, count );
		}
		if ( !m_sortedList.empty() )
		{
			Msg("Total %d entities (%d empty, %d edicts)\n", (int)std::ssize( m_sortedList ), m_emptyCount, edicts );
		}
	}
private:
	std::flat_multiset< CBaseEntity *, CEntityReportLess > m_sortedList;
	int		m_emptyCount;
};

static void report_entities( const CCommand &args );
static ConCommand report_entities_command( side_registry(), "report_entities", report_entities, "Lists all entities" );
static void report_entities( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSortedEntityList list;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		list.AddEntityToList( pEntity );
		pEntity = gEntList.NextEnt( pEntity );
	}
	list.ReportEntityList();
}

static void report_touchlinks( const CCommand &args );
static ConCommand report_touchlinks_command( side_registry(), "report_touchlinks", report_touchlinks, "Lists all touchlinks" );
static void report_touchlinks( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSortedEntityList list;
	CBaseEntity *pEntity = gEntList.FirstEnt();
	const char *pClassname = nullptr;
	if ( args.ArgC() > 1 )
	{
		pClassname = args.Arg(1);
	}
	while ( pEntity )
	{
		if ( !pClassname || FClassnameIs(pEntity, pClassname) )
		{
			touchlink_t *root = ( touchlink_t * )pEntity->GetDataObject( TOUCHLINK );
			if ( root )
			{
				touchlink_t *link = root->nextLink;
				while ( link != root )
				{
					list.AddEntityToList( link->entityTouched );
					link = link->nextLink;
				}
			}
		}
		pEntity = gEntList.NextEnt( pEntity );
	}
	list.ReportEntityList();
}

static void report_simthinklist( const CCommand &args );
static ConCommand report_simthinklist_command( side_registry(), "report_simthinklist", report_simthinklist, "Lists all simulating/thinking entities" );
static void report_simthinklist( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBaseEntity *pTmp[NUM_ENT_ENTRIES];
	int count = SimThink_ListCopy( pTmp, std::size( pTmp ) );

	CSortedEntityList list;
	for ( int i = 0; i < count; i++ )
	{
		if ( !pTmp[i] )
			continue;

		list.AddEntityToList( pTmp[i] );
	}
	list.ReportEntityList();
}

CBaseHandle CGlobalEntityList::AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum )
{
	CBaseHandle h = CBaseEntityList::AddNetworkableEntity( pEnt, index, iForcedSerialNum );
	UpdateName( pEnt, h );
	return h;
}

CBaseHandle CGlobalEntityList::AddNonNetworkableEntity( IHandleEntity *pEnt )
{
	CBaseHandle h = CBaseEntityList::AddNonNetworkableEntity( pEnt );
	UpdateName( pEnt, h );
	return h;
}

edict_t* CGlobalEntityList::GetEdict( CBaseHandle hEnt ) const
{
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	if ( pUnk )
		return pUnk->GetNetworkable()->GetEdict();
	else
		return nullptr;
}

CBaseNetworkable* CGlobalEntityList::GetBaseNetworkable( CBaseHandle hEnt ) const
{
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	if ( pUnk )
		return pUnk->GetNetworkable()->GetBaseNetworkable();
	else
		return nullptr;
}

IServerNetworkable* CGlobalEntityList::GetServerNetworkable( CBaseHandle hEnt ) const
{
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	if ( pUnk )
		return pUnk->GetNetworkable();
	else
		return nullptr;
}

CBaseEntity* CGlobalEntityList::GetBaseEntity( CBaseHandle hEnt ) const
{
	IServerUnknown *pUnk = static_cast<IServerUnknown*>(LookupEntity( hEnt ));
	if ( pUnk )
		return EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
	else
		return nullptr;
}

enum
{
	SERIAL_MASK = 0x7fff // the max value of a serial number, rolls back to 0 when it hits this limit
};

void CEntInfo::ClearLinks()
{
	m_pPrev = m_pNext = this;
}

CBaseEntityList::CEntInfoList::CEntInfoList()
{
	m_pHead = nullptr;
	m_pTail = nullptr;
}

// NOTE: Cut from UtlFixedLinkedList<>, UNDONE: Find a way to share this code
void CBaseEntityList::CEntInfoList::LinkBefore( CEntInfo *pBefore, CEntInfo *pElement )
{
	contract_assert( pElement );
	
	// Unlink it if it's in the list at the moment
	Unlink(pElement);
	
	// The element *after* our newly linked one is the one we linked before.
	pElement->m_pNext = pBefore;
	
	if (pBefore == nullptr)
	{
		// In this case, we're linking to the end of the list, so reset the tail
		pElement->m_pPrev = m_pTail;
		m_pTail = pElement;
	}
	else
	{
		// Here, we're not linking to the end. Set the prev pointer to point to
		// the element we're linking.
		pElement->m_pPrev = pBefore->m_pPrev;
		pBefore->m_pPrev = pElement;
	}
	
	// Reset the head if we linked to the head of the list
	if (pElement->m_pPrev == nullptr)
	{
		m_pHead = pElement;
	}
	else
	{
		pElement->m_pPrev->m_pNext = pElement;
	}
}

void CBaseEntityList::CEntInfoList::LinkAfter( CEntInfo *pAfter, CEntInfo *pElement )
{
	contract_assert( pElement );
	
	// Unlink it if it's in the list at the moment
	if ( IsInList(pElement) )
		Unlink(pElement);
	
	// The element *before* our newly linked one is the one we linked after
	pElement->m_pPrev = pAfter;
	if (pAfter == nullptr)
	{
		// In this case, we're linking to the head of the list, reset the head
		pElement->m_pNext = m_pHead;
		m_pHead = pElement;
	}
	else
	{
		// Here, we're not linking to the end. Set the next pointer to point to
		// the element we're linking.
		pElement->m_pNext = pAfter->m_pNext;
		pAfter->m_pNext = pElement;
	}
	
	// Reset the tail if we linked to the tail of the list
	if (pElement->m_pNext == nullptr )
	{
		m_pTail = pElement;
	}
	else
	{
		pElement->m_pNext->m_pPrev = pElement;
	}
}

void CBaseEntityList::CEntInfoList::Unlink( CEntInfo *pElement )
{
	if (IsInList(pElement))
	{
		// If we're the first guy, reset the head
		// otherwise, make our previous node's next pointer = our next
		if ( pElement->m_pPrev )
		{
			pElement->m_pPrev->m_pNext = pElement->m_pNext;
		}
		else
		{
			m_pHead = pElement->m_pNext;
		}
		
		// If we're the last guy, reset the tail
		// otherwise, make our next node's prev pointer = our prev
		if ( pElement->m_pNext )
		{
			pElement->m_pNext->m_pPrev = pElement->m_pPrev;
		}
		else
		{
			m_pTail = pElement->m_pPrev;
		}
		
		// This marks this node as not in the list, 
		// but not in the free list either
		pElement->ClearLinks();
	}
}

bool CBaseEntityList::CEntInfoList::IsInList( CEntInfo *pElement )
{
	return pElement->m_pPrev != pElement;
}

CBaseEntityList::CBaseEntityList()
{
	// These are not in any list (yet)
	int i;
	for ( i = 0; i < NUM_ENT_ENTRIES; i++ )
	{
		m_EntPtrArray[i].ClearLinks();
		m_EntPtrArray[i].m_SerialNumber = (std::rand()& SERIAL_MASK); // generate random starting serial number
		m_EntPtrArray[i].m_pEntity = nullptr;
	}

	// make a free list of the non-networkable entities
	// Initially, all the slots are free.
	for ( i=MAX_EDICTS+1; i < NUM_ENT_ENTRIES; i++ )
	{
		CEntInfo *pList = &m_EntPtrArray[i];
		m_freeNonNetworkableList.AddToTail( pList );
	}
}

CBaseEntityList::~CBaseEntityList()
{
	CEntInfo *pList = m_activeList.Head();

	while ( pList )
	{
		CEntInfo *pNext = pList->m_pNext;
		RemoveEntityAtSlot( GetEntInfoIndex( pList ) );
		pList = pNext;
	}
}

CBaseHandle CBaseEntityList::AddNetworkableEntity( IHandleEntity *pEnt, int index, int iForcedSerialNum )
{
	contract_assert( index >= 0 && index < MAX_EDICTS );
	return AddEntityAtSlot( pEnt, index, iForcedSerialNum );
}

CBaseHandle CBaseEntityList::AddNonNetworkableEntity( IHandleEntity *pEnt )
{
	// Find a slot for it.
	CEntInfo *pSlot = m_freeNonNetworkableList.Head();
	if ( !pSlot )
	{
		Warning( "CBaseEntityList::AddNonNetworkableEntity: no free slots!\n" );
		contract_assert( 0 );
		return CBaseHandle();
	}

	// Move from the free list into the allocated list.
	m_freeNonNetworkableList.Unlink( pSlot );
	int iSlot = GetEntInfoIndex( pSlot );
	
	return AddEntityAtSlot( pEnt, iSlot, -1 );
}

void CBaseEntityList::RemoveEntity( CBaseHandle handle )
{
	RemoveEntityAtSlot( handle.GetEntryIndex() );
}

CBaseHandle CBaseEntityList::AddEntityAtSlot( IHandleEntity *pEnt, int iSlot, int iForcedSerialNum )
{
	// Init the CSerialEntity.
	CEntInfo *pSlot = &m_EntPtrArray[iSlot];
	contract_assert( pSlot->m_pEntity == nullptr );
	pSlot->m_pEntity = pEnt;

	// Force the serial number (client-only)?
	if ( iForcedSerialNum != -1 )
	{
		pSlot->m_SerialNumber = iForcedSerialNum;
		
			// Only the client should force the serial numbers.
			contract_assert( false );
	}
	
	// Update our list of active entities.
	m_activeList.AddToTail( pSlot );
	CBaseHandle retVal( iSlot, pSlot->m_SerialNumber );

	// Tell the entity to store its handle.
	pEnt->SetRefEHandle( retVal );

	// Notify any derived class.
	OnAddEntity( pEnt, retVal );
	return retVal;
}

void CBaseEntityList::RemoveEntityAtSlot( int iSlot )
{
	contract_assert( iSlot >= 0 && iSlot < NUM_ENT_ENTRIES );

	CEntInfo *pInfo = &m_EntPtrArray[iSlot];

	if ( pInfo->m_pEntity )
	{
		pInfo->m_pEntity->SetRefEHandle( INVALID_EHANDLE );

		// Notify the derived class that we're about to remove this entity.
		OnRemoveEntity( pInfo->m_pEntity, CBaseHandle( iSlot, pInfo->m_SerialNumber ) );

		// Increment the serial # so ehandles go invalid.
		pInfo->m_pEntity = nullptr;
		pInfo->m_SerialNumber = ( pInfo->m_SerialNumber+1)& SERIAL_MASK;

		m_activeList.Unlink( pInfo );

		// Add the slot back to the free list if it's a non-networkable entity.
		if ( iSlot >= MAX_EDICTS )
		{
			m_freeNonNetworkableList.AddToTail( pInfo );
		}
	}
}

void CBaseEntityList::OnAddEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
}

void CBaseEntityList::OnRemoveEntity( IHandleEntity *pEnt, CBaseHandle handle )
{
}

int CBaseEntityList::GetEntInfoIndex( const CEntInfo *pEntInfo ) const
{
	contract_assert( pEntInfo );
	int index = (int)(pEntInfo - m_EntPtrArray);
	contract_assert( index >= 0 && index < NUM_ENT_ENTRIES );
	return index;
}

CBaseHandle CBaseEntityList::GetNetworkableHandle( int iEntity ) const
{
	contract_assert( iEntity >= 0 && iEntity < MAX_EDICTS );
	if ( m_EntPtrArray[iEntity].m_pEntity )
		return CBaseHandle( iEntity, m_EntPtrArray[iEntity].m_SerialNumber );
	else
		return CBaseHandle();
}

IHandleEntity* CBaseEntityList::LookupEntity( const CBaseHandle &handle ) const
{
	if ( !handle.IsValid() )
		return nullptr;

	// You can use this to determine when something is trying to resolve
	// handles to static props as if they were handles to ordinary props,
	// but in practice this is done a great deal and seems benign.
	/*
	// 0x40000000 = STATICPROP_EHANDLE_MASK
	AssertMsg( ( handle.GetSerialNumber() != (0x40000000 >> NUM_SERIAL_NUM_SHIFT_BITS) ) , 
		"Tried to look up a static prop as if it was a regular entity, a bad pointer and doom are the result.\n" ); 
	*/

	const CEntInfo *pInfo = &m_EntPtrArray[ handle.GetEntryIndex() ];
	if ( pInfo->m_SerialNumber == handle.GetSerialNumber() )
		return pInfo->m_pEntity;
	else
		return nullptr;
}

IHandleEntity* CBaseEntityList::LookupEntityByNetworkIndex( int edictIndex ) const
{
	// (Legacy support).
	if ( edictIndex < 0 )
		return nullptr;

	contract_assert( edictIndex < NUM_ENT_ENTRIES );
	return m_EntPtrArray[edictIndex].m_pEntity;
}

CBaseHandle CBaseEntityList::FirstHandle() const
{
	if ( !m_activeList.Head() )
		return INVALID_EHANDLE;

	int index = GetEntInfoIndex( m_activeList.Head() );
	return CBaseHandle( index, m_EntPtrArray[index].m_SerialNumber );
}

CBaseHandle CBaseEntityList::NextHandle( CBaseHandle hEnt ) const
{
	int iSlot = hEnt.GetEntryIndex();
	CEntInfo *pNext = m_EntPtrArray[iSlot].m_pNext;
	if ( !pNext )
		return INVALID_EHANDLE;

	int index = GetEntInfoIndex( pNext );

	return CBaseHandle( index, m_EntPtrArray[index].m_SerialNumber );
}

CBaseHandle CBaseEntityList::InvalidHandle()
{
	return INVALID_EHANDLE;
}

const CEntInfo *CBaseEntityList::FirstEntInfo() const
{
	return m_activeList.Head();
}

const CEntInfo *CBaseEntityList::NextEntInfo( const CEntInfo *pInfo ) const
{
	return pInfo->m_pNext;
}

const CEntInfo *CBaseEntityList::GetEntInfoPtr( const CBaseHandle &hEnt ) const
{
	int iSlot = hEnt.GetEntryIndex();
	return &m_EntPtrArray[iSlot];
}

const CEntInfo *CBaseEntityList::GetEntInfoPtrByIndex( int index ) const
{
	return &m_EntPtrArray[index];
}

void CBaseEntityList::ForceEntSerialNumber( int iEntIndex, int iSerialNumber )
{
	m_EntPtrArray[iEntIndex].m_SerialNumber = iSerialNumber;
}
