#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation
import ks.tier1;


class IPlayerManager;
class IGameManager;
class IMatchVoice;

class IMatchSystem
{
public:
	IPlayerManager * GetPlayerManager();

	IMatchVoice * GetMatchVoice();

};
