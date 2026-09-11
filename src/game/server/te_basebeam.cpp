//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "te_basebeam.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEBaseBeam::CTEBaseBeam( const char *name ) :
  CBaseTempEntity( name )
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
CTEBaseBeam::~CTEBaseBeam( void )
{
}



IMPLEMENT_REFLECT_SERVERCLASS( CTEBaseBeam, DT_BaseBeam )

