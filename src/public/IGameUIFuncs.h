#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core;
#include "inputsystem/ButtonCode.h"

class IGameUIFuncs
{
public:
	virtual bool		IsKeyDown( const char *keyname, bool& isdown ) = 0;
	virtual const char	*GetBindingForButtonCode( ButtonCode_t code ) = 0;
	virtual ButtonCode_t GetButtonCodeForBind( const char *pBind, int userId = -1 ) = 0;
	virtual void		GetVideoModes( struct vmode_s **liststart, int *count ) = 0;
	virtual void		SetFriendsID( uint friendsID, const char *friendsName ) = 0;
	virtual void		GetDesktopResolution( int &width, int &height ) = 0;
};

inline constexpr const char *VENGINE_GAMEUIFUNCS_VERSION = "VENGINE_GAMEUIFUNCS_VERSION005";
