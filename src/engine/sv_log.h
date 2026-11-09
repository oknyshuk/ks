#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// sv_log.h
// Server logging functions

#include <igameevents.h>
import std;
import ks.tier1;
#include "filesystem.h"

class CLog
{
public:
	CLog();
	~CLog();

public: // game event listener
	
	void FireGameEvent( IGameEvent *event );

public: 

	bool IsActive( void );	// true if logging is "on"
	void SetLoggingState( bool state );	// set the logging state to true (on) or false (off)
	
	bool UsingLogAddress( void );
	bool AddLogAddress( netadr_t addr, uint64 ullToken = 0ull );
	bool DelLogAddress( netadr_t addr );
	void DelAllLogAddress( void );
	void ListLogAddress( void );
	
	void Open( void );  // opens logging file
	void Close( void );	// closes logging file

	void Init( void );
	void Reset( void );	// reset all logging streams
	void Shutdown( void );
	
	[[gnu::format(printf, 2, 3)]] void Printf( const char *fmt, ... );	// log a line to log file
	void Print( const char * text );
	void PrintServerVars( void ); // prints server vars to log file

	void RunFrame();

private:

	bool m_bActive;		// true if we're currently logging

	struct LogAddressDestination_t
	{
		netadr_t m_adr;
		uint64 m_ullToken;
	};
	std::vector< LogAddressDestination_t >	m_LogAddrDestinations;		// Server frag log stream is sent to the address(es) in this list
	FileHandle_t			m_hLogFile;        // File where frag log is put.
	double					m_flLastLogFlush;
	bool					m_bFlushLog;
};

extern CLog g_Log;
