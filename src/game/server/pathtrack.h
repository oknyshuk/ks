//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PATHTRACK_H
#define PATHTRACK_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif


#include "entityoutput.h"


//-----------------------------------------------------------------------------
// Spawnflag for CPathTrack
//-----------------------------------------------------------------------------
#define SF_PATH_DISABLED		0x00000001
//#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN	0x00000008
#define SF_PATH_TELEPORT		0x00000010
#define SF_PATH_ALTERNATE		0x00008000


enum TrackOrientationType_t
{
	TrackOrientation_Fixed = 0,
	TrackOrientation_FacePath,
	TrackOrientation_FacePathAngles,
};


//-----------------------------------------------------------------------------
// Paths!
//-----------------------------------------------------------------------------
class CPathTrack : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPathTrack, CServerOnlyPointEntity );

public:
	CPathTrack();

	void		Spawn( void );
	void		Activate( void );
	void		DrawDebugGeometryOverlays();
	
	void		ToggleAlternatePath( void );
	void		EnableAlternatePath( void );
	void		DisableAlternatePath( void );
	bool		HasAlternathPath() const;

	void		TogglePath( void );
	void		EnablePath( void );
	void		DisablePath( void );

	static CPathTrack	*ValidPath( CPathTrack *ppath, int testFlag = true );		// Returns ppath if enabled, NULL otherwise

	CPathTrack	*GetNextInDir( bool bForward );
	CPathTrack	*GetNext( void );
	CPathTrack	*GetPrevious( void );
	
	CPathTrack	*Nearest( const Vector &origin );
	//CPathTrack *LookAhead( Vector &origin, float dist, int move );
	CPathTrack *LookAhead( Vector &origin, float dist, int move, CPathTrack **pNextNext = NULL );

	TrackOrientationType_t GetOrientationType();
	QAngle GetOrientation( bool bForwardDir );

	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;

	float GetRadius() const { return m_flRadius; }

	// These four methods help for circular path checking. Call BeginIteration
	// before iterating, EndInteration afterwards. Call Visit on each path in the
	// list. Then you can use HasBeenVisited to see if you've visited the node
	// already, which means you've got a circular or lasso path. You can use the
	// macro BEGIN_PATH_TRACK_ITERATION below to simplify the calls to
	// BeginInteration + EndIteration.
	static void BeginIteration();
	static void EndIteration();
	void		Visit();
	bool		HasBeenVisited() const;

private:
	void		Project( CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist );
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	
	static CPathTrack *Instance( edict_t *pent );

	[[= ks::reflect::Input{ .name = "InPass", .type = FIELD_VOID } ]] void InputPass( inputdata_t &inputdata );
	
	[[= ks::reflect::Input{ .name = "ToggleAlternatePath", .type = FIELD_VOID } ]] void InputToggleAlternatePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnableAlternatePath", .type = FIELD_VOID } ]] void InputEnableAlternatePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisableAlternatePath", .type = FIELD_VOID } ]] void InputDisableAlternatePath( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "TogglePath", .type = FIELD_VOID } ]] void InputTogglePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EnablePath", .type = FIELD_VOID } ]] void InputEnablePath( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "DisablePath", .type = FIELD_VOID } ]] void InputDisablePath( inputdata_t &inputdata );

	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "radius" } ]] float		m_flRadius;
	float		m_length;
	[[= ks::reflect::Key{ .name = "altpath" } ]] string_t	m_altName;
    int			m_nIterVal;
	[[= ks::reflect::Key{ .name = "orientationtype" } ]] TrackOrientationType_t m_eOrientationType;

	[[= ks::reflect::Key{ .name = "OnPass" } ]] COutputEvent m_OnPass;

	static int	s_nCurrIterVal;
	static bool s_bIsIterating;
};

//-----------------------------------------------------------------------------
// Used to make sure circular iteration works all nice
//-----------------------------------------------------------------------------
#define BEGIN_PATH_TRACK_ITERATION() CPathTrackVisitor _visit

class CPathTrackVisitor
{
public:
	CPathTrackVisitor() { CPathTrack::BeginIteration(); }
	~CPathTrackVisitor() { CPathTrack::EndIteration(); }
};



#endif // PATHTRACK_H
