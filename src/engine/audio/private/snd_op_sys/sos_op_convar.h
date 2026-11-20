#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import ks.platform;
import ks.simd;
import ks.tier1.convar;
#include "sos_op.h"
import std;

//-----------------------------------------------------------------------------
// simple operator for getting a convar value
//-----------------------------------------------------------------------------
struct CSosOperatorConvar_t : CSosOperator_t
{
	CSosOperatorConvar_t()
		:
	m_ConVarRef( ( ConVar * )nullptr )
	{
		// Do nothing...
	}

	char m_nConvar[64];
	ConVarRef m_ConVarRef;
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorConvar : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorConvar )

};


//-----------------------------------------------------------------------------
// simple operator for setting a convar value
//-----------------------------------------------------------------------------
struct CSosOperatorSetConvar_t : CSosOperator_t
{
	CSosOperatorSetConvar_t()
		:
	m_ConVarRef( ( ConVar * )nullptr )
	{
		// Do nothing...
	}

	char m_nConvar[64];
	ConVarRef m_ConVarRef;
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
};

class CSosOperatorSetConvar : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSetConvar )

};
