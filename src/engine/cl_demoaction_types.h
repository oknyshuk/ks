#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//-----------------------------------------------------------------------------
// Purpose: Skips ahead in demo to specified frame/time
//-----------------------------------------------------------------------------
#include "cl_demoaction.h"
#include "shake.h"
#include "client_textmessage.h"
import ks.tier1.keyvalues;
import ks.tier1.strtools;
import ks.tier1.text_writer;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;

class CDemoActionSkipAhead : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	bool Update( const DemoActionTimingContext& tc );
	void FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void SetSkipToTick( int tick );
	void SetSkipToTime( float t );
	
public:

	bool		m_bUsingSkipTick;
	int			m_nSkipToTick;
	float		m_flSkipToTime;

	friend class CBaseActionSkipAheadDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Simply stops playback of demo
//-----------------------------------------------------------------------------
class CDemoActionStopPlayback : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Update( const DemoActionTimingContext& tc );
	void			FireAction( void );

public:

	friend class CBaseActionStopPlaybackDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Skips ahead in demo to specified frame/time
//-----------------------------------------------------------------------------
class CDemoActionPlayCommands : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	void FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetCommandStream( char const *stream );
	char const	*GetCommandStream( void ) const;

public:

	enum
	{
		MAX_COMMAND_STREAM = 256,
	};

	char		m_szCommandStream[ MAX_COMMAND_STREAM ];
};

//-----------------------------------------------------------------------------
// Purpose: Screen fade actions
//-----------------------------------------------------------------------------
class CDemoActionScreenFadeStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	void FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	ScreenFade_t *GetScreenFade( void );

public:
	ScreenFade_t		fade;

	friend class CBaseActionScreenFadeStartDialog;
};

//-----------------------------------------------------------------------------
// Purpose: Text message actions
//-----------------------------------------------------------------------------
class CDemoActionTextMessageStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	void			FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	client_textmessage_t *GetTextMessage( void );

	void						SetMessageText( char const *text );
	char const					*GetMessageText( void ) const;

	void						SetFontName( char const *font );
	char const					*GetFontName( void ) const;

public:
	enum
	{
		MAX_MESSAGE_TEXT = 512,
		MAX_FONT_NAME = 64,
	};

	char						m_szMessageText[ MAX_MESSAGE_TEXT ];
	char						m_szUIFont[ MAX_FONT_NAME ];
	client_textmessage_t		message;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CDemoActionCDTrackStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	void			FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetTrack( int track );
	int			GetTrack( void ) const;

public:
	int			m_nCDTrack;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CDemoActionCDTrackStop : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	void			FireAction( void );

public:
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionPlaySoundStart : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	void			FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetSoundName( char const *name );
	char const	*GetSoundName( void ) const;

public:

	enum
	{
		MAX_SOUND_NAME = 128,
	};

	char		m_szSoundName[ MAX_SOUND_NAME ];
};

//-----------------------------------------------------------------------------
// Purpose: Base for actions that go on until a specified stop frame/time
//-----------------------------------------------------------------------------
class CBaseDemoActionWithStopTime : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;

public:
	bool Init( KeyValues *pInitData );
	bool Update( const DemoActionTimingContext& tc );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetStopTick( int tick );
	void		SetStopTime( float time );
	
public:

	bool		m_bUsingStopTick;
	int			m_nStopTick;
	float		m_flStopTime;

	friend class CBaseActionWithStopTimeDialog;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionChangePlaybackRate : public CBaseDemoActionWithStopTime
{
	typedef CBaseDemoActionWithStopTime BaseClass;

public:
	CDemoActionChangePlaybackRate();

	bool Init( KeyValues *pInitData );
	void FireAction( void );
	void	OnActionFinished( void );

	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetPlaybackRate( float rate );
	float		GetPlaybackRate( void ) const;

public:

	float		m_flPlaybackRate;
	float		m_flSavePlaybackRate;

	friend class CBaseActionChangePlaybackRateDialog;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoActionPausePlayback : public CBaseDemoActionWithStopTime
{
	typedef CBaseDemoActionWithStopTime BaseClass;

public:
	CDemoActionPausePlayback();

	bool Init( KeyValues *pInitData );
	void FireAction( void );

	void SaveKeysToBuffer( int depth, ks::text_writer& buf );

	void		SetPauseTime( float t );
	float		GetPauseTime( void ) const;

public:

	float		m_flPauseTime;

	friend class CBaseActionPauseDialog;
};

class CDemoActionZoom : public CBaseDemoAction
{
	typedef CBaseDemoAction BaseClass;
public:
	CDemoActionZoom();

	bool Init( KeyValues *pInitData );
	bool Update( const DemoActionTimingContext& tc );
	void FireAction( void );
	void SaveKeysToBuffer( int depth, ks::text_writer& buf );
public:

	bool			m_bSpline;
	bool			m_bStayout;

	float			m_flFinalFOV;
	float			m_flFOVRateOut;  // degress per second
	float			m_flFOVRateIn;	 // degrees per second
	float			m_flHoldTime;

// for playback
	float			m_flFOVStartTime;
	float			m_flOriginalFOV;

	friend class CBaseActionZoomDialog;
};
