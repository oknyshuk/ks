// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2003 Valve Corporation

// main precompiled header for client files

import std;
import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.interfaces;








#ifndef DEDICATED
#include "engineui.h"
#include "localize/ilocalize.h"
#endif

#include "icolorcorrectiontools.h"
#include "materialsystem/IColorCorrection.h"

const int g_nPreviewImageWidth  = 128;
const int g_nPreviewImageHeight =  96;

ConVar mat_colorcorrection( "mat_colorcorrection", "1", FCVAR_CHEAT );
ConVar mat_colcorrection_disableentities( "mat_colcorrection_disableentities", "0" );
ConVar mat_colcorrection_editor( "mat_colcorrection_editor", "0" );


//-----------------------------------------------------------------------------
// Main interface to the performance tools 
//-----------------------------------------------------------------------------

class CColorCorrectionTools : public IColorCorrectionTools
{
public:
	void		Init( void );
	void		Shutdown( void );

	bool		ShouldPause() const;

	void		GrabPreColorCorrectedFrame( int x, int y, int width, int height );
	void		UpdateColorCorrection( );

	void		SetFinalOperation( IColorOperation *pOp );
};

#include "facade/icolorcorrectiontools.inl"


static CColorCorrectionTools g_ColorCorrectionTools;
IColorCorrectionTools *colorcorrectiontools = &g_ColorCorrectionTools;

void CColorCorrectionTools::Init( void )
{
}

void CColorCorrectionTools::Shutdown( void )
{
}

bool CColorCorrectionTools::ShouldPause() const
{
	return false;
}

void CColorCorrectionTools::GrabPreColorCorrectedFrame( int x, int y, int width, int height )
{
}

void CColorCorrectionTools::UpdateColorCorrection( )
{
}

void CColorCorrectionTools::SetFinalOperation( IColorOperation *pOp )
{
}

void PrintColorCorrection()
{
	ConMsg( "Default weight : %0.5f\n", colorcorrection->GetLookupWeight(-1) );
	ConMsg( "Weight 0       : %0.5f\n", colorcorrection->GetLookupWeight(0) );
	ConMsg( "Weight 1       : %0.5f\n", colorcorrection->GetLookupWeight(1) );
	ConMsg( "Weight 2       : %0.5f\n", colorcorrection->GetLookupWeight(2) );
	ConMsg( "Weight 3       : %0.5f\n", colorcorrection->GetLookupWeight(3) );
}

static ConCommand print_colorcorrection( "print_colorcorrection", PrintColorCorrection, "Display the color correction layer information.", FCVAR_CHEAT );
