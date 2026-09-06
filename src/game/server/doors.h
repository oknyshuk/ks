//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOORS_H
#define DOORS_H

#include "reflect_annotations.h"
#pragma once


#include "locksounds.h"
#include "entityoutput.h"

//Since I'm here, might as well explain how these work.  Base.fgd is the file that connects
//flags to entities.  It is full of lines with this number, a label, and a default value.
//Voila, dynamicly generated checkboxes on the Flags tab of Entity Properties.

// doors
#define SF_DOOR_ROTATE_YAW			0		// yaw by default
#define	SF_DOOR_START_OPEN_OBSOLETE	1
#define SF_DOOR_ROTATE_BACKWARDS	2
#define SF_DOOR_NONSOLID_TO_PLAYER	4
#define SF_DOOR_PASSABLE			8
#define SF_DOOR_ONEWAY				16
#define	SF_DOOR_NO_AUTO_RETURN		32
#define SF_DOOR_ROTATE_ROLL			64
#define SF_DOOR_ROTATE_PITCH		128
#define SF_DOOR_PUSE				256	// door can be opened by player's use button.
#define SF_DOOR_NONPCS				512	// NPC can't open
#define SF_DOOR_PTOUCH				1024 // player touch opens
#define SF_DOOR_LOCKED				2048	// Door is initially locked
#define SF_DOOR_SILENT				4096	// Door plays no audible sound, and does not alert NPCs when opened
#define	SF_DOOR_USE_CLOSES			8192	// Door can be +used to close before its autoreturn delay has expired.
#define SF_DOOR_SILENT_TO_NPCS		16384	// Does not alert NPC's when opened.
#define SF_DOOR_IGNORE_USE			32768	// Completely ignores player +use commands.
#define SF_DOOR_NEW_USE_RULES		65536	// For func_door entities, behave more like prop_door_rotating with respect to +USE (changelist 242482)
#define SF_DOOR_START_BREAKABLE		524288


enum FuncDoorSpawnPos_t
{
	FUNC_DOOR_SPAWN_CLOSED = 0,
	FUNC_DOOR_SPAWN_OPEN,
};


