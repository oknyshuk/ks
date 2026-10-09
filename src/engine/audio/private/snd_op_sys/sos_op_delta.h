#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

#include "sos_op.h"

//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorDelta_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	
	float m_flLastInput;
	float m_flLastTime;

};

class CSosOperatorDelta : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorDelta )

};
