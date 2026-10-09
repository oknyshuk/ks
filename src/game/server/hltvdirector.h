#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "GameEventListener.h"
#include <igamesystem.h>
#include <ihltvdirector.h>
#include <ihltv.h>
import std;
#include "ihltvdirector.h"
#include "igamesystem.h"
#include "baseentity.h"
#include "shareddefs.h"

inline constexpr auto HLTV_MIN_DIRECTOR_DELAY = 3; // minimum delay if director is enabled
inline constexpr auto HLTV_MAX_DELAY = 120; // maximum delay


#define MAX_NUM_CAMERAS				64	// support up to 64 fixed cameras per level

#define MIN_SHOT_LENGTH				4.0f  // minimum time of a cut (seconds)
#define MAX_SHOT_LENGTH				8.0f  // maximum time of a cut (seconds)
#define DEF_SHOT_LENGTH				6.0f  // average time of a cut (seconds)

class CHLTVGameEvent
{
public:
		int			m_Tick;		// tick of this command
		int			m_Priority; // game event priority
		IGameEvent	*m_Event;	// IGameEvent
};

// Orders events by tick; transparent so the history can be searched by tick.
struct CHLTVGameEventLess
{
	using is_transparent = void;
	bool operator()( const CHLTVGameEvent &e1, const CHLTVGameEvent &e2 ) const { return e1.m_Tick < e2.m_Tick; }
	bool operator()( const CHLTVGameEvent &e, int tick ) const { return e.m_Tick < tick; }
	bool operator()( int tick, const CHLTVGameEvent &e ) const { return tick < e.m_Tick; }
};

class CHLTVDirector : public CGameEventListener, public CBaseGameSystemPerFrame, public IHLTVDirector
{
public:
	DECLARE_CLASS_NOBASE( CHLTVDirector );

	virtual char const *Name() { return "CHLTVDirector"; }

	CHLTVDirector();
	virtual ~CHLTVDirector();

	virtual void AddHLTVServer( IHLTVServer *hltv )override; // give the director an HLTV interface 
	virtual void RemoveHLTVServer( IHLTVServer *hltv ) override;
	virtual IHLTVServer* GetHLTVServer( int nIndex ) override { return m_HltvServers[ nIndex ].m_pHLTVServer; }
	virtual int GetHLTVServerCount() override { return std::ssize( m_HltvServers ); }
	int		GetDirectorTick( void );	// get current broadcast tick from director
	int		GetPVSEntity( void ); // get current view entity (PVS)
	Vector	GetPVSOrigin( void ); // get current PVS origin, if PVS entity is 0
	float	GetDelay( void ); // returns current delay in seconds
	bool	IsActive( void );

	virtual const char** GetModEvents(); // returns list of event names forwarded to HLTV clients

	void	BuildCameraList( void );

	// Starts automatic recording of the current session
	void	StartAutoRecording( void );
		

public: // IGameEventListener Interface
	virtual void	FireGameEvent( IGameEvent * event );
	
public: // CBaseGameSystem overrides

	virtual bool	Init();
	virtual void	Shutdown();
	virtual void	FrameUpdatePostEntityThink();
	virtual void	LevelInitPostEntity();
	virtual const char	*GetFixedCameraEntityName( void ) { return "point_viewcontrol"; }

			bool	SetCameraMan( int iPlayerIndex );
			int		GetCameraMan() { return m_iCameraManIndex; }


protected:

	virtual void	StartNewShot();	
	virtual void	StartRandomShot();
	virtual void	StartDelayMessage();
	virtual void	StartBestFixedCameraShot(bool bForce);
	virtual void	StartBestPlayerCameraShot();
	virtual void	StartFixedCameraShot(int iCamera, int iTarget);
	virtual void	StartChaseCameraShot(int iTarget1, int iTarget2, int distance, int phi, int theta, bool bInEye);
	virtual void	UpdateSettings();
	virtual void	AnalyzePlayers();
	virtual void 	AnalyzeCameras();
	virtual bool	StartCameraManShot();
	virtual void	StartInstantBroadcastShot();
	virtual void	FinishCameraManShot();
	virtual void	BuildActivePlayerList();
	virtual const CHLTVGameEvent *FindBestGameEvent();
	virtual void	CreateShotFromEvent( const CHLTVGameEvent *ge );

	// events with equal ticks stay in arrival order
	using EventHistory_t = std::multiset<CHLTVGameEvent, CHLTVGameEventLess>;

	EventHistory_t::const_iterator FindFirstEvent( int tick ) const; // finds first event >= tick
	void	CheckHistory();
	void	RemoveEventsFromHistory(int tick); // removes all commands < tick, or all if tick -1
	
	struct HltvServerRecord_t
	{
		IHLTVServer		*m_pHLTVServer;	// interface to servers HLTV object
		CBasePlayer		*m_pHLTVClient; // the HLTV fake client
	};
	std::vector< HltvServerRecord_t > m_HltvServers;

	float			m_fDelay;	// hltv delay in seconds
	int				m_nBroadcastTick; // world time that is currently "on the air"
	int				m_iPVSEntity;	// entity for PVS center
	Vector			m_vPVSOrigin;	// PVS origin if PVS entity is 0
	int				m_iCameraMan;	//  >0 if current view entity is a cameraman
	int				m_nNextShotTick;	// time for the next scene cut
	int				m_iLastPlayer;		// last player in random rotation

	int				m_nNextAnalyzeTick;	
		
	int				m_nNumFixedCameras;	//number of cameras in current map
	CBaseEntity		*m_pFixedCameras[MAX_NUM_CAMERAS]; // fixed cameras (point_viewcontrol)
	
	int				m_nNumActivePlayers;	//number of cameras in current map
	CBasePlayer		*m_pActivePlayers[MAX_PLAYERS]; // fixed cameras (point_viewcontrol)
	int				m_iCameraManIndex;		// entity index of current camera man or 0
	
	EventHistory_t	m_EventHistory;
};

extern IGameSystem* HLTVDirectorSystem();
extern CHLTVDirector* HLTVDirector();
