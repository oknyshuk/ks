//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "c_basedoor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CBaseDoor
#undef CBaseDoor
#endif

IMPLEMENT_REFLECT_CLIENTCLASS( C_BaseDoor, DT_BaseDoor, CBaseDoor )

C_BaseDoor::C_BaseDoor( void )
{
	m_flWaveHeight = 0.0f;
}

C_BaseDoor::~C_BaseDoor( void )
{
}
