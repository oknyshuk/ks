#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.vstdlib;
#include "filesystem_engine.h"
#include "proto_oob.h"
#include "net_chan.h"
#include "inetmsghandler.h"
#include "protocol.h" // CONNECTIONLESS_HEADER
#include "sv_filter.h"
#include "sys.h"
#define BZ_NO_STDIO
#include "../utils/bzip2/bzlib.h"


import std;

inline constexpr int IPPROTO_TCP = 6;
inline constexpr int IPPROTO_UDP = 17;

#undef SOCKET
typedef ks::platform::socket *SOCKET;


#include "sv_rcon.h"
#ifndef DEDICATED
#include "cl_rcon.h"
#endif

void Con_RunFrame( void );									// call to handle socket updates, etc.

