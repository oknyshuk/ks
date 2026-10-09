#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
import ks.tier1;


class IPlayerManager;
class IGameManager;
class IServerManager;
class ISearchManager;
class IMatchVoice;
class IDatacenter;
class IDlcManager;

class IMatchSystem
{
public:
	virtual IPlayerManager * GetPlayerManager() = 0;

	virtual IMatchVoice * GetMatchVoice() = 0;

	virtual IServerManager * GetUserGroupsServerManager() = 0;

	virtual ISearchManager * CreateGameSearchManager( KeyValues *pParams ) = 0;

	virtual IDatacenter * GetDatacenter() = 0;

	virtual IDlcManager * GetDlcManager() = 0;
};
