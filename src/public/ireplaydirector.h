#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
import ks.tier1;

class IReplayServer;

inline constexpr const char *INTERFACEVERSION_REPLAYDIRECTOR = "ReplayDirector001";

class IReplayDirector
{
public:
	virtual	~IReplayDirector() {}

	virtual bool	IsActive( void ) = 0; // true if director is active

	virtual void	SetReplayServer( IReplayServer *Replay ) = 0; // give the director the engine Replay interface 
	virtual IReplayServer* GetReplayServer( void ) = 0; // get current Replay server interface
	
	virtual int		GetDirectorTick( void ) = 0;	// get current broadcast tick from director
	virtual int		GetPVSEntity( void ) = 0; // get current view entity (PVS), 0 if coords are used
	virtual Vector	GetPVSOrigin( void ) = 0; // get current PVS origin
	virtual float	GetDelay( void ) = 0; // returns current delay in seconds

	virtual const char**	GetModEvents() = 0;
};
