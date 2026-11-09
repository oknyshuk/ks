// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// The client / server / handler classes of the engine are closed hierarchies (game vs HLTV). A base class
// carries a kind tag; what used to be a virtual call is a switch on the tag, defined here where both
// derived classes are complete.

import ks.tier1;
#include "common.h"
#include "quakedef.h"
#include "hltvserver.h"
#include "sv_client.h"
#include "hltvclient.h"
#include "server.h"
#include "client.h"
#include "hltvclientstate.h"
#include "cl_demo.h"
#include "hltvdemo.h"
import std;

#include "facade/inetchannelhandler.inl"
#include "facade/iclient.inl"
#include "facade/iserver.inl"
#include "facade/baseclient.inl"
#include "facade/baseclientstate.inl"
#include "facade/baseserver.inl"
#include "facade/idemo.inl"

void CClientFrame::DeleteThis()
{
	if ( m_bHltvFrame )
		delete static_cast<CHLTVFrame*>( this );
	else
		delete this;
}
