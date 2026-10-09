#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorSysTime_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutputClientElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputHostElapsed, SO_SINGLE )

};

class CSosOperatorSysTime : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSysTime )

};
