#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

import std;
import ks.tier1;
struct TitleDataFieldsDescription_t;

struct UserProfileData
{
	float	reputation;
	int32	difficulty;
	int32	sensitivity;
	int32	yaxis;
	int32	vibration;
	int32	color1, color2;
	int32	action_autoaim;
	int32	action_autocenter;
	int32	action_movementcontrol;
	int32	region;
	int32	achearned;
	int32	cred;
	int32	zone;
	int32	titlesplayed;
	int32	titleachearned;
	int32	titlecred;
};

//Players are a wrapper or a networked player, as such they may not have all the information current, particularly when first created.
class IPlayer
{
public:
	enum OnlineState_t
	{
		STATE_OFFLINE,
		STATE_NO_MULTIPLAYER,
		STATE_ONLINE,
	};

public:
	//Info
	virtual XUID GetXUID() = 0;
	virtual int GetPlayerIndex() = 0;

	virtual char const * GetName() = 0;

	virtual OnlineState_t GetOnlineState() = 0;
};

struct MatchmakingData;
class IPlayerRankingDataStore; 

class IPlayerLocal : public IPlayer
{
public:
	virtual const UserProfileData& GetPlayerProfileData() = 0;

	virtual MatchmakingData * GetPlayerMatchmakingData( void ) = 0;
	virtual void UpdatePlayerMatchmakingData( int mmDataType ) = 0;
	virtual void ResetPlayerMatchmakingData( int mmDataScope ) = 0;

	virtual const void * GetPlayerTitleData( int iTitleDataIndex ) = 0;
	virtual void UpdatePlayerTitleData( TitleDataFieldsDescription_t const *fdKey, const void *pvNewTitleData, int numNewBytes ) = 0;

	virtual void SetNeedsSave( void ) = 0;

	virtual bool IsTitleDataStorageConnected( void ) = 0;
};
