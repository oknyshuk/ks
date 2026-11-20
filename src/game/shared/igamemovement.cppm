// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

module;
#include "gametrace.h"
#include "vphysics_interface.h"
export module ks.game.igamemovement;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.game.entitycore;
import ks.game.shareddefs;

// The movement interfaces both sides implement. They name the entity cores, not a side's entity
// classes, so one movement implementation can serve both.
export
{
enum
{
	WL_NotInWater=0,
	WL_Feet,
	WL_Waist,
	WL_Eyes
};

//-----------------------------------------------------------------------------
// An entity identifier that works in both game + client dlls
//-----------------------------------------------------------------------------

typedef CBaseHandle EntityHandle_t;

inline constexpr auto INVALID_ENTITY_HANDLE = INVALID_EHANDLE;

//-----------------------------------------------------------------------------
// Functions the engine provides to IGameMovement to assist in its movement.
//-----------------------------------------------------------------------------

class IMoveHelper
{
public:
	// Methods associated with a particular entity
	virtual	char const*		GetName( EntityHandle_t handle ) const = 0;

	// sets the entity being moved
	virtual void	SetHost( CEntityCore *host ) = 0;

	// Adds the trace result to touch list, if contact is not already in list.
	virtual void	ResetTouchList( void ) = 0;
	virtual bool	AddToTouched( const CGameTrace& tr, const Vector& impactvelocity ) = 0;
	virtual void	ProcessImpacts( void ) = 0;

	// Numbered line printf
	virtual void	Con_NPrintf( int idx, char const* fmt, ... ) = 0;

	// These have separate server vs client impementations
	virtual void	StartSound( const Vector& origin, int channel, char const* sample, float volume, soundlevel_t soundlevel, int fFlags, int pitch ) = 0;
	virtual void	StartSound( const Vector& origin, const char *soundname ) = 0; 
	virtual void	PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, Vector& origin, Vector& angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 ) = 0;

	// Apply falling damage to m_pHostPlayer based on m_pHostPlayer->m_flFallVelocity.
	virtual bool	PlayerFallingDamage( void ) = 0;

	// Apply falling damage to m_pHostPlayer based on m_pHostPlayer->m_flFallVelocity.
	virtual void	PlayerSetAnimation( PLAYER_ANIM playerAnim ) = 0;

	virtual IPhysicsSurfaceProps *GetSurfaceProps( void ) = 0;

	virtual bool IsWorldEntity( const CBaseHandle &handle ) = 0;

protected:
	// Clients shouldn't call delete directly
	virtual			~IMoveHelper() {}
};

//-----------------------------------------------------------------------------
// Name of the class implementing the game movement.
//-----------------------------------------------------------------------------

inline constexpr const char *INTERFACENAME_GAMEMOVEMENT = "GameMovement001";

//-----------------------------------------------------------------------------
// Forward declarations.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Encapsulated input parameters to player movement.
//-----------------------------------------------------------------------------

class CMoveData
{
public:
	bool			m_bFirstRunOfFunctions : 1;
	bool			m_bGameCodeMovedPlayer : 1;
	bool			m_bNoAirControl : 1;

	EntityHandle_t	m_nPlayerHandle;	// edict index on server, client entity handle on client

	int				m_nImpulseCommand;	// Impulse command issued.
	QAngle			m_vecViewAngles;	// Command view angles (local space)
	QAngle			m_vecAbsViewAngles;	// Command view angles (world space)
	int				m_nButtons;			// Attack buttons.
	int				m_nOldButtons;		// From host_client->oldbuttons;
	float			m_flForwardMove;
	float			m_flSideMove;
	float			m_flUpMove;
	
	float			m_flMaxSpeed;
	float			m_flClientMaxSpeed;

	// Variables from the player edict (sv_player) or entvars on the client.
	// These are copied in here before calling and copied out after calling.
	Vector			m_vecVelocity;			// edict::velocity		// Current movement direction.
	Vector			m_vecTrailingVelocity;
	float			m_flTrailingVelocityTime;
	QAngle			m_vecAngles;			// edict::angles
	QAngle			m_vecOldAngles;
	
// Output only
	float			m_outStepHeight;	// how much you climbed this move
	Vector			m_outWishVel;		// This is where you tried 
	Vector			m_outJumpVel;		// This is your jump velocity

	// Movement constraints	(radius 0 means no constraint)
	Vector			m_vecConstraintCenter;
	float			m_flConstraintRadius;
	float			m_flConstraintWidth;
	float			m_flConstraintSpeedFactor;
	bool			m_bConstraintPastRadius;		///< If no, do no constraining past Radius.  If yes, cap them to SpeedFactor past radius

	void			SetAbsOrigin( const Vector &vec );
	const Vector	&GetAbsOrigin() const;

private:
	Vector			m_vecAbsOrigin;		// edict::origin
};

const Vector &CMoveData::GetAbsOrigin() const
{
	return m_vecAbsOrigin;
}

// This is implemented with a more exhaustive test in gamemovement.cpp.  We check if the origin being requested is
//  inside solid, which it never should be
void CMoveData::SetAbsOrigin( const Vector &vec )
{
	m_vecAbsOrigin = vec;
}

//-----------------------------------------------------------------------------
// Purpose: The basic player movement interface
//-----------------------------------------------------------------------------

class IGameMovement
{
public:
	virtual			~IGameMovement( void ) {}
	
	// Process the current movement command
	virtual void	ProcessMovement( CPlayerMoveCore *pPlayer, CMoveData *pMove ) = 0;
	virtual void	Reset( void ) = 0;
	virtual void	StartTrackPredictionErrors( CPlayerMoveCore *pPlayer ) = 0;
	virtual void	FinishTrackPredictionErrors( CPlayerMoveCore *pPlayer ) = 0;
	virtual void	DiffPrint( char const *fmt, ... ) = 0;

	// Allows other parts of the engine to find out the normal and ducked player bbox sizes
	virtual Vector const&	GetPlayerMins( bool ducked ) const = 0;
	virtual Vector const&	GetPlayerMaxs( bool ducked ) const = 0;
	virtual Vector const&   GetPlayerViewOffset( bool ducked ) const = 0;

	virtual bool		IsMovingPlayerStuck( void ) const = 0;
	virtual CPlayerMoveCore *GetMovingPlayer( void ) const = 0;
	virtual void		UnblockPusher( CPlayerMoveCore *pPlayer, CEntityCore *pPusher ) = 0;

	virtual void SetupMovementBounds( CMoveData *pMove ) = 0;
};
}
