// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "c_te_basebeam.h"

//-----------------------------------------------------------------------------
// Purpose: Contains common variables for all beam TEs
//-----------------------------------------------------------------------------
C_TEBaseBeam::C_TEBaseBeam( void )
{
	m_nModelIndex	= 0;
	m_nHaloIndex	= 0;
	m_nStartFrame	= 0;
	m_nFrameRate	= 0;
	m_fLife			= 0.0;
	m_fWidth		= 0;
	m_fEndWidth		= 0;
	m_nFadeLength	= 0;
	m_fAmplitude	= 0;
	r = g = b = a = 0;
	m_nSpeed		= 0;
	m_nFlags		= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBaseBeam::~C_TEBaseBeam( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBaseBeam::PreDataUpdate( DataUpdateType_t updateType ) 
{ 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBaseBeam::PostDataUpdate( DataUpdateType_t updateType )
{
	contract_assert( 0 );
}


IMPLEMENT_CLIENTCLASS(C_TEBaseBeam, DT_BaseBeam, CTEBaseBeam);

IMPLEMENT_REFLECT_TABLE( C_TEBaseBeam, DT_BaseBeam );
