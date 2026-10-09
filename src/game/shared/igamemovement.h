#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
#include "imovehelper.h"
#include "const.h"
#include "shared_classnames.h"

class CBasePlayer;
class CBaseEntity;

//-----------------------------------------------------------------------------
// Name of the class implementing the game movement.
//-----------------------------------------------------------------------------

inline constexpr const char *INTERFACENAME_GAMEMOVEMENT = "GameMovement001";

//-----------------------------------------------------------------------------
// Forward declarations.
//-----------------------------------------------------------------------------

class IMoveHelper;

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

inline const Vector &CMoveData::GetAbsOrigin() const
{
	return m_vecAbsOrigin;
}


// This is implemented with a more exhaustive test in gamemovement.cpp.  We check if the origin being requested is
//  inside solid, which it never should be
inline void CMoveData::SetAbsOrigin( const Vector &vec )
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
	virtual void	ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove ) = 0;
	virtual void	Reset( void ) = 0;
	virtual void	StartTrackPredictionErrors( CBasePlayer *pPlayer ) = 0;
	virtual void	FinishTrackPredictionErrors( CBasePlayer *pPlayer ) = 0;
	virtual void	DiffPrint( char const *fmt, ... ) = 0;

	// Allows other parts of the engine to find out the normal and ducked player bbox sizes
	virtual Vector const&	GetPlayerMins( bool ducked ) const = 0;
	virtual Vector const&	GetPlayerMaxs( bool ducked ) const = 0;
	virtual Vector const&   GetPlayerViewOffset( bool ducked ) const = 0;

	virtual bool		IsMovingPlayerStuck( void ) const = 0;
	virtual CBasePlayer *GetMovingPlayer( void ) const = 0;
	virtual void		UnblockPusher( CBasePlayer *pPlayer, CBaseEntity *pPusher ) = 0;

	virtual void SetupMovementBounds( CMoveData *pMove ) = 0;
};
