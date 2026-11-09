// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "c_movie_display.h"

IMPLEMENT_REFLECT_CLIENTCLASS( C_MovieDisplay, DT_MovieDisplay, CMovieDisplay )

C_MovieDisplay::C_MovieDisplay()
{
}

C_MovieDisplay::~C_MovieDisplay()
{
}

//-----------------------------------------------------------------------------
// Purpose: Recieve a message from the server
//-----------------------------------------------------------------------------
void C_MovieDisplay::ReceiveMessage( int classID, bf_read &msg )
{
	// Make sure our IDs match
	if ( classID != GetClientClass()->m_ClassID )
	{
		// Message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	// currently we only get one message - that we want to take over as the master
	m_bWantsToBeMaster = true;
}
