#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IEngine
{
public:
	enum
	{
		QUIT_NOTQUITTING = 0,
		QUIT_TODESKTOP,
		QUIT_RESTART
	};

	// Engine State Flags
	enum EngineState_t
	{
		DLL_INACTIVE = 0,		// no dll
		DLL_ACTIVE,				// engine is focused
		DLL_CLOSE,				// closing down dll
		DLL_RESTART,			// engine is shutting down but will restart right away
		DLL_PAUSED,				// engine is paused, can become active from this state
	};



	bool	Load( bool dedicated, const char *rootdir );
	void	Unload( void );
	void	SetNextState( EngineState_t iNextState );
	EngineState_t GetState( void );

	void	Frame( void );

	float	GetFrameTime( void );
	float	GetCurTime( void );

	int		GetQuitting( void );
	void	SetQuitting( int quittype );
};

extern IEngine *eng;
