#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IVModeManager
{
public:
	virtual void	Init( void ) = 0;
	// HL2 will ignore, TF2 will change modes.
	virtual void	SwitchMode( bool commander, bool force ) = 0;
	virtual void	LevelInit( const char *newmap ) = 0;
	virtual void	LevelShutdown( void ) = 0;
};

extern IVModeManager *modemanager;
