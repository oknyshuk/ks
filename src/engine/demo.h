#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

#include "net.h"
#include "demofile/demoformat.h"
#include "netmessages_signon.h"

class CDemoFile;
class ServerClass;
class CGameInfo;
struct CDemoPlaybackParameters_t;
struct DemoImportantTick_t;
struct DemoImportantGameEvent_t;
class CDemoFile;


class IDemoRecorder 
{
public:
	enum class recorder_kind_t : unsigned char { demo, hltv };
	explicit IDemoRecorder( recorder_kind_t kind ) : m_recorder_kind( kind ) {}

	~IDemoRecorder() {} // TODO: Should be virtual?

	// Notify the demo recorder of the client sign-on state, so it knows whether the
	// string tables and entity data are fully populated.
	void	SetSignonState( SIGNONSTATE state );

	// Write entity send-tables so we can fix up deltas on later versions of the client
	// Note that this usually gets done as part of StartRecording, so it's a bit odd that it's in
	// the interface here.
	void	RecordServerClasses( ServerClass *pClasses );
	
	// Recording commands
	void	RecordMessages( bf_read &data, int bits ); // add messages to current packet
	void	RecordPacket( void ); // packet finished, write all recorded stuff to file
	void	RecordCommand( const char *cmdstring );  // record a console command
	void	RecordUserInput( int cmdnumber );  // record a user input command

	// This function is pretty scary.  It relies on the client code explicitly
	// versioning the embedded data (which the only client, blob particles, doesn't
	// seem to do!)
	//
	// Also, I am not sure how the callbacks are referenced in cross-version playback.
	//
	// You should use protocol buffers instead, with RecordMessages().  This way all
	// cross-version stuff is handled for you and backwards compatibility is a lot
	// simpler.
	void	RecordCustomData( int iCallbackIndex, const void *pData, std::size_t iDataLength ); //record a chunk of custom data

	// Notify the demo recorder that an entity is about to teleport, and
	// stop it from being interpolated between its old and new positions.
	//
	// TODO: Shouldn't this specify which entity is being teleported? Currently it seems
	// to only do anything in client-recorded demos, and notifies that the current client
	// is being teleported.  (Also, nobody seems to ever call this function in CSGO, so
	// it's hard to verify my assumptions about it!)
	void	ResetDemoInterpolation();

	// TODO: This doesn't really belong here; different demo objects have different processes
	// for handling start/stop recording.
	void	StartRecording( const char *filename, bool bContinuously );

	// Called during client disconnect.  Should probably be named 'Shutdown' or 'Detach'.
	//
	// NOTE: There is an unused CGameInfo* parameter here that probably came from DotA2.
	// We never define that structure so it is impossible to pass anything aside from NULL.
	//
	// It seems its purpose is to attach some extra metadata about the game that wasn't
	// known at the start of recording to the header of the demo.  This lets you populate
	// the playback interface with information about when kills happened, towers died, etc.
	void	StopRecording( const CGameInfo* pGameInfo = nullptr );

	// True between StartRecording and StopRecording
	bool	IsRecording( void );

	// This is used by cdll_engine_int to provide an interface to the client's demo
	// recorder.  However, they don't seem to have any callers through that wrapper.
	int		GetRecordingTick( void );

	// These don't seem to be used anywhere, and are probably specific to the type
	// of demo being recorded, as opposed to belonging in IDemoRecorder.
	//
	// TODO: Delete these for real instead of just commenting them.
	//
	// virtual CDemoFile *GetDemoFile() = 0;
	//
	// virtual void	PauseRecording( void ) = 0;
	// virtual void	ResumeRecording( void ) = 0;

	// This function is especially odd.  This is generally handled by StartRecording/
	// SetSignonState or something similar to write the initial string tables, and not
	// called externally.  Updates are handled via the usual string table update
	// messages via RecordMessages().  Not sure where it would be used externally!
	//
	// virtual void	RecordStringTables() = 0; 

protected:
	recorder_kind_t m_recorder_kind;
};


class IDemoPlayer
{
public:
	enum class player_kind_t : unsigned char { demo, hltv };
	explicit IDemoPlayer( player_kind_t kind ) : m_player_kind( kind ) {}


	CDemoFile *GetDemoStream();
	int		GetPlaybackStartTick( void );
	int		GetPlaybackTick( void );
//	virtual int		GetPlaybackDeltaTick( void ) = 0;
//	virtual int		GetPacketTick( void ) = 0;
	
	bool	IsPlayingBack( void ) const; // true if demo loaded and playing back
	bool	IsPlaybackPaused( void ) const; // true if playback paused
	bool	IsPlayingTimeDemo( void ) const; // true if playing back in timedemo mode
	bool	IsSkipping( void ) const; // true, if demo player skipping trough packets
	bool	CanSkipBackwards( void ) const; // true if demoplayer can skip backwards

	void	SetPlaybackTimeScale( float timescale ); // sets playback timescale
	float	GetPlaybackTimeScale( void ); // get playback timescale

	void	PausePlayback( float seconds ); // pause playback n seconds, -1 = forever
	void	SkipToTick( int tick, bool bRelative, bool bPause ); // goto a specific tick, 0 = start, -1 = end
	void	SkipToImportantTick( const DemoImportantTick_t *pTick );
	void	ResumePlayback( void ); // resume playback
	void	StopPlayback( void );	// stop playback, close file
	void	InterpolateViewpoint(); // override viewpoint
	netpacket_t *ReadPacket( void ); // read packet from demo file

	void ResetDemoInterpolation();

	CDemoPlaybackParameters_t const * GetDemoPlaybackParameters();

	void SetPacketReadSuspended( bool bSuspendPacketReading );

	void	SetImportantEventData( const KeyValues *pData );
	int		FindNextImportantTick( int nCurrentTick, const char *pEventName = nullptr ); // -1 = no next important tick
	int		FindPreviousImportantTick( int nCurrentTick, const char *pEventName = nullptr ); // -1 = no previous important tick
	const DemoImportantTick_t *GetImportantTick( int nIndex );
	const DemoImportantGameEvent_t *GetImportantGameEvent( const char *pszEventName );
	void	ListImportantTicks( void );
	void	ListHighlightData( void );
	void	SetHighlightXuid( uint64 xuid, bool bLowlights );

	bool	ScanDemo( const char *filename, const char* pszMode );

protected:
	player_kind_t m_player_kind;
};

#if !defined( DEDICATED )
extern IDemoPlayer *demoplayer;	// reference to current demo player
extern IDemoRecorder *demorecorder; // reference to current demo recorder
#endif
