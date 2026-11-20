#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include <const.h>
import std;
import ks.platform;
import ks.simd;
import ks.tier1.fixed_pool;
import ks.tier1.createinterface;

class CFrameSnapshot;

inline constexpr auto MAX_CLIENT_FRAMES = 128;

class CClientFrame
{
public:

	CClientFrame( void );
	CClientFrame( int tickcount );
	CClientFrame( CFrameSnapshot *pSnapshot );
	~CClientFrame();

	// Deletes the frame as the class it really is (CClientFrame or CHLTVFrame, see netdispatch.cpp)
	void DeleteThis();
	void Init( CFrameSnapshot *pSnapshot );
	void Init( int tickcount );

	// Accessors to snapshots. The data is protected because the snapshots are reference-counted.
	inline CFrameSnapshot*	GetSnapshot() const { return m_pSnapshot; };
	void					SetSnapshot( CFrameSnapshot *pSnapshot );
	void					CopyFrame( CClientFrame &frame );
	bool				IsMemPoolAllocated() { return !m_bHltvFrame; }

public:

	// State of entities this frame from the POV of the client.
	int					last_entity;	// highest entity index
	int					tick_count;	// server tick of this snapshot
	CClientFrame*		m_pNext;

	// Used by server to indicate if the entity was in the player's pvs
	std::bitset<MAX_EDICTS>	transmit_entity; // if bit n is set, entity n will be send to client
	std::bitset<MAX_EDICTS>	*from_baseline;	// if bit n is set, this entity was send as update from baseline
	std::bitset<MAX_EDICTS>	*transmit_always; // if bit is set, don't do PVS checks before sending (HLTV only)

protected:
	bool				m_bHltvFrame = false;	// the object is a CHLTVFrame

private:

	// Index of snapshot entry that stores the entities that were active and the serial numbers
	// for the frame number this packed entity corresponds to
	// m_pSnapshot MUST be private to force using SetSnapshot(), see reference counters
	CFrameSnapshot		*m_pSnapshot;
};

// TODO substitute CClientFrameManager with an intelligent structure (Tree, hash, cache, etc)
class CClientFrameManager
{
public:
	CClientFrameManager(void)  : m_ClientFramePool( MAX_CLIENT_FRAMES, ks::fixed_pool::grow::slow ) {	m_Frames = nullptr; }
	~CClientFrameManager(void) { DeleteClientFrames(-1); }

	int				AddClientFrame( CClientFrame *pFrame ); // returns current count
	CClientFrame	*GetClientFrame( int nTick, bool bExact = true );
	void			DeleteClientFrames( int nTick );	// -1 for all
	int				CountClientFrames( void );	// returns number of frames in list
	void			RemoveOldestFrame( void );  // removes the oldest frame in list
	bool			DeleteUnusedClientFrame( CClientFrame* pFrame );
	int				GetOldestTick()const{ return m_Frames ? m_Frames->tick_count : 0; }

	CClientFrame*	AllocateFrame();
	CClientFrame*	AllocateAndInitFrame( int nTick );

private:
	void			FreeFrame( CClientFrame* pFrame );

	CClientFrame	*m_Frames;		// updates can be delta'ed from here
	ks::object_pool< CClientFrame >	m_ClientFramePool;
};
