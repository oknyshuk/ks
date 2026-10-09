#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorEntryTime_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutputSoundDuration, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputSoundElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputEntryElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputStopElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputHostElapsed, SO_SINGLE )

	HSOUNDSCRIPTHASH m_nScriptHash;
};

class CSosOperatorEntryTime : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorEntryTime )

};
