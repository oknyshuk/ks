#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Client VoteController

#include "shareddefs.h"
#include "GameEventListener.h"
#include "reflect_annotations.h"
#include "dt_recv.h"
#include "client_class.h"
#include "c_baseentity.h"

class [[= ks::reflect::NetTable{ .name = "DT_VoteController" } ]]
      C_VoteController : public C_BaseEntity, public CGameEventListener
{
	DECLARE_CLASS( C_VoteController, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_VoteController();
	virtual ~C_VoteController();

	virtual void	Spawn( void );
	virtual void	ClientThink( void );

	static void		RecvProxy_VoteType( const CRecvProxyData *pData, void *pStruct, void *pOut );
	static void		RecvProxy_VoteOption( const CRecvProxyData *pData, void *pStruct, void *pOut );

	void			FireGameEvent( IGameEvent *event );
protected:
	void			ResetData();

	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_VoteType, ks::reflect::WireSide::Recv>{} ]] int				m_iActiveIssueIndex;
	[[= ks::reflect::Net{} ]] int				m_iOnlyTeamToVote;
	[[= ks::reflect::Net{} ]]
	[[= ks::reflect::Proxy<RecvProxy_VoteOption, ks::reflect::WireSide::Recv>{} ]] int				m_nVoteOptionCount[MAX_VOTE_OPTIONS];
	int				m_iVoteChoiceIndex;
	[[= ks::reflect::Net{} ]] int				m_nPotentialVotes;
	bool			m_bVotesDirty;	// Received a vote, so remember to tell the Hud
	bool			m_bTypeDirty;	// Vote type changed, so show or hide the Hud
	[[= ks::reflect::Net{} ]] bool			m_bIsYesNoVote;
};
