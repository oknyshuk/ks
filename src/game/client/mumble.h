#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

// Purpose: Contains an interface to enable positional audio support in Mumble

#include "igamesystem.h"
#include "GameEventListener.h"

class CMumbleSystem : public CBaseGameSystemPerFrame
{
public:
	// Methods of IGameSystem
	virtual bool	Init();
	virtual void	LevelInitPostEntity();
	virtual void	LevelShutdownPreEntity();
	virtual void	PostRender();
};

IGameSystem *MumbleSystem();
