// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: random stream class

import std;
import ks.vstdlib;
#include "cdll_int.h"

//-----------------------------------------------------------------------------
//
// implementation of IUniformRandomStream
//
//-----------------------------------------------------------------------------
class CEngineUniformRandomStream : public IUniformRandomStream
{
public:
	// Sets the seed of the random number generator
	void	SetSeed( int iSeed )
	{
		// Never call this from the client or game!
		contract_assert( 0 );
	}

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f )
	{
		return ::RandomFloat( flMinVal, flMaxVal );
	}

	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f )
	{
		return ::RandomFloatExp( flMinVal, flMaxVal, flExponent );
	}

	int		RandomInt( int iMinVal, int iMaxVal )
	{
		return ::RandomInt( iMinVal, iMaxVal );
	}
};

static CEngineUniformRandomStream s_EngineRandomStream;
static InterfaceReg s_Reg_CEngineUniformRandomStream_IUniformRandomStream( []() -> void * { return static_cast<IUniformRandomStream *>( &s_EngineRandomStream ); }, VENGINE_CLIENT_RANDOM_INTERFACE_VERSION );;
