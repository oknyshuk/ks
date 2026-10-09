#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Shared data definition file for touch links

#include "ehandle.h"

#if defined( CLIENT_DLL )
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;
#else
class CBaseEntity;
typedef CHandle<CBaseEntity> EHANDLE;
#endif

//-----------------------------------------------------------------------------
// Purpose: for resolving touch/untouch pairs
//-----------------------------------------------------------------------------
enum touchlink_flags_t
{
	FTOUCHLINK_START_TOUCH = 0x00000001,
};

struct touchlink_t
{
#if defined( CLIENT_DLL )
	C_BaseEntity		*entityTouched;
#else
	EHANDLE				entityTouched;
#endif
	int					touchStamp;
	touchlink_t			*nextLink;
	touchlink_t			*prevLink;
	int					flags;
};

// means this touchlink is managed external to the main physics system
inline constexpr auto TOUCHSTAMP_EVENT_DRIVEN = -1;