class [[= ks::reflect::NetTable{ .name = "DT_BaseDoor" } ]]
      [[= ks::reflect::KeyFrom<"m_ls.sLockedSound", ks::reflect::Key{ .name = "locked_sound", .as = FIELD_SOUNDNAME } >{} ]]
      [[= ks::reflect::KeyFrom<"m_ls.sUnlockedSound", ks::reflect::Key{ .name = "unlocked_sound", .as = FIELD_SOUNDNAME } >{} ]]
      CBaseDoor : public CBaseToggle
{
public:
	DECLARE_CLASS( CBaseDoor, CBaseToggle );

	DECLARE_SERVERCLASS();

	void Spawn( void );
	void Precache( void );
	bool CreateVPhysics();
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual void StartBlocked( CBaseEntity *pOther );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void EndBlocked( void );

	void Activate( void );

	virtual int	ObjectCaps( void ) 
	{
		int flags = BaseClass::ObjectCaps();
		if ( HasSpawnFlags( SF_DOOR_PUSE ) )
			return flags | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS;

		return flags;
	};

	DECLARE_DATADESC();

	// This is ONLY used by the node graph to test movement through a door
	[[= ks::reflect::Input{ .name = "SetToggleState", .type = FIELD_FLOAT } ]] void InputSetToggleState( inputdata_t &inputdata );
	virtual void SetToggleState( int state );

	virtual bool IsRotatingDoor() { return false; }
	virtual bool ShouldSavePhysics();
	// used to selectivly override defaults
	void DoorTouch( CBaseEntity *pOther );

	// local functions
	int DoorActivate( );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorHitBottom( void );
	void UpdateAreaPortals( bool isOpen );
	void Unlock( void );
	void Lock( void );
	int GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax );

	// Input handlers
	[[= ks::reflect::Input{ .name = "Close", .type = FIELD_VOID } ]] void InputClose( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Lock", .type = FIELD_VOID } ]] void InputLock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Open", .type = FIELD_VOID } ]] void InputOpen( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetSpeed", .type = FIELD_FLOAT } ]] void InputSetSpeed( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "movedir" } ]] Vector m_vecMoveDir;		// The direction of motion for linear moving doors.

	locksound_t m_ls;			// door lock sounds
	
	byte	m_bLockedSentence;	
	byte	m_bUnlockedSentence;

	[[= ks::reflect::Key{ .name = "forceclosed" } ]] bool	m_bForceClosed;			// If set, always close, even if we're blocked.
	bool	m_bDoorGroup;
	bool	m_bLocked;				// Whether the door is locked
	[[= ks::reflect::Key{ .name = "ignoredebris" } ]] bool	m_bIgnoreDebris;
	
	[[= ks::reflect::Key{ .name = "spawnpos" } ]] FuncDoorSpawnPos_t m_eSpawnPosition;

	[[= ks::reflect::Key{ .name = "dmg" } ]] float	m_flBlockDamage;		// Damage inflicted when blocked.
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "noise1" } ]] string_t	m_NoiseMoving;		//Start/Looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "noise2" } ]] string_t	m_NoiseArrived;		//End sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "startclosesound" } ]] string_t	m_NoiseMovingClosed;		//Start/Looping sound
	[[= ks::reflect::As{ FIELD_SOUNDNAME } ]] [[= ks::reflect::Key{ .name = "closesound" } ]] string_t	m_NoiseArrivedClosed;		//End sound
	[[= ks::reflect::Key{ .name = "chainstodoor" } ]] string_t	m_ChainTarget;		///< Entity name to pass Touch and Use events to

	CNetworkVar( float, m_flWaveHeight, [[= ks::reflect::Net{ .bits = 8, .low = 0.0f, .high = 8.0f, .flags = SPROP_ROUNDUP } ]] [[= ks::reflect::Key{ .name = "WaveHeight" } ]] );

	// Outputs
	[[= ks::reflect::Key{ .name = "OnBlockedClosing" } ]] COutputEvent m_OnBlockedClosing;		// Triggered when the door becomes blocked while closing.
	[[= ks::reflect::Key{ .name = "OnBlockedOpening" } ]] COutputEvent m_OnBlockedOpening;		// Triggered when the door becomes blocked while opening.
	[[= ks::reflect::Key{ .name = "OnUnblockedClosing" } ]] COutputEvent m_OnUnblockedClosing;		// Triggered when the door becomes unblocked while closing.
	[[= ks::reflect::Key{ .name = "OnUnblockedOpening" } ]] COutputEvent m_OnUnblockedOpening;		// Triggered when the door becomes unblocked while opening.
	[[= ks::reflect::Key{ .name = "OnFullyClosed" } ]] COutputEvent m_OnFullyClosed;			// Triggered when the door reaches the fully closed position.
	[[= ks::reflect::Key{ .name = "OnFullyOpen" } ]] COutputEvent m_OnFullyOpen;				// Triggered when the door reaches the fully open position.
	[[= ks::reflect::Key{ .name = "OnClose" } ]] COutputEvent m_OnClose;					// Triggered when the door is told to close.
	[[= ks::reflect::Key{ .name = "OnOpen" } ]] COutputEvent m_OnOpen;					// Triggered when the door is told to open.
	[[= ks::reflect::Key{ .name = "OnLockedUse" } ]] COutputEvent m_OnLockedUse;				// Triggered when the user tries to open a locked door.

	void			StartMovingSound( void );
	virtual void	StopMovingSound( void );
	void			MovingSoundThink( void );
	
	bool		ShouldLoopMoveSound( void ) { return m_bLoopMoveSound; }
	[[= ks::reflect::Key{ .name = "loopmovesound" } ]] bool		m_bLoopMoveSound;			// Move sound loops until stopped

private:
	void ChainUse( void );	///< Chains +use on through to m_ChainTarget
	void ChainTouch( CBaseEntity *pOther );	///< Chains touch on through to m_ChainTarget
	void SetChaining( bool chaining )	{ m_isChaining = chaining; }	///< Latch to prevent recursion
	bool m_isChaining;

	void CloseAreaPortalsThink( void );	///< Delays turning off area portals when closing doors to prevent visual artifacts
};

#endif // DOORS_H
