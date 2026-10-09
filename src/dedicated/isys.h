#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

import std;
import ks.tier1;

class CDedicatedAppSystemGroup;


class ISys
{
public:
	virtual				~ISys( void ) { } 

	virtual bool		LoadModules( CDedicatedAppSystemGroup *pAppSystemGroup ) = 0;

	virtual	void		Sleep( int msec ) = 0;
	virtual bool		GetExecutableName( char *out ) = 0;
	virtual void		ErrorMessage( int level, const char *msg ) = 0;

	virtual void		WriteStatusText( char *szText ) = 0;
	virtual void		UpdateStatus( int force ) = 0;

	virtual long		LoadLibrary( const char *lib ) = 0;
	virtual void		FreeLibrary( long library ) = 0;

	virtual bool		CreateConsoleWindow( void ) = 0;
	virtual void		DestroyConsoleWindow( void ) = 0;

	virtual void		ConsoleOutput ( char *string ) = 0;
	virtual char		*ConsoleInput (void) = 0;
	[[gnu::format(printf, 2, 3)]] virtual void		Printf( const char *fmt, ...) = 0;
};

extern ISys *sys;
