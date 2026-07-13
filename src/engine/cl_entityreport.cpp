//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_entityreport( "cl_entityreport", "0", FCVAR_CHEAT, "For debugging, draw entity states to console" );

// How quickly to move rolling average for entityreport
#define BITCOUNT_AVERAGE 0.95f
// How long to flush item when something important happens
#define EFFECT_TIME  1.5f
// How long to latch peak bit count for item
#define PEAK_LATCH_TIME 2.0f;

//-----------------------------------------------------------------------------
// Purpose: Entity report event types
//-----------------------------------------------------------------------------
enum
{
	FENTITYBITS_ZERO = 0,
	FENTITYBITS_ADD = 0x01,
	FENTITYBITS_LEAVEPVS = 0x02,
	FENTITYBITS_DELETE = 0x04,
};

//-----------------------------------------------------------------------------
// Purpose: Data about an entity
//-----------------------------------------------------------------------------
class CEntityBits
{
public:
	CEntityBits() :
		bits( 0 ),
		average( 0.0f ),
		peak( 0 ),
		peaktime( 0.0f ),
		flags( 0 ),
		effectfinishtime( 0.0f ),
		deletedclientclass( NULL )
	{
	}

	// Bits used for last message
	int				bits;
	// Rolling average of bits used
	float			average;
	// Last bit peak
	int				peak;
	// Time at which peak was last reset
	float			peaktime;
	// Event info
	int				flags;
	// If doing effect, when it will finish
	float			effectfinishtime;
	// If event was deletion, remember client class for a little bit
	ClientClass		*deletedclientclass;
};

class CEntityReportManager
{
public:

	void Reset();
	void Record( int entnum, int bitcount );
	void Add( int entnum );
	void LeavePVS( int entnum );
	void DeleteEntity( int entnum, ClientClass *pclass );

	int  Count();
	CEntityBits *Base();

private:
	CUtlVector< CEntityBits > m_EntityBits;
};

static CEntityReportManager g_EntityReportMgr;

void CL_ResetEntityBits( void )
{
	g_EntityReportMgr.Reset();
}

void CL_RecordAddEntity( int entnum )
{
	g_EntityReportMgr.Add( entnum );
}

void CL_RecordEntityBits( int entnum, int bitcount )
{
	g_EntityReportMgr.Record( entnum, bitcount );
}

void CL_RecordLeavePVS( int entnum )
{
	g_EntityReportMgr.LeavePVS( entnum );
}

void CL_RecordDeleteEntity( int entnum, ClientClass *pclass )
{
	g_EntityReportMgr.DeleteEntity( entnum, pclass );
}

//-----------------------------------------------------------------------------
// Purpose: Wipe structure ( level transition/startup )
//-----------------------------------------------------------------------------
void CEntityReportManager::Reset()
{
	m_EntityBits.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Record activity
// Input  : entnum - 
//			bitcount - 
//-----------------------------------------------------------------------------
void CEntityReportManager::Record( int entnum, int bitcount )
{
	if ( entnum < 0 || entnum >= MAX_EDICTS ) 
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];

	slot->bits = bitcount;
	// Update average
	slot->average = ( BITCOUNT_AVERAGE ) * slot->average + ( 1.f - BITCOUNT_AVERAGE ) * bitcount;

	// Recompute peak
	if ( realtime >= slot->peaktime )
	{
		slot->peak = 0.0f;
		slot->peaktime = realtime + PEAK_LATCH_TIME;
	}

	// Store off peak
	if ( bitcount > slot->peak )
	{
		slot->peak = bitcount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Record entity add event
// Input  : entnum - 
//-----------------------------------------------------------------------------
void CEntityReportManager::Add( int entnum )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_ADD;
	slot->effectfinishtime = realtime + EFFECT_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: record entity leave event
// Input  : entnum - 
//-----------------------------------------------------------------------------
void CEntityReportManager::LeavePVS( int entnum )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_LEAVEPVS;
	slot->effectfinishtime = realtime + EFFECT_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: record entity deletion event
// Input  : entnum - 
//			*pclass - 
//-----------------------------------------------------------------------------
void CEntityReportManager::DeleteEntity( int entnum, ClientClass *pclass )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_DELETE;
	slot->effectfinishtime = realtime + EFFECT_TIME;
	slot->deletedclientclass = pclass;
}

int CEntityReportManager::Count()
{
	return m_EntityBits.Count();
}

CEntityBits *CEntityReportManager::Base()
{
	return m_EntityBits.Base();
}
