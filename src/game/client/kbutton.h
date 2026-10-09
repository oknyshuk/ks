#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "shareddefs.h"

struct kbutton_t
{
	struct Split_t
	{
		// key nums holding it down
		int		down[ 2 ];		
		// low bit is down state
		int		state;			
	};

	Split_t		&GetPerUser( int nSlot = -1 );

	Split_t		m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];
};
