#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.platform;
import ks.simd;
import ks.tier1.ns_address;
#include "userid.h"
#include "ipuserfilter.h"
#include "userid.h"

void Filter_Init( void );
void Filter_Shutdown( void );

bool Filter_ShouldDiscardID( unsigned int userid );
bool Filter_ShouldDiscard( const ns_address& adr );
void Filter_SendBan( const ns_address& adr );
const char *GetUserIDString( const USERID_t& id );

bool Filter_IsUserBanned( const USERID_t& userid );

extern std::vector< ipfilter_t > g_IPFilters;
extern std::vector< userfilter_t > g_UserFilters;
