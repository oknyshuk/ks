#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: AI Utility classes for building the initial AI Networks

#include "ai_basenpc.h"
#include "ai_node.h"

//###########################################################
//  >> HintNodeData
//
// This is a chunk of data that's passed to a hint node entity
// when it's created from a CNodeEnt.
//###########################################################
enum HintIgnoreFacing_t
{
	HIF_NO,
	HIF_YES,
	HIF_DEFAULT,
};


struct HintNodeData
{
	string_t	strEntityName;
	Vector		vecPosition;
	[[= ks::reflect::Key{ .name = "hinttype" } ]] short		nHintType;
	int			nNodeID;
	[[= ks::reflect::Key{ .name = "Group" } ]] string_t	strGroup;
	[[= ks::reflect::Key{ .name = "StartHintDisabled" } ]] int			iDisabled;
	[[= ks::reflect::Key{ .name = "generictype" } ]] string_t	iszGenericType;
	[[= ks::reflect::Key{ .name = "hintactivity" } ]] string_t	iszActivityName;
	[[= ks::reflect::Key{ .name = "TargetNode" } ]] int			nTargetWCNodeID;
	[[= ks::reflect::Key{ .name = "IgnoreFacing" } ]] HintIgnoreFacing_t fIgnoreFacing;
	[[= ks::reflect::Key{ .name = "MinimumState" } ]] NPC_STATE	minState;
	[[= ks::reflect::Key{ .name = "MaximumState" } ]] NPC_STATE	maxState;
	[[= ks::reflect::Key{ .name = "radius" } ]] int			nRadius;

	[[= ks::reflect::Key{ .name = "nodeid" } ]] int			nWCNodeID;			// Node ID assigned by worldcraft (not same as engine!)

	DECLARE_SIMPLE_DATADESC();
};

//###########################################################
//  >> CNodeEnt
//
// This is the entity that is loaded in from worldcraft.
// It is only used to build the network and is deleted
// immediately
//###########################################################
class CNodeEnt : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CNodeEnt, CServerOnlyPointEntity );

public:
	void SetOwnerEntity_Impl( CBaseEntity* pOwner ) { BaseClass::SetOwnerEntity_Impl( nullptr ); }

	static int			m_nNodeCount;

	void	Spawn_Impl( void );
	using CBaseEntity::Spawn;
	int		Spawn( const char *pMapData );


	CNodeEnt(void);

public:
	HintNodeData		m_NodeData;
};

//###########################################################
// >> CAI_TestHull 
//
// a modelless clip hull that verifies reachable nodes by 
// walking from every node to each of it's connections//
//###########################################################
class CAI_TestHull : public CAI_BaseNPC
{
	DECLARE_CLASS( CAI_TestHull, CAI_BaseNPC );
private:
	static CAI_TestHull*	pTestHull;								// Hull for testing connectivity

public:
	static CAI_TestHull*	GetTestHull(void);						// Get the test hull
	static void				ReturnTestHull(void);					// Return the test hull

	bool					bInUse;
	void			Precache_Impl();
	void					Spawn_Impl(void);
	int				ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~(FCAP_ACROSS_TRANSITION|FCAP_DONT_SAVE); }

	virtual bool			IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;

	~CAI_TestHull(void);
};
