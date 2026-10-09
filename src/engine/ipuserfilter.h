#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// filter.h

#include "userid.h"

typedef struct
{
	unsigned	mask;
	unsigned	compare;
	float       banEndTime; // 0 for permanent ban
	float       banTime;
} ipfilter_t;

typedef struct
{
	USERID_t userid;
	float	banEndTime;
	float	banTime;
} userfilter_t;

inline constexpr int MAX_IPFILTERS = 32768;
inline constexpr int MAX_USERFILTERS = 32768;
