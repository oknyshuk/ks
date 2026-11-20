#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.platform;
import ks.platform;
import ks.simd;
#include "inputsystem/ButtonCode.h"

class IGameUIFuncs
{
public:
	bool		IsKeyDown( const char *keyname, bool& isdown );
	const char	*GetBindingForButtonCode( ButtonCode_t code );
	ButtonCode_t GetButtonCodeForBind( const char *pBind, int userId = -1 );
	void		GetVideoModes( struct vmode_s **liststart, int *count );
	void		SetFriendsID( uint friendsID, const char *friendsName );
	void		GetDesktopResolution( int &width, int &height );
};

inline constexpr const char *VENGINE_GAMEUIFUNCS_VERSION = "VENGINE_GAMEUIFUNCS_VERSION005";
