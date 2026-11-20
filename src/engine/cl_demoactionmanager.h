#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CBaseDemoAction;
struct democmdinfo_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class IDemoActionManager
{
public:

	void		Init( void );
	void		Shutdown( void );

	void		StartPlaying( char const *demfilename );
	void		StopPlaying();

	void		Update( bool newframe, int demoframe, float demotime );

	void		SaveToBuffer( ks::text_writer& buf );
	void		SaveToFile( void );

	char const	*GetCurrentDemoFile( void );

	int			GetActionCount( void );
	CBaseDemoAction *GetAction( int index );
	void		AddAction( CBaseDemoAction *action );
	void		RemoveAction( CBaseDemoAction *action );

	bool		IsDirty( void ) const;
	void		SetDirty( bool dirty );

	void		ReloadFromDisk( void );

	void		DispatchEvents();
	void		InsertFireEvent( CBaseDemoAction *action );

	bool		OverrideView( democmdinfo_t& info, int tick );
};

extern IDemoActionManager *demoaction;
