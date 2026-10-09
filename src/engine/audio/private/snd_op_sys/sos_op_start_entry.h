#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct CSosOperatorStartEntry_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputStart, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputStartDelay, SO_SINGLE )
	HSOUNDSCRIPTHASH m_nScriptHash;
	int m_nHasStarted;
	bool m_bTriggerOnce;
};

class CSosOperatorStartEntry : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorStartEntry )
};
